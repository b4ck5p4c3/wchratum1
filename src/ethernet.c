#include "ethernet.h"
#include "prdk.h"
#include "inline.h"
#include <ch32v30x.h>
#include <stdint.h>
#include <string.h>
#include <debug.h>
// #define printf(x, ...) do { } while (0)

static void EnqueueTxFromInterrupt(PacketBuffer* buf, int len);

#define PHY_PN_SWITCH_AUTO (2 << 2)

#define PHY_ANLPAR_SELECTOR 0x1F
#define PHY_ANLPAR_SELECT_CSMACD 0x01

#define PACKET_POOL_LEN (9)
#define PACKET_RING_LEN (PACKET_POOL_LEN - 1)
#define PACKET_RING_MASK (PACKET_RING_LEN - 1)
#define PACKET_RING_INC(x) do { (x) = ((x) + 1) & PACKET_RING_MASK; } while (0)

static PacketBuffer kPacketPool[PACKET_POOL_LEN];
static ETH_DMADESCTypeDef kReceiveRing[PACKET_RING_LEN], kTransmitRing[PACKET_RING_LEN];
static uint8_t kReceiveRd, kReceiveWr, kTransmitRd, kTransmitWr;
static int8_t kInPool;
_Static_assert(IS_POW2(PACKET_RING_LEN), "PACKET_RING_LEN must be pow2 for PACKET_RING_INC to work");

uint16_t current_phy_address = 0;

bool phy_link_ready = false;

static inline void* getBuffer1Addr(ETH_DMADESCTypeDef* dma) {
  return (void*)dma->Buffer1Addr;
}

static inline unsigned getRxFrameLength(uint32_t Status) {
  return ((Status & ETH_DMARxDesc_FL) >> 16) - 4;
}

static inline PacketBuffer* getPacketFromRing(ETH_DMADESCTypeDef* dma) {
  return (PacketBuffer*)(getBuffer1Addr(dma) - offsetof(PacketBuffer, Frame));
}

static void EthernetDeinitialize() {
  RCC_AHBPeriphResetCmd(RCC_AHBPeriph_ETH_MAC, ENABLE);
  RCC_AHBPeriphResetCmd(RCC_AHBPeriph_ETH_MAC, DISABLE);
}

static void EthernetSoftwareReset() {
  ETH->DMABMR |= ETH_DMABMR_SR;

  while (ETH->DMABMR & ETH_DMABMR_SR) {}
}

static void EthernetSetClock(void) {
  RCC_PLL3Cmd(DISABLE);
  RCC_PREDIV2Config(RCC_PREDIV2_Div2);  // HSE_ext = 8 MHz /2 -> 4 MHz
  RCC_PLL3Config(RCC_PLL3Mul_15);       // 4 MHz *15 -> 60 MHz
  RCC_PLL3Cmd(ENABLE);
#ifdef ETHERNET_DEBUG
  printf("EthernetSetClock waiting...\n");
#endif
  while (RCC_GetFlagStatus(RCC_FLAG_PLL3RDY) == RESET) {}
#ifdef ETHERNET_DEBUG
  printf("EthernetSetClock finished\n");
#endif
}

