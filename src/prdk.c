#include "prdk.h"
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <stdio.h>
#include <string.h>
#include "packet.h"
#include "prng.h"
#include "random.h"
#include "inline.h"

#define SVC_CHARGEN (19)
#define SVC_NTP (123)
#define ETHERMTU_UDP (ETHERMTU - sizeof(struct ip) - sizeof(struct udphdr))

// #define printf(x, ...) do { } while (0)

extern MACAddress kMACAddress;
static uint32_t kIPv4Net = MkIPv4Net(192, 168, 141, 123);

int PacketRxInterrupt(uint8_t *frame, int len)
{
  printf("Received %u:\r\n", len);
  for (uint32_t i = 0; i < len; i++) {
    printf("%02x ", frame[i]);
    if ((i & 0x1F) == 0x1F)
      printf("\r\n");
  }
  printf("\r\n");

  // TODO: check `len` here and there
  struct ether_header *eth = (struct ether_header *)frame;
  printf("&eth: %p\r\n", eth);
  if (eth->ether_type == hton16(ETHERTYPE_IP)) {
    struct ip* ip = (struct ip*)(eth + 1);
    if (ip->ip_v == 4 && ip->ip_hl == 5 && /* (ip->ip_off & (IP_MF | IP_OFFMASK)) == 0 && */
        ip->ip_dst.s_addr == kIPv4Net) {
      if (ip->ip_p == IPPROTO_ICMP) {
        struct icmp* icmp = (struct icmp*)(ip + 1);
        if (icmp->icmp_type == ICMP_ECHO && icmp->icmp_code == 0) {
          icmp->icmp_type = ICMP_ECHOREPLY;
          ip->ip_ttl = IPDEFTTL;
          ip->ip_dst = ip->ip_src;
          ip->ip_src.s_addr = kIPv4Net;
          memcpy(eth->ether_dhost, eth->ether_shost, ETHER_ADDR_LEN);
          memcpy(eth->ether_shost, kMACAddress.bytes, ETHER_ADDR_LEN);
          return sizeof(*eth) + ntoh16(ip->ip_len);
        }
      } else if (ip->ip_p == IPPROTO_UDP) {
        struct udphdr* udp = (struct udphdr*)(ip + 1);
        if (udp->uh_dport == hton16(SVC_CHARGEN) && ip->ip_ttl == MAXTTL &&
            ntoh16(udp->uh_ulen) >= sizeof(*udp) + sizeof(uint8_t)) {
          uint8_t payload = *(uint8_t*)(udp + 1);
          memcpy(eth->ether_dhost, eth->ether_shost, ETHER_ADDR_LEN);
          memcpy(eth->ether_shost, kMACAddress.bytes, ETHER_ADDR_LEN);
          ip->ip_ttl = 1;
          ip->ip_len = hton16(ETHERMTU);
          ip->ip_dst = ip->ip_src;
          ip->ip_src.s_addr = kIPv4Net;
          udp->uh_dport = udp->uh_sport;
          udp->uh_sport = hton16(SVC_CHARGEN);
          udp->uh_ulen = hton16(ETHERMTU - sizeof(*ip));
          _Static_assert(ETHERMTU_UDP % 4 == 0, "MTU=N*u32");
          uint32_t *dat = (uint32_t*)(udp + 1), *end = dat + ETHERMTU_UDP / 4;
          if (payload == 'T') {
            for (; dat != end; dat++)
              *dat = RandomUInt32();
          } else if (payload == 'P') {
            for (; dat != end; dat++)
              *dat = PcgUInt32();
          } else if (payload == 'p') {
            for (uint16_t* d16 = (uint16_t*)dat; d16 != (uint16_t*)end; d16++)
              *d16 = PcgUInt16();
          } else if (payload == 'C') {
            _Static_assert(ETHERMTU_UDP % (4 * 4) == 0, "MTU=N*u128");
            for (; dat != end; dat += 4) {
              const uint64_t r = CSPrngUInt64x2(dat);
              dat[2] = r;
              dat[3] = r >> 32;
            }
          }
          return sizeof(*eth) + ETHERMTU;
        }
      }
    }
  } else if (eth->ether_type == hton16(ETHERTYPE_ARP)) {
    // `len` check is pointless as it's always 64+
    // TODO: postpone to mainloop?
    struct arphdr* arp = (struct arphdr*)(eth + 1);
    if (arp->ar_hrd == hton16(ARPHRD_ETHER) && arp->ar_pro == hton16(ETHERTYPE_IP) &&
        arp->ar_hln == ETHER_ADDR_LEN && arp->ar_pln == sizeof(struct in_addr)) {
      struct ether_arp *earp = (struct ether_arp *)arp;
      if (arp->ar_op == hton16(ARPOP_REQUEST) && memcmp(earp->arp_tpa, &kIPv4Net, 4) == 0) {
        // TODO: read RFC :-)
        memcpy(eth->ether_dhost, eth->ether_shost, ETHER_ADDR_LEN);
        memcpy(eth->ether_shost, kMACAddress.bytes, ETHER_ADDR_LEN);
        arp->ar_op = hton16(ARPOP_REPLY);
        memcpy(earp->arp_tha, earp->arp_sha, ETHER_ADDR_LEN + sizeof(struct in_addr));
        memcpy(earp->arp_sha, kMACAddress.bytes, ETHER_ADDR_LEN);
        memcpy(earp->arp_spa, &kIPv4Net, sizeof(struct in_addr));
        return sizeof(*eth) + sizeof(*earp);
      } else if (arp->ar_op == hton16(ARPOP_REPLY)) {
      }
    }
  }
  return PACKET_PROCESSED;
}
