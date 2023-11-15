/**
 * @file device_manager.cpp
 */

#include <ethernet/device_manager.h>
#include <linux/if_packet.h>
#include <iostream>

/**
 * @brief Constructor of `DeviceManager`.
 */
DeviceManager::DeviceManager(NetworkLayer *net): 
    name2id(), id2device(), next_device_ID(0)
{
    epoll_server = new EpollServer(net);

    // Find all devices.
    int ret = 0;
    char errbuf[PCAP_ERRBUF_SIZE] = "";
    pcap_if_t *devsp = NULL;
    ret = pcap_findalldevs(&devsp, errbuf);
    if(ret != 0 || devsp == NULL){
        std::cerr << "No device available: " << errbuf << std::endl;
        return;
    }

    // Since my network stack only support Ethernet II now, I leave behind 
    // devices using Ethernet II. Here, they are who have MAC addresses.
    // Moreover, I also discarded "lo"'s because they seemed not to have 
    // selectable descriptors available.
    bool lo_appeared = false;
    for(pcap_if_t *dev = devsp; dev != NULL; dev = dev->next){
        if(!lo_appeared){
            if(!strcmp(dev->name, "lo")){
                lo_appeared = true;
                continue;
            }
        }
        if (dev->addresses != NULL) {
            struct sockaddr_ll *s = (struct sockaddr_ll *)dev->addresses->addr;
            std::string dev_name = dev->name;
            memcpy(all_dev[dev_name], s->sll_addr, ETHER_ADDR_LEN);
        }
    }

    // Free devsp.
    pcap_freealldevs(devsp);
}

/**
 * @brief Destructor of `DeviceManager`. Free memory allocated by `addDevice`.
 */
DeviceManager::~DeviceManager()
{
    delete epoll_server;
    for(auto id = id2device.begin(); 
        id != id2device.end(); id++){
        delete id->second;
    }
}

/**
 * @brief Add a device to the library for sending/receiving packets.
 *
 * @param device Name of network device to send/receive packet on.
 * @return A non-negative _device-ID_ on success, `-1` on error.
 */
int 
DeviceManager::addDevice(const char* device)
{
    std::string device_name = device;

    auto dev_it = all_dev.find(device_name);
    if(dev_it == all_dev.end()){
        std::cerr << device << " is not one of all devices!" << std::endl;
        return -1;
    }

    auto it = name2id.find(device_name);
    if(it != name2id.end()){
        std::cerr << "Device " << device << " is already added!" << std::endl;
        return -1;
    }
    else{
        name2id[device_name] = next_device_ID;
        Device* new_device = new Device(device, dev_it->second, 
                                        next_device_ID);
        id2device[next_device_ID] = new_device;
        epoll_server->addRead(new_device->getFD(), new_device);
        return next_device_ID++;
    }
}

/**
 * @brief Find a device added by `addDevice`.
 *
 * @param device Name of the network device.
 * @return A non-negative _device-ID_ on success, `-1` if no such device
 * was found.
 */
int 
DeviceManager::findDevice(const char* device)
{
    std::string device_name = device;
    auto it = name2id.find(device_name);
    if(it != name2id.end()){
        return it->second;
    }
    else{
        std::cerr << "Device " << device << " not found!" << std::endl;
        return -1;
    }
}

/**
 * @brief Encapsulate some data into an Ethernet II frame and send it.
 *
 * @param buf Pointer to the payload.
 * @param len Length of the payload.
 * @param ethtype EtherType field value of this frame.
 * @param dest_ip IP address of the destination.
 * @param id ID of the device(returned by `addDevice`) to send on.
 * @return 0 on success, -1 on error.
 * @see addDevice
 */
int 
DeviceManager::sendFrame(const void* buf, int len, int ethtype, 
                         struct in_addr dest_ip, int id)
{
    auto it = id2device.find(id);
    if(it != id2device.end()){
        return it->second->sendFrame(buf, len, ethtype, dest_ip);
    }
    else{
        std::cerr << "No device " << id << "!" << std::endl;
        return -1;
    }
}

/**
 * @brief Encapsulate some data into an Ethernet II frame and send it.
 *
 * @param buf Pointer to the payload.
 * @param len Length of the payload.
 * @param ethtype EtherType field value of this frame.
 * @param dest_ip IP address of the destination.
 * @see addDevice
 */
