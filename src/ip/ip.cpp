/**
 * @file ip.cpp
 */

#include "ip.h"
#include <iostream>
#include <thread>

/**
 * @brief Constructor of `NetworkLayer`. Initialize device manager.
 */
NetworkLayer::NetworkLayer(): callback(NULL), device_manager(this)
{
    if(device_manager.addAllDevice() == -1){
        std::cerr << "Device manager construction failed in network layer!\n";
        return;
    }
    std::thread(DeviceManager::readLoop, device_manager.epoll_server).detach();
}

/**
 * @brief Send an IP packet to specified host.
 *
 * @param src Source IP address.
 * @param dest Destination IP address.
 * @param proto Value of `protocol` field in IP header.
 * @param buf pointer to IP payload
 * @param len Length of IP payload
 * @return 0 on success, -1 on error.
 */
int 
NetworkLayer::sendIPPacket(const struct in_addr src, const struct in_addr dest,
                           int proto, const void* buf, int len)
{
    return 0;
}

/**
 * @brief Register a callback function to be called each time an IP
 * packet was received.
 *
 * @param callback The callback function.
 * @return 0 on success , -1 on error.
 * @see IPPacketReceiveCallback
 */
int 
NetworkLayer::setIPPacketReceiveCallback(IPPacketReceiveCallback callback)
{
    this->callback = callback;
    return 0;
}

/**
 * @brief Manully add an item to routing table. Useful when talking
 * with real Linux machines.
 *
 * @param dest The destination IP prefix.
 * @param mask The subnet mask of the destination IP prefix.
 * @param nextHopMAC MAC address of the next hop.
 * @param device Name of device to send packets on.
 * @return 0 on success , -1 on error
 */
int 
NetworkLayer::setRoutingTable(const struct in_addr dest, 
                              const struct in_addr mask, 
                              const void* nextHopMAC, const char *device)
{
    return 0;
}

/**
 * @brief Actual callback function used in my network stack on receiving an 
 * IP packet.
 * 
 * @param buf Pointer to the IP packet.
 * @param len Length of the IP packet.
 * @return Length after it consumes from buf, i.e., length that should be 
 * passed to transport layer. 0 if the frame doesn't need to be passed.
 * -1 on error.
 */
unsigned int 
NetworkLayer::callBack(const u_char *buf, int len)
{
    int rest_len = len;
    EthernetHeader ipv4_header = *(EthernetHeader *)buf;

    return rest_len;
}