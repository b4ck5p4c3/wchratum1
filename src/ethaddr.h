#ifndef UUID_144A7A95_2BD3_4246_9062_E04F93608132
#define UUID_144A7A95_2BD3_4246_9062_E04F93608132

#include <stdint.h>

#define ETH_ALEN 6

// Use index=0 for R32_ETH_MACA0, index=1 for R32_ETH_MACA1 and so on.
void MacAddressInitialize(uint8_t mac[ETH_ALEN], unsigned index);

#endif // 144A7A95_2BD3_4246_9062_E04F93608132
