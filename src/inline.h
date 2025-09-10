#ifndef UUID_DD72C598_BBF8_4AF6_A07D_7DF50FCFF931
#define UUID_DD72C598_BBF8_4AF6_A07D_7DF50FCFF931

#define ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))
#define ARRAY_END(x) (x + ARRAY_LEN(x))
#define IS_POW2(x) (((x) & (x - 1)) == 0)

static inline uintptr_t ptrtou(void* p) {
  return (uintptr_t)p;
}

#if _BYTE_ORDER == _LITTLE_ENDIAN
#define MkIPv4Net(a, b, c, d) \
  (((uint32_t)a) | (((uint32_t)b) << 8) | (((uint32_t)c) << 16) | (((uint32_t)d) << 24))

static inline uint16_t hton16(uint16_t x) {
  return (x >> 8) | (x << 8);
}
static inline uint16_t ntoh16(uint16_t x) {
  return hton16(x);
}
#else
#error WUT?! CH32V307 is _LITTLE_ENDIAN
#endif

#endif // DD72C598_BBF8_4AF6_A07D_7DF50FCFF931
