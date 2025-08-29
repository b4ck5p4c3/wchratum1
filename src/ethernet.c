#include "ethernet.h"

#include <ch32v30x.h>
#include <stdint.h>
#include <string.h>
#include <debug.h>

#define PHY_PN_SWITCH_AUTO (2 << 2)

#define PHY_ANLPAR_SELECTOR_FIELD 0x1F
#define PHY_ANLPAR_SELECTOR_VALUE 0x01

__attribute__((__aligned__(
    4))) ETH_DMADESCTypeDef tx_dma_descriptors[ETHERNET_TX_BUFFER_COUNT];
__attribute__((__aligned__(
    4))) ETH_DMADESCTypeDef rx_dma_descriptors[ETHERNET_RX_BUFFER_COUNT];

__attribute__((__aligned__(
    4))) uint8_t tx_buffer[ETHERNET_TX_BUFFER_COUNT * ETHERNET_TX_BUFFER_SIZE];
__attribute__((__aligned__(
    4))) uint8_t rx_buffer[ETHERNET_RX_BUFFER_COUNT * ETHERNET_RX_BUFFER_SIZE];

uint16_t current_phy_address = 0;

ETH_DMADESCTypeDef* current_tx_dma_descriptor;
ETH_DMADESCTypeDef* current_rx_dma_descriptor;

bool phy_link_ready = false;

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
  RCC_PREDIV2Config(RCC_PREDIV2_Div2);
  RCC_PLL3Config(RCC_PLL3Mul_15);
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

#if (ETHERNET_PHY_MODE == ETHERNET_PHY_MODE_10M_INTERNAL)
  ETH_DMAITConfig(ETH_DMA_IT_NIS | ETH_DMA_IT_R | ETH_DMA_IT_T |
                      ETH_DMA_IT_AIS | ETH_DMA_IT_RBU | ETH_DMA_IT_PHYLINK,
                  ENABLE);
#else
  ToDo()
#endif

  ETH_DMATxDescChainInit(tx_dma_descriptors, tx_buffer,
                         ETHERNET_TX_BUFFER_COUNT);
  ETH_DMARxDescChainInit(rx_dma_descriptors, rx_buffer,
                         ETHERNET_RX_BUFFER_COUNT);
  current_tx_dma_descriptor = &tx_dma_descriptors[0];
  current_rx_dma_descriptor = &rx_dma_descriptors[0];
  NVIC_EnableIRQ(ETH_IRQn);
}

static bool EthernetProcessRXInterrupt(uint8_t** buffer, uint32_t* length) {
  if ((current_rx_dma_descriptor->Status & ETH_DMARxDesc_OWN) != RESET) {
    if ((ETH->DMASR & ETH_DMASR_RBUS) != RESET) {
      ETH->DMASR = ETH_DMASR_RBUS;
      ETH->DMARPDR = 0;
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
}

static void EthernetPHYBusyWait() {
  for (uint32_t i = 0; i < SystemCoreClock / 8; i++) {
    asm volatile("nop");
  }
}

static void EthernetProcessPHYLinkInterrupt() {
#if (ETHERNET_PHY_MODE == ETHERNET_PHY_MODE_10M_INTERNAL)
  uint16_t phy_anlpar = ETH_ReadPHYRegister(current_phy_address, PHY_ANLPAR);
  uint16_t phy_status = ETH_ReadPHYRegister(current_phy_address, PHY_BSR);

  if ((phy_status & PHY_Linked_Status) && (phy_anlpar == 0)) {
    ETH_WritePHYRegister(current_phy_address, PHY_BCR, PHY_Reset);
    EXTEN->EXTEN_CTR &= ~EXTEN_ETH_10M_EN;
    EthernetPHYBusyWait();
    EXTEN->EXTEN_CTR |= EXTEN_ETH_10M_EN;
    phy_link_ready = false;
    EthernetPHYLinkChangeInterrupt(phy_link_ready);
    ETH_WritePHYRegister(current_phy_address, PHY_MDIX, PHY_PN_SWITCH_AUTO);
    return;
  }

  if ((phy_status & (PHY_Linked_Status)) &&
      (phy_status & PHY_AutoNego_Complete)) {
    phy_status = ETH_ReadPHYRegister(current_phy_address, PHY_STATUS);
    if (phy_status & (1 << 2)) {
      ETH->MACCR |= ETH_Mode_FullDuplex;
    } else {
      if ((phy_anlpar & PHY_ANLPAR_SELECTOR_FIELD) !=
          PHY_ANLPAR_SELECTOR_VALUE) {
        ETH->MACCR |= ETH_Mode_FullDuplex;
      } else {
        ETH->MACCR &= ~ETH_Mode_FullDuplex;
      }
    }
    ETH->MACCR &= ~(ETH_Speed_100M | ETH_Speed_1000M);
    phy_link_ready = true;
    EthernetPHYLinkChangeInterrupt(phy_link_ready);
    ETH_Start();
  } else {
    ETH_WritePHYRegister(current_phy_address, PHY_BCR, PHY_Reset);
    EXTEN->EXTEN_CTR &= ~EXTEN_ETH_10M_EN;
    EthernetPHYBusyWait(500);
    EXTEN->EXTEN_CTR |= EXTEN_ETH_10M_EN;
    phy_link_ready = false;
    EthernetPHYLinkChangeInterrupt(phy_link_ready);
    ETH_WritePHYRegister(current_phy_address, PHY_MDIX, PHY_PN_SWITCH_AUTO);
  }
#else
  ToDo();
#endif
}

bool EthernetTransmit(const uint8_t* packet, uint32_t length) {
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
    ETH->DMATPDR = 0;
  }
  current_tx_dma_descriptor =
      (ETH_DMADESCTypeDef*)(current_tx_dma_descriptor->Buffer2NextDescAddr);
  return true;
}

__attribute__((weak)) void EthernetPHYLinkChangeInterrupt(bool phy_link_ready) {
}

__attribute__((weak)) void EthernetRXInterrupt(const uint8_t* packet,
                                               uint32_t length) {}

__attribute__((interrupt("WCH-Interrupt-fast"))) void ETH_IRQHandler() {
  uint32_t status = ETH->DMASR;

  if (status & ETH_DMA_IT_AIS) {
    if (status & ETH_DMA_IT_RBU) {
      ETH_DMAClearITPendingBit(ETH_DMA_IT_RBU);
    }
    ETH_DMAClearITPendingBit(ETH_DMA_IT_AIS);
  }

  if (status & ETH_DMA_IT_NIS) {
    if (status & ETH_DMA_IT_R) {
      uint8_t* buffer = NULL;
      uint32_t length = 0;
      if (EthernetProcessRXInterrupt(&buffer, &length)) {
        EthernetReceivedInterrupt(buffer, length);
      }
      ETH_DMAClearITPendingBit(ETH_DMA_IT_R);
    }
    if (status & ETH_DMA_IT_T) {
      ETH_DMAClearITPendingBit(ETH_DMA_IT_T);
    }
    if (status & ETH_DMA_IT_PHYLINK) {
      EthernetProcessPHYLinkInterrupt();
      ETH_DMAClearITPendingBit(ETH_DMA_IT_PHYLINK);
    }
    if (status & ETH_DMA_IT_TBU) {
      ETH->DMATPDR = 0;
      ETH_DMAClearITPendingBit(ETH_DMA_IT_TBU);
    }
    ETH_DMAClearITPendingBit(ETH_DMA_IT_NIS);
  }
}