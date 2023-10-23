/**
 * @file device_manager.h
 * @brief Class supporting network device management.
 */

#ifndef ETHERNET_DEVICE_MANAGER_H
#define ETHERNET_DEVICE_MANAGER_H

#include "epoll_server.h"
#include <pcap.h>
#include <cstring>
#include <string>

/**
 * @brief Class supporting network device management derived from 
 * `EpollServer`.
 */
class DeviceManager
{
private:
    int next_device_ID;
    std::map<std::string, int> name2id;
    std::map<int, Device *> id2device;
    std::map<std::string, u_char[ETHER_ADDR_LEN]> all_dev;
public:
    EpollServer *epoll_server;
    
    DeviceManager();
    ~DeviceManager();
    int addDevice(const char* device);
    int findDevice(const char* device);
    int sendFrame(const void* buf, int len, int ethtype, 
                  const void* destmac, int id);
    int setFrameReceiveCallback(frameReceiveCallback callback, int id);
    void setFrameReceiveCallbackAll(frameReceiveCallback callback);
    void listAllDevice();
    int addAllDevice();
    int capNext(int id);
    int capLoop(int id, int cnt);
};

#endif /**< ETHERNET_DEVICE_MANAGER_H */