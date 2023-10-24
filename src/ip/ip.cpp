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
    callback(NULL), device_manager(this), 
    timer_running(false), routing_table(&device_manager)
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
    // Check protocol
    if((proto != IPv4_PROTOCOL_TCP) && 
       (proto != IPv4_PROTOCOL_TESTING1) &&
       (proto != IPv4_PROTOCOL_TESTING2))
    {
        std::cerr << "Protocol " << proto << " not supported!" << std::endl;
        return -1;
    }

    u_char *packet = new u_char[SIZE_IPv4 + len];
    memcpy(packet + SIZE_IPv4, buf, len);
    IPv4Header *ipv4_header = (IPv4Header *)packet;

    // Version & IHL
    ipv4_header->version_IHL = IPv4_VERSION | DEFAULT_IHL;
    // Type of Service
    ipv4_header->service_type = DEFAULT_TOS;
    // Total Length
    ipv4_header->total_len = SIZE_IPv4 + len;
    // Identification
    ipv4_header->id = DEFAULT_ID;
    // Reserved bit, flags and Fragment Offset
    ipv4_header->flags_offset = DEFAULT_FLAGS_OFFSET;
    // Time to Live
    ipv4_header->ttl = DEFAULT_TTL;
    // Protocol
    ipv4_header->protocal = proto;
    // Checksum: firstly set to 0
    ipv4_header->checksum = 0;
    // Addresses
    ipv4_header->src_addr = src;
    ipv4_header->dst_addr = dest;
    // Calculate checksum
    ipv4_header->checksum = calculate_checksum((const u_short *)ipv4_header, 
                                               SIZE_IPv4 >> 1);
    
    // Look up routing table and send it to link layer.
    int device_id = routing_table.findEntry(ipv4_header->dst_addr);
    if(device_id == -1){
        std::cerr << "IP address " << ipv4_header->dst_addr.s_addr;
        std::cerr << " not found!" << std::endl;
        delete[] packet;
        return -1;
    }
    if(
        device_manager.sendFrame(
            (const void *)packet, ipv4_header->total_len,
            ETHTYPE_IPv4, ipv4_header->dst_addr, device_id
        ) == -1
    ){
        std::cerr << "Send frame Error!" << std::endl;
        delete[] packet;
        return -1;
    }

    delete[] packet;
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
 * 
 * @see RFC791 & RFC790 & RFC3692 and 
 * https://www.iana.org/assignments/protocol-numbers/protocol-numbers.xhtml
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
    // See RFC790 & RFC3692 and 
    // https://www.iana.org/assignments/protocol-numbers/protocol-numbers.xhtml
    switch (ipv4_header.protocal)
    {
    case IPv4_PROTOCOL_TCP:
        if(!routing_table.findMyIP(ipv4_header.dst_addr)){
            rest_len = 0;
            int device_id = routing_table.findEntry(ipv4_header.dst_addr);
            if(device_id != -1){
                u_char *dst_addr = (u_char *)&ipv4_header.dst_addr;
                u_char *src_addr = (u_char *)&ipv4_header.src_addr;
                printf("Can't route from %02x.%02x.%02x.%02x to "
                       "%02x.%02x.%02x.%02x!\n",
                       src_addr[0], src_addr[1], src_addr[2], src_addr[3],
                       dst_addr[0], dst_addr[1], dst_addr[2], dst_addr[3]);
                return -1;
            }
            else{
                if(
                    device_manager.sendFrame(
                        buf, len, ETHTYPE_IPv4, ipv4_header.dst_addr, device_id
                    ) == -1
                ){
                    std::cerr << "Routing error: frame sending failed!\n";
                    return -1;
                }
            }
        }
        break;

    case IPv4_PROTOCOL_TESTING1:
        rest_len = 0;
        
        break;

    case IPv4_PROTOCOL_TESTING2:

        rest_len = 0;
        break;
    
    default:
        std::cerr << "Protocol " << ipv4_header.protocal;
        std::cerr << " is not implemented!" << std::endl;
        rest_len = -1;
        break;
    }

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