#ifndef UUID_0F2378B8_FE48_4D63_ADE7_3F0713B0CFE7
#define UUID_0F2378B8_FE48_4D63_ADE7_3F0713B0CFE7

#include <stdint.h>

void PrngInit();

uint16_t PcgUInt16(); // 24 ticks
uint32_t PcgUInt32(); // 27 ticks
uint64_t CSPrngUInt64(); // 268 ticks
uint64_t CSPrngUInt64x2(uint32_t dst[2]); // 269 ticks

#endif // 0F2378B8_FE48_4D63_ADE7_3F0713B0CFE7
