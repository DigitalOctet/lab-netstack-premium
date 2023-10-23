/**
 * @file ip.cpp
 */

#include "../ethernet/endian.h"
#include "ip.h"
#include "packet.h"
#include <iostream>
#include <thread>

/**
 * @brief Constructor of `NetworkLayer`. Initialize device manager.
 */
NetworkLayer::NetworkLayer(): 
    callback(NULL), device_manager(this), timer_running(false)
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
                              const void *nextHopMAC, const char *device)
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
    IPv4Header ipv4_header = *(IPv4Header *)buf;
    int header_len, options_len;

    // Version
    if(GET_VERSION(ipv4_header.version_IHL) != IPv4_VERSION){
        std::cerr << "Version field error! It's not an IPv4 packet!\n";
        return -1;
    }
    
    // IHL
    header_len = GET_IHL(ipv4_header.version_IHL) << 2;
    if(header_len < SIZE_IPv4){
        std::cerr << "IHL(=" << header_len / 4 << ") error: " << "less than ";
        std::cerr << "5!" << std::endl;
        return -1;
    }
    options_len = header_len - SIZE_IPv4;
    rest_len -= header_len;

    // Ignore Type of Service, Total Length, Identification.
    // NOTE: DF, MF, and Fragment Offset are also ignored because packets that 
    // are fragmented have already been dropped by link layer, so we don't 
    // care about fragmentation here.
    // Reserved bit
    if(GET_RESERVED(ipv4_header.flags_offset) != RESERVED_BIT){
        std::cerr << "Reserved bit is not 0!" << std::endl;
        return -1;
    }

    // Time to Live
    // NOTE: In RFC791, it indicates the maximum time the datagram is allowed 
    // to remain in the internet system. But in practice, it is usually 
    // decreased once every hop.(And that's how IPv6 works.) For simplicity,
    // I'll just do that.
    if(ipv4_header.ttl == 0){
        std::cout << "Packet timeout!" << std::endl;
        return 0;
    }
    ipv4_header.ttl -= 1;

    // Header Checksum
    u_short sum = calculate_checksum((const u_short *)buf, header_len >> 1);
    if(sum != 0){
        std::cerr << "Checksum error!" << std::endl;
        return -1;
    }

    // Protocol
    

    return rest_len;
}

/**
 * @brief Used for sending routing messages.
 * @param interval_seconds
 */
void 
NetworkLayer::timerCallback(int interval_milliseconds)
{
    while(true){
        timer_mutex.lock();
        if(!timer_running){
            timer_mutex.unlock();
            break;
        }
        timer_mutex.unlock();
        std::this_thread::sleep_for(
            std::chrono::milliseconds(interval_milliseconds)
        );
        // sendIPPacket();
    }
}

/**
 * @brief Start `timer_thread`.
 * @param interval_seconds
 */
void 
NetworkLayer::startTimer(int interval_milliseconds)
{
    // No need to lock here. Because when `timer_running` is false, 
    // `timer_thread` isn't running, and when it's true, its value won't be 
    // changed.
    if (!timer_running){
        timer_running = true;
        timer_thread = std::thread(&NetworkLayer::timerCallback, 
                                   this, interval_milliseconds);
    }
}

/**
 * @brief Stop `timer_thread`
 */
void 
NetworkLayer::stopTimer()
{
    timer_mutex.lock();
    if(timer_running){
        timer_running = false;
        timer_mutex.unlock();
        if (timer_thread.joinable())
        {
            timer_thread.join();
        }
    }
    else{
        timer_mutex.unlock();
    }
}