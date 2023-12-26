/**
 * @file tcb.cpp
 */

#include <tcp/tcb.h>
#include <string.h>
#include <chrono>

TCB::TCB(): seq_init(false), window(), ack(0), pending(),
            socket_state(SocketState::UNOPENED), state(ConnectionState::CLOSED)
{
    sem_init(&semaphore, 0, 0);
}

/**
 * @brief Get the current sequence number.
 */
unsigned int 
TCB::getSequence()
{
    if(seq_init){
        return seq;
    }

    seq_init = true;
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto duration = currentTime.time_since_epoch();
    auto microseconds = \
        std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    return (microseconds >> 2) & 0xffffffffu;
}

/**
 * @brief Set sequence number.
 */
void 
TCB::setSequence(unsigned int sequence)
{
    seq += sequence;
}

/**
 * @brief Get acknowledgement number.
 */
unsigned int
TCB::getAcknowledgement()
{
    return ack;
}

/**
 * @brief Set acknowledgement number
 */
void 
TCB::setAcknowledgement(unsigned int acknowledgement)
{
    ack = acknowledgement;
}

/**
 * @brief Get window size.
 */
u_short
TCB::getWindow()
{
    return window.size;
}

/**
 * @brief Write LEN bytes to window from BUF.
 */
void 
TCB::writeWindow(const u_char *buf, int len)
{
    window.mutex.lock();
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
 */
ssize_t 
TCB::readWindow(u_char *buf, int len)
{
    window.mutex.lock();
    len = len > window.n - window.size ? window.n - window.size : len;
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
    window.mutex.unlock();
    return len;
}

/**
 * @brief Set window size of the other end.
 */
void 
TCB::setDestWindow(u_short window)
{
    dst_window = window;
}

/**
 * @brief Get window size of the other end.
 */
u_short 
TCB::getDestWindow()
{
    return dst_window;
}