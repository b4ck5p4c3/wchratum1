/* SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2025 Egor Koleda, Leonid Evdokimov. All Rights Reserved.
 */
#include "ethaddr.h"
#include "esig.h"
#include "chaskey.h"
#include <string.h>

void MacAddressInitialize(uint8_t mac[ETHER_ADDR_LEN], unsigned index) {
  // "EthernetMAC0Addr" - context for MAC address generation.
  const uint32_t C_ETHE = UINT32_C(0x45746865);
  const uint32_t C_RNET = UINT32_C(0x726e6574);
  const uint32_t C_MAC0 = UINT32_C(0x4d414330);
  const uint32_t C_ADDR = UINT32_C(0x41646472);

  // Numbers from djb's sleeve https://cr.yp.to/snuffle/salsafamily-20071225.pdf
  const uint32_t C_EXPA = UINT32_C(0x61707865);
  const uint32_t C_ND_0 = UINT32_C(0x3020646e);
  const uint32_t C_8_BY = UINT32_C(0x79622d38);
  const uint32_t C_TE_K = UINT32_C(0x6b206574);

  // WCHNET_GetMacAddr() sets MAC=(uint8_t*)(ROM_CFG_USERADR_ID+5) and leaks board ID to the wire.
  // Let's rather use something like KDF despite weak 64-bit key.
  uint32_t ctx[4] = {C_ETHE, C_RNET, C_MAC0 + index, C_ADDR};

  // The first word of key has only few bits of entopy.
  // The last word of tkey is 0xFFFFFFFF on some boards.
  uint32_t key[4] = {
      C_EXPA ^ (((uint32_t)R16_ESIG_FLACAP()) << 16) ^ (((uint32_t)OB->Data1) << 8) ^ OB->Data0,
      C_ND_0 ^ R32_ESIG_UNIID1(),
      C_8_BY ^ R32_ESIG_UNIID2(),
      C_TE_K ^ R32_ESIG_UNIID3(),
  };

  chaskey8(ctx, key);

  ctx[0] ^= ctx[2];
  ctx[1] ^= ctx[3];

  memcpy(mac, ctx, 6);
  mac[0] = (mac[0] & 0xF0) | 0x02;  // Unicast, Locally administered, Administratively assigned
}
