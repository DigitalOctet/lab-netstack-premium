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
TransportLayer::TransportLayer(): fd2tcb(), tcbs(), bitmap(PORT_END)
{
    default_fd = open("/dev/null", O_RDWR, 0);
    if(default_fd < 0){
        std::cerr << "Open \"dev/null\" error!" << std::endl;
        return;
    }

    network_layer = new NetworkLayer(this);
    std::this_thread::sleep_for(std::chrono::milliseconds(50000));
    // std::thread(&TransportLayer::updateRetrans, this).detach();
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
    static TransportLayer *instance = NULL;
    static std::mutex mutex;
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
    if((protocol != 0) && (protocol != IPPROTO_TCP)) {
        return __real_socket(domain, type, protocol);
    }

    int fd = dup(default_fd);
    TCB *tcb = new TCB();
    tcb_mutex.lock();
    fd2tcb[fd] = tcb;
    tcbs.insert(tcb);
    tcb_mutex.unlock();
    return fd;
}

int 
TransportLayer::_bind(int socket, const struct sockaddr *address, 
                      socklen_t address_len)
{
    tcb_mutex.lock();
    auto it = fd2tcb.find(socket);
    if(it == fd2tcb.end()){
        tcb_mutex.unlock();
        return __real_bind(socket, address, address_len);
    }
    // Invalid `address_len`
    if(address_len != sizeof(struct sockaddr_in)){
        errno = EINVAL;
        return -1;
    }

    // If the socket has already been bound to an address
    TCB *tcb = it->second;
    tcb->bind_mutex.lock();
    tcb_mutex.unlock();
    if(tcb->socket_state != SocketState::UNSPECIFIED){
        tcb->bind_mutex.unlock();
        errno = EINVAL;
        return -1;
    }

    const struct sockaddr_in *addr = (const struct sockaddr_in *)address;
    if(addr->sin_family != AF_INET){
        tcb->bind_mutex.unlock();
        errno = EAFNOSUPPORT; // Address family not supported by protocol
        return -1;
    }
    if(addr->sin_addr.s_addr != INADDR_ANY){
        if(!network_layer->findIP(addr->sin_addr)){
            tcb->bind_mutex.unlock();
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
    tcb->bind_mutex.unlock();
    return 0;
}

int 
TransportLayer::_listen(int socket, int backlog)
{
    tcb_mutex.lock();
    auto it = fd2tcb.find(socket);
    if(it == fd2tcb.end()){
        tcb_mutex.unlock();
        return __real_listen(socket, backlog);
    }

    size_t port;
    TCB *tcb = it->second;

    tcb->bind_mutex.lock();
    tcb_mutex.unlock();
    if(tcb->socket_state == SocketState::ACTIVE){
        tcb->bind_mutex.unlock();
        errno = EINVAL;
        return -1;
    }
    else if(tcb->socket_state == SocketState::PASSIVE){
        // `listen()` doesn't fail if it has already been called on the same 
        // socket. I don't know its behavior in this case. Here I simply 
        // do nothing.
        tcb->bind_mutex.unlock();
        return 0;
    }
    else if(tcb->socket_state == SocketState::UNSPECIFIED){
        // If socket hasn't been previously bound to an address, bind it to the 
        // default IP address and an ephemeral port number.
        port = generatePort();
        if(port == BITMAP_ERROR){
            tcb->bind_mutex.unlock();
            errno = EADDRINUSE;
            return -1;
        }
        tcb->src_port = change_order((u_short)port);
        tcb->src_addr = network_layer->getIP();
        tcb->socket_state = SocketState::BOUND;
    }
    else if(tcb->socket_state == SocketState::BOUND){
        if(bitmap.bitmap_test(change_order(tcb->src_port))){
            tcb->bind_mutex.unlock();
            errno = EADDRINUSE;
            return -1;
        }
        else{
            bitmap.bitmap_mark(change_order(tcb->src_port));
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
    tcb->bind_mutex.unlock();
    return 0;
}

int 
TransportLayer::_connect(int socket, const struct sockaddr *address, 
                         socklen_t address_len)
{
    tcb_mutex.lock();
    auto it = fd2tcb.find(socket);
    if(it == fd2tcb.end()){
        tcb_mutex.unlock();
        return __real_connect(socket, address, address_len);
    }

    // Invalid `address_len`
    if(address_len != sizeof(struct sockaddr_in)){
        tcb_mutex.unlock();
        errno = EINVAL;
        return -1;
    }

    TCB *tcb = it->second;

    // Bind socket to source and destination addresses.
    tcb->bind_mutex.lock();
    tcb_mutex.unlock();
    if(tcb->socket_state == SocketState::UNSPECIFIED){
        size_t port = generatePort();
        if (port == BITMAP_ERROR) { // Cannot assign a port
            tcb->bind_mutex.unlock();
            errno = EADDRNOTAVAIL;
            return -1;
        }
        tcb->src_addr = network_layer->getIP();
        tcb->src_port = change_order((u_short)port);
        bitmap.bitmap_mark(change_order(tcb->src_port));
        tcb->socket_state = SocketState::BOUND;
    }
    else if(tcb->socket_state == SocketState::BOUND){
        bitmap.bitmap_mark(change_order(tcb->src_port));
    }
    else if((tcb->socket_state == SocketState::ACTIVE) || 
            (tcb->socket_state == SocketState::PASSIVE))
    {
        tcb->bind_mutex.unlock();
        errno = EISCONN;
        return -1;
    }
    tcb->socket_state = SocketState::ACTIVE;
    tcb->conn_mutex.lock();
    tcb->bind_mutex.unlock();
    struct sockaddr_in *address_in = (struct sockaddr_in *)address;
    tcb->dst_addr = address_in->sin_addr;
    tcb->dst_port = address_in->sin_port;

    // Send SYN
    if(!sendSegment(tcb, SegmentType::SYN, NULL, 0)){
        tcb->conn_mutex.unlock();
        errno = ECONNREFUSED;
        return -1;
    }
    tcb->state = ConnectionState::SYN_SENT;
    tcb->conn_mutex.unlock();
    sem_wait(&tcb->semaphore);
    if(tcb->state == ConnectionState::CLOSED){
        bitmap.bitmap_reset(change_order(tcb->src_port));
        delete tcb;
        errno = EBADF;
        return -1;
    }
    return 0;
}

int 
TransportLayer::_accept(int socket, struct sockaddr *address, 
                        socklen_t *address_len)
{
    tcb_mutex.lock();
    auto it = fd2tcb.find(socket);
    if(it == fd2tcb.end()){
        tcb_mutex.unlock();
        return __real_accept(socket, address, address_len);
    }

    TCB *conn_tcb;
    TCB *listen_tcb = it->second;
    listen_tcb->bind_mutex.lock();
    tcb_mutex.unlock();
    if(listen_tcb->socket_state != SocketState::PASSIVE){
        listen_tcb->bind_mutex.unlock();
        errno = EINVAL;
        return -1;
    }
    listen_tcb->accepting_cnt++;
    listen_tcb->bind_mutex.unlock();
    sem_wait(&listen_tcb->semaphore); // Wait for a ready connection
    listen_tcb->bind_mutex.lock();
    listen_tcb->accepting_cnt--;
    if(listen_tcb->state == ConnectionState::CLOSED){
        if(listen_tcb->accepting_cnt == 0){
            bitmap.bitmap_delete(change_order(listen_tcb->src_port));
            delete listen_tcb;
        }
        listen_tcb->bind_mutex.unlock();
        errno = EINVAL;
        return -1;
    }
    listen_tcb->bind_mutex.unlock();

    listen_tcb->pending_mutex.lock();
    if(listen_tcb->pending.empty()){
        listen_tcb->pending_mutex.unlock();
        errno = EINVAL;
        return -1;
    }
    conn_tcb = listen_tcb->pending.front();
    listen_tcb->pending.pop_front();
    bitmap.bitmap_add(change_order(conn_tcb->src_port));
    listen_tcb->pending_mutex.unlock();

    // Create file description and bind it to the connected tcb.
    int fd = dup(default_fd);
    tcb_mutex.lock();
    fd2tcb[fd] = conn_tcb;
    tcbs.insert(conn_tcb);
    tcb_mutex.unlock();

    // Return address
    if(address != NULL){
        struct sockaddr_in client_addr;
        memset(&client_addr, 0, sizeof(client_addr));
        client_addr.sin_family = AF_INET;
        client_addr.sin_addr = conn_tcb->dst_addr;
        client_addr.sin_port = conn_tcb->dst_port;
        if(*address_len > sizeof(client_addr)){
            *address_len = sizeof(client_addr);
        }
        memcpy(address, &client_addr, *address_len);
    }

    return fd;
}

ssize_t 
TransportLayer::_read(int fildes, void *buf, size_t nbyte)
{
    tcb_mutex.lock();
    auto it = fd2tcb.find(fildes);
    if(it == fd2tcb.end()){
        tcb_mutex.unlock();
        return __real_read(fildes, buf, nbyte);
    }

    TCB *tcb = it->second;
    ssize_t nread = 0, n;
    tcb->conn_mutex.lock();
    tcb_mutex.unlock();
    if((tcb->state != ConnectionState::ESTABLISHED) && 
       (tcb->state != ConnectionState::CLOSE_WAIT))
    {
        tcb->conn_mutex.unlock();
        errno = ENOTCONN;
        return -1;
    }
    tcb->reading_cnt++;
    tcb->conn_mutex.unlock();

    // Busy waiting can happen here
    u_char *bufp = (u_char *)buf;
    while(nbyte > 0){
        n = tcb->readWindow(bufp, nbyte);
        if(n > 0){
            nbyte -= n;
            nread += n;
            bufp += n;
            tcb->conn_mutex.lock();
            sendSegment(tcb, SegmentType::ACK, NULL, 0);
            tcb->conn_mutex.unlock();
        }
        if(tcb->state == ConnectionState::CLOSE_WAIT){
            break;
        }
    }

    tcb->conn_mutex.lock();
    tcb->reading_cnt--;
    if(tcb->closed){
        if((tcb->reading_cnt == 0) && (tcb->writing_cnt == 0)){
            tcb->state = ConnectionState::FIN_WAIT1;
            sendSegment(tcb, SegmentType::FIN_ACK, NULL, 0);
            tcb->conn_mutex.unlock();
        }
        else{
            tcb->conn_mutex.unlock();
        }
    }
    else{
        tcb->conn_mutex.unlock();
    }

    return nread;
}

/**
 * @note It won't block if the window size of the other end is 0.
 */
ssize_t 
TransportLayer::_write(int fildes, const void *buf, size_t nbyte)
{
    size_t nwrite = 0;
    tcb_mutex.lock();
    auto it = fd2tcb.find(fildes);
    if(it == fd2tcb.end()){
        tcb_mutex.unlock();
        return __real_write(fildes, buf, nbyte);
    }

    TCB *tcb = it->second;
    u_short dest_window;
    tcb->conn_mutex.lock();
    tcb_mutex.unlock();
    if((tcb->state == ConnectionState::FIN_WAIT1) || 
       (tcb->state == ConnectionState::FIN_WAIT2))
    {
        tcb->conn_mutex.unlock();
        // return EOF
        return 0;
    }
    if(tcb->state != ConnectionState::ESTABLISHED){
        tcb->conn_mutex.unlock();
        // NOTE: The behavior of writing to a listening socket can be 
        // different on Linux machines.
        errno = EPIPE;
        return -1;
    }
    tcb->writing_cnt++;
    tcb->conn_mutex.unlock();

    // Busy waiting can happen here
    const u_char *bufp = (const u_char *)buf;
    while(nbyte > 0){
        dest_window = tcb->getDestWindow();
        if(dest_window >= nbyte){
            tcb->conn_mutex.lock();
            sendSegment(tcb, SegmentType::ACK, bufp, nbyte);
            tcb->conn_mutex.unlock();
            nwrite += nbyte;
            bufp += nbyte;
            nbyte = 0;
            tcb->setDestWindow(dest_window - nbyte);
        }
        else if(dest_window > 0){
            tcb->conn_mutex.lock();
            sendSegment(tcb, SegmentType::ACK, bufp, dest_window);
            tcb->conn_mutex.unlock();
            nwrite += dest_window;
            bufp += dest_window;
            nbyte -= dest_window;
            tcb->setDestWindow(0);
        }
        if((tcb->state != ConnectionState::ESTABLISHED) && 
           (tcb->state != ConnectionState::CLOSE_WAIT))
        {
            break;
        }
    }

    tcb->conn_mutex.lock();
    tcb->writing_cnt--;
    if(tcb->closed){
        if((tcb->reading_cnt == 0) && (tcb->writing_cnt == 0)){
            tcb->state = ConnectionState::FIN_WAIT1;
            sendSegment(tcb, SegmentType::FIN_ACK, NULL, 0);
            tcb->conn_mutex.unlock();
        }
        else{
            tcb->conn_mutex.unlock();
        }
    }
    else{
        tcb->conn_mutex.unlock();
    }
    
    return nwrite;
}

/**
 * @see https://man7.org/linux/man-pages/man2/close.2.html
 * 
 *  Multithreaded processes and close()
 *     It is probably unwise to close file descriptors while they may be
 *     in use by system calls in other threads in the same process.
 *     Since a file descriptor may be reused, there are some obscure
 *     race conditions that may cause unintended side effects.
 *
 *     Furthermore, consider the following scenario where two threads
 *     are performing operations on the same file descriptor:
 *
 *     (1)  One thread is blocked in an I/O system call on the file
 *          descriptor.  For example, it is trying to write(2) to a pipe
 *          that is already full, or trying to read(2) from a stream
 *          socket which currently has no available data.
 *
 *     (2)  Another thread closes the file descriptor.
 *
 *     The behavior in this situation varies across systems.  On some
 *     systems, when the file descriptor is closed, the blocking system
 *     call returns immediately with an error.
 *
 *     On Linux (and possibly some other systems), the behavior is
 *     different: the blocking I/O system call holds a reference to the
 *     underlying open file description, and this reference keeps the
 *     description open until the I/O system call completes.  (See
 *     open(2) for a discussion of open file descriptions.)  Thus, the
 *     blocking system call in the first thread may successfully
 *     complete after the close() in the second thread.
 * 
 * My implementation follow the behavior of the Linux machines.
 */
int 
TransportLayer::_close(int fildes)
{
    tcb_mutex.lock();
    auto it = fd2tcb.find(fildes);
    if(it == fd2tcb.end()){
        tcb_mutex.unlock();
        return __real_close(fildes);
    }

    TCB *tcb = it->second;
    tcb->bind_mutex.lock();
    if(tcb->socket_state == SocketState::UNSPECIFIED){
        // Put erasure before deletion to prevent other functions from finding 
        // this tcb.
        __real_close(fildes);
        fd2tcb.erase(fildes);
        tcbs.erase(tcb);
        tcb_mutex.unlock();
        // Wait for potential `bind()` to finish to avoid segmentation fault
        tcb->bind_mutex.unlock();
        delete tcb;
        return 0;
    }

    if(tcb->socket_state == SocketState::BOUND){
        __real_close(fildes);
        fd2tcb.erase(fildes);
        tcbs.erase(tcb);
        tcb_mutex.unlock();
        tcb->bind_mutex.unlock();
        delete tcb;
        return 0;
    }

    if(tcb->socket_state == SocketState::PASSIVE){
        __real_close(fildes);
        fd2tcb.erase(fildes);
        tcbs.erase(tcb);
        tcb_mutex.unlock();
        tcb->state = ConnectionState::CLOSED;
        if(tcb->accepting_cnt == 0){
            bitmap.bitmap_delete(change_order(tcb->src_port));
            delete tcb;
        }
        else{
            for(int i = 0; i < tcb->accepting_cnt; i++){
                sem_post(&tcb->semaphore);
            }
        }
        tcb->bind_mutex.unlock();
        return 0;
    }
    
    if(tcb->socket_state == SocketState::ACTIVE){
        __real_close(fildes);
        fd2tcb.erase(fildes);
        tcb->conn_mutex.lock();
        tcb_mutex.unlock();
        tcb->bind_mutex.unlock();
        if(tcb->state == ConnectionState::SYN_SENT){
            tcb->state = ConnectionState::CLOSED;
            tcb->conn_mutex.unlock();
            sem_post(&tcb->semaphore);
            return 0;
        }
        if(tcb->state == ConnectionState::ESTABLISHED){
            tcb->closed = true;
            if((tcb->reading_cnt == 0) && (tcb->writing_cnt == 0)){
                tcb->state = ConnectionState::FIN_WAIT1;
                sendSegment(tcb, SegmentType::FIN_ACK, NULL, 0);
                tcb->conn_mutex.unlock();
                sem_wait(&tcb->fin_sem);
            }
            else{
                tcb->conn_mutex.unlock();
            }
            return 0;
        }
        if(tcb->state == ConnectionState::CLOSE_WAIT){
            tcb->closed = true;
            if((tcb->reading_cnt == 0) && (tcb->writing_cnt == 0)){
                tcb->state = ConnectionState::LAST_ACK;
                sendSegment(tcb, SegmentType::FIN_ACK, NULL, 0);
                tcb->conn_mutex.unlock();
                sem_wait(&tcb->fin_sem);
            }
            else{
                tcb->conn_mutex.unlock();
            }
            return 0;
        }
    }
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

    struct addrinfo *p = (struct addrinfo *)malloc(sizeof(struct addrinfo));
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
    p->ai_addr = (struct sockaddr *)malloc(sizeof(struct sockaddr_in));
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
    size_t idx = bitmap.bitmap_scan_and_flip(PORT_BEGIN, 1, false);
    return idx == BITMAP_ERROR ? BITMAP_ERROR : idx;
}

/**
 * @brief Send a TCP segment.
 * 
 * @param
 * @return `true` on success, `false` on error.
 */
bool 
TransportLayer::sendSegment(TCB *tcb, SegmentType::SegmentType type, 
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
    pseudo_header->length = change_order((u_short)(SIZE_TCP + len));

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
        tcb->updateSequence(1);
        break;
    
    case SegmentType::SYN_ACK:
        tcp_header->ctl_bits = ControlBits::SYN | ControlBits::ACK;
        tcb->updateSequence(1);
        break;
    
    case SegmentType::ACK:
        tcp_header->ctl_bits = ControlBits::ACK;
        tcb->updateSequence(len);
        break;
    
    case SegmentType::FIN:
        tcp_header->ctl_bits = ControlBits::FIN;
        tcb->updateSequence(1);
        break;
    
    case SegmentType::FIN_ACK:
        tcp_header->ctl_bits = ControlBits::FIN | ControlBits::ACK;
        tcb->updateSequence(1);
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
    tcb->insertRetrans(segment, change_order(tcp_header->seq), SIZE_TCP + len);
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
    unsigned int header_len, seq, ack_num;
    u_short window, max_seg;
    bool has_max_seg = false;

    // Calculate checksum
    pseudo_header->src_addr = src_addr;
    pseudo_header->dst_addr = dst_addr;
    pseudo_header->zero = 0;
    pseudo_header->protocol = IPPROTO_TCP;
    pseudo_header->length = change_order((u_short)len);
    u_short sum = calculate_checksum((const u_char *)pseudo_header,
                                     SIZE_PSEUDO + len);
    if(sum != 0){
        std::cerr << "TCP checksum error!" << std::endl;
        return false;
    }

    // Sequence number and acknowledgement number
    seq = change_order(tcp_header->seq);
    ack_num = change_order(tcp_header->ack);
    window = change_order(tcp_header->window);
    header_len = GET_OFF(tcp_header->data_off);
    rest_len -= header_len;

    // Find maximum segment size option
    for(int i = SIZE_TCP; i < header_len; i++){
        if(buf[i] == OptionType::MAX_SEG_SIZE){
            max_seg = change_order(*(u_short *)(buf + 2));
            has_max_seg = true;
            break;
        }
    }

    // Control bits
    u_char urg, ack, psh, rst, syn, fin;
    SegmentType::SegmentType type;
    urg = tcp_header->ctl_bits & ControlBits::URG; // Not implemented
    ack = tcp_header->ctl_bits & ControlBits::ACK;
    psh = tcp_header->ctl_bits & ControlBits::PSH;
    rst = tcp_header->ctl_bits & ControlBits::RST;
    syn = tcp_header->ctl_bits & ControlBits::SYN;
    fin = tcp_header->ctl_bits & ControlBits::FIN;

    if(rst){
        type = SegmentType::RST;
    }
    else if(syn && !ack){
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

    bool flag = false;
    switch (type)
    {
    case SegmentType::RST:
        tcb_mutex.lock();
        for(auto it: tcbs){
            if((it->state == ConnectionState::LISTEN) &&
               (it->src_addr.s_addr == dst_addr.s_addr) && 
               (it->src_port == tcp_header->dst_port))
            {
                for(auto i: it->received){
                    if((i->dst_addr.s_addr == src_addr.s_addr) &&
                       (i->dst_port == tcp_header->src_port))
                    {
                        flag = true;
                        it->received.erase(i);
                        delete i;
                        break;
                    }
                }
                break;
            }
            else if((it->state != ConnectionState::CLOSED) &&
                    (it->src_addr.s_addr == dst_addr.s_addr) &&
                    (it->src_port == tcp_header->dst_port) &&
                    (it->dst_addr.s_addr == src_addr.s_addr) &&
                    (it->dst_port == tcp_header->src_port))
            {

            }
            if(flag){
                break;
            }
        }
        tcb_mutex.unlock();
        break;

    case SegmentType::SYN:
        tcb_mutex.lock();
        for(auto it: tcbs)
        {
            if((it->state == ConnectionState::LISTEN) &&
               (it->src_addr.s_addr == dst_addr.s_addr) && 
               (it->src_port == tcp_header->dst_port))
            {
                if(it->pending.size() + it->received.size() < it->backlog)
                {
                    TCB *tcb = new TCB();
                    tcb->src_addr = dst_addr;
                    tcb->src_port = tcp_header->dst_port;
                    tcb->dst_addr = src_addr;
                    tcb->dst_port = tcp_header->src_port;
                    tcb->socket_state = SocketState::ACTIVE;
                    tcb->state = ConnectionState::SYN_RCVD;
                    tcb->setAcknowledgement(seq + 1);
                    tcb->setDestWindow(window);
                    if(has_max_seg) it->setMaxSegSize(max_seg);
                    it->received.insert(tcb);
                    sendSegment(tcb, SegmentType::SYN_ACK, NULL, 0);
                }
                break;
            }
        }
        tcb_mutex.unlock();
        break;
    
    case SegmentType::SYN_ACK:
        tcb_mutex.lock();
        for(auto it: tcbs)
        {
            it->conn_mutex.lock();
            if(it->state == ConnectionState::SYN_SENT){
                if((it->src_addr.s_addr == dst_addr.s_addr) && 
                   (it->src_port == tcp_header->dst_port) &&
                   (it->dst_addr.s_addr == src_addr.s_addr) &&
                   (it->dst_port == tcp_header->src_port))
                {
                    it->setAcknowledgement(seq + 1);
                    it->setDestWindow(window);
                    it->setSndUna(ack_num);
                    if(has_max_seg) it->setMaxSegSize(max_seg);
                    it->state = ConnectionState::ESTABLISHED;
                    sendSegment(it, SegmentType::ACK, NULL, 0);
                    it->conn_mutex.unlock();
                    sem_post(&it->semaphore);
                    break;
                }
            }
            it->conn_mutex.unlock();
        }
        tcb_mutex.unlock();
        break;
    
    case SegmentType::ACK:
        tcb_mutex.lock();
        for(auto it: tcbs)
        {
            it->bind_mutex.lock();
            it->conn_mutex.lock();
            switch (it->state)
            {
            case ConnectionState::LISTEN:
                it->conn_mutex.unlock();
                if((it->socket_state == SocketState::PASSIVE) &&
                   (it->src_addr.s_addr == dst_addr.s_addr) &&
                   (it->src_port == tcp_header->dst_port))
                {
                    for(auto i: it->received)
                    {
                        if((i->dst_addr.s_addr == src_addr.s_addr) &&
                           (i->dst_port == tcp_header->src_port))
                        {
                            i->setAcknowledgement(seq + rest_len);
                            i->setDestWindow(window);
                            i->state = ConnectionState::ESTABLISHED;
                            it->received.erase(i);
                            it->pending.push_back(i);
                            sem_post(&it->semaphore);
                            flag = true;
                            break;
                        }
                    }
                    for(auto i: it->pending)
                    {
                        if((i->dst_addr.s_addr == src_addr.s_addr) &&
                           (i->dst_port == tcp_header->src_port))
                        {
                            flag = true;
                            if(seq != i->getAcknowledgement()){
                                break;
                            }
                            int size = i->getMaxSegSize();
                            if((size != -1) && (size < len)){
                                break;
                            }
                            i->setAcknowledgement(seq + rest_len);
                            i->setDestWindow(window);
                            i->setSndUna(ack_num);
                            if(rest_len != 0){
                                i->writeWindow(buf + header_len, rest_len);
                            }
                        }
                    }
                }
                break;

            case ConnectionState::ESTABLISHED:
                if((it->src_addr.s_addr == dst_addr.s_addr) && 
                   (it->src_port == tcp_header->dst_port) &&
                   (it->dst_addr.s_addr == src_addr.s_addr) &&
                   (it->dst_port == tcp_header->src_port))
                {
                    flag = true;
                    if(seq != it->getAcknowledgement()){
                        it->conn_mutex.unlock();
                        break;
                    }
                    int size = it->getMaxSegSize();
                    if((size != -1) && (size < len)){
                        it->conn_mutex.unlock();
                        break;
                    }
                    it->setAcknowledgement(seq + rest_len);
                    it->setDestWindow(window);
                    it->setSndUna(ack_num);
                    if(rest_len != 0){
                        it->writeWindow(buf + header_len, rest_len);
                    }
                    it->conn_mutex.unlock();
                }
                else{
                    it->conn_mutex.unlock();
                }
                break;

            case ConnectionState::FIN_WAIT1:
                if((it->src_addr.s_addr == dst_addr.s_addr) && 
                   (it->src_port == tcp_header->dst_port) &&
                   (it->dst_addr.s_addr == src_addr.s_addr) &&
                   (it->dst_port == tcp_header->src_port))
                {
                    flag = true;
                    if(seq != it->getAcknowledgement()){
                        it->conn_mutex.unlock();
                        break;
                    }
                    it->setAcknowledgement(seq + rest_len);
                    it->setDestWindow(window);
                    if(rest_len != 0){
                        it->writeWindow(buf + header_len, rest_len);
                        sendSegment(it, SegmentType::ACK, NULL, 0);
                    }
                    if(ack_num == it->getSequence()){
                        it->state = ConnectionState::FIN_WAIT2;
                    }
                    it->setSndUna(ack_num);
                    it->conn_mutex.unlock();
                }
                else{
                    it->conn_mutex.unlock();
                }
                break;
            
            case ConnectionState::LAST_ACK:
                if((it->src_addr.s_addr == dst_addr.s_addr) && 
                   (it->src_port == tcp_header->dst_port) &&
                   (it->dst_addr.s_addr == src_addr.s_addr) &&
                   (it->dst_port == tcp_header->src_port))
                {
                    flag = true;
                    if(ack_num == it->getSequence()){
                        it->state = ConnectionState::CLOSED;
                        it->conn_mutex.unlock();
                        tcbs.erase(it);
                        bitmap.bitmap_delete(change_order(it->src_port));
                        sem_post(&it->fin_sem);
                        delete it;
                        break;
                    }
                }
                it->conn_mutex.unlock();
                break;
            
            default:
                it->conn_mutex.unlock();
                break;
            }
            it->bind_mutex.unlock();

            if(flag){
                break;
            }
        }
        tcb_mutex.unlock();
        break;
    
    case SegmentType::FIN:
        break;
    
    case SegmentType::FIN_ACK:
        tcb_mutex.lock();
        for(auto it: tcbs)
        {
            it->conn_mutex.lock();
            if((it->state == ConnectionState::ESTABLISHED) &&
               (it->src_addr.s_addr == dst_addr.s_addr) && 
               (it->src_port == tcp_header->dst_port) &&
               (it->dst_addr.s_addr == src_addr.s_addr) &&
               (it->dst_port == tcp_header->src_port))
            {
                if(seq != it->getAcknowledgement()){
                    it->conn_mutex.unlock();
                    break;
                }
                it->setAcknowledgement(seq + 1);
                it->setDestWindow(window);
                it->state = ConnectionState::CLOSE_WAIT;
                it->conn_mutex.unlock();
                sendSegment(it, SegmentType::ACK, NULL, 0);
                break;
            }
            else if(((it->state == ConnectionState::FIN_WAIT1) || 
                     (it->state == ConnectionState::FIN_WAIT2)) &&
                    (it->src_addr.s_addr == dst_addr.s_addr) && 
                    (it->src_port == tcp_header->dst_port) &&
                    (it->dst_addr.s_addr == src_addr.s_addr) &&
                    (it->dst_port == tcp_header->src_port))
            {
                if(seq != it->getAcknowledgement()){
                    it->conn_mutex.unlock();
                    break;
                }
                it->setAcknowledgement(seq + 1);
                it->setDestWindow(window);
                it->state = ConnectionState::TIMED_WAIT;
                sendSegment(it, SegmentType::ACK, NULL, 0);
                it->conn_mutex.unlock();
                std::thread(&TransportLayer::timedWait, this, it).detach();
                break;
            }
            else if((it->state == ConnectionState::LISTEN) &&
                    (it->src_addr.s_addr == dst_addr.s_addr) &&
                    (it->src_port == tcp_header->dst_port))
            {
                for(auto i: it->pending){
                    if((i->dst_addr.s_addr == src_addr.s_addr) &&
                       (i->dst_port == tcp_header->src_port))
                    {
                        flag = true;
                        if(seq != i->getAcknowledgement()){
                            i->conn_mutex.unlock();
                            break;
                        }
                        i->setAcknowledgement(seq + 1);
                        i->setDestWindow(window);
                        i->state = ConnectionState::CLOSE_WAIT;
                        sendSegment(i, SegmentType::ACK, NULL, 0);
                        it->conn_mutex.unlock();
                        break;
                    }
                }
            }
            else{
                it->conn_mutex.unlock();
            }
            if(flag){
                break;
            }
        }
        tcb_mutex.unlock();
        break;

    default:
        break;
    }
    return true;
}

/**
 * @brief Let TCB sleep for a while and clean up its resources.
 */
void 
TransportLayer::timedWait(TCB *tcb)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(2 * tcb->srtt));
    tcb_mutex.lock();
    tcbs.erase(tcb);
    tcb_mutex.unlock();
    bitmap.bitmap_delete(change_order(tcb->src_port));
    sem_post(&tcb->fin_sem);
    delete tcb;
    return;
}

/**
 * @brief Retransmission
 */
void 
TransportLayer::updateRetrans()
{
    while(true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        tcb_mutex.lock();
        for(auto tcb: tcbs){
            tcb->retrans_mutex.lock();
            for(auto it = tcb->retrans_list.begin(); 
                it != tcb->retrans_list.end(); ) 
            {
                RetransElem *e = *it;
                if(e->seq < tcb->getSndUna()){
                    delete e;
                    it = tcb->retrans_list.erase(it);
                }
                else{
                    e->time++;
                    if(e->time == RETRANS_TIME){
                        e->time = 0;
                        network_layer->sendIPPacket(tcb->src_addr, 
                                                    tcb->dst_addr, IPPROTO_TCP, 
                                                    e->segment + SIZE_PSEUDO, 
                                                    e->len);
                    }
                    it++;
                }
            }
            tcb->retrans_mutex.unlock();
        }
        tcb_mutex.unlock();
    }
    return;
}