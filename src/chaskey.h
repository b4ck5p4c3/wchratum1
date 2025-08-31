#ifndef UUID_DAD69A2B_C9FF_4AAF_9E1F_11FFC380A46B
#define UUID_DAD69A2B_C9FF_4AAF_9E1F_11FFC380A46B

#include "stdint.h"

void chaskey8(uint32_t msg[4], const uint32_t key[4]);
uint64_t chaskey8_64x64(uint32_t v0, uint32_t v1, const uint32_t key[4]);
uint64_t chaskey8_64x128(uint32_t dst[2], uint32_t v0, uint32_t v1, const uint32_t key[4]);

#endif