void 
DeviceManager::sendFrameAll(const void* buf, int len, int ethtype, 
                            struct in_addr dest_ip)
{
    for(auto &it: id2device){
        it.second->sendFrame(buf, len, ethtype, dest_ip);
    }
}

/**
 * @brief Register a callback function to be called each time an
 * Ethernet II frame was received.
 *
 * @param callback the callback function.
 * @param id ID of the device to have callback as its callback function.
 * @return 0 on success, -1 on error.
 * @see frameReceiveCallback
 */
int 
DeviceManager::setFrameReceiveCallback(frameReceiveCallback callback, int id)
{
    auto it = id2device.find(id);
    if(it != id2device.end()){
        it->second->setFrameReceiveCallback(callback);
        return 0;
    }
    else{
        return -1;
    }
}

/**
 * @brief Register a callback function to be called each time an
 * Ethernet II frame was received for all devices.
 *
 * @param callback the callback function.
 * @see frameReceiveCallback
 */
void 
DeviceManager::setFrameReceiveCallbackAll(frameReceiveCallback callback)
{
    for(auto &it: id2device){
        it.second->setFrameReceiveCallback(callback);
    }
}

/**
 * @brief List all devices found by `pcap_findalldevs`. This function is 
 * specially for Checkpoint 1.
 */
void 
DeviceManager::listAllDevice()
{
    for(auto &dev: all_dev){
        std::cout << dev.first << ": " << std::endl;
        printf("\tMAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n", 
                dev.second[0], dev.second[1], dev.second[2],
                dev.second[3], dev.second[4], dev.second[5]);
    }
}

/**
 * @brief Add all devices found by `pcap_findalldevs`. This function is used 
 * by network layer to add all devices to the device manager. I didn't use 
 * `addDevice` because there is no need to find `dev` in `all_dev`.
 * 
 * @return 0 on success, -1 on error.
 */
int 
DeviceManager::addAllDevice()
{
    for(auto &dev: all_dev){
        name2id[dev.first] = next_device_ID;
        Device* new_device = new Device(dev.first.c_str(), dev.second,
                                        next_device_ID);
        id2device[next_device_ID] = new_device;
        epoll_server->addRead(new_device->getFD(), new_device);
        next_device_ID++;
    }
    return 0;
}

/**
 * @brief Capture a frame on device id.
 * 
 * @param id ID of the device that will capture a frame.
 * @return 0 on success, -1 on error.
 */
int 
DeviceManager::capNext(int id)
{
    auto it = id2device.find(id);
    if(it != id2device.end()){
        it->second->capNext();
        return 0;
    }
    else{
        std::cerr << "No device " << id;
        std::cerr << "! Can't receive packet!" << std::endl;
        return -1;
    }
}

/**
 * @brief Captures `cnt` frames on device `id`.
 * 
 * @param id ID of the device that will capture frames.
 * @param cnt An integer that tells pcap_loop() how many packets it should 
 * sniff for before returning (a non-positive value means it should sniff 
 * until an error occurs).
 * @return 0 on success, -1 on error.
 */
int 
DeviceManager::capLoop(int id, int cnt)
{
    auto it = id2device.find(id);
    if(it != id2device.end()){
        return it->second->capLoop(cnt);
    }
    else{
        std::cerr << "No device " << id;
        std::cerr << "! Can't receive packet!" << std::endl;
        return -1;
    }
}

/**
 * @brief Function used by a sub-thread of my network stack to wait for 
 * packets to arrive.
 * 
 * @param epoll_server Passed from the father thread.
 * @note `while(true)` may be optimized.
 */
void
DeviceManager::readLoop(EpollServer *epoll_server)
{
    while (true)
    {
        epoll_server->waitRead();
    }
}

/**
 * @brief Set IP address for `device`.
 * 
 * @param addr IP address of the device.
 * @param device_name Name of the device.
 */
void 
DeviceManager::setIP(struct in_addr addr, const char *device_name)
{
    int device_id = findDevice(device_name);
    if(device_id != -1){
        Device *device = id2device[device_id];
        device->setIP(addr);
    }
    else{
        std::cerr << "(DeviceManager::setIP) Find device error!" << std::endl;
        for(auto &i: name2id){
            std::cerr << i.first << " " << i.second << std::endl;
        }
    }
    return;
}

/**
 * @brief Request ARP for all devices.
 */
void 
DeviceManager::requestARP()
{
    for(auto &it: id2device){
        if(!it.second->request_ARP()){
            std::cerr << "Device " << it.first << " request ARP error!\n"; 
        }
    }
}