/**
 * @file tcp.h
 * @brief Library supporting POSIX-compatible socket library functions with a
 * class implementing TCP while exposing POSIX-like functions to wrappers.
 */

#pragma once

#include "bitmap.h"
#include "segment.h"
#include "tcb.h"
#include <ip/ip.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <semaphore.h>
#include <map>

#define PORT_BEGIN 49152
#define PORT_END   65536

class TransportLayer
{
private:
    TransportLayer();
    ~TransportLayer();
    TransportLayer(const TransportLayer &) = delete;
    TransportLayer &operator=(const TransportLayer &) = delete;

    int default_fd;
    std::map<int, TCB *> fd2tcb;
    std::set<TCB *> tcbs;
    std::mutex tcb_mutex;
    NetworkLayer *network_layer;
    BitMap bitmap;

    // Private helper functions
    size_t generatePort();
    // Send segments
    bool sendSegment(TCB *socket, SegmentType::SegmentType type, 
                     const void *buf, int len);

public:
    static TransportLayer &getInstance();

    // Socket interface
    int _socket(int domain, int type, int protocol);
    int _bind(int socket, const struct sockaddr *address, 
              socklen_t address_len);
    int _listen(int socket, int backlog);
    int _connect(int socket, const struct sockaddr *address, 
                 socklen_t address_len);
    int _accept(int socket, struct sockaddr *address, socklen_t *address_len);
    ssize_t _read(int fildes, void *buf, size_t nbyte);
    ssize_t _write(int fildes, const void *buf, size_t nbyte);
    int _close(int fildes);
    int _getaddrinfo(const char *node, const char *service,
                     const struct addrinfo *hints, struct addrinfo **res);

    // Receiving segments
    bool callBack(const u_char *buf, int len, 
                  struct in_addr src_addr, struct in_addr dst_addr);

    // Timed wait thread
    void timedWait(TCB *tcb);
    // Retransmission
    void updateRetrans();
};