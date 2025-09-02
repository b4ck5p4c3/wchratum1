/* SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2025 Egor Koleda, Leonid Evdokimov. All Rights Reserved.
 */
#ifndef UUID_144A7A95_2BD3_4246_9062_E04F93608132
#define UUID_144A7A95_2BD3_4246_9062_E04F93608132

#include <sys/types.h>
// toolchain-riscv/riscv-wch-elf/include/sys/types.h declares in_addr_t and in_port_t
#define _IN_TYPES_DEFINED_ 1
#include <net/if_arp.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

// Use index=0 for R32_ETH_MACA0, index=1 for R32_ETH_MACA1 and so on.
void MacAddressInitialize(uint8_t mac[ETHER_ADDR_LEN], unsigned index);

#endif // 144A7A95_2BD3_4246_9062_E04F93608132
