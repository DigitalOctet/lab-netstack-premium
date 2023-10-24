/**
 * @file device.h
 * @brief Class of devices supporting sending/receiving Ethernet II frames.
 */

#ifndef ETHERNET_DEVICE_H
#define ETHERNET_DEVICE_H

#include "frame.h"
#include <pcap.h>
#include <map>

/**
 * @brief Process a frame upon receiving it. 
 *
 * @param buf Pointer to the frame.
 * @param len Length of the frame.
 * @return 0 on success, -1 on error.
 * 
 * @note 1. I didn't use the same interface as that in README.pdf, because
 * I want to register different callback functions for devices. 
 * @note 2. Callback functions like these are just for testing because they 
 * don't have control over the instance of a specific layer.
 */
typedef int (* frameReceiveCallback)(const void *, int);

/**
 * @brief Class of devices supporting sending/receiving Ethernet II frames.
 * 
 * @note For `dst_MAC_addr`. A device should actually maintain a table 
 * storing IP-to-MAC relationship. But in the virtual network, an Ethernet is 
 * just an end-to-end link with two end devices. For now I just implement 
 * like this, setting destination MAC address to that of the other end. But 
 * a group of hosts can actually be considered as a virtual Ethernet. I'll 
 * implement that when I have more time.
 */
class Device
{
private:
    pcap_t *handle;
    frameReceiveCallback callback;
    int frame_id;
    int fd;
    struct in_addr ip_addr;
    u_char mac_addr[ETHER_ADDR_LEN];
    u_char dst_MAC_addr[ETHER_ADDR_LEN];
    inline bool is_valid_length(int len);
    inline bool check_MAC(u_char MAC[ETHER_ADDR_LEN]);
    bool handle_ARP(const u_char *buf);
    bool reply_ARP(
        const u_char sender_MAC[ETHER_ADDR_LEN], 
        const struct in_addr sender_IP,
        const u_char target_MAC[ETHER_ADDR_LEN],
        const struct in_addr target_IP
    );
public:
    Device(const char* device, u_char mac[ETHER_ADDR_LEN]);
    ~Device();
    int sendFrame(const void* buf, int len, 
                  int ethtype, const struct in_addr dest_ip);
    void setFrameReceiveCallback(frameReceiveCallback callback);
    int capNext();
    int capLoop(int cnt);
    int capNextEx(struct pcap_pkthdr **header, const u_char **data);
    int getFD();
    unsigned int callBack(const u_char *buf, int len);
    void setIP(struct in_addr addr);
};

#endif /**< ETHERNET_DEVICE_H */