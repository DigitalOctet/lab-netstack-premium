/**
 * @file socket_class.h
 * @brief Class that manages sockets.
 * @note It doesn't support Simultaneous connection now.
 * 
 *                              +---------+ ---------\      active OPEN
 *                              |  CLOSED |            \    -----------
 *                              +---------+<---------\   \   create TCB
 *                                |     ^              \   \  snd SYN
 *                   passive OPEN |     |   CLOSE        \   \
 *                   ------------ |     | ----------       \   \
 *                    create TCB  |     | delete TCB         \   \
 *                                V     |                      \   \
 *                              +---------+            CLOSE    |    \
 *                              |  LISTEN |          ---------- |     |
 *                              +---------+          delete TCB |     |
 *                   rcv SYN      |     |     SEND              |     |
 *                  -----------   |     |    -------            |     V
 * +---------+      snd SYN,ACK  /       \   snd SYN          +---------+
 * |         |<-----------------           ------------------>|         |
 * |   SYN   |                    rcv SYN                     |   SYN   |
 * |   RCVD  |<-----------------------------------------------|   SENT  |
 * |         |                    snd ACK                     |         |
 * |         |------------------           -------------------|         |
 * +---------+   rcv ACK of SYN  \       /  rcv SYN,ACK       +---------+
 *   |           --------------   |     |   -----------
 *   |                  x         |     |     snd ACK
 *   |                            V     V
 *   |  CLOSE                   +---------+
 *   | -------                  |  ESTAB  |
 *   | snd FIN                  +---------+
 *   |                   CLOSE    |     |    rcv FIN
 *   V                  -------   |     |    -------
 * +---------+          snd FIN  /       \   snd ACK          +---------+
 * |  FIN    |<-----------------           ------------------>|  CLOSE  |
 * | WAIT-1  |------------------                              |   WAIT  |
 * +---------+          rcv FIN  \                            +---------+
 *   | rcv ACK of FIN   -------   |                            CLOSE  |
 *   | --------------   snd ACK   |                           ------- |
 *   V        x                   V                           snd FIN V
 * +---------+                  +---------+                   +---------+
 * |FINWAIT-2|                  | CLOSING |                   | LAST-ACK|
 * +---------+                  +---------+                   +---------+
 *   |                rcv ACK of FIN |                 rcv ACK of FIN |
 *   |  rcv FIN       -------------- |    Timeout=2MSL -------------- |
 *   |  -------              x       V    ------------        x       V
 *    \ snd ACK                 +---------+delete TCB         +---------+
 *     ------------------------>|TIME WAIT|------------------>| CLOSED  |
 *                              +---------+                   +---------+
 *
 *                      TCP Connection State Diagram
 */

#pragma once

#include <tcp/window.h>
#include <netinet/ip.h>
#include <semaphore.h>
#include <mutex>
#include <queue>
#include <set>

typedef enum {
    URG = 0x20, /* Urgent Pointer field significant */
    ACK = 0x10, /* Acknowledgment field significant */
    PSH = 0x08, /* Push Function */
    RST = 0x04, /* Reset the connection */
    SYN = 0x02, /* Synchronize sequence numbers */
    FIN = 0x01, /* No more data from sender */
}ControlBits;


typedef enum {
    UNOPENED, /* Partially opened */
    BOUND,    /* Called bind() on it successfully */
    ACTIVE,   /* Socket for transmitting data */
    PASSIVE,  /* Socket for listening */
}SocketState;

typedef enum {
    CLOSED,
    SYN_SENT,
    ESTABLISHED,
    LISTEN,
    SYN_RCVD,
    FIN_WAIT1,
    FIN_WAIT2,
    CLOSING,
    TIMED_WAIT,
    CLOSE_WAIT,
    LAST_ACK,
}ConnectionState;

/**
 * @brief Transmission Control Block
 */
class TCB
{
private:
    bool seq_init;
    unsigned int seq;
    unsigned int ack;
    Window window;
    u_short dst_window;
public:
    SocketState socket_state;
    struct in_addr src_addr;
    u_short src_port;
    struct in_addr dst_addr;
    u_short dst_port;
    // For listening socket
    int backlog;
    std::queue<TCB *> pending;
    std::set<TCB *> received;

    sem_t semaphore; // Used by both listening socket and connecting socket
    std::mutex mutex;
    ConnectionState state;

    TCB();
    ~TCB() = default;
    unsigned int getSequence();
    void setSequence(unsigned int sequence);
    unsigned int getAcknowledgement();
    void setAcknowledgement(unsigned int ack);
    u_short getWindow();
    void writeWindow(const u_char *buf, int len);
    ssize_t readWindow(u_char *buf, int len);
    void setDestWindow(u_short window);
    u_short getDestWindow();
};