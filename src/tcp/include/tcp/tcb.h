/**
 * @file tcb.h
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

#include <tcp/segment.h>
#include <tcp/window.h>
#include <netinet/ip.h>
#include <semaphore.h>
#include <list>
#include <mutex>
#include <queue>
#include <set>

namespace ControlBits {
    enum ControlBits {
        URG = 0x20, // Urgent Pointer field significant. 
                    // Not implemented because it's rarely used.
        ACK = 0x10, // Acknowledgment field significant
        PSH = 0x08, // Push Function. 
                    // Ignored because my implementation doesn't buffer data.
        RST = 0x04, // Reset the connection.
        SYN = 0x02, // Synchronize sequence numbers
        FIN = 0x01, // No more data from sender
    };
}

namespace SocketState {
    enum SocketState{
        UNSPECIFIED, // Partially opened
        BOUND,       // Called bind() on it successfully
        ACTIVE,      // Socket for transmitting data
        PASSIVE,     // Socket for listening
    };
}

namespace ConnectionState {
    enum ConnectionState {
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
    };
}

/**
 * @brief Transmission Control Block
 */
class TCB
{
private:
    bool seq_init;
    unsigned int snd_una; // send unacknowledged
    unsigned int snd_nxt; // send next
    unsigned int rcv_nxt; // receive next
    Window window;
    u_short snd_wnd;   // send window
    int max_seg;
    int64_t getTimeMicro();
    int64_t getTimeMilli();
public:
    SocketState::SocketState socket_state;
    struct in_addr src_addr;
    u_short src_port;
    struct in_addr dst_addr;
    u_short dst_port;
    // For listening socket
    int backlog;
    std::list<TCB *> pending;
    std::set<TCB *> received;

    sem_t semaphore; // Used by both listening socket and connecting socket
    sem_t fin_sem;
    std::mutex bind_mutex;
    std::mutex conn_mutex;
    std::mutex pending_mutex;
    int accepting_cnt;
    int reading_cnt;
    int writing_cnt;
    bool closed;
    ConnectionState::ConnectionState state;

    // TODO: round-trip time (milliseconds) and variance 
    // Set RTT to 100ms now.
    int64_t srtt;
    int64_t rttvar;
    // List to retransmit
    std::list<RetransElem *> retrans_list;
    std::mutex retrans_mutex;

    TCB();
    ~TCB();
    unsigned int getSequence();
    void updateSequence(unsigned int delta);
    void setSndUna(unsigned int sequence);
    unsigned int getSndUna();
    unsigned int getAcknowledgement();
    void setAcknowledgement(unsigned int ack);
    u_short getWindow();
    void writeWindow(const u_char *buf, int len, bool push);
    bool readWindow(u_char *buf, int len, ssize_t *nread);
    void setDestWindow(u_short window);
    u_short getDestWindow();
    void setMaxSegSize(u_short size);
    int getMaxSegSize();
    void insertRetrans(u_char *segment, unsigned int seq, int len);
};
