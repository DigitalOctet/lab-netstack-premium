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

/**
 * @brief Process a frame upon receiving it.
 *
 * @param buf Pointer to the frame.
 * @param len Length of the frame.
 * 
 * @note I didn't use the same interface as that in README.pdf, because
 * I want to register different callback function for devices. And the return 
 * value of callback is unnecessary.
 */
typedef void (* frameReceiveCallback)(const void *, int);

/**
 * @brief Class of devices supporting sending/receiving Ethernet II frames.
 */
class Device
{
private:
    pcap_t *handle;
    frameReceiveCallback callback;
    int frame_id;
    u_char mac_addr[ETHER_ADDR_LEN];
    inline bool is_valid_length(int len);
public:
    Device(const char *device, u_char mac[ETHER_ADDR_LEN]);
    ~Device();
    int sendFrame(const void* buf, int len, int ethtype, const void* destmac);
    void setFrameReceiveCallback(frameReceiveCallback callback);
    int capNext();
    int capLoop(int cnt);
};

#endif /**< ETHERNET_DEVICE_H */