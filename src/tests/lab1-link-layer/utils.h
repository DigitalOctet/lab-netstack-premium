/**
 * @file utils.h
 * @brief Define some macros and variables for tests of lab1.
 */

#include "../../ethernet/device_manager.h"

/* Ethernet header */
struct EthernetHeader
{
    u_char ether_dhost[ETHER_ADDR_LEN]; /* Destination host address */
	u_char ether_shost[ETHER_ADDR_LEN]; /* Source host address */
	u_short ether_type;                 /* IP? ARP? RARP? etc */
};

/* Source device. */
#define SRC_DEVICE "veth1-2"

/* Destination device. */
#define DST_DEVICE "veth2-1"

/* Ethernet II type. */
#define ETHTYPE 0x0800

/* The mac address of ns1 on my machine. */
u_char ns1_mac[ETHER_ADDR_LEN] = {0x66, 0xcc, 0xc7, 0xf0, 0x49, 0x84};

/* The mac address of ns1 on my machine. */
u_char ns2_mac[ETHER_ADDR_LEN] = {0x3e, 0x10, 0x9e, 0xf1, 0xcb, 0x3c};

/* The mac address of ns3 on my machine. */
u_char ns3_mac[ETHER_ADDR_LEN] = {0xea, 0x58, 0x7b, 0x6e, 0x46, 0xc3};

/* The mac address of ns3 on my machine. */
u_char ns4_mac[ETHER_ADDR_LEN] = {0x86, 0x5f, 0x70, 0x13, 0x8a, 0x6c};

/** 
 * @brief The payload we use now is not a packet from link layer. It's just 
 * the Zen of Python, by Tim Peters.
 * 
 * strlen(payload) == 824
 */
const char *payload = "Beautiful is better than ugly.\n"
"Explicit is better than implicit.\n"
"Simple is better than complex.\n"
"Complex is better than complicated.\n"
"Flat is better than nested.\n"
"Sparse is better than dense.\n"
"Readability counts.\n"
"Special cases aren't special enough to break the rules.\n"
"Although practicality beats purity.\n"
"Errors should never pass silently.\n"
"Unless explicitly silenced.\n"
"In the face of ambiguity, refuse the temptation to guess.\n"
"There should be one-- and preferably only one --obvious way to do it.\n"
"Although that way may not be obvious at first unless you're Dutch.\n"
"Now is better than never.\n"
"Although never is often better than *right* now.\n"
"If the implementation is hard to explain, it's a bad idea.\n"
"If the implementation is easy to explain, it may be a good idea.\n"
"Namespaces are one honking great idea -- let's do more of those!\n";