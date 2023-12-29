/**
 * @file epoll_server.h
 * 
 * @brief Defines a server class `EpollServer` using epoll for receiving 
 * frames in a non-blocking way. I finally chose epoll because it's more 
 * efficient than other motheds like select or poll.
 */

#pragma once

#include "device.h"
#include <sys/epoll.h>
#include <map>

class NetworkLayer;
class TransportLayer;

#define MAX_EVENTS 256
#define TIMEOUT 100

class EpollServer
{
private:
    int epfd;
    struct epoll_event events[MAX_EVENTS];
    std::map<int, Device *> fd2device;
    NetworkLayer *network_layer;
    TransportLayer *transport_layer;
public:
    EpollServer(NetworkLayer *net, TransportLayer *trans);
    ~EpollServer();
    int addRead(int fd, Device *device);
    int waitRead();
};
