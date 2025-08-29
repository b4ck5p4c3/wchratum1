#ifndef ETHERNET_H_
#define ETHERNET_H_

#include <stdint.h>
#include <stdbool.h>

#include "packet.h"

#define ETHERNET_TX_BUFFER_COUNT 2
#define ETHERNET_RX_BUFFER_COUNT 4

#define ETHERNET_TX_BUFFER_SIZE 1520
#define ETHERNET_RX_BUFFER_SIZE 1520

#define ETHERNET_USE_HARDWARE_CHECKSUM

#define ETHERNET_DEBUG

typedef enum {
    ETHERNET_PHY_MODE_10M_INTERNAL,
    ETHERNET_PHY_MODE_100M_EXTERNAL,
    ETHERNET_PHY_MODE_1000M_EXTERNAL
} EthernetPHYMode;

#define ETHERNET_PHY_MODE ETHERNET_PHY_MODE_10M_INTERNAL

void EthernetInitialize(const MACAddress* mac_address);
bool EthernetTransmit(const uint8_t* packet, uint32_t length);

void EthernetPHYLinkChangeInterrupt(bool phy_link_ready);
void EthernetReceivedInterrupt(const uint8_t* packet, uint32_t length);

#endif