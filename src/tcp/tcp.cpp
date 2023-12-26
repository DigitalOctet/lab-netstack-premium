/**
 * @file tcp.cpp
 * 
 * @note I didn't find a way to deal with segmentation faults triggered by 
 * invalid pointers.
 * @note My implementation is implicitly reentrant. If the sockets and pointers 
 * passed to my functions don't share the same memory, they are reentrant. If 
 * multiple threads use the same socket, they are thread safe.
 */

#include <ethernet/endian.h>
#include <tcp/real_socket.h>
#include <tcp/tcp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <mutex>

#define SOMAXCONN 4096

/**
 * @brief Constructor of `TransportLayer`. Open "/dev/null" as a default file 
 * descriptor. This is used for allocating new file descriptors while remaining 
 * the semantics of the standard one.
 */
TransportLayer::TransportLayer(): fd2tcb(), bitmap(PORT_END - PORT_BEGIN)
{
    default_fd = open("/dev/null", O_RDWR, 0);
    if(default_fd < 0){
        std::cerr << "Open \"dev/null\" error!" << std::endl;
        return;
    }

    network_layer = new NetworkLayer();
}

/**
 * @brief Destructor of `TransportLayer`.
 */
TransportLayer::~TransportLayer()
{
    if(network_layer)
        delete network_layer;
    __real_close(default_fd);
    for(auto &i: fd2tcb) { // If the sockets aren't closed by user.
        __real_close(i.first);
        delete i.second;
        fd2tcb.erase(i.first);
    }
}

/**
 * @brief Get the instance of TransportLayer. If it doesn't exist, create a 
 * new one.
 */
TransportLayer &
TransportLayer::getInstance()
{
    mutex.lock();
    if(!instance){
        instance = new TransportLayer();
    }
    mutex.unlock();

    return *instance;
}

/**
 * @brief Create an empty socket with its transimission control block.
 * @details Check whether arguments are supported. 
 * Allocate a new file descriptor for the socket. 
 * Create a new `Socket` instance.
 */
int 
TransportLayer::_socket(int domain, int type, int protocol)
{
    if((domain != AF_INET) || (type != SOCK_STREAM)) {
        return __real_socket(domain, type, protocol);
    }
    if((protocol != 0) || (protocol != IPPROTO_TCP)) {
        return __real_socket(domain, type, protocol);
    }

    int fd = dup(default_fd);
    TCB *tcb = new TCB();
    fd2tcb[fd] = tcb;
    return fd;
}

int 
TransportLayer::_bind(int socket, const struct sockaddr *address, 
                      socklen_t address_len)
{
    auto it = fd2tcb.find(socket);
    if(it == fd2tcb.end()){
        return __real_bind(socket, address, address_len);
    }

    // Invalid `address_len`
    if(address_len != sizeof(struct sockaddr_in)){
        errno = EINVAL;
        return -1;
    }

    // If the socket has already been bound to an address
    TCB *tcb = it->second;
    tcb->mutex.lock();
    if(tcb->socket_state != SocketState::UNOPENED){
        tcb->mutex.unlock();
        errno = EINVAL;
        return -1;
    }

    const struct sockaddr_in *addr = (const struct sockaddr_in *)address;
    if(addr->sin_family != AF_INET){
        tcb->mutex.unlock();
        errno = EAFNOSUPPORT; // Address family not supported by protocol
        return -1;
    }
    if(addr->sin_addr.s_addr != INADDR_ANY){
        if(!network_layer->findIP(addr->sin_addr)){
            tcb->mutex.unlock();
            errno = EADDRNOTAVAIL; // Cannot assign requested address
            return -1;
        }
        else{
            tcb->src_addr = addr->sin_addr;
        }
    }
    else{
        tcb->src_addr = network_layer->getIP();
    }
    tcb->src_port = addr->sin_port;
    tcb->socket_state = SocketState::BOUND;
    tcb->mutex.unlock();
    return 0;
}

