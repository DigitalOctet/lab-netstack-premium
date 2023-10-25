/**
 * @file device_manager.h
 * @brief Class supporting network device management.
 */

#ifndef ETHERNET_DEVICE_MANAGER_H
#define ETHERNET_DEVICE_MANAGER_H

#include "epoll_server.h"
#include "frame.h"
#include <pcap.h>
#include <cstring>
#include <map>
#include <string>

class NetworkLayer;

/**
 * @brief Class supporting network device management.
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
    
    DeviceManager(NetworkLayer *net);
    ~DeviceManager();
    int addDevice(const char* device);
    int findDevice(const char* device);
    int sendFrame(const void* buf, int len, int ethtype, 
                  struct in_addr dest_ip, int id);
    void sendFrameAll(const void* buf, int len, int ethtype, 
                     struct in_addr dest_ip);
    int setFrameReceiveCallback(frameReceiveCallback callback, int id);
    void setFrameReceiveCallbackAll(frameReceiveCallback callback);
    void listAllDevice();
    int addAllDevice();
    int capNext(int id);
    int capLoop(int id, int cnt);
    void readLoop(EpollServer *epoll_server);
    void setIP(struct in_addr addr, const char *device);
    void requestARP();
};

#endif /**< ETHERNET_DEVICE_MANAGER_H */