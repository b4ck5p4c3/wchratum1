#ifndef DHCP_H_
#define DHCP_H_

#include "packet.h"

#define MAX_DNS_SERVERS 2
#define MAX_GATEWAYS 1
#define MAX_NTP_SERVERS 2

typedef enum {
  DHCP_STATE_NONE,
  DHCP_STATE_SENT_DISCOVER,
  DHCP_STATE_SENT_REQUEST,
  DHCP_STATE_READY
} DHCPState;

typedef struct {
  IPAddress client_ip;
  IPAddress subnet;
  IPAddress gateways[MAX_GATEWAYS];
  IPAddress ntp_server[MAX_NTP_SERVERS];
  IPAddress dns_servers[MAX_DNS_SERVERS];
  uint32_t renewal_time;
} DHCPResult;

void DHCPReset();
DHCPState DHCPGetState();

void DHCPSendDiscover(const MACAddress* mac_address);
void DHCPProcessPacket(const MACAddress* mac_address, const uint8_t* packet, uint32_t length);

void DHCPReadyInterrupt(DHCPResult result);

#endif