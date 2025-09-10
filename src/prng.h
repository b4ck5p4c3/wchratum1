#ifndef UUID_0F2378B8_FE48_4D63_ADE7_3F0713B0CFE7
#define UUID_0F2378B8_FE48_4D63_ADE7_3F0713B0CFE7

#include <stdint.h>

void PrngInit(void);

// That's not djb's fast-key-erasure RNG, but it still some form of key erasure.  Also, it improves
// bit bias measurably: Chaskey8-CTR does not pass scipy.stats.binomtest without key erasure.
void CSPrngInjectLowbits(uint32_t x);
void CSPrngInjectRandom(void);

uint16_t PcgUInt16(void);                  // 24 ticks
uint32_t PcgUInt32(void);                  // 27 ticks
uint64_t CSPrngUInt64(void);               // 264 ticks
uint64_t CSPrngUInt64x2(uint32_t dst[2]);  // 266 ticks

#endif // 0F2378B8_FE48_4D63_ADE7_3F0713B0CFE7
