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
    int capNextEx(struct pcap_pkthdr **header, const u_char **data);
    int getFD();
    unsigned int callBack(const u_char *buf, int len);
};

#endif /**< ETHERNET_DEVICE_H */