static void EthernetInitializeRegisters(ETH_InitTypeDef* ethernet_init,
                                        uint16_t phy_address) {
  uint32_t value = 0;

  value = ETH->MACMIIAR;
  value &= MACMIIAR_CR_MASK;
  value |= ETH_MACMIIAR_CR_Div42;
  ETH->MACMIIAR = value;

  value = ETH->MACCR;
  value &= MACCR_CLEAR_MASK;
  value |=
      (uint32_t)(ethernet_init->ETH_AutoNegotiation |
                 ethernet_init->ETH_Watchdog | ethernet_init->ETH_Jabber |
                 ethernet_init->ETH_InterFrameGap |
                 ethernet_init->ETH_CarrierSense | ethernet_init->ETH_Speed |
                 ethernet_init->ETH_ReceiveOwn |
                 ethernet_init->ETH_LoopbackMode | ethernet_init->ETH_Mode |
                 ethernet_init->ETH_ChecksumOffload |
                 ethernet_init->ETH_RetryTransmission |
                 ethernet_init->ETH_AutomaticPadCRCStrip |
                 ethernet_init->ETH_BackOffLimit |
                 ethernet_init->ETH_DeferralCheck);
  ETH->MACCR = value;
#if (ETHERNET_PHY_MODE == ETHERNET_PHY_MODE_10M_INTERNAL)
  ETH->MACCR |= ETH_Internal_Pull_Up_Res_Enable; /*  */
#endif
  ETH->MACFFR =
      (ethernet_init->ETH_ReceiveAll | ethernet_init->ETH_SourceAddrFilter |
       ethernet_init->ETH_PassControlFrames |
       ethernet_init->ETH_BroadcastFramesReception |
       ethernet_init->ETH_DestinationAddrFilter |
       ethernet_init->ETH_PromiscuousMode |
       ethernet_init->ETH_MulticastFramesFilter |
       ethernet_init->ETH_UnicastFramesFilter);

  ETH->MACHTHR = ethernet_init->ETH_HashTableHigh;
  ETH->MACHTLR = ethernet_init->ETH_HashTableLow;

  value = ETH->MACFCR;
  value &= MACFCR_CLEAR_MASK;
  value |= (uint32_t)((ethernet_init->ETH_PauseTime << 16) |
                      ethernet_init->ETH_ZeroQuantaPause |
                      ethernet_init->ETH_PauseLowThreshold |
                      ethernet_init->ETH_UnicastPauseFrameDetect |
                      ethernet_init->ETH_ReceiveFlowControl |
                      ethernet_init->ETH_TransmitFlowControl);
  ETH->MACFCR = value;

  ETH->MACVLANTR = (uint32_t)(ethernet_init->ETH_VLANTagComparison |
                              ethernet_init->ETH_VLANTagIdentifier);

  value = ETH->DMAOMR;
  value &= DMAOMR_CLEAR_MASK;
  value |= (uint32_t)(ethernet_init->ETH_DropTCPIPChecksumErrorFrame |
                      ethernet_init->ETH_ReceiveStoreForward |
                      ethernet_init->ETH_FlushReceivedFrame |
                      ethernet_init->ETH_TransmitStoreForward |
                      ethernet_init->ETH_TransmitThresholdControl |
                      ethernet_init->ETH_ForwardErrorFrames |
                      ethernet_init->ETH_ForwardUndersizedGoodFrames |
                      ethernet_init->ETH_ReceiveThresholdControl |
                      ethernet_init->ETH_SecondFrameOperate);
  ETH->DMAOMR = value;

  ETH_WritePHYRegister(phy_address, PHY_BCR, PHY_Reset);
  ETH_WritePHYRegister(phy_address, PHY_MDIX, PHY_PN_SWITCH_AUTO);
}

static void EthernetInitializeDma(void) {
  for (int i = 0; i < ARRAY_LEN(kPacketPool); i++)
    if (ETHER_ALIGN != (((uintptr_t)kPacketPool[i].Frame) & 3))
      printf("ERR: misaligned kPacketPool[%d].Frame: %p\r\n", i, kPacketPool[i].Frame);

  for (ETH_DMADESCTypeDef* dma = kTransmitRing; dma != ARRAY_END(kTransmitRing); dma++) {
    dma->Status = ETH_DMATxDesc_CIC_TCPUDPICMP_Full | ETH_DMATxDesc_FS | ETH_DMATxDesc_LS |
                  ETH_DMATxDesc_IC;  // not ETH_DMATxDesc_OWN
    dma->ControlBufferSize = 0;
    dma->Buffer1Addr = 0;
    dma->Buffer2NextDescAddr = 0;
  }
  ARRAY_END(kTransmitRing)[-1].Status |= ETH_DMATxDesc_TER;

  // Assuming Descriptor hop length (DSL[4:0]) to be 0. It is true after boot.
  for (ETH_DMADESCTypeDef* dma = kReceiveRing; dma != ARRAY_END(kReceiveRing); dma++) {
    PacketBuffer* pkt = &kPacketPool[dma - kReceiveRing];
    dma->Status = ETH_DMARxDesc_OWN;
    dma->ControlBufferSize = ETH_DMARxDesc_RBS1 & sizeof(pkt->Frame);
    dma->Buffer1Addr = ptrtou(pkt->Frame);
    dma->Buffer2NextDescAddr = 0;
  }
  ARRAY_END(kReceiveRing)[-1].ControlBufferSize |= ETH_DMARxDesc_RER;

  kTransmitRd = kReceiveRd = 0;
  kTransmitWr = kReceiveWr = 0;
  kInPool = PACKET_POOL_LEN - 1;

  ETH->DMATDLAR = ptrtou(kTransmitRing);
  ETH->DMARDLAR = ptrtou(kReceiveRing);

#if (ETHERNET_PHY_MODE == ETHERNET_PHY_MODE_10M_INTERNAL)
  ETH_DMAITConfig(ETH_DMA_IT_NIS | ETH_DMA_IT_R | ETH_DMA_IT_T | ETH_DMA_IT_AIS | ETH_DMA_IT_RBU |
                      ETH_DMA_IT_PHYLINK,
                  ENABLE);
#else
  ToDo()
#endif

  ETH_DMAReceptionCmd(ENABLE);
  ETH_DMATransmissionCmd(ENABLE);
}