int 
TransportLayer::_listen(int socket, int backlog)
{
    auto it = fd2tcb.find(socket);
    if(it == fd2tcb.end()){
        return __real_listen(socket, backlog);
    }

    TCB *tcb = it->second;
    tcb->mutex.lock();
    if(tcb->socket_state == SocketState::ACTIVE){
        tcb->mutex.unlock();
        errno = EINVAL;
        return -1;
    }
    else if(tcb->socket_state == SocketState::PASSIVE){
        // `listen()` doesn't fail if it has already been called on the same 
        // socket. I don't know its behavior. Here I simply discard all 
        // connections.
        while(!tcb->pending.empty()){
            tcb->pending.pop();
        }
    }

    // If the backlog argument is greater than the value in
    // /proc/sys/net/core/somaxconn, then it is silently capped to that
    // value.  Since Linux 5.4, the default in this file is 4096.
    // My implementation supports 4096.
    // @see https://man7.org/linux/man-pages/man2/listen.2.html
    if(backlog > SOMAXCONN){
        backlog = SOMAXCONN;
    }
    else if(backlog <= 0){
        // NOTE: I didn't find any documents that discribe the minimum value 
        // of backlog. But on my Linux machine, when backlog is less than or 
        // equal to 0, a server can still set up a conncetion in the 
        // background. So I set the min value of backlog to 1 for an easier 
        // implementation.
        backlog = 1;
    }
    tcb->backlog = backlog;
    tcb->socket_state = SocketState::PASSIVE;
    tcb->state = ConnectionState::LISTEN;
    tcb->mutex.unlock();
    return 0;
}

/**
 * 
 */
int 
TransportLayer::_connect(int socket, const struct sockaddr *address, 
                         socklen_t address_len)
{
    auto it = fd2tcb.find(socket);
    if(it == fd2tcb.end()){
        return __real_connect(socket, address, address_len);
    }

    // Invalid `address_len`
    if(address_len != sizeof(struct sockaddr_in)){
        errno = EINVAL;
        return -1;
    }

    // socket has already been set on a connection
    TCB *tcb = it->second;
    tcb->mutex.lock();
    if((tcb->socket_state == SocketState::ACTIVE) || 
       (tcb->socket_state == SocketState::PASSIVE))
    {
        tcb->mutex.unlock();
        errno = EISCONN;
        return -1;
    }

    // Bind socket to source and destination addresses.
    struct sockaddr_in *address_in = (struct sockaddr_in *)address;
    size_t port = generatePort();
    if (port == BITMAP_ERROR) { // Cannot assign a port
        tcb->mutex.unlock();
        errno = EADDRNOTAVAIL;
        return -1;
    }
    if(tcb->socket_state == SocketState::UNOPENED){
        tcb->src_addr = network_layer->getIP();
        tcb->src_port = change_order((u_short)port);
    }
    tcb->dst_addr = address_in->sin_addr;
    tcb->dst_port = address_in->sin_port;
    tcb->socket_state = SocketState::ACTIVE;

    // Send SYN
    sendSegment(tcb, SegmentType::SYN, NULL, 0);
    tcb->state = ConnectionState::SYN_SENT;
    tcb->mutex.unlock();
    sem_wait(&tcb->semaphore);
    return 0;
}

int 
TransportLayer::_accept(int socket, struct sockaddr *address, 
                        socklen_t *address_len)
{
    auto it = fd2tcb.find(socket);
    if(it == fd2tcb.end()){
        return __real_accept(socket, address, address_len);
    }

    TCB *conn_tcb;
    TCB *listen_tcb = it->second;
    listen_tcb->mutex.lock();
    if(listen_tcb->socket_state != SocketState::PASSIVE){
        listen_tcb->mutex.unlock();
        errno = EINVAL;
        return -1;
    }
    listen_tcb->mutex.unlock();
    sem_wait(&listen_tcb->semaphore); // Wait for a ready connection

    listen_tcb->mutex.lock();
    conn_tcb = listen_tcb->pending.front();
    listen_tcb->pending.pop();
    listen_tcb->mutex.unlock();
    int fd = dup(default_fd);
    fd2tcb[fd] = conn_tcb;

    // Return address
    if(address != NULL){
        struct sockaddr_in client_addr;
        memset(&client_addr, 0, sizeof(client_addr));
        client_addr.sin_family = AF_INET;
        client_addr.sin_addr = conn_tcb->src_addr;
        client_addr.sin_port = conn_tcb->src_port;
        if(*address_len > sizeof(client_addr)){
            *address_len = sizeof(client_addr);
        }
        memcpy(address, &client_addr, *address_len);
    }

    return fd;
}

