#include <ch32v30x.h>
#include <debug.h>

#include "dhcp.h"
#include "ethernet.h"
#include "ethaddr.h"
#include "random.h"
#include "prng.h"
#include "esig.h"
#include "chaskey.h"

MACAddress kMACAddress;

void EthernetPHYLinkChangeInterrupt(bool phy_link_ready) {
  if (phy_link_ready) {
    printf("Link is UP\n");
    DHCPReset();
  } else {
    printf("Link is DOWN\n");
  }
}

const uint8_t kTestPacket[] =
    "\xff\xff\xff\xff\xff\xff\x04\x18\xd6\xc3\x2e\xbf\x08\x00"
    "\x45\x00\x01\x1c\x00\x00\x40\x00\x40\x11\x2d\xce\x0a\x00\x02\x04"
    "\xff\xff\xff\xff"
    "\xde\x49\x27\x11\x01\x08\xc8\x28"
    "\x02\x0b\x00\x5a\x02\x00\x0a\x04\x18\xd6\xc3\x2e\xbf\x0a\x00\x02"
    "\x04\x01\x00\x06\x04\x18\xd6\xc3\x2e\xbf\x13\x00\x06\x04\x18\xd6"
    "\xc3\x2e\xbf\x12\x00\x01\x48\x0a\x00\x04\x00\x15\xfb\x4d\x16\x00"
    "\x0a\x35\x2e\x30\x2e\x31\x34\x2e\x36\x36\x30\x1b\x00\x0a\x35\x2e"
    "\x30\x2e\x31\x34\x2e\x36\x36\x30\x15\x00\x03\x55\x50\x35\x1d\x00"
    "\x09\x34\x2e\x38\x2e\x33\x2e\x38\x32\x32\x17\x00\x01\x01\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";

void EthernetReceivedInterrupt(const uint8_t* packet, uint32_t length) {
  printf("Received %lu: ", length);
  for (uint32_t i = 0; i < length; i++) {
    printf("%02x ", packet[i]);
  }
  printf("\n");
}

void ProcessDHCP() {
  switch (DHCPGetState()) {
    case DHCP_STATE_NONE:
    case DHCP_STATE_SENT_DISCOVER:
      DHCPSendDiscover(&kMACAddress);
      break;
  }
}

static void PrintUniID(void) {
  printf("Data0: %04x\r\n", OB->Data0);
  printf("Data1: %04x\r\n", OB->Data1);
  printf("FlaCap: %04x\r\n", R16_ESIG_FLACAP());
  printf("UniID1: %08lx\r\n", R32_ESIG_UNIID1());
  printf("UniID2: %08lx\r\n", R32_ESIG_UNIID2());
  printf("UniID3: %08lx\r\n", R32_ESIG_UNIID3());
}

static const uint32_t STK_CTLR_STCLK = UINT32_C(1) << 2; // 0: HCLK/8, 1: HCLK serves as time base.
static const uint32_t STK_CTLR_STE = UINT32_C(1) << 0; // 0: counter stops, 1: STK enabled.

static void uBench(void) {
  const unsigned count = 4096;
  SysTick->CTLR |= STK_CTLR_STE;
  const unsigned shift = (SysTick->CTLR & STK_CTLR_STCLK) ? 0 : 3;
  uint32_t tmp = PcgUInt32(), msg[4], key[4];
  for (int i = 0; i < 4; i++)
    msg[i] = PcgUInt32(), key[i] = PcgUInt32();
  const uint64_t start = SysTick->CNT;
  for (unsigned i = count; i; i--) {
    switch (5) {
      case 1:
        tmp ^= SysTick->CNT;  // 6 ticks, same for ^=, += and *=
        break;
      case 2:
        chaskey8(msg, key);  // 262 ticks
        break;
      case 3:
        tmp += RandomUInt32();  // ~48 ticks
        break;
      case 4:
        tmp += CSPrngUInt64();  // 267 ticks
        break;
      case 5:
        tmp += PcgUInt32();  // 27 ticks
        break;
      default:
        __NOP();  // ~4.00 tick/call
    }
  }
  const uint64_t end = SysTick->CNT;
  const uint64_t dt = end - start;
  const uint64_t dtick = dt << shift;
  const float perop = ((float)dtick) / count;
  printf("uBench: %lu dt%s\r\n", (uint32_t)dt, (dt >> 32) ? " WARN: 64-bit!" : "");
  printf("uBench: ~%lu tick%s\r\n", (uint32_t)dtick, (dtick >> 32) ? " WARN: 64-bit!" : "");
  printf("uBench: ~%d.%02d tick/call\r\n", (int)perop, (int)(perop * 100) % 100);
  printf("uBench: 0x%lx goats teleported\r\n", tmp ^ msg[0] ^ key[0]);
}

int main() {
  SystemCoreClockUpdate();
  Delay_Init();
  USART_Printf_Init(115200);
  RandomInitialize();
  PrngInit();

  printf("Initializing\r\n");
  printf("Clock: %lu\r\n", SystemCoreClock);
  PrintUniID();
  uBench();

  MacAddressInitialize(kMACAddress.bytes, 0);
  printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
         kMACAddress.bytes[0], kMACAddress.bytes[1], kMACAddress.bytes[2],
         kMACAddress.bytes[3], kMACAddress.bytes[4], kMACAddress.bytes[5]);
  EthernetInitialize(&kMACAddress);

  for (;;) {
    printf("Running: %d\n", EthernetTransmit(kTestPacket, sizeof(kTestPacket) - 1));
    ProcessDHCP();
    Delay_Ms(1000);
  }
}