void EthernetInitialize(const MACAddress* mac_address) {
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_ETH_MAC | RCC_AHBPeriph_ETH_MAC_Tx |
                            RCC_AHBPeriph_ETH_MAC_Rx,
                        ENABLE);

#if (ETHERNET_PHY_MODE == ETHERNET_PHY_MODE_10M_INTERNAL)
  current_phy_address = 1;
  EthernetSetClock();
  EXTEN->EXTEN_CTR |= EXTEN_ETH_10M_EN;
#else
  ToDo();
#endif

  EthernetDeinitialize();
  EthernetSoftwareReset();

  ETH_InitTypeDef ethernet_init;
  ETH_StructInit(&ethernet_init);
  ethernet_init.ETH_Mode = ETH_Mode_FullDuplex;
#if (ETHERNET_PHY_MODE == ETHERNET_PHY_MODE_10M_INTERNAL)
  ethernet_init.ETH_Speed = ETH_Speed_10M;
#else
  ToDo();
#endif
  ethernet_init.ETH_AutoNegotiation = ETH_AutoNegotiation_Enable;
  ethernet_init.ETH_LoopbackMode = ETH_LoopbackMode_Disable;
  ethernet_init.ETH_RetryTransmission = ETH_RetryTransmission_Disable;
  ethernet_init.ETH_AutomaticPadCRCStrip = ETH_AutomaticPadCRCStrip_Disable;
  ethernet_init.ETH_ReceiveAll = ETH_ReceiveAll_Disable;
  ethernet_init.ETH_BroadcastFramesReception =
      ETH_BroadcastFramesReception_Enable;
  ethernet_init.ETH_PromiscuousMode = ETH_PromiscuousMode_Disable;
  ethernet_init.ETH_MulticastFramesFilter = ETH_MulticastFramesFilter_Perfect;
  ethernet_init.ETH_UnicastFramesFilter = ETH_UnicastFramesFilter_Perfect;
  ethernet_init.ETH_DropTCPIPChecksumErrorFrame =
      ETH_DropTCPIPChecksumErrorFrame_Enable;
  ethernet_init.ETH_ReceiveStoreForward = ETH_ReceiveStoreForward_Enable;
  ethernet_init.ETH_TransmitStoreForward = ETH_TransmitStoreForward_Enable;
  ethernet_init.ETH_ForwardErrorFrames = ETH_ForwardErrorFrames_Enable;
  ethernet_init.ETH_ForwardUndersizedGoodFrames =
      ETH_ForwardUndersizedGoodFrames_Enable;
  ethernet_init.ETH_SecondFrameOperate = ETH_SecondFrameOperate_Disable;
  EthernetInitializeRegisters(&ethernet_init, current_phy_address);

  ETH->MACA0HR = (uint32_t)((mac_address->bytes[5] << 8) | mac_address->bytes[4]);
  ETH->MACA0LR = (uint32_t)(mac_address->bytes[0] | (mac_address->bytes[1] << 8) |
                            (mac_address->bytes[2] << 16) | (mac_address->bytes[3] << 24));

  EthernetInitializeDma();

  NVIC_EnableIRQ(ETH_IRQn);
}

static void PacketDoneFromInterrupt(PacketBuffer* pkt) {
  if (kInPool < 0) {
    kInPool = pkt - kPacketPool;  // TODO: not properly implemented yet.
  } else {
    ETH_DMADESCTypeDef* dma = &kReceiveRing[kReceiveWr];
    const uint32_t rxStatus = dma->Status;
    if (rxStatus & ETH_DMARxDesc_OWN) {
      // FIXME: do something about that
      printf("IMPOSSIBRU! PacketBuffer leaks\r\n");
      return;  // Still OWNed by DMA.
    }
    PACKET_RING_INC(kReceiveWr);
    dma->Buffer1Addr = ptrtou(pkt->Frame);
    dma->Status = ETH_DMARxDesc_OWN;
  }
}

