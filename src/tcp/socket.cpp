/**
 * @file socket.cpp
 */

#include <tcp/socket.h>
#include <tcp/tcp.h>

int __wrap_socket(int domain, int type, int protocol)
{
    return TransportLayer::getInstance()._socket(domain, type, protocol);
}

int __wrap_bind(int socket, const struct sockaddr *address,
                socklen_t address_len)
{
    return TransportLayer::getInstance()._bind(socket, address, address_len);
}

int __wrap_listen(int socket, int backlog)
{
    return TransportLayer::getInstance()._listen(socket, backlog);
}

int __wrap_connect(int socket, const struct sockaddr *address,
                   socklen_t address_len)
{
    return TransportLayer::getInstance()._connect(socket, address, address_len);
}

int __wrap_accept(int socket, struct sockaddr *address, 
                  socklen_t *address_len)
{
    return TransportLayer::getInstance()._accept(socket, address, address_len);
}

ssize_t __wrap_read(int fildes, void *buf, size_t nbyte)
{
    return TransportLayer::getInstance()._read(fildes, buf, nbyte);
}

ssize_t __wrap_write(int fildes, const void *buf, size_t nbyte)
{
    return TransportLayer::getInstance()._write(fildes, buf, nbyte);
}

int __wrap_close(int fildes)
{
    return TransportLayer::getInstance()._close(fildes);
}

int __wrap_getaddrinfo(const char *node, const char *service,
                       const struct addrinfo *hints, struct addrinfo **res)
{
    return TransportLayer::getInstance()._getaddrinfo(node, service, 
                                                      hints, res);
}