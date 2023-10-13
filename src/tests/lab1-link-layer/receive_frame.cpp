/**
 * @file receive_frame.cpp
 * @brief (Checkpoint 2) Capture frames from veth2-1. In this scenario, there
 * are 20 frames sent from veth1-2.
 */

#include "../../ethernet/endian.h"
#include "utils.h"
#include <iostream>

/**
 * @brief Process a frame upon receiving it. Just print the length, source and 
 * destination MAC addresses, and payload string.
 *
 * @param buf Pointer to the frame.
 * @param len Length of the frame.
 */
void 
my_callback(const void *buf, int len)
{
    const unsigned char *frame = (const unsigned char *)buf;
    EthernetHeader eth_header = *(EthernetHeader *)frame;
    eth_header.ether_type = change_order(eth_header.ether_type);
    const char *payload = (const char *)(frame + SIZE_ETHERNET);
    printf("Source: %02x:%02x:%02x:%02x:%02x:%02x,\t"
           "Destination: %02x:%02x:%02x:%02x:%02x:%02x\n"
           "EHTER TYPE: 0x%04x\n\n"
           "Payload(length = %d):\n%.*s\n\n", 
           eth_header.ether_shost[0], eth_header.ether_shost[1], 
           eth_header.ether_shost[2], eth_header.ether_shost[3], 
           eth_header.ether_shost[4], eth_header.ether_shost[5], 
           eth_header.ether_dhost[0], eth_header.ether_dhost[1], 
           eth_header.ether_dhost[2], eth_header.ether_dhost[3], 
           eth_header.ether_dhost[4], eth_header.ether_dhost[5], 
           eth_header.ether_type, 
           len - SIZE_ETHERNET, len - SIZE_ETHERNET, payload);
}

int main()
{
    DeviceManager device_manager;
    int device_id = device_manager.addDevice(DST_DEVICE);
    if(device_id == -1){
        std::cerr << "Add " << DST_DEVICE << " failed!" << std::endl;
        return 0;
    }
    
    device_manager.setFrameReceiveCallback(my_callback, device_id);
    if(device_manager.capLoop(device_id, 20) != 0){
        std::cerr << "Capture failed!" << std::endl;
    }
    return 0;
}