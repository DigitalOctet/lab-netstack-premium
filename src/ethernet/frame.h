/**
 * @file frame.h
 * 
 * @brief Utilities for constructing an Ethernet Type II frame.
 * 
 * 
 * 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14
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

/* Ethernet II type: ARP. */
#define ETHTYPE_ARP  0x0806

/* Ethernet header */
struct EthernetHeader
{
    u_char ether_dhost[ETHER_ADDR_LEN]; /* Destination host address */
	u_char ether_shost[ETHER_ADDR_LEN]; /* Source host address */
	u_short ether_type;                 /* IP? ARP? RARP? etc */
};

#endif /**< ETHERNET_FRAME_H */