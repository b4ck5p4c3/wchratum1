#include "chaskey.h"

// Chaskey is somewhat better than HalfSipHash as a PRF for 32-bit machines.
// https://mouha.be/chaskey/
// https://github.com/ocaml/ocaml/pull/24#issuecomment-156463770
// https://lore.kernel.org/all/CAHmME9rPmH=wP_eHYopt8ZPG9TSN7bos3fGOuqKL2HjQW-2SWA@mail.gmail.com/

#define ROTL(x, b) (uint32_t)(((x) >> (32 - (b))) | ((x) << (b)))

// ≈262 ticks due to lack of native ROT in RV32IMACF
void chaskey8(uint32_t v[4], const uint32_t key[4]) {
  const int rounds = 8;

  const uint32_t k0 = key[0], k1 = key[1], k2 = key[2], k3 = key[3];

  uint32_t v0 = v[0] ^ k0;
  uint32_t v1 = v[1] ^ k1;
  uint32_t v2 = v[2] ^ k2;
  uint32_t v3 = v[3] ^ k3;

  for (int i = rounds; i; i--) {
    v0 += v1; v1 = ROTL(v1,  5); v1 ^= v0; v0 = ROTL(v0, 16);
    v2 += v3; v3 = ROTL(v3,  8); v3 ^= v2;
    v0 += v3; v3 = ROTL(v3, 13); v3 ^= v0;
    v2 += v1; v1 = ROTL(v1,  7); v1 ^= v2; v2 = ROTL(v2, 16);
  }

  v[0] = v0 ^ k0;
  v[1] = v1 ^ k1;
  v[2] = v2 ^ k2;
  v[3] = v3 ^ k3;
}
