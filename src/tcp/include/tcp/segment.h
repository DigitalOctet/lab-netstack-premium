/**
 * @file segment.h
 * 
 * @brief Utilities for constructing TCP segments.
 * 
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |          Source Port          |       Destination Port        |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                        Sequence Number                        |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                    Acknowledgment Number                      |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |  Data |           |U|A|P|R|S|F|                               |
 * | Offset| Reserved  |R|C|S|S|Y|I|            Window             |
 * |       |           |G|K|H|T|N|N|                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |           Checksum            |         Urgent Pointer        |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                    Options                    |    Padding    |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                             data                              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *                          TCP Header Format
 *
 *        Note that one tick mark represents one bit position.
 */

#pragma once

#include <arpa/inet.h>

/* TCP headers excluding options are 20 bytes. */
#define SIZE_TCP 20
/* Psuedo header */
#define SIZE_PSEUDO 12
/* Data Offset */
#define DEFAULT_OFF (5 << 4)
#define GET_OFF(x)  (((u_char)(x) >> 2) & ~0x3)

/* Time to retransmit a segment(in 5 milliseconds) */
#define RETRANS_TIME 4000

/* Segment type */
namespace SegmentType {
    enum SegmentType {
        SYN,
        SYN_ACK,
        ACK,
        FIN,
        FIN_ACK,
        RST,
    };
}

namespace OptionType {
    enum OptionType {
        END = 0,
        NO_OP = 1,
        MAX_SEG_SIZE = 2,
    };
}


/* Pseudo header */
struct PseudoHeader
{
    struct in_addr src_addr;
    struct in_addr dst_addr;
    u_char zero;
    u_char protocol;
    u_short length;
};

/* TCP header excluding options */
struct TCPHeader 
{
    u_short src_port;
    u_short dst_port;
    unsigned int seq;
    unsigned int ack;
    u_char data_off;
    u_char ctl_bits;
    u_short window;
    u_short checksum;
    u_short urgent;
};

/**
 * @class An object of RetransElem is an element in the retransmit queue that 
 * maintains the segment to retransmit, length of the segment, and time from 
 * the segment is first sent up to now.
 */
class RetransElem
{
public:
    u_char *segment;
    unsigned int seq;
    int len;
    int time;

    RetransElem(u_char *seg, unsigned int s, int l);
    ~RetransElem();
};

/**
 * @brief Calculate checksum of the whole segment.
 * @param segment Header address.
 * @param len Length of the segment in terms of byte.
 * @return checksum
 */
u_short calculate_checksum(const u_char *segment, int len);