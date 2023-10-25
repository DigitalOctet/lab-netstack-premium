/**
 * @file epoll_server.h
 * 
 * @brief Defines a server class `EpollServer` using epoll for receiving 
 * frames in a non-blocking way. I finally chose epoll because it's more 
 * efficient than other motheds like select or poll.
 */

#ifndef ETHERNET_EPOLL_SERVER_H
#define ETHERNET_EPOLL_SERVER_H

#include "device.h"
#include <sys/epoll.h>
#include <map>

class NetworkLayer;

#define MAX_EVENTS 256
#define TIMEOUT 100

class EpollServer
{
private:
    int epfd;
    struct epoll_event events[MAX_EVENTS];
    std::map<int, Device *> fd2device;
    NetworkLayer *network_layer;
public:
    EpollServer(NetworkLayer *net);
    ~EpollServer();
    int addRead(int fd, Device *device);
    int waitRead();
};

#endif /**< ETHERNET_EPOLL_SERVER_H */