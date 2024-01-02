/**
 * @file tcb.cpp
 */

#include <tcp/tcb.h>
#include <string.h>
#include <chrono>

TCB::TCB(): 
    seq_init(false), window(), pending(), accepting_cnt(0), max_seg(-1),
    reading_cnt(0), writing_cnt(0), closed(false), srtt(100), rttvar(0),
    socket_state(SocketState::UNSPECIFIED), state(ConnectionState::CLOSED)
{
    sem_init(&semaphore, 0, 0);
    sem_init(&fin_sem, 0, 0);
}

/**
 * @brief Clear all elements in retransmit queue, pending queue and received 
 * set.
 */
TCB::~TCB()
{
    retrans_mutex.lock();
    for(auto e: retrans_list){
        delete e;
    }
    retrans_mutex.unlock();

    for(auto it: received){
        delete it;
    }
    pending_mutex.lock();
    while(!pending.empty()){
        auto it = pending.front();
        delete it;
        pending.pop_front();
    }
    pending_mutex.unlock();
}

/**
 * @brief Obtain time in microseconds.
 */
int64_t 
TCB::getTimeMicro()
{
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto duration = currentTime.time_since_epoch();
    auto microseconds = \
        std::chrono::duration_cast<std::chrono::microseconds>(duration);
    return microseconds.count();
}

/**
 * @brief Obtain time in milliseconds.
 */
int64_t 
TCB::getTimeMilli()
{
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto duration = currentTime.time_since_epoch();
    auto milliseconds = \
        std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    return milliseconds.count();
}

/**
 * @brief Get the current sequence number.
 */
unsigned int 
TCB::getSequence()
{
    if(seq_init){
        return snd_nxt;
    }

    seq_init = true;
    auto microseconds = getTimeMicro();
    snd_nxt = snd_una = (microseconds >> 2) & 0xffffffffu;
    return snd_nxt;
}

/**
 * @brief Add DELTA to the next sequence number to send, i.e., snd_nxt.
 */
void 
TCB::updateSequence(unsigned int delta)
{
    snd_nxt += delta;
}

/**
 * @brief Set the first unacknowledged number.
 */
void 
TCB::setSndUna(unsigned int sequence)
{
    snd_una = sequence;
}

unsigned int 
TCB::getSndUna()
{
    return snd_una;
}

/**
 * @brief Get acknowledgement number, the next sequence number to receive.
 */
unsigned int
TCB::getAcknowledgement()
{
    return rcv_nxt;
}

/**
 * @brief Set acknowledgement number
 */
void 
TCB::setAcknowledgement(unsigned int acknowledgement)
{
    rcv_nxt = acknowledgement;
}

/**
 * @brief Get window size.
 */
u_short
TCB::getWindow()
{
    return window.size > UINT16_MAX ? UINT16_MAX : window.size;
}

/**
 * @brief Write LEN bytes to window from BUF.
 */
void 
TCB::writeWindow(const u_char *buf, int len, bool push)
{
    window.mutex.lock();
    window.push = push || window.push;
    int rest = window.n - window.rear;
    if(rest >= len){
        memcpy(window.buf + window.rear, buf, len);
    }
    else{
        memcpy(window.buf + window.rear, buf, rest);
        memcpy(window.buf, buf + rest, len - rest);
    }
    window.rear = (window.rear + len) % window.n;
    window.size -= len;
    window.mutex.unlock();
}

/**
 * @brief Read LEN bytes from window to BUF.
 * 
 * @param buf buffer to read to
 * @param len maximum length to read
 * @param nread pointer to number of bytes that have been read.
 * @return Whether PUSH is on. If PUSH is on, `read` will return immediately. 
 * Otherwise, it will suspend until LEN bytes have been read or PUSH becomes on.
 */
bool 
TCB::readWindow(u_char *buf, int len, ssize_t *nread)
{
    window.mutex.lock();
    len = (len > window.n - window.size) ? (window.n - window.size) : len;
    if(len != 0){
        int rest = window.n - window.front;
        if(rest >= len){
            memcpy(buf, window.buf + window.front, len);
        }
        else{
            memcpy(buf, window.buf + window.front, rest);
            memcpy(buf + rest, window.buf, len - rest);
        }
        window.front = (window.front + len) % window.n;
        window.size += len;
    }
    *nread = len;
    if(window.push){
        if(window.size == window.n){
            window.push = false;
        }
        window.mutex.unlock();
        return true;
    }
    window.mutex.unlock();
    return false;
}

/**
 * @brief Set window size of the other end.
 */
void 
TCB::setDestWindow(u_short window)
{
    snd_wnd = window;
}

/**
 * @brief Get window size of the other end.
 */
u_short 
TCB::getDestWindow()
{
    return snd_wnd;
}

void 
TCB::setMaxSegSize(u_short size)
{
    max_seg = size;
}

int 
TCB::getMaxSegSize()
{
    return max_seg;
}

/**
 * @brief After successfully sending a segment, insert the segment to the 
 * retransmit list waiting for ack or timeout.
 */
void 
TCB::insertRetrans(u_char *segment, unsigned int seq, int len)
{
    RetransElem *e = new RetransElem(segment, seq, len);
    retrans_mutex.lock();
    retrans_list.push_back(e);
    retrans_mutex.unlock();
    return;
}