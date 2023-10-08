/**
 * @file device.cpp
 */

#include "device.h"
#include "endian.h"
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <cstring>
#include <iostream>

/**
 * @brief Constructor of `Device`. Initialize pcap session.
 * 
 * @param device The device name to open for sending/receiving frames.
 */
Device::Device(const char *device)
{
    // Open handler.
    char errbuf[PCAP_ERRBUF_SIZE] = "";
    handle = pcap_open_live(device, BUFSIZ, 1, 1000, errbuf);

    // Get mac address.
    int sd;
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(struct ifreq));
 
    if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        std::cerr << "Socket() failed to get socket descriptor!" << std::endl;
    }
    memcpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));
    if (ioctl(sd, SIOCGIFHWADDR, &ifr) < 0) {
        std::cerr << "ioctl() failed to get source MAC address" << std::endl;
    }
    close(sd);
 
    memcpy(mac_addr, ifr.ifr_hwaddr.sa_data, 6);
}

/**
 * @brief Destructor of `Device`. Close `handle`.
 */
Device::~Device()
{
    pcap_close(handle);
}

/**
 * @brief Check whether the length of the frame is valid. 
 * 
 * Follow the rule of Ethernet II, i.e., the length of data is in [46, 1500]. 
 * This is slightly different from IEEE 802.3, because we don't use padding.
 */
inline bool
Device::is_valid_length(int len)
{
    return len >= 46 && len <= 1500;
}

/**
* @brief Encapsulate some data into an Ethernet II frame and send it.
*
* @param buf Pointer to the payload.
* @param len Length of the payload.
* @param ethtype EtherType field value of this frame.
* @param destmac MAC address of the destination.
* @return 0 on success, -1 on error.
* @see addDevice
*/
int 
Device::sendFrame(const void* buf, int len, int ethtype, const void* destmac)
{
    int frame_len;
    u_char *frame;
    if(!is_valid_length(len)){
        std::cerr << "Data length invalid!" << std::endl;
    }
    frame_len = SIZE_ETHERNET + len;
    frame = new u_char[frame_len];
    memcpy(frame, destmac, ETHER_ADDR_LEN);
    memcpy(frame + ETHER_ADDR_LEN, mac_addr, ETHER_ADDR_LEN);
    memcpy(frame + 2 * ETHER_ADDR_LEN, (const void*)change_order(ethtype), 
           ETHER_TYPE_LEN);
    memcpy(frame + SIZE_ETHERNET, buf, len);
    if(pcap_sendpacket(handle, frame, frame_len) != 0){
        std::cerr << "Send frame failed!" << std::endl;
        delete[] frame;
        return -1;
    }
    delete[] frame;
    return 0;
}