/**
 * @note The behavior of reading and writing to a socket without a connection 
 * isn't specified in the document. My experiment show that it will cause 
 * the process to exit.
 */
ssize_t 
TransportLayer::_read(int fildes, void *buf, size_t nbyte)
{
    auto it = fd2tcb.find(fildes);
    if(it == fd2tcb.end()){
        return __real_read(fildes, buf, nbyte);
    }

    TCB *tcb = it->second;
    ssize_t rc;
    tcb->mutex.lock();
    if(tcb->socket_state != SocketState::ACTIVE){
        exit(0);
    }
    else if(tcb->socket_state != ConnectionState::FIN_WAIT1){
        exit(0);
    }
    rc = tcb->readWindow((u_char *)buf, nbyte);
    tcb->mutex.unlock();
    return rc;
}

/**
 * @note It won't block if the window size of the other end is 0.
 */
ssize_t 
TransportLayer::_write(int fildes, const void *buf, size_t nbyte)
{
    auto it = fd2tcb.find(fildes);
    if(it == fd2tcb.end()){
        return __real_write(fildes, buf, nbyte);
    }

    TCB *tcb = it->second;
    u_short dest_window;
    tcb->mutex.lock();
    if(tcb->socket_state != SocketState::ACTIVE){
        exit(0);
    }
    else if(tcb->state != ConnectionState::CLOSE_WAIT){
        exit(0);
    }
    dest_window = tcb->getDestWindow();
    if(dest_window >= nbyte){
        sendSegment(tcb, SegmentType::ACK, buf, nbyte);
        nbyte = 0;
        tcb->setDestWindow(dest_window - nbyte);
    }
    else{
        sendSegment(tcb, SegmentType::ACK, buf, dest_window);
        nbyte -= dest_window;
        tcb->setDestWindow(0);
    }
    tcb->mutex.unlock();
    return nbyte;
}

int 
TransportLayer::_close(int fildes)
{
    auto it = fd2tcb.find(fildes);
    if(it == fd2tcb.end()){
        return __real_close(fildes);
    }

    TCB *tcb = it->second;
    tcb->mutex.lock();
    if(tcb->state == ConnectionState::ESTABLISHED){
        tcb->state = ConnectionState::FIN_WAIT1;
    }
    else if(tcb->state == ConnectionState::LAST_ACK){
        tcb->state = ConnectionState::CLOSED;
    }
    else{
        
    }
    sendSegment(tcb, SegmentType::FIN_ACK, NULL, 0);
    tcb->mutex.unlock();
    return 0;
}

int 
TransportLayer::_getaddrinfo(const char *node, const char *service,
                             const struct addrinfo *hints, 
                             struct addrinfo **res)
{
    bool valid = true;

    struct in_addr addr;
    if((node != NULL) && (inet_pton(AF_INET, node, &addr) <= 0)){
        valid = false;
    }

    int int_port;
    u_short port;
    if(valid){
        if(service != NULL){
            int_port = atoi(service);
            if(int_port < 0 || int_port >= PORT_END){
                valid = false;
            }
            else{
                port = int_port;
            }
        }
        else if(node == NULL){
            valid == false;
        }
        else {
            valid = false;
        }
    }

    if(valid){
        if(hints != NULL){
            if((hints->ai_family != AF_INET) || 
               (hints->ai_socktype != SOCK_STREAM) ||
               (hints->ai_protocol != IPPROTO_TCP) ||
               (hints->ai_flags != 0))
            {
                valid = false;
            }
        }
    }

    if(!valid){
        return __real_getaddrinfo(node, service, hints, res);
    }

    struct addrinfo *p = new addrinfo();
    memset(p, 0, sizeof(*p));
    if(hints != NULL) {
        p->ai_flags = hints->ai_flags;
        p->ai_family = hints->ai_family;
        p->ai_socktype = hints->ai_socktype;
        p->ai_protocol = hints->ai_protocol;
    }
    else {
        p->ai_flags = 0;
        p->ai_family = AF_INET;
        p->ai_socktype = SOCK_STREAM;
        p->ai_protocol = IPPROTO_TCP;
    }
    p->ai_canonname = NULL;
    p->ai_addrlen = sizeof(struct sockaddr);
    ((struct sockaddr_in *)(p->ai_addr))->sin_family = AF_INET;
    if(service != NULL){
        ((struct sockaddr_in *)(p->ai_addr))->sin_port = change_order(port);
    }
    if(node != NULL){
        ((struct sockaddr_in *)(p->ai_addr))->sin_addr = addr;
    }
    else{
        ((struct sockaddr_in *)(p->ai_addr))->sin_addr.s_addr = INADDR_ANY;
    }
    *res = p;
    return 0;
}