static void EthernetRXInterrupt(void) {
  // XXX: weird WCH CH32V307 behavior is - DMA does not take ownership over the the last buffer
  // if 7 out of 8 are already taken. It raises RBUS instead while still having one free buffer.
  // That behavior is not documented. However it simplifies implementation here & there.
  while (1 /* kReceiveRd != kReceiveWr */) {
    ETH_DMADESCTypeDef* dma = &kReceiveRing[kReceiveRd];
    const uint32_t rxStatus = dma->Status;
    if (rxStatus & ETH_DMARxDesc_OWN)
      return;  // Still OWNed by DMA. Spurious interrupt?
    if (getBuffer1Addr(dma) == 0)
      return;  // OWNed by CPU, but having no PacketBuffer. Spurious interrupt? XXX: potential BUG here.

    PACKET_RING_INC(kReceiveRd);

    int next = PACKET_PROCESSED;
    const bool errorSummary = rxStatus & ETH_DMARxDesc_ES;
    const bool firstAndLastByte = (rxStatus & ETH_DMARxDesc_FS) && (rxStatus & ETH_DMARxDesc_LS);
    if (!errorSummary && firstAndLastByte)
      next = PacketRxInterrupt(getBuffer1Addr(dma), getRxFrameLength(rxStatus));
    if (next >= 0) {
      PacketBuffer* pkt = getPacketFromRing(dma);
      dma->Buffer1Addr = 0;               // keep CPU ownership of Descriptor
      EnqueueTxFromInterrupt(pkt, next);  // TODO: maybe, return back if kTransmitRing is full
    }
    if (next == PACKET_PROCESSED)
      PacketDoneFromInterrupt(getPacketFromRing(dma));
  }
#if 0
  if ((current_rx_dma_descriptor->Status & ETH_DMARxDesc_OWN) != RESET) {
    if ((ETH->DMASR & ETH_DMASR_RBUS) != RESET) {
      ETH->DMASR = ETH_DMASR_RBUS;
      ETH_ResumeDMATransmission();
    }
    return false;
  }

  bool failed = false;

  if (((current_rx_dma_descriptor->Status & ETH_DMARxDesc_ES) == RESET) &&
      ((current_rx_dma_descriptor->Status & ETH_DMARxDesc_LS) != RESET) &&
      ((current_rx_dma_descriptor->Status & ETH_DMARxDesc_FS) != RESET)) {
    *length =
        ((current_rx_dma_descriptor->Status & ETH_DMARxDesc_FL) >> 16) - 4;
    *buffer = (uint8_t*)current_rx_dma_descriptor->Buffer1Addr;
  } else {
    failed = true;
  }
  current_rx_dma_descriptor->Status |= ETH_DMARxDesc_OWN;
  current_rx_dma_descriptor =
      (ETH_DMADESCTypeDef*)(current_rx_dma_descriptor->Buffer2NextDescAddr);
  return !failed;
#endif
}

static void EthernetTXInterrupt(void) {
  while (1) {
    ETH_DMADESCTypeDef* dma = &kTransmitRing[kTransmitWr];
    const uint32_t txStatus = dma->Status;
    if (txStatus & ETH_DMARxDesc_OWN)
      return;  // Still OWNed by DMA. Spurious interrupt?
    if (getBuffer1Addr(dma) == 0)
      return;  // OWNed by CPU, but having no PacketBuffer. Spurious interrupt? XXX: potential BUG here.
    PacketDoneFromInterrupt(getPacketFromRing(dma));
    dma->Buffer1Addr = 0;
    PACKET_RING_INC(kTransmitWr);
  }
}

static void EthernetPHYBusyWait() {
  for (uint32_t i = 0; i < SystemCoreClock / 8; i++) {
    asm volatile("nop");
  }
}

const uint16_t PHYSR_Loopback_10M = UINT16_C(1) << 3;
const uint16_t PHYSR_Full_10M = UINT16_C(1) << 2;

