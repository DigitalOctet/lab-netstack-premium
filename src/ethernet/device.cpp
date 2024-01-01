/**
 * @file device.cpp
 */

#include <ethernet/device.h>
#include <ethernet/endian.h>
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
Device::Device(const char *device, u_char mac[ETHER_ADDR_LEN], int i): 
    callback(NULL), frame_id(0), fd(-1), ip_addr({0}), id(i)
{
    // Open handler.
    char errbuf[PCAP_ERRBUF_SIZE] = "";
    handle = pcap_open_live(device, BUFSIZ, 1, 1000, errbuf);
    if(handle == NULL){
        std::cerr << "Couldn't find default device: " << errbuf << std::endl;
        return;
    }

    // Put the handle into non-blocking mode.
    int ret;
    ret = pcap_setnonblock(handle, 1, errbuf);
    if(ret == -1){
        std::cerr << "Set non-block error: " << errbuf << std::endl;
        return;
    }

    // Get the corresponding file descriptor.
    fd = pcap_get_selectable_fd(handle);
    if(fd == -1){
        std::cerr << "No selectable descriptor available for ";
        std::cerr << device << std::endl;
        return;
    }

    // Copy mac address.
    memcpy(mac_addr, mac, ETHER_ADDR_LEN);
    memset(dst_MAC_addr, 0, ETHER_ADDR_LEN);
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
 * If the length is less than 46, pad the packet with 0.
 */
inline bool
Device::is_valid_length(int len)
{
    return (len <= MAX_PAYLOAD);
}

/**
* @brief Encapsulate some data into an Ethernet II frame and send it.
*
* @param buf Pointer to the payload.
* @param len Length of the payload.
* @param ethtype EtherType field value of this frame.
* @param dest_ip IP address of the destination.
* @return 0 on success, -1 on error.
* @see addDevice
*/
int 
Device::sendFrame(const void* buf, int len, 
                  int ethtype, const struct in_addr dest_ip)
{
    int frame_len, real_len;
    u_char *frame;
    if(!is_valid_length(len)){
        std::cerr << "Data length invalid: " << len << " !" << std::endl;
        return -1;
    }
    real_len = len < MIN_PAYLOAD ? MIN_PAYLOAD : len;
    u_short correct_ethtype = change_order((u_short)ethtype);
    frame_len = SIZE_ETHERNET + real_len;
    frame = new u_char[frame_len];
    if(!check_MAC()){
        std::cerr << "Send frame failed: destination MAC unavailable!\n";
        delete[] frame;
        return -1;
    }
    memcpy(frame, dst_MAC_addr, ETHER_ADDR_LEN);
    memcpy(frame + ETHER_ADDR_LEN, mac_addr, ETHER_ADDR_LEN);
    memcpy(frame + 2 * ETHER_ADDR_LEN, &correct_ethtype, ETHER_TYPE_LEN);
    memcpy(frame + SIZE_ETHERNET, buf, len);
    memset(frame + SIZE_ETHERNET + len, 0, real_len - len);
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
    char errbuf[PCAP_ERRBUF_SIZE] = "";
    int ret;
    ret = pcap_setnonblock(handle, 0, errbuf);
    if(ret == -1){
        std::cerr << "Set non-block error: " << errbuf << std::endl;
        return -1;
    }
    struct pcap_pkthdr header;	/* The header that pcap gives us */
    const u_char *frame = pcap_next(handle, &header);
    if(frame == NULL){
        std::cerr << "Frame capture failed!" << std::endl;
        return -1;
    }

    if(callback){
        frame_id++;
        std::cout << "Frame " << frame_id << " captured." << std::endl;
        return callback(frame, header.len);
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

/**
 * @brief Capture the next arrived on this device. Wrapper of `pcap_next_ex`.
 * 
 * @param header The header that pcap gives us.
 * @param data Pointer to pointer to data.
 * @return 0 if no packets are currently available, -1 on error, 1 on success.
 * @see pcap_next_ex
 */
int 
Device::capNextEx(struct pcap_pkthdr **header, const u_char **data)
{
    int ret;
    ret = pcap_next_ex(handle, header, data);
    if(ret < 0){
        std::cerr << "Frame capture failed(" << ret << ")!" << std::endl;
        return -1;
    }
    else if(ret == 0){ // No packets are currently available.
        return 0;
    }

    frame_id++;

    int offset = 0;
    if(callback){
        callback(*data, (*header)->caplen);
    }

    return ret;
}

/**
 * @brief Get file descriptor corresponding to the device if it is available.
 * @return FD on success, -1 on error.
 */
int 
Device::getFD()
{
    if(fd == -1){
        std::cerr << "File descriptor not available!" << std::endl;
    }
    return fd;
}

/**
 * @brief Actual callback function used in my network stack on receiving a 
 * frame.
 * 
 * @param buf Pointer to the frame.
 * @param len Length of the frame.
 * @return Length after it consumes from buf, i.e., length that should be 
 * passed to network layer. 0 if the frame doesn't need to be passed.
 * -1 on error.
 */
int 
Device::callBack(const u_char *buf, int len)
{
    int rest_len = len;
    EthernetHeader *eth_header = (EthernetHeader *)buf;
    eth_header->ether_type = change_order(eth_header->ether_type);
    switch (eth_header->ether_type)
    {
    case ETHTYPE_IPv4:
        rest_len -= SIZE_ETHERNET;
        if(!check_MAC(eth_header->ether_dhost)){
            return -1;
        }
        break;

    case ETHTYPE_ARP:
        rest_len = 0;
        if(len != SIZE_ETHERNET + MIN_PAYLOAD){
            std::cout << "Length error: an invalid ARP packet received!\n";
            return -1;
        }
        if(!check_MAC(eth_header->ether_dhost)){
            return -1;
        }
        else if(!handle_ARP(buf + SIZE_ETHERNET)){
            return -1;
        }
        break;
    
    default:
        printf("Ethernet type %04x is not implemented!\n", 
               eth_header->ether_type);
        rest_len = -1;
        break;
    }

    return rest_len;
}

/**
 * @brief Check whether this device is the destination.
 * 
 * @param MAC Destination MAC address of the frame.
 * @return true if it is, false otherwise
 * 
 * @note Now it should be or there is an error.
 */
inline bool 
Device::check_MAC(u_char MAC[ETHER_ADDR_LEN])
{
    bool is_broadcast = true;
    for(int i = 0; i < ETHER_ADDR_LEN; i++){
        if(MAC[i] == 0xff){
            is_broadcast = false;
            break;
        }
    }
    if(is_broadcast){
        return true;
    }
    for(int i = 0; i < ETHER_ADDR_LEN; i++){
        if(mac_addr[i] != MAC[i]){
            return false;
        }
    }
    return true;
}

/**
 * @brief Check whether `dst_MAC_addr` is not empty.
 * 
 * @return true if it is, false otherwise
 * 
 */
inline bool 
Device::check_MAC()
{
    for(int i = 0; i < ETHER_ADDR_LEN; i++){
        if(mac_addr[i] != 0){
            return true;
        }
    }
    return false;
}

/**
 * @brief Set destination MAC address on receiving an ARP frame and reply if
 * it is a request.
 * 
 * @param buf Frame excluding Ethernet header.
 * @return true on success or if it's useless, false on error.
 * 
 * @note The logic of responding and replying should be changed after my 
 * virtual Ethernet is implemented.
 */
bool 
Device::handle_ARP(const u_char *buf)
{
    // Error handling
    struct ARPPacket *arp = (struct ARPPacket *)buf;
    if(arp->hardware_type != HARDWARE_TYPE_REVERSED){
        std::cout << "Invalid hardware type!" << std::endl;
        return true;
    }
    if(arp->protocol_type != ETHTYPE_IPv4_REVERSED){
        std::cout << "Invalid protocol type!" << std::endl;
        return true;
    }
    if(arp->hardware_size != HARDWARE_SIZE){
        std::cerr << "Invalid hardware size!" << std::endl;
        return false;
    }
    if(arp->protocol_size != PROTOCOL_SIZE){
        std::cerr << "Invalid protocol size!" << std::endl;
        return false;
    }

    // Handle a request or reply.
    bool ret;
    arp_mutex.lock();
    // Reply ARP has exact MAC and IP address.
    if(IS_ARP_REQUEST(arp->opcode)){
        memcpy(dst_MAC_addr, arp->sender_MAC_addr, ETHER_ADDR_LEN);
        ret = reply_ARP(mac_addr, ip_addr, 
                        arp->sender_MAC_addr, 
                        *(struct in_addr *)arp->sender_IP_addr);
    }
    else if(IS_ARP_REPLY(arp->opcode)){
        if(!check_MAC(arp->target_MAC_addr)){
            ret = false;
        }
        memcpy(dst_MAC_addr, arp->sender_MAC_addr, ETHER_ADDR_LEN);
        ret = true;
    }
    arp_mutex.unlock();
    return ret;
}

/**
 * @brief Send an ARP reply packet back.
 * 
 * @param sender_MAC Sender MAC address of the reply ARP packet
 * @param sender_IP Sender IP address of the reply ARP packet
 * @param target_MAC Target MAC address of the reply ARP packet
 * @param target_IP Target IP address of the reply ARP packet
 * @return true on success, false on failure.
 */
bool 
Device::reply_ARP(
    const u_char sender_MAC[ETHER_ADDR_LEN], 
    const struct in_addr sender_IP,
    const u_char target_MAC[ETHER_ADDR_LEN],
    const struct in_addr target_IP
)
{
    u_char *buf = new u_char[MIN_PAYLOAD];
    memset(buf, 0, MIN_PAYLOAD);
    struct ARPPacket *arp = (struct ARPPacket *)buf;
    arp->hardware_type = HARDWARE_TYPE_REVERSED;
    arp->protocol_type = ETHTYPE_IPv4_REVERSED;
    arp->hardware_size = HARDWARE_SIZE;
    arp->protocol_size = PROTOCOL_SIZE;
    arp->opcode = ARP_REPLY_REVERSED;
    memcpy(arp->sender_MAC_addr, sender_MAC, ETHER_ADDR_LEN);
    memcpy(arp->sender_IP_addr, &sender_IP, IPv4_ADDR_LEN);
    memcpy(arp->target_MAC_addr, target_MAC, ETHER_ADDR_LEN);
    memcpy(arp->target_IP_addr, &target_IP, IPv4_ADDR_LEN);
    int ret = sendFrame(arp, MIN_PAYLOAD, ETHTYPE_ARP, target_IP);
    delete[] buf;
    return ret == 0 ? true : false;
}

/**
 * @brief Send an ARP request packet back.
 * 
 * @return true on success, false on failure.
 */
bool 
Device::request_ARP()
{
    u_char target_MAC[ETHER_ADDR_LEN];
    for(int i = 0; i < ETHER_ADDR_LEN; i++){
        target_MAC[i] = 0xff;
    }
    struct in_addr target_IP;
    target_IP.s_addr = IPv4_ADDR_BROADCAST;
    u_char *buf = new u_char[MIN_PAYLOAD];
    memset(buf, 0, MIN_PAYLOAD);
    struct ARPPacket *arp = (struct ARPPacket *)buf;
    arp->hardware_type = HARDWARE_TYPE_REVERSED;
    arp->protocol_type = ETHTYPE_IPv4_REVERSED;
    arp->hardware_size = HARDWARE_SIZE;
    arp->protocol_size = PROTOCOL_SIZE;
    arp->opcode = ARP_REQUEST_REVERSED;
    memcpy(arp->sender_MAC_addr, mac_addr, ETHER_ADDR_LEN);
    memcpy(arp->sender_IP_addr, &ip_addr, IPv4_ADDR_LEN);
    memcpy(arp->target_MAC_addr, target_MAC, ETHER_ADDR_LEN);
    memcpy(arp->target_IP_addr, &target_IP, IPv4_ADDR_LEN);
    arp_mutex.lock();
    int ret = sendFrame(buf, MIN_PAYLOAD, ETHTYPE_ARP, target_IP);
    arp_mutex.unlock();
    delete[] buf;
    return ret == 0 ? true : false;
}

/**
 * @brief Set IP address for `device`.
 * @param addr IP address of the device.
 */
void 
Device::setIP(struct in_addr addr)
{
    ip_addr = addr;
}