/**
 * @brief Generate an unused port number using bitmap.
 * @return `BITMAP_ERROR` on error, port number on success.
 */
size_t
TransportLayer::generatePort()
{
    size_t idx = bitmap.bitmap_scan_and_flip(0, 1, false);
    return idx == BITMAP_ERROR ? BITMAP_ERROR : idx + PORT_BEGIN;
}

/**
 * @brief Send a TCP segment.
 * 
 * @param
 * @return `true` on success, `false` on error.
 */
bool 
TransportLayer::sendSegment(TCB *tcb, SegmentType type, 
                            const void *buf, int len)
{
    int rc;
    int total_len = SIZE_PSEUDO + SIZE_TCP + len;
    u_char *segment = new u_char[total_len];
    memcpy(segment + SIZE_PSEUDO + SIZE_TCP, buf, len);
    PseudoHeader *pseudo_header = (PseudoHeader *)segment;
    TCPHeader *tcp_header = (TCPHeader *)(segment + SIZE_PSEUDO);

    // Pseudo header
    pseudo_header->src_addr = tcb->src_addr;
    pseudo_header->dst_addr = tcb->dst_addr;
    pseudo_header->zero = 0;
    pseudo_header->protocol = IPPROTO_TCP;
    pseudo_header->length = SIZE_TCP + len;

    // Port numbers
    tcp_header->src_port = tcb->src_port;
    tcp_header->dst_port = tcb->dst_port;
    // Sequence number
    tcp_header->seq = change_order(tcb->getSequence());
    // Acknowledgement number
    if(type != SegmentType::SYN){
        tcp_header->ack = change_order(tcb->getAcknowledgement());
    }
    else{
        tcp_header->ack = 0;
    }
    // Data Offset
    tcp_header->data_off = DEFAULT_OFF;
    // Control Bits
    switch (type)
    {
    case SegmentType::SYN:
        tcp_header->ctl_bits = ControlBits::SYN;
        tcb->setSequence(tcp_header->seq + 1);
        break;
    
    case SegmentType::SYN_ACK:
        tcp_header->ctl_bits = ControlBits::SYN | ControlBits::ACK;
        tcb->setSequence(tcp_header->seq + 1);
        break;
    
    case SegmentType::ACK:
        tcp_header->ctl_bits = ControlBits::ACK;
        tcb->setSequence(tcp_header->seq + len);
        break;
    
    case SegmentType::FIN:
        tcp_header->ctl_bits = ControlBits::FIN;
        break;
    
    case SegmentType::FIN_ACK:
        tcp_header->ctl_bits = ControlBits::FIN | ControlBits::ACK;
        break;

    default:
        break;
    }
    // Window
    tcp_header->window = change_order(tcb->getWindow());
    // Checksum
    tcp_header->checksum = 0;
    // Urgent Pointer
    if(!(tcp_header->ctl_bits & ControlBits::URG)){
        tcp_header->urgent = 0;
    }
    tcp_header->checksum = calculate_checksum(segment, total_len);

    rc = network_layer->sendIPPacket(tcb->src_addr, tcb->dst_addr, IPPROTO_TCP, 
                                     segment + SIZE_PSEUDO, SIZE_TCP + len);
    if(rc == -1){
        std::cerr << "Send segment error!" << std::endl;
        delete[] segment;
        return false;
    }
    delete[] segment;
    return true;
}

/**
 * @brief Callback function on receiving a TCP segment.
 * 
 * @param buf Pointer to the TCP segment.
 * @param len Length of the TCP segment.
 * @param src_addr Source address of the IP datagram.
 * @param dst_addr Destination address of the IP datagram.
 * @return `true` on success, `false` on error.
 */