static void Internal10MPhyReset(void) {
  ETH_WritePHYRegister(current_phy_address, PHY_BCR, PHY_Reset);
  EXTEN->EXTEN_CTR &= ~EXTEN_ETH_10M_EN;
  EthernetPHYBusyWait();
  EXTEN->EXTEN_CTR |= EXTEN_ETH_10M_EN;
  phy_link_ready = false;
  EthernetPHYLinkChangeInterrupt(phy_link_ready);
  ETH_WritePHYRegister(current_phy_address, PHY_MDIX, PHY_PN_SWITCH_AUTO);
}

static void EthernetProcessPHYLinkInterrupt() {
#if (ETHERNET_PHY_MODE == ETHERNET_PHY_MODE_10M_INTERNAL)
  const uint16_t phy_anlpar = ETH_ReadPHYRegister(current_phy_address, PHY_ANLPAR);
  const uint16_t phy_basic_status = ETH_ReadPHYRegister(current_phy_address, PHY_BSR);

  if ((phy_basic_status & PHY_Linked_Status) && (phy_anlpar == 0)) {
    Internal10MPhyReset();
    return;
  }

  if ((phy_basic_status & PHY_Linked_Status) && (phy_basic_status & PHY_AutoNego_Complete)) {
    const uint16_t phy_status = ETH_ReadPHYRegister(current_phy_address, PHY_STATUS);
    if (phy_status & PHYSR_Full_10M) {
      ETH->MACCR |= ETH_Mode_FullDuplex;
    } else if ((phy_anlpar & PHY_ANLPAR_SELECTOR) != PHY_ANLPAR_SELECT_CSMACD) {
      ETH->MACCR |= ETH_Mode_FullDuplex;
    } else {
      ETH->MACCR &= ~ETH_Mode_FullDuplex;
    }
    ETH->MACCR &= ~(ETH_Speed_100M | ETH_Speed_1000M);
    phy_link_ready = true;
    EthernetPHYLinkChangeInterrupt(phy_link_ready);
    ETH_Start();
  } else {
    Internal10MPhyReset();
  }
#else
  ToDo();
#endif
}

static bool IsTxDmaSuspended(void) {
  uint32_t txProcessStatus = (ETH->DMASR & ETH_DMASR_TPS);
  printf("txProcessStatus: %08lx\r\n", txProcessStatus);
  return txProcessStatus == ETH_DMASR_TPS_Suspended;
}

static void EnqueueTxFromInterrupt(PacketBuffer* pkt, int len) {
  ETH_DMADESCTypeDef* dma = &kTransmitRing[kTransmitRd];
  const uint32_t txStatus = dma->Status;
  if (txStatus & ETH_DMATxDesc_OWN) {
    PacketDoneFromInterrupt(pkt);
    return;  // FIXME: account TX queue overrun
  }

  PACKET_RING_INC(kTransmitRd);

  dma->ControlBufferSize = len & ETH_DMATxDesc_TBS1;
  dma->Buffer1Addr = ptrtou(pkt->Frame);
  /* fence ? */
  dma->Status = txStatus | ETH_DMATxDesc_OWN;

  if (IsTxDmaSuspended()) {
    printf("TX RESUMING\r\n");
    ETH_ResumeDMATransmission();
  } else {
    printf("TX ONGOING\r\n");
  }
}

bool EthernetTransmit(const uint8_t* packet, uint32_t length) {
#if 0
  if (!phy_link_ready) {
    return false;
  }

  if ((current_tx_dma_descriptor->Status & ETH_DMATxDesc_OWN) != RESET) {
    printf("nigga!!\n");
    return false;
  }

  current_tx_dma_descriptor->ControlBufferSize = (length & ETH_DMATxDesc_TBS1);
  memcpy((uint8_t*) current_tx_dma_descriptor->Buffer1Addr, packet, length);

#ifdef ETHERNET_USE_HARDWARE_CHECKSUM
  current_tx_dma_descriptor->Status |=
      ETH_DMATxDesc_LS | ETH_DMATxDesc_FS | ETH_DMATxDesc_CIC_TCPUDPICMP_Full;
#else
  current_tx_dma_descriptor->Status |= ETH_DMATxDesc_LS | ETH_DMATxDesc_FS;
#endif

  current_tx_dma_descriptor->Status |= ETH_DMATxDesc_OWN;

  if ((ETH->DMASR & ETH_DMASR_TBUS) != RESET) {
    ETH->DMASR = ETH_DMASR_TBUS;
    ETH_ResumeDMATransmission();
  }
  current_tx_dma_descriptor =
      (ETH_DMADESCTypeDef*)(current_tx_dma_descriptor->Buffer2NextDescAddr);
  return true;
#endif
}

