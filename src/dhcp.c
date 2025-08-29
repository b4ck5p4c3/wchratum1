#include "dhcp.h"

#include <string.h>

#include "ethernet.h"
#include "random.h"

typedef struct __attribute__((__packed__)) {
  uint8_t message_type;
  uint8_t hardware_type;
  uint8_t hardware_address_length;
  uint8_t hops;
  uint32_t transaction_id;
  uint16_t seconds_elapsed;
  uint16_t bootp_flags;
  IPAddress client_address;
  IPAddress your_client_address;
  IPAddress next_server_address;
  IPAddress relay_agent_address;
  uint8_t client_hardware_address[16];
  uint8_t server_host_name[64];
  uint8_t boot_file_name[128];
  uint32_t magic_cookie;
} DHCPPacket;

#define DHCP_MESSAGE_TYPE_BOOT_REQUEST 1
#define DHCP_MESSAGE_TYPE_BOOT_REPLY 2

#define DHCP_HARDWARE_TYPE_MAC 1
#define DHCP_HARDWARE_ADDRESS_LENGTH_MAC 6

#define DHCP_OPTION_MESSAGE_TYPE 53
#define DHCP_OPTION_PARAMETER_REQUEST_LIST 55
#define DHCP_OPTION_PARAMETER_REQUEST_LIST_ITEM_SUBNET 1
#define DHCP_OPTION_PARAMETER_REQUEST_LIST_ITEM_ROUTER 3
#define DHCP_OPTION_PARAMETER_REQUEST_LIST_ITEM_DNS 6
#define DHCP_OPTION_PARAMETER_REQUEST_LIST_ITEM_NTP 42
#define DHCP_OPTION_END 0xFF

#define DHCP_MAGIC_COOKIE 0x63538263u
#define DHCP_DISCOVER_PACKET_SIZE \
  (ROUND_PACKET_SIZE(sizeof(DHCPPacket) + 3 + 6 + 1))

#define ROUND_PACKET_SIZE(x) (((x) + 15) / 16 * 16)

uint32_t dhcp_state = DHCP_STATE_NONE;
uint32_t current_transaction_id = 0;

void DHCPReset() {
  dhcp_state = DHCP_STATE_NONE;
}

DHCPState DHCPGetState() {
  return dhcp_state;
}

void DHCPSendDiscover(const MACAddress* mac_address) {
  current_transaction_id = RandomUInt32();

  uint8_t raw_packet[MIN_UDP_PACKET_SIZE + DHCP_DISCOVER_PACKET_SIZE] = {
      0};

  DHCPPacket* packet = (DHCPPacket*)(raw_packet + UDP_DATA_OFFSET);

  packet->message_type = DHCP_MESSAGE_TYPE_BOOT_REQUEST;
  packet->hardware_type = DHCP_HARDWARE_TYPE_MAC;
  packet->hardware_address_length = DHCP_HARDWARE_ADDRESS_LENGTH_MAC;
  packet->transaction_id = SWAP_BYTES_U32(current_transaction_id);
  memcpy(packet->client_hardware_address, mac_address, sizeof(MACAddress));
  packet->magic_cookie = DHCP_MAGIC_COOKIE;

  uint8_t* options = raw_packet + UDP_DATA_OFFSET + sizeof(DHCPPacket);

  options[0] = DHCP_OPTION_MESSAGE_TYPE;
  options[1] = 1;
  options[2] = 1;
  options[3] = DHCP_OPTION_PARAMETER_REQUEST_LIST;
  options[4] = 4;
  options[5] = DHCP_OPTION_PARAMETER_REQUEST_LIST_ITEM_SUBNET;
  options[6] = DHCP_OPTION_PARAMETER_REQUEST_LIST_ITEM_ROUTER;
  options[7] = DHCP_OPTION_PARAMETER_REQUEST_LIST_ITEM_DNS;
  options[8] = DHCP_OPTION_PARAMETER_REQUEST_LIST_ITEM_NTP;
  options[9] = DHCP_OPTION_END;

  FillUDPHeader(raw_packet + UDP_HEADER_OFFSET, 67, 68,
                DHCP_DISCOVER_PACKET_SIZE);
  FillIPv4Header(raw_packet + IPV4_HEADER_OFFSET, IPV4_ZERO_ADDRESS,
                 IPV4_BROADCAST_ADDRESS,
                 sizeof(UDPHeader) + DHCP_DISCOVER_PACKET_SIZE,
                 IPV4_HEADER_PROTOCOL_UDP);
  FillEthernetHeader(raw_packet, mac_address, &kBroadcastMACAddress,
                     ETHERNET_HEADER_TYPE_IPV4);

  printf("sent dhcp discover: %d\n", EthernetTransmit(raw_packet, sizeof(raw_packet)));

  dhcp_state = DHCP_STATE_SENT_DISCOVER;
}

void DHCPProcessPacket(const MACAddress* mac_address, const uint8_t* packet,
                       uint32_t length) {}