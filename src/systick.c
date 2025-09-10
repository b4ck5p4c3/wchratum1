#include "systick.h"
#include <stdio.h>
#include <stdbool.h>

static bool SysTickDebugEnabled;

void SysTickDebugMain(void) {
  if (!SysTickDebugEnabled)
    return;
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
      printf("Tick of %08lx_%08lx from 0x%08lx_%08lx to 0x%08lx_%08lx\r\n", hiDt, loDt, hiPrev,
             loPrev, hiNext, loNext);
      skip_one = true;
    } else {
      skip_one = false;
    }
    prev = next;
  }
}
