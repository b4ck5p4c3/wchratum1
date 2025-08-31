#include "prng.h"
#include "random.h"
#include "chaskey.h"

#define inline static inline // due to lack of PCG_INLINE and PCG_EXTERN_INLINE
#include "pcg_variants.h"
#undef inline

#include <ch32v30x.h>

static uint32_t kPrngKey[4];
static uint32_t kCtrHigh, kCtrLow;
static pcg32i_random_t pcg32;

void PrngInit() {
  uint32_t ctx[4];
  for (int i = 0; i < 4; i++) {
    kPrngKey[i] = RandomUInt32();
    ctx[i] = RandomUInt32();
  }
  chaskey8(kPrngKey, ctx);  // making key unbiased
  kCtrHigh = kCtrLow = 0;

  uint64_t hilo = CSPrngUInt64();
  pcg32i_srandom_r(&pcg32, (uint32_t)(hilo >> 32), (uint32_t)(hilo));
}

uint64_t CSPrngUInt64() {
  kCtrLow++;
  kCtrHigh += !kCtrLow;
  return chaskey8_64x64(kCtrLow, kCtrHigh, kPrngKey);
}

uint64_t CSPrngUInt64x2(uint32_t dst[2]) {
  kCtrLow++;
  kCtrHigh += !kCtrLow;
  return chaskey8_64x128(dst, kCtrLow, kCtrHigh, kPrngKey);
}

uint16_t PcgUInt16() {
  return pcg_setseq_32_xsh_rs_16_random_r(&pcg32);
  // pcg_setseq_32_xsh_rr_16_random_r(&pcg32); takes 31 ticks due to rotation
}

uint32_t PcgUInt32() {
  return pcg32i_random_r(&pcg32);
  // pcg32_random_r(&pcg64); takes 46 ticks as it needs 64x64->64 multiplication
}