bool 
TransportLayer::callBack(const u_char *buf, int len, 
                         struct in_addr src_addr, struct in_addr dst_addr)
{
    int rest_len = len;
    TCPHeader *tcp_header = (TCPHeader *)buf;
    PseudoHeader *pseudo_header = (PseudoHeader *)(buf - SIZE_PSEUDO);
    TCB *tcb;
    unsigned int header_len, seq, ack;
    u_short window;

    // Port number

    // Calculate checksum
    pseudo_header->src_addr = dst_addr;
    pseudo_header->dst_addr = src_addr;
    pseudo_header->zero = 0;
    pseudo_header->protocol = IPPROTO_TCP;
    pseudo_header->length = len;
    u_short sum = calculate_checksum((const u_char *)pseudo_header,
                                     SIZE_PSEUDO + len);
    if(sum != 0){
        std::cerr << "TCP checksum error!" << std::endl;
        return false;
    }

    // Sequence number and acknowledgement number
    seq = change_order(tcp_header->seq);
    ack = change_order(tcp_header->ack);
    window = change_order(tcp_header->window);
    header_len = GET_OFF(tcp_header->data_off);
    rest_len -= header_len;

    // Control bits
    u_char urg, ack, psh, rst, syn, fin;
    SegmentType type;
    urg = tcp_header->ctl_bits & ControlBits::URG;
    ack = tcp_header->ctl_bits & ControlBits::ACK;
    psh = tcp_header->ctl_bits & ControlBits::PSH;
    rst = tcp_header->ctl_bits & ControlBits::RST;
    syn = tcp_header->ctl_bits & ControlBits::SYN;
    fin = tcp_header->ctl_bits & ControlBits::FIN;
    if(syn && !ack){
        type = SegmentType::SYN;
    }
    else if(syn && ack){
        type = SegmentType::SYN_ACK;
    }
    else if(fin && !ack){
        type = SegmentType::FIN;
    }
    else if(fin && ack){
        type = SegmentType::FIN_ACK;
    }
    else if(!syn && !fin && ack){
        type = SegmentType::ACK;
    }

