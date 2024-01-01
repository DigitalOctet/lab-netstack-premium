/**
 * @file ip.cpp
 */

#include <ethernet/endian.h>
#include <ethernet/frame.h>
#include <ip/ip.h>
#include <ip/packet.h>
#include <algorithm>
#include <iostream>
#include <thread>

/**
 * @brief Constructor of `NetworkLayer`. Initialize device manager.
 */
NetworkLayer::NetworkLayer(TransportLayer *trans): 
    callback(NULL), device_manager(this, trans), 
    timer_running(false), routing_table(&device_manager)
{
    if(device_manager.addAllDevice() == -1){
        std::cerr << "Device manager construction failed in network layer!\n";
        return;
    }
    routing_table.setMyIP();
    std::thread(&DeviceManager::readLoop, 
                &device_manager, device_manager.epoll_server).detach();
    startTimer(2500);
}

/**
 * @brief Destructor
 */
NetworkLayer::~NetworkLayer()
{
    std::cout << "Network layer destruction." << std::endl;
    stopTimer();
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
    int rc;

    // Check protocol
    if((proto != IPv4_PROTOCOL_TCP) && 
       (proto != IPv4_PROTOCOL_TESTING1) &&
       (proto != IPv4_PROTOCOL_TESTING2))
    {
        std::cerr << "Protocol " << proto << " not supported!" << std::endl;
        return -1;
    }

    u_char *packet = new u_char[SIZE_IPv4 + len];
    memset(packet, 0, SIZE_IPv4 + len);
    memcpy(packet + SIZE_IPv4, buf, len);
    IPv4Header *ipv4_header = (IPv4Header *)packet;

    // Version & IHL
    ipv4_header->version_IHL = IPv4_VERSION | DEFAULT_IHL;
    // Type of Service
    ipv4_header->service_type = DEFAULT_TOS;
    // Total Length
    u_short total_len = SIZE_IPv4 + len;
    ipv4_header->total_len = change_order(total_len);
    // Identification
    ipv4_header->id = DEFAULT_ID;
    // Reserved bit, flags and Fragment Offset
    ipv4_header->flags_offset = DEFAULT_FLAGS_OFFSET;
    // Time to Live
    ipv4_header->ttl = DEFAULT_TTL;
    // Protocol
    ipv4_header->protocol = proto;
    // Checksum: firstly set to 0
    ipv4_header->checksum = 0;
    // Addresses
    ipv4_header->src_addr = src;
    ipv4_header->dst_addr = dest;
    // Calculate checksum
    u_short checksum = calculate_checksum((const u_short *)ipv4_header, 
                                           SIZE_IPv4 >> 1);
    ipv4_header->checksum = checksum;
    
    // Send packets
    if(proto == IPv4_PROTOCOL_TESTING1 || proto == IPv4_PROTOCOL_TESTING2){
        // Broadcast
        device_manager.sendFrameAll(
            (const void *)packet, total_len, ETHTYPE_IPv4, dest
        );
    }
    else{
        // Look up routing table and send it to link layer.
        int device_id = routing_table.findEntry(ipv4_header->dst_addr);
        if(device_id == -1){
            fprintf(stderr, "IP address %x not found!\n", 
                    ipv4_header->dst_addr.s_addr);
            delete[] packet;
            return -1;
        }
        rc = device_manager.sendFrame((const void *)packet, total_len, 
                                      ETHTYPE_IPv4, ipv4_header->dst_addr, 
                                      device_id);
        if(rc == -1){
            std::cerr << "Send frame Error!" << std::endl;
            delete[] packet;
            return -1;
        }
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
 * @param nextHopMAC MAC address of the next hop.(ignored in my implementation)
 * @param device Name of device to send packets on.
 * @return 0 on success , -1 on error
 */
int 
NetworkLayer::setRoutingTable(const struct in_addr dest, 
                              const struct in_addr mask, 
                              const void *nextHopMAC, const char *device)
{
    routing_table.table_mutex.lock();
    Entry e;
    e.IP_addr = dest;
    e.mask = mask;
    e.device_id = device_manager.findDevice(device);
    bool exist =false;
    for(auto &entry: routing_table.routing_table){
        if(entry.IP_addr.s_addr == e.IP_addr.s_addr &&
            entry.mask.s_addr == e.mask.s_addr)
        {
            exist = true;
            break;
        }
    }
    if(!exist){
        routing_table.routing_table.push_back(e);
    }
    routing_table.table_mutex.unlock();
    return 0;
}

/**
 * @brief Actual callback function used in my network stack on receiving an 
 * IP packet.
 * 
 * @param buf Pointer to the IP packet.
 * @param len Length of the IP packet.
 * @param device_id ID of the device receiving the packet.
 * @return Length after it consumes from buf, i.e., length that should be 
 * passed to transport layer. 0 if the frame doesn't need to be passed.
 * -1 on error.
 * 
 * @see RFC791 & RFC790 & RFC3692 and 
 * https://www.iana.org/assignments/protocol-numbers/protocol-numbers.xhtml
 */
int 
NetworkLayer::callBack(const u_char *buf, int len, int device_id, 
                       int *header_len)
{
    int rest_len;
    IPv4Header ipv4_header = *(IPv4Header *)buf;
    int options_len;
    int rc;

    // Version
    if(GET_VERSION(ipv4_header.version_IHL) != IPv4_VERSION){
        std::cerr << "Version field error! It's not an IPv4 packet!\n";
        return -1;
    }
    
    // IHL
    *header_len = GET_IHL(ipv4_header.version_IHL) << 2;
    if(*header_len < SIZE_IPv4){
        std::cerr << "IHL(=" << *header_len / 4 << ") error: " << "less than ";
        std::cerr << "5!" << std::endl;
        return -1;
    }
    options_len = *header_len - SIZE_IPv4;
    // `rest_len` can be different from `len - header_len` due to the padding 
    // 0 of the Ethernet frame.
    rest_len = (u_short)change_order(ipv4_header.total_len) - *header_len;

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
    // decreased once per hop.(And that's how IPv6 works.) For simplicity,
    // I'll just do that.
    if(ipv4_header.ttl == 0){
        std::cout << "Packet timeout!" << std::endl;
        return 0;
    }
    ipv4_header.ttl -= 1;

    // Header Checksum
    u_short sum = calculate_checksum((const u_short *)buf, (*header_len) >> 1);
    if(sum != 0){
        std::cerr << "Checksum error!" << std::endl;
        return -1;
    }

    // Protocol
    // See RFC790 & RFC3692 and 
    // https://www.iana.org/assignments/protocol-numbers/protocol-numbers.xhtml
    switch (ipv4_header.protocol)
    {
    case IPv4_PROTOCOL_TCP:
        if(!routing_table.findMyIP(ipv4_header.dst_addr)){
            rest_len = 0;
            int to_device_id = routing_table.findEntry(ipv4_header.dst_addr);
            if(to_device_id == -1){
                u_char *dst_addr = (u_char *)&ipv4_header.dst_addr;
                u_char *src_addr = (u_char *)&ipv4_header.src_addr;
                printf("Can't route from %02x.%02x.%02x.%02x to "
                       "%02x.%02x.%02x.%02x!\n",
                       src_addr[0], src_addr[1], src_addr[2], src_addr[3],
                       dst_addr[0], dst_addr[1], dst_addr[2], dst_addr[3]);
                return -1;
            }
            else{
                rc = device_manager.sendFrame(buf, len, ETHTYPE_IPv4, 
                                              ipv4_header.dst_addr, 
                                              to_device_id);
                if(rc == -1){
                    std::cerr << "Routing error: frame sending failed!\n";
                    return -1;
                }
            }
        }
        break;

    case IPv4_PROTOCOL_TESTING1:
        if(!handleHello(buf, len, device_id)){
            return -1;
        }
        rest_len = 0;
        break;

    case IPv4_PROTOCOL_TESTING2:
        if(!handleLinkState(buf , len, device_id)){
            return -1;
        }
        rest_len = 0;
        break;
    
    default:
        std::cerr << "Protocol " << (unsigned int)ipv4_header.protocol;
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
        device_manager.requestARP();
        std::this_thread::sleep_for(
            std::chrono::milliseconds(interval_milliseconds)
        );
        sendHelloPacket();
        std::this_thread::sleep_for(
            std::chrono::milliseconds(interval_milliseconds)
        );
        sendLinkStatePacket();
        std::this_thread::sleep_for(
            std::chrono::milliseconds(interval_milliseconds)
        );
        routing_table.updateStates();
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

/**
 * @brief Send HELLO packets from all devices. One device, one packet.
 * @return true on success, false on error.
 */
bool 
NetworkLayer::sendHelloPacket()
{
    if(routing_table.my_IP_addrs.size() != 0){
        struct in_addr dest;
        int min_len = MIN_PAYLOAD - SIZE_IPv4;
        u_char *packet = new u_char[min_len];
        dest.s_addr = IPv4_ADDR_BROADCAST;
        memset(packet, 0, min_len);
        memcpy(packet, &routing_table.my_IP_addrs[0], IPv4_ADDR_LEN);
        packet[IPv4_ADDR_LEN] = 0x01; // is_request
        packet[IPv4_ADDR_LEN + 2] = 60u;
        sendIPPacket(routing_table.my_IP_addrs[0], dest, 
                     IPv4_PROTOCOL_TESTING1, packet, min_len);
        delete[] packet;
    }
    return true;
}

/**
 * @brief Send link state packets from all devices. One device, one packet.
 * @return true on success, false on error.
 */
bool 
NetworkLayer::sendLinkStatePacket()
{
    int addr_size = routing_table.my_IP_addrs.size();
    if(addr_size != 0){
        struct in_addr dest;
        dest.s_addr = IPv4_ADDR_BROADCAST;
        u_char *packet;
        int min_len = MIN_PAYLOAD - SIZE_IPv4;
        int max_len = MAX_PAYLOAD - SIZE_IPv4;
        int len = 12; // seq, age, and size
        int neighbor_size = routing_table.neighbors.size();
        len += ((addr_size + neighbor_size) << 3);
        len = (len < min_len) ? min_len : len;
        if(len > max_len){
            std::cerr << "Link state packet too large!" << std::endl;
            return false;
        }
        packet = new u_char[len];
        memset(packet, 0, len);
        unsigned int reversed_seq = change_order(routing_table.seq);
        routing_table.seq++;
        memcpy(packet, &reversed_seq, 4);
        unsigned int reversed_age = change_order(60u);
        memcpy(packet + 4, &reversed_age, 4);
        int offset = 12;
        u_short addr_size_reversed = change_order((u_short)addr_size);
        u_short neighbor_size_reversed = change_order((u_short)neighbor_size);
        memcpy(packet + 8, &addr_size_reversed, 2);
        memcpy(packet + 10, &neighbor_size_reversed, 2);
        for(auto &addr: routing_table.my_IP_addrs){
            memcpy(packet + offset, &addr, 4);
            offset += 4;
        }
        for(auto &addr: routing_table.masks){
            memcpy(packet + offset, &addr, 4);
            offset += 4;
        }
        for(auto &neighbor: routing_table.neighbors){
            memcpy(packet + offset, &neighbor.first.s_addr, 4);
            offset += 4;
            unsigned int reversed_dist = change_order(1u);
            memcpy(packet + offset, &reversed_dist, 4);
            offset += 4;
        }
        sendIPPacket(routing_table.my_IP_addrs[0], dest, 
                     IPv4_PROTOCOL_TESTING2, packet, len);
        delete[] packet;
    }
    return true;
}

/**
 * @brief HELLO packet handler.
 * 
 * @param buf IPv4 header + HELLO packet.
 * @param len Length of IPv4 packet.
 * @param device_id Device ID where the packet comes from.
 * @return true on success, false on failure.
 */
bool 
NetworkLayer::handleHello(const u_char *buf, int len, int device_id)
{
    bool ret;
    u_short is_request = *(u_short *)(buf + SIZE_IPv4 + IPv4_ADDR_LEN);
    is_request = change_order(is_request);
    struct in_addr dest_ip = *(struct in_addr *)(buf + SIZE_IPv4);
    if(is_request){ // Send back
        u_char *packet = new u_char[len];
        memcpy(packet, buf, len);
        packet[SIZE_IPv4 + IPv4_ADDR_LEN] = 0x00; // reply
        struct in_addr src_ip = routing_table.my_IP_addrs[0];
        memcpy(packet + SIZE_IPv4, &src_ip, IPv4_ADDR_LEN);
        if(
            device_manager.sendFrame(packet, len, ETHTYPE_IPv4,
                                     dest_ip, device_id) == -1
        ){
            delete[] packet;
            std::cout << "(NetworkLayer::handleHello) send frame error!\n";
            return false;
        }
    }
    
    // Update neighbors
    bool exist = false;
    u_short age = *(u_short *)(buf + SIZE_IPv4 + IPv4_ADDR_LEN + 2);
    age = change_order(age);
    unsigned int age_is_request = ((unsigned int)age << 16) | is_request;
    routing_table.neighbor_mutex.lock();
    for(int i = 0; i < routing_table.neighbors.size(); i++){
        if(routing_table.neighbors[i].first.s_addr == dest_ip.s_addr){
            routing_table.neighbors[i] = std::make_pair(dest_ip, 
                                                        age_is_request);
            exist = true;
            break;
        }
    }
    if(!exist){
        routing_table.neighbors.push_back(
            std::make_pair(dest_ip, (unsigned int)age)
        );
        routing_table.ip2device[dest_ip.s_addr] = device_id;
    }
    routing_table.neighbor_mutex.unlock();

    return true;
}

/**
 * @brief Link state packet handler.
 * 
 * @param buf IPv4 header + Link state packet.
 * @param len Length of IPv4 packet.
 * @param device_id Device ID where the packet comes from.
 * @return true on success, false on failure.
 */
bool 
NetworkLayer::handleLinkState(const u_char *buf, int len, int device_id)
{
    // Retrieve link state packet
    const u_char *buffer = buf + SIZE_IPv4;
    LinkStatePacket *link_state_packet = new LinkStatePacket;
    unsigned int reversed_seq = *(unsigned int *)buffer;
    link_state_packet->seq = change_order(reversed_seq);
    buffer += 4;
    unsigned int reversed_age = *(unsigned int *)buffer;
    link_state_packet->age = change_order(reversed_age);
    buffer += 4;
    u_short addr_size = *(u_short *)buffer;
    addr_size = change_order(addr_size);
    buffer += 2;
    u_short neighbor_size = *(u_short *)buffer;
    neighbor_size = change_order(neighbor_size);
    struct in_addr IP_addr;
    buffer += 2;
    for(int i = 0; i < addr_size; i++){
        memcpy(&IP_addr, buffer + 4 * i, 4);
        link_state_packet->router_id.push_back(IP_addr);
    }
    buffer += 4 * addr_size;
    struct in_addr mask;
    for(int i = 0; i < addr_size; i++){
        memcpy(&mask, buffer + 4 * i, 4);
        link_state_packet->mask.push_back(mask);
    }
    buffer += 4 * addr_size;
    unsigned int *neighbors = (unsigned int *)buffer;
    for(int i = 0; i < neighbor_size; i++){
        struct in_addr id;
        unsigned int dist;
        id.s_addr =  neighbors[2 * i];
        dist = neighbors[2 * i + 1];
        link_state_packet->neighbors.push_back(std::make_pair(id, dist));
    }

    // If the packet is from itself
    if(routing_table.my_IP_addrs[0].s_addr == 
       link_state_packet->router_id[0].s_addr)
    {
        return true;
    }

    // Update link states
    bool exist = false;
    routing_table.link_state_mutex.lock();
    for(int i = 0; i < routing_table.link_state_list.size(); i++)
    {   
        LinkStatePacket *link_state = routing_table.link_state_list[i];
        if(link_state->router_id[0].s_addr == 
           link_state_packet->router_id[0].s_addr)
        {
            if(link_state->seq <= link_state_packet->seq){
                routing_table.link_state_list[i] = link_state_packet;
                delete link_state;
                for(auto id: routing_table.device_ids){
                    if(id != device_id){
                        struct in_addr dest;
                        dest.s_addr = IPv4_ADDR_BROADCAST;
                        device_manager.sendFrame(buf, len, ETHTYPE_IPv4, 
                                                 dest, id);
                    }
                }
            }
            exist = true;
            break;
        }
    }
    if(!exist){
        routing_table.link_state_list.push_back(link_state_packet);
    }
    routing_table.link_state_mutex.unlock();
    return true;
}

/**
 * @brief Get the first IP address.
 */
struct in_addr 
NetworkLayer::getIP()
{
    return routing_table.my_IP_addrs[0];
}

/**
 * @brief Find whether ADDR is one of the host's IP address.
 */
bool 
NetworkLayer::findIP(const struct in_addr addr)
{
    routing_table.table_mutex.lock();
    for(auto &i: routing_table.my_IP_addrs){
        if(i.s_addr == addr.s_addr){
            routing_table.table_mutex.unlock();
            return true;
        }
    }
    routing_table.table_mutex.unlock();
    return false;
}