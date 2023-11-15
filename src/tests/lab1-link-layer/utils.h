/**
 * @file utils.h
 * @brief Define some macros and variables for tests of lab1.
 */

#include <ethernet/endian.h>
#include <ethernet/device_manager.h>

/* Source device. */
#define SRC_DEVICE "veth1-2"

/* Destination device. */
#define DST_DEVICE "veth2-1"

/* The mac address of veth1-2 on my machine. */
u_char veth1_2_mac[ETHER_ADDR_LEN] = {0xf6, 0x05, 0xd4, 0x2b, 0xdb, 0x5f};

/* The mac address of veth2-1 on my machine. */
u_char veth2_1_mac[ETHER_ADDR_LEN] = {0x4a, 0x5b, 0x71, 0x31, 0x4e, 0x2d};

/* The mac address of ns3 on my machine. */
// u_char ns3_mac[ETHER_ADDR_LEN] = {0xea, 0x58, 0x7b, 0x6e, 0x46, 0xc3};

/* The mac address of ns4 on my machine. */
// u_char ns4_mac[ETHER_ADDR_LEN] = {0x86, 0x5f, 0x70, 0x13, 0x8a, 0x6c};

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

/**
 * @brief Process a frame upon receiving it. Just print the length, source and 
 * destination MAC addresses, and payload string.
 *
 * @param buf Pointer to the frame.
 * @param len Length of the frame.
 */
int 
test_callback(const void *buf, int len)
{
    const unsigned char *frame = (const unsigned char *)buf;
    EthernetHeader eth_header = *(EthernetHeader *)frame;
    eth_header.ether_type = change_order(eth_header.ether_type);
    const char *payload = (const char *)(frame + SIZE_ETHERNET);
    
    printf("Source: %02x:%02x:%02x:%02x:%02x:%02x,\t"
           "Destination: %02x:%02x:%02x:%02x:%02x:%02x\n"
           "EHTER TYPE: 0x%04x\n\n",
           eth_header.ether_shost[0], eth_header.ether_shost[1], 
           eth_header.ether_shost[2], eth_header.ether_shost[3], 
           eth_header.ether_shost[4], eth_header.ether_shost[5], 
           eth_header.ether_dhost[0], eth_header.ether_dhost[1], 
           eth_header.ether_dhost[2], eth_header.ether_dhost[3], 
           eth_header.ether_dhost[4], eth_header.ether_dhost[5],
           eth_header.ether_type);
    if(eth_header.ether_type == ETHTYPE_IPv4){
        printf("Payload(length = %d):\n%.*s\n\n", 
               len - SIZE_ETHERNET, len - SIZE_ETHERNET, payload);
    }
    else{
        printf("Payload(length = %d)\n\n", len - SIZE_ETHERNET);
    }
    return 0;
}