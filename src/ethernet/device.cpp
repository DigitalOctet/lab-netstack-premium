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
Device::Device(const char *device, u_char mac[ETHER_ADDR_LEN]): 
    callback(NULL), frame_id(0)
{
    // Open handler.
    char errbuf[PCAP_ERRBUF_SIZE] = "";
    handle = pcap_open_live(device, BUFSIZ, 1, 1000, errbuf);
    if(handle == NULL){
        std::cerr << "Couldn't find default device: " << errbuf << std::endl;
    }

    // Copy mac address.
    memcpy(mac_addr, mac, ETHER_ADDR_LEN);
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
    u_short correct_ethtype = change_order(ethtype);
    frame_len = SIZE_ETHERNET + len;
    frame = new u_char[frame_len];
    memcpy(frame, destmac, ETHER_ADDR_LEN);
    memcpy(frame + ETHER_ADDR_LEN, mac_addr, ETHER_ADDR_LEN);
    memcpy(frame + 2 * ETHER_ADDR_LEN, &correct_ethtype, ETHER_TYPE_LEN);
    memcpy(frame + SIZE_ETHERNET, buf, len);
    if(pcap_sendpacket(handle, frame, frame_len) != 0){
        std::cerr << "Send frame failed!" << std::endl;
        delete[] frame;
        return -1;
    }
    delete[] frame;
    return 0;
}

/**
 * @brief Register a callback function to be called each time an
 * Ethernet II frame was received.
 *
 * @param callback the callback function.
 * @see frameReceiveCallback
 */
void 
Device::setFrameReceiveCallback(frameReceiveCallback callback)
{
    this->callback = callback;
}

/**
 * @brief Capture a frame on this device.
 * @note The semantics of this function is different from that of `pcap_next`,
 * because it calls the callback function previously registered.
 */
int 
Device::capNext()
{
    struct pcap_pkthdr header;	/* The header that pcap gives us */
    const u_char *frame = pcap_next(handle, &header);
    if(frame == NULL){
        std::cerr << "Frame capture failed!" << std::endl;
        return -1;
    }

    if(callback){
        frame_id++;
        std::cout << "Frame " << frame_id << " captured." << std::endl;
        callback(frame, header.len);
        return 0;
    }
    else{
        std::cerr << "Callback function not registerd!" << std::endl;
        return -1;
    }
}

/**
 * @brief Captures `cnt` frames on this device.
 * 
 * @param cnt An integer that tells `capLoop` how many packets it should 
 * sniff for before returning (a non-positive value means it should sniff 
 * until an error occurs).
 * @return 0 on success, -1 on error.
 * 
 * @note I didn't use `pcap_loop` because I wanted to track the amount of 
 * received frames.
 */
int 
Device::capLoop(int cnt)
{
    if(cnt <= 0){
        while(true){
            if(capNext() == -1){
                return -1;
            }
        }
    }
    else{
        for(int i = 0; i < cnt; i++){
            if(capNext() == -1){
                return -1;
            }
        }
        return 0;
    }
}