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
 * 
 * @note I didn't use the same interface as that in README.pdf, because
 * I want to register different callback functions for devices. 
 */
typedef int (* frameReceiveCallback)(const void *, int);

/**
 * @brief Class of devices supporting sending/receiving Ethernet II frames.
 */
class Device
{
private:
    pcap_t *handle;
    frameReceiveCallback callback;
    int frame_id;
    int fd;
    u_char mac_addr[ETHER_ADDR_LEN];
    inline bool is_valid_length(int len);
public:
    Device(const char* device, u_char mac[ETHER_ADDR_LEN]);
    ~Device();
    int sendFrame(const void* buf, int len, int ethtype, const void* destmac);
    void setFrameReceiveCallback(frameReceiveCallback callback);
    int capNext();
    int capLoop(int cnt);
    int capBuf();
    int getFD();
};

#endif /**< ETHERNET_DEVICE_H */