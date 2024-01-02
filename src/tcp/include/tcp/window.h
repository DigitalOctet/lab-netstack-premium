/**
 * @file window.h
 * @brief Window. Simply allocate a 32KB buffer for each connection.
 */

#pragma once

#include <mutex>

#define MAX_BUFFER_SIZE (1 << 20)
#define MAX_WINDOW_SIZE (1 << 15)

class Window
{
public:
    u_char *buf;       // Buffer array
    unsigned int n;             // Maximum number of slots
    unsigned int front;         // buf[front%n] is first item
    unsigned int rear;          // buf[(rear-1)%n] is last item
    unsigned int size;      // remaining number of bytes of the buffer
    std::mutex mutex;  // Protects accesses to buf
    bool push;

    Window();
    ~Window();
};