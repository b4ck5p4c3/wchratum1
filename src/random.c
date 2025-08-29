#include "random.h"

#include <ch32v30x.h>
#include <ch32v30x_rng.h>
#include <ch32v30x_rcc.h>

void RandomInitialize() {
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_RNG, ENABLE);
  RNG_Cmd(ENABLE);
}

uint32_t RandomUInt32() {
  while (RNG_GetFlagStatus(RNG_FLAG_DRDY) == RESET) {}
  return RNG_GetRandomNumber();
}