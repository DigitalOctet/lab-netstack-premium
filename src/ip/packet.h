/**
 * @file packet.h
 * 
 * @brief Utilities for constructing IPv4 packets.
 * 
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |Version|  IHL  |Type of Service|          Total Length         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |         Identification        |Flags|      Fragment Offset    |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |  Time to Live |    Protocol   |         Header Checksum       |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                       Source Address                          |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                    Destination Address                        |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                    Options                    |    Padding    |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *                 Example Internet Datagram Header
 */

#ifndef IP_PACKET_H
#define IP_PACKET_H

#include <netinet/ip.h>
#include <sys/types.h>

/* IPv4 addresses are 4 bytes */
#define IPv4_ADDR_LEN	4
/* IPv4 headers excluding options are 20 bytes(true when sending packets) */
#define SIZE_IPv4  20

/* Get some fields shorter than a byte. */
/* Version(just check its correctness) */
#define VERSION_MASK 0xf0
#define GET_VERSION(x) ((x) & VERSION_MASK)
#define IPv4_VERSION 0x40
/* IHL */
#define IHL_MASK 0x0f
#define GET_IHL(x) ((x) & IHL_MASK)
#define DEFAULT_IHL 5
/* Type of Service */
#define DEFAULT_TOS 0
/* Identification */
#define DEFAULT_ID  0
/* Reserved bit, flags and Fragment Offset */
#define RESERVED_MASK 0x0080
#define GET_RESERVED(x) ((x) & RESERVED_MASK)
#define RESERVED_BIT 0x00
#define DEFAULT_FLAGS_OFFSET 0x0040
/* Time to Live */
#define DEFAULT_TTL 255
/* Protocol */
/* TCP */
#define IPv4_PROTOCOL_TCP      6
/* HELLO/ECHO */
#define IPv4_PROTOCOL_TESTING1 253
/* Routing message */
#define IPv4_PROTOCOL_TESTING2 254

/* IPv4 header excluding options */
struct IPv4Header
{
    u_char version_IHL;
    u_char service_type;
    u_short total_len;
    u_short id;
    u_short flags_offset;
    u_char ttl;
    u_char protocal;
    u_short checksum;
    struct in_addr src_addr;
    struct in_addr dst_addr;
};

/**
 * @brief Calculate checksum of the whole packet header.
 * @param header Header address.
 * @param len Length of the IPv4 packet header in terms of 16 bits.
 * @return checksum
 */
u_short calculate_checksum(const u_short *header, int len);

#endif /**< IP_PACKET_H */