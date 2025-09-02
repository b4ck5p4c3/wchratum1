#include <ch32v30x.h>
#include <debug.h>

#include "dhcp.h"
#include "ethernet.h"
#include "ethaddr.h"
#include "random.h"
#include "prng.h"
#include "esig.h"
#include "chaskey.h"
#include "systick.h"

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

typedef enum {
  readSysTick,
  callSysTick32,
  callSysTick64,
  callChaskey8,
  callRandomUInt32,
  callCSPrngUInt64,
  callCSPrngUInt64x2,
  callPcgUInt32,
} uBenchEnum;

static void uBench(void) {
  const unsigned count = 4096;
  SysTick->CTLR |= STK_CTLR_STE;
  const unsigned shift = (SysTick->CTLR & STK_CTLR_STCLK) ? 0 : 3;
  uint32_t tmp = PcgUInt32(), msg[4], key[4];
  for (int i = 0; i < 4; i++)
    msg[i] = PcgUInt32(), key[i] = PcgUInt32();
  const uint64_t start = SysTick->CNT;
  for (unsigned i = count; i; i--) {
    switch (callPcgUInt32) {
      case readSysTick:
        tmp ^= SysTick->CNT;  // 6 ticks, same for ^=, += and *=
        break;
      case callSysTick32:
        tmp ^= SysTickCnt32(); // 5 ticks
        break;
      case callSysTick64:
        tmp ^= SysTickCnt64();  // 14 tick
        break;
      case callChaskey8:
        chaskey8(msg, key);  // 262 ticks
        break;
      case callRandomUInt32:
        tmp += RandomUInt32();  // ~48 ticks
        break;
      case callCSPrngUInt64:
        tmp += CSPrngUInt64();
        break;
      case callCSPrngUInt64x2:
        tmp += CSPrngUInt64(msg);
        break;
      case callPcgUInt32:
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
  for (int i = 0; i < 4; i++)
      tmp ^= msg[i] ^ key[i];
  printf("uBench: 0x%lx goats teleported\r\n", tmp); // ensure that code is not optimized out
}

void TickLoop(void) {
  SysTick->CTLR |= STK_CTLR_STCLK;
  uint64_t prev = SysTickCnt64();
  bool skip_one = false;
  for (;;) {
    uint64_t next = SysTickCnt64();
    uint64_t dt = next - prev;
    uint32_t hiDt = dt >> 32, loDt = dt;
    uint32_t hiPrev = prev >> 32, loPrev = prev;
    uint32_t hiNext = next >> 32, loNext = next;
    if (!skip_one && (hiDt > 0 || loDt > 0x20 || hiPrev != hiNext)) {
      printf("Tick of %08lx_%08lx from 0x%08lx_%08lx to 0x%08lx_%08lx\r\n", hiDt, loDt,
             hiPrev, loPrev, hiNext, loNext);
      skip_one = true;
    } else {
      skip_one = false;
    }
    prev = next;
  }
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
  TickLoop();

  for (;;) {
    printf("Running: %d\n", EthernetTransmit(kTestPacket, sizeof(kTestPacket) - 1));
    ProcessDHCP();
    Delay_Ms(1000);
  }
}
