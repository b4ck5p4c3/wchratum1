#ifndef PACKET_H_
#define PACKET_H_

#include <stdint.h>

typedef struct __attribute__((__packed__)) {
  uint8_t bytes[6];
} MACAddress;

extern const MACAddress kBroadcastMACAddress;

typedef uint32_t IPAddress;

#define ETHERNET_HEADER_TYPE_IPV4 0x0800
#define ETHERNET_HEADER_TYPE_ARP 0x0806

typedef struct __attribute__((__packed__)) {
  MACAddress destination;
  MACAddress source;
  uint16_t type;
} EthernetHeader;

#define IPV4_HEADER_PROTOCOL_UDP 0x11
#define IPV4_ZERO_ADDRESS 0
#define IPV4_BROADCAST_ADDRESS 0xFFFFFFFF

typedef struct __attribute__((__packed__)) {
  uint8_t version_header_length;
  uint8_t type_of_service;
  uint16_t total_length;
  uint16_t identification;
  uint16_t flags;
  uint8_t ttl;
  uint8_t protocol;
  uint16_t checksum;
  IPAddress source;
  IPAddress destination;
} IPv4Header;

typedef struct __attribute__((__packed__)) {
  uint16_t source_port;
  uint16_t destination_port;
  uint16_t length;
  uint16_t checksum;
} UDPHeader;

#define ETHERNET_HEADER_OFFSET 0
#define IPV4_HEADER_OFFSET sizeof(EthernetHeader)
#define UDP_HEADER_OFFSET (IPV4_HEADER_OFFSET + sizeof(IPv4Header))
#define UDP_DATA_OFFSET (UDP_HEADER_OFFSET + sizeof(UDPHeader))

#define MIN_UDP_PACKET_SIZE UDP_DATA_OFFSET

#define SWAP_BYTES_U16(x) (((x) >> 8) | (((x) & 0xFF) << 8))
#define SWAP_BYTES_U32(x) (((x) >> 24) | (((x) & 0xFF0000) >> 8) | (((x) & 0xFF00) << 8) | (((x) & 0xFF) << 24))

void FillIPv4Header(uint8_t* raw_header, IPAddress source, IPAddress destination, uint16_t total_length, uint8_t protocol);
void FillEthernetHeader(uint8_t* raw_header, const MACAddress* source, const MACAddress* destination, uint16_t type);
void FillUDPHeader(uint8_t* raw_header, uint16_t source_port, uint16_t destination_port, uint16_t length);

#endif