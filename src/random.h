#ifndef RANDOM_H_
#define RANDOM_H_

#include <stdint.h>

void RandomInitialize();
uint32_t RandomUInt32(); // ~48.70 tick/call

#endif
