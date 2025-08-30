#ifndef UUID_2E75090C_CA49_4134_9336_75DA0831EA34
#define UUID_2E75090C_CA49_4134_9336_75DA0831EA34

#include <ch32v30x.h>

// Chapter 31 Electronic Signature (ESIG).
//
// This chapter applies to the whole family of CH32F20x, CH32V20x,
// CH32V30x and CH32V31x.
//
// The electronic signature contains chip identification information:
// The capacity of the flash memory area and the unique identification.
// It is programmed into the system storage area of the memory module
// by the manufacturer at the factory, and can be read by SWD (SDI) or
// application code.

// Flash capacity register
static uint16_t R16_ESIG_FLACAP() {
  return *(__I uint16_t*)(0x1FFFF7E0);
}

#ifndef ROM_CFG_USERADR_ID
#  define ROM_CFG_USERADR_ID 0x1FFFF7E8
#endif

static uint32_t R32_ESIG_UNIID1() {
  return *(__I uint32_t*)(ROM_CFG_USERADR_ID);
}

static uint32_t R32_ESIG_UNIID2() {
  return *(__I uint32_t*)(ROM_CFG_USERADR_ID + 4);
}

// My WCH CH32V307 devboard reports 0xFFFFFFFF as UNIID3 ¯\_(ツ)_/¯
static uint32_t R32_ESIG_UNIID3() {
  return *(__I uint32_t*)(ROM_CFG_USERADR_ID + 8);
}

#endif