__attribute__((weak)) void EthernetPHYLinkChangeInterrupt(bool phy_link_ready) {}

__attribute__((interrupt("WCH-Interrupt-fast"))) void ETH_IRQHandler() {
  uint32_t status = ETH->DMASR;

  // Note well, MMCI, PMTI and TSTI are unconditionally enabed.
  printf("\r\nETH_IRQHandler: %08lx\r\n", status);

  _Static_assert(ARRAY_LEN(kReceiveRing) == ARRAY_LEN(kTransmitRing), "owned[] assumes equality");
  char owned[ARRAY_LEN(kReceiveRing) + 1];
  for (int i = 0; i < ARRAY_LEN(kReceiveRing); i++)
    owned[i] = (kReceiveRing[i].Status & ETH_DMARxDesc_OWN) ? '_' : 'C';
  owned[ARRAY_LEN(kReceiveRing)] = 0;
  printf("Rx owned: %s, ", owned);
  for (int i = 0; i < ARRAY_LEN(kTransmitRing); i++)
    owned[i] = (kTransmitRing[i].Status & ETH_DMATxDesc_OWN) ? '_'
               : (getBuffer1Addr(&kTransmitRing[i]) == 0)    ? '0'
                                                             : 'C';
  printf("Tx owned: %s\r\n", owned);

  if (status & ETH_DMASR_AIS) {  // Abnormal interrupt
    printf("Abnormal interrupt:\r\n");
    // Neither enabled, nor handled:
    //  - DMASR[1]: The sending process is stopped;
    //  - DMASR[3]: Send Jabber timeout;
    //  - DMASR[4]: Receive FIFO overflow;
    //  - DMASR[5]: transmit data underflow;
    //  - DMASR[8]: The receiving process is stopped;
    //  - DMASR[9]: Receive watchdog timeout;
    //  - DMASR[10]: Early transmit;
    //  - DMASR[13]: Bus error.
    if (status & ETH_DMASR_RBUS) {  // -DMASR[7]: Receive buffer unavailable.
      printf("Receive buffer unavailable.\r\n");
      // ETH_MACReceptionCmd(DISABLE);
      // ETH_DMAReceptionCmd(DISABLE);
      // ETH_DMAReceptionCmd(ENABLE);
      // ETH_DMAITConfig(ETH_DMA_IT_RBU, DISABLE);
      // ETH_ResumeDMATransmission();
      ETH_DMAClearITPendingBit(ETH_DMA_IT_RBU);
    }
    ETH_DMAClearITPendingBit(ETH_DMA_IT_AIS);
  }

  if (status & ETH_DMASR_NIS) {  // Normal interrupt summary
    printf("Normal interrupt:\r\n");
    if (status & ETH_DMASR_RS) {  // DMASR[6]: Receive interrupt
      EthernetRXInterrupt();
      ETH_DMAClearITPendingBit(ETH_DMA_IT_R);
    }
    if (status & ETH_DMASR_TS) {  // DMASR[0]: Send interrupt
      EthernetTXInterrupt();
      ETH_DMAClearITPendingBit(ETH_DMA_IT_T);
    }
    if (status & ETH_DMA_IT_PHYLINK) {  // DMASR[31]:Internal 10M PHY connection state change
      printf("PHY connection state change\r\n");
      EthernetProcessPHYLinkInterrupt();
      ETH_DMAClearITPendingBit(ETH_DMA_IT_PHYLINK);
    }
    if (status & ETH_DMASR_TBUS) {  // DMASR[2]: The transmit buffer is not available
      printf("Transmit buffer not available\r\n");
      // ETH_ResumeDMATransmission();
      ETH_DMAClearITPendingBit(ETH_DMA_IT_TBU);
      // XXX: Note, ETH_DMAITConfig() does not emable ETH_DMA_IT_TBU.
    }
    // ETH_DMAITConfig() does not enable ETH_DMA_IT_ER (DMASR[14]: Early receive interrupt)
    ETH_DMAClearITPendingBit(ETH_DMA_IT_NIS);
  }
}
