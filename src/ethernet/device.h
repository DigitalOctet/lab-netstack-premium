/**
 * @file device.h
 * @brief Class of devices supporting sending/receiving Ethernet II frames.
 */

#ifndef ETHERNET_DEVICE_H
#define ETHERNET_DEVICE_H

#include <pcap.h>

/* Ethernet addresses are 6 bytes */
#define ETHER_ADDR_LEN	6
/* Ethernet types are 2 bytes */
#define ETHER_TYPE_LEN	6
/* Ethernet CRC checksums are 4 bytes */
#define ETHER_CRC_LEN	4
/* ethernet headers are always exactly 14 bytes */
#define SIZE_ETHERNET 14

/* Ethernet header */
struct EthernetHeader
{
    u_char ether_dhost[ETHER_ADDR_LEN]; /* Destination host address */
	u_char ether_shost[ETHER_ADDR_LEN]; /* Source host address */
	u_short ether_type;                 /* IP? ARP? RARP? etc */
};

/**
 * @brief Class of devices supporting sending/receiving Ethernet II frames.
 */
class Device
{
private:
    pcap_t *handle;
    u_char mac_addr[ETHER_ADDR_LEN];
    inline bool is_valid_length(int len);
public:
    Device(const char *device);
    ~Device();
    int sendFrame(const void* buf, int len, int ethtype, const void* destmac);
};

#endif /**< ETHERNET_DEVICE_H */