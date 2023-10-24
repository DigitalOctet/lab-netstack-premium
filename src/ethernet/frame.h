/**
 * @file frame.h
 * 
 * @brief Utilities for constructing an Ethernet Type II frame.
 * 
 * 
 * 0  1  2  3  4  5  6  7  8  9  10 11 12 13 14
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |80 00 20 7A 3F 3E|80 00 20 20 3A AE|08 00|        IP, ARP, etc         |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * | Destination MAC |    Source MAC   | Type|          Payload            |
 * +-----------------+-----------------+-----+-----------------------------+
 * |          MAC Header(14 bytes)           |     Data(46-1500 bytes)     |
 * +-----------------------------------------+-----------------------------+
 */

#ifndef ETHERNET_FRAME_H
#define ETHERNET_FRAME_H

#include "../ip/packet.h"
#include <sys/types.h>

/* Ethernet addresses are 6 bytes */
#define ETHER_ADDR_LEN	6
/* Ethernet types are 2 bytes */
#define ETHER_TYPE_LEN	6
/* Ethernet CRC checksums are 4 bytes */
#define ETHER_CRC_LEN	4
/* ethernet headers are always exactly 14 bytes */
#define SIZE_ETHERNET   14

/* Ethernet II type: IPv4. */
#define ETHTYPE_IPv4 0x0800

/* Ethernet II type: IPv4(reversed version). */
#define ETHTYPE_IPv4_REVERSED 0x0008

/* Ethernet II type: ARP. */
#define ETHTYPE_ARP  0x0806

/* Ethernet header */
struct EthernetHeader
{
    u_char ether_dhost[ETHER_ADDR_LEN]; /* Destination host address */
	u_char ether_shost[ETHER_ADDR_LEN]; /* Source host address */
	u_short ether_type;                 /* IP? ARP? RARP? etc */
};

/* Valid ARP packets are 28 bytes */
#define SIZE_ARP  28
/* Hardware type */
#define HARDWARE_TYPE_REVERSED 0x0100
/* Hardware size */
#define HARDWARE_SIZE 6
/* Protocol size */
#define PROTOCOL_SIZE 4
/* Request Opcode(reversed version) */
#define ARP_REQUEST_REVERSED 0x0100
/* Reply Opcode(reversed version) */
#define ARP_REPLY_REVERSED 0x0200
/* Check whether the packet is a request */
#define IS_ARP_REQUEST(x) ((x) == ARP_REQUEST_REVERSED)
/* Check whether the packet is a reply */
#define IS_ARP_REPLY(x) ((x) == ARP_REPLY_REVERSED)

/* ARP packet */
struct ARPPacket
{
	u_short hardware_type; // 0
	u_short protocol_type; // 2
	u_char hardware_size;  // 4
	u_char protocol_size;  // 5
	u_short opcode;        // 6
	u_char sender_MAC_addr[ETHER_ADDR_LEN]; // 8
	struct in_addr sender_IP_addr;   // 14
	u_char target_MAC_addr[ETHER_ADDR_LEN]; // 18
	struct in_addr target_IP_addr;  // 24 -- 28
};

#endif /**< ETHERNET_FRAME_H */