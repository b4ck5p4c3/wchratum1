#include "packet.h"

#include <string.h>

const MACAddress kBroadcastMACAddress = {
    .bytes = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};

void FillIPv4Header(uint8_t* raw_header, IPAddress source,
                    IPAddress destination, uint16_t data_length,
                    uint8_t protocol) {
  IPv4Header* header = (IPv4Header*)raw_header;
  header->version_header_length = 0b01000101;
  header->type_of_service = 0;
  header->total_length = SWAP_BYTES_U16(data_length + sizeof(IPv4Header));
  header->flags = 0;
  header->ttl = 128;
  header->protocol = protocol;
  header->source = source;
  header->destination = destination;
}

void FillEthernetHeader(uint8_t* raw_header, const MACAddress* source,
                        const MACAddress* destination, uint16_t type) {
  EthernetHeader* header = (EthernetHeader*)raw_header;
  memcpy(&header->source, source, sizeof(MACAddress));
  memcpy(&header->destination, destination, sizeof(MACAddress));
  header->type = SWAP_BYTES_U16(type);
}

void FillUDPHeader(uint8_t* raw_header, uint16_t source_port,
                   uint16_t destination_port, uint16_t data_length) {
  UDPHeader* header = (UDPHeader*)raw_header;
  header->source_port = SWAP_BYTES_U16(source_port);
  header->destination_port = SWAP_BYTES_U16(destination_port);
  header->length = SWAP_BYTES_U16(data_length + sizeof(UDPHeader));
}