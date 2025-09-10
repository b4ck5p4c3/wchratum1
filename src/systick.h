#ifndef UUID_1D827417_88B9_4A3D_84C5_FB9E83DFA122
#define UUID_1D827417_88B9_4A3D_84C5_FB9E83DFA122

#include <ch32v30x.h>

static const uint32_t STK_CTLR_STE = UINT32_C(1) << 0; // 0: counter stops, 1: STK enabled.
static const uint32_t STK_CTLR_STCLK = UINT32_C(1) << 2; // 0: HCLK/8, 1: HCLK serves as time base.

static inline uint32_t SysTickCnt32(void) {
  vu32 *lohi = (vu32*)&SysTick->CNT;
  return lohi[0];
}

// ch32v30x has no way to read 64-bit register in an atomic way. If we read $low first and $high
// second (like gcc does, while reading SysTick->CNT as a whole), than the following is posisble:
//   Loop tick of 00000001_00000011 from 0x00000132_ffffffee to 0x00000133_ffffffff
// If we read $high first, and $low second, than the opposite situation is possible.  So the code
// read $high twice.  It's not bulletproof in sight of possible interrupts, but it's probably okay
// as Interrupts are usually short and $high ticks every ≈29 seconds (or ≈238s in case of HCLK/8).
static inline uint64_t SysTickCnt64(void) {
  vu32 *lohi = (vu32*)&SysTick->CNT;
  uint32_t hiBefore = lohi[1];
  uint32_t lo = lohi[0];
  uint32_t hiAfter = lohi[1];
  uint32_t hi = (lo & 0x80000000) ? hiBefore : hiAfter;
  return (((uint64_t)hi) << 32) | lo;
}

void SysTickDebugMain(void);

#endif // 1D827417_88B9_4A3D_84C5_FB9E83DFA122
