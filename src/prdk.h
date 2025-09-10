#ifndef UUID_63CAAE0B_B449_465D_95F5_2C7B31382D55
#define UUID_63CAAE0B_B449_465D_95F5_2C7B31382D55

#include <sys/types.h>
// toolchain-riscv/riscv-wch-elf/include/sys/types.h declares in_addr_t and in_port_t
#define _IN_TYPES_DEFINED_ 1
#include <net/if_arp.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <ch32v30x_eth.h>

// Each buffer is in one of following states:
//  - at DMA Pending Rx queue, waiting for RxDMA to stop to put new buffers in use
//  - at RxDMA queue, waiting for incoming data
//  - at CPU, either being read by PacketRx() in-place or prepared for PacketEnqueueTx()
//  - at Incoming queue, waiting for main() loop to handle the packet
//  - at DMA Pending Tx queue, waiting for TxDMA to stop to throw more frames at FIFO
//  - at TxDMA queue, waiting for frame to be sent
//  - at Pool, waiting for some CPU process to want to craft an outgoing frame
typedef union {
  uint32_t AlignedAndPaddedFrame[(ETHER_ALIGN + ETHER_MAX_LEN) / 4];
  struct {
    uint8_t Padding[ETHER_ALIGN];
    union {
      // ETH_MAX_PACKET_SIZE is slightly larger, it includes VLAN tag and two bytes of ETHER_ALIGN.
      // However, ETHER_MAX_LEN includes CRC.
      uint8_t Frame[ETHER_MAX_LEN];
      struct {
        struct ether_header eth;
        union {
          struct ether_arp arp;
          // `ip4` is not part of the union to avoid unintentional 32-bit alignment.
          // struct { struct ip ip4; union { struct udphdr udp; struct icmp icmp; }; };
        };
      };
    };
  };
} PacketBuffer;
_Static_assert((ETHER_ALIGN + ETHER_MAX_LEN) % 4 == 0, "Just look at AlignedAndPaddedFrame");

#define PACKET_PROCESSED   (-1)
#define PACKET_TO_MAINLOOP (-2)

int PacketRxInterrupt(uint8_t *frame, int len);

#endif // 63CAAE0B_B449_465D_95F5_2C7B31382D55
