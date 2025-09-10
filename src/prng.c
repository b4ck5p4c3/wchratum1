#include "prng.h"
#include "random.h"
#include "chaskey.h"
#include "systick.h"
#include "inline.h"

#define inline static inline // due to lack of PCG_INLINE and PCG_EXTERN_INLINE
#include "pcg_variants.h"
#undef inline

static uint32_t kPrngKey[4];
static uint32_t kInject, kInCount;
static pcg32i_random_t pcg32;

static void CSPrngInit(void) {
  for (uint32_t* p = kPrngKey; p != ARRAY_END(kPrngKey); p++)
    *p = RandomUInt32();
  // RandomUInt32() stream passes binomial test for only ≈two bits.
  // Few rounds are done over the stream to stretch entropy out of those bits.
  for (int round = 0; round < 16; round++) {  // Too much crypto :-P  That's okay for init()
    uint32_t trng[4];
    for (uint32_t* p = trng; p != ARRAY_END(trng); p++)
      *p = rotlu32(RandomUInt32(), round * 2);
    chaskey8(kPrngKey, trng);  // making kPrngKey[] less biased, hopefully
  }
  kInject = kInCount = 0;
}

static void PcgInit(void) {
  uint64_t hilo = CSPrngUInt64();
  pcg32i_srandom_r(&pcg32, (uint32_t)(hilo >> 32), (uint32_t)(hilo));
}

void PrngInit() {
  CSPrngInit();
  PcgInit();
}

void CSPrngInjectLowbits(uint32_t in) {
  kInCount++;
  const uint32_t shift = kInCount & 31;
  const uint32_t ndx = (kInCount >> 5) & (ARRAY_LEN(kPrngKey) - 1);
  if (!shift)
    kPrngKey[ndx] ^= kInject;
  kInject ^= in << shift;
}

void CSPrngInjectRandom(void) {
  kInCount += 32;
  const uint32_t ndx = (kInCount >> 5) & (ARRAY_LEN(kPrngKey) - 1);
  kPrngKey[ndx] ^= RandomUInt32();
}

uint64_t CSPrngUInt64() {
  uint64_t hilo = SysTickCnt64();
  uint32_t hi = hilo >> 32;
  uint32_t lo = hilo;
  return chaskey8_64x64(lo, hi, kPrngKey);
}

uint64_t CSPrngUInt64x2(uint32_t dst[2]) {
  uint64_t hilo = SysTickCnt64();
  uint32_t hi = hilo >> 32;
  uint32_t lo = hilo;
  return chaskey8_64x128(dst, lo, hi, kPrngKey);
}

uint16_t PcgUInt16() {
  return pcg_setseq_32_xsh_rs_16_random_r(&pcg32);
  // pcg_setseq_32_xsh_rr_16_random_r(&pcg32); takes 31 ticks due to rotation
}

uint32_t PcgUInt32() {
  return pcg32i_random_r(&pcg32);
  // pcg32_random_r(&pcg64); takes 46 ticks as it needs 64x64->64 multiplication
}