    switch (type)
    {
    case SegmentType::SYN:
        for(auto &it: fd2tcb)
        {
            it.second->mutex.lock();
            if((it.second->socket_state == SocketState::PASSIVE) && 
               (it.second->state == ConnectionState::LISTEN) &&
               (it.second->src_addr.s_addr == dst_addr.s_addr) && 
               (it.second->src_port == tcp_header->dst_port))
            {
                if(it.second->pending.size() + it.second->received.size() 
                    < it.second->backlog)
                {
                    tcb = new TCB();
                    tcb->src_addr = dst_addr;
                    tcb->src_port = tcp_header->dst_port;
                    tcb->dst_addr = src_addr;
                    tcb->dst_port = tcp_header->src_port;
                    tcb->socket_state = SocketState::ACTIVE;
                    tcb->setAcknowledgement(seq + 1);
                    tcb->setDestWindow(window);
                    tcb->state = ConnectionState::SYN_RCVD;
                    sendSegment(tcb, SegmentType::SYN_ACK, NULL, 0);
                    it.second->received.insert(tcb);
                }
                it.second->mutex.unlock();
                break;
            }
            it.second->mutex.unlock();
        }
        break;
    
    case SegmentType::SYN_ACK:
        for(auto &it: fd2tcb)
        {
            it.second->mutex.lock();
            if((it.second->socket_state == SocketState::ACTIVE) && 
               (it.second->state == ConnectionState::SYN_SENT) &&
               (it.second->src_addr.s_addr == dst_addr.s_addr) && 
               (it.second->src_port == tcp_header->dst_port) &&
               (it.second->dst_addr.s_addr == src_addr.s_addr) &&
               (it.second->dst_port == tcp_header->src_port))
            {
                it.second->setAcknowledgement(seq + 1);
                it.second->setDestWindow(window);
                it.second->state = ConnectionState::ESTABLISHED;
                sendSegment(it.second, SegmentType::ACK, NULL, 0);
                it.second->mutex.unlock();
                break;
            }
            it.second->mutex.unlock();
        }
        break;
    
    case SegmentType::ACK:
        bool flag = false;
        for(auto &it: fd2tcb)
        {
            it.second->mutex.lock();
            switch (it.second->state)
            {
            case ConnectionState::LISTEN:
                if((it.second->socket_state == SocketState::PASSIVE) &&
                   (it.second->src_addr.s_addr == dst_addr.s_addr) &&
                   (it.second->src_port == tcp_header->dst_port))
                {
                    for(auto &i: it.second->received)
                    {
                        if((i->dst_addr.s_addr == src_addr.s_addr) &&
                           (i->dst_port == tcp_header->src_port))
                        {
                            i->setAcknowledgement(seq + 1);
                            i->setDestWindow(window);
                            i->state = ConnectionState::ESTABLISHED;
                            it.second->received.erase(i);
                            it.second->pending.push(i);
                            sem_post(&tcb->semaphore);
                            flag = true;
                            break;
                        }
                    }
                }
                break;

            case ConnectionState::ESTABLISHED:
                if((it.second->socket_state == SocketState::ACTIVE) && 
                   (it.second->src_addr.s_addr == dst_addr.s_addr) && 
                   (it.second->src_port == tcp_header->dst_port) &&
                   (it.second->dst_addr.s_addr == src_addr.s_addr) &&
                   (it.second->dst_port == tcp_header->src_port))
                {
                    flag = true;
                    if(seq != it.second->getAcknowledgement()){
                        break;
                    }
                    it.second->setAcknowledgement(seq + rest_len);
                    it.second->setDestWindow(window);
                    if(rest_len != 0){
                        it.second->writeWindow(buf + header_len, rest_len);
                    }
                    sendSegment(it.second, SegmentType::ACK, NULL, 0);
                }
                break;

            case ConnectionState::FIN_WAIT1:
                if((it.second->src_addr.s_addr == dst_addr.s_addr) && 
                   (it.second->src_port == tcp_header->dst_port) &&
                   (it.second->dst_addr.s_addr == src_addr.s_addr) &&
                   (it.second->dst_port == tcp_header->src_port))
                {
                    flag = true;
                    if(ack != it.second->getSequence()){
                        break;
                    }
                    it.second->state = ConnectionState::FIN_WAIT2;
                }
                break;
            
            case ConnectionState::LAST_ACK:
                if((it.second->src_addr.s_addr == dst_addr.s_addr) && 
                   (it.second->src_port == tcp_header->dst_port) &&
                   (it.second->dst_addr.s_addr == src_addr.s_addr) &&
                   (it.second->dst_port == tcp_header->src_port))
                {
                    flag = true;
                    if(ack != it.second->getSequence()){
                        break;
                    }
                    it.second->state = ConnectionState::CLOSED;
                    it.second->socket_state = SocketState::UNOPENED;
                }
                break;
            
            default:
                break;
            }

            it.second->mutex.unlock();
            if(flag){
                break;
            }
        }
        break;
    
    case SegmentType::FIN:
        break;
    
    case SegmentType::FIN_ACK:
        for(auto &it: fd2tcb)
        {
            it.second->mutex.lock();
            if((it.second->state == ConnectionState::ESTABLISHED) &&
               (it.second->src_addr.s_addr == dst_addr.s_addr) && 
               (it.second->src_port == tcp_header->dst_port) &&
               (it.second->dst_addr.s_addr == src_addr.s_addr) &&
               (it.second->dst_port == tcp_header->src_port))
            {
                it.second->setAcknowledgement(seq + 1);
                it.second->setDestWindow(window);
                it.second->state = ConnectionState::CLOSE_WAIT;
                sendSegment(it.second, SegmentType::ACK, NULL, 0);
                it.second->mutex.unlock();
                break;
            }
            else if((it.second->state == ConnectionState::FIN_WAIT2) &&
                    (it.second->src_addr.s_addr == dst_addr.s_addr) && 
                    (it.second->src_port == tcp_header->dst_port) &&
                    (it.second->dst_addr.s_addr == src_addr.s_addr) &&
                    (it.second->dst_port == tcp_header->src_port))
            {
                it.second->setAcknowledgement(seq + 1);
                it.second->setDestWindow(window);
                it.second->state = ConnectionState::TIMED_WAIT;
                sendSegment(it.second, SegmentType::ACK, NULL, 0);
                it.second->mutex.unlock();
                break;
            }
            it.second->mutex.unlock();
        }
        break;

    default:
        break;
    }
}