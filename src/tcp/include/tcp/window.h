/**
 * @file window.h
 * @brief Window. Simply allocate a 64KB buffer for each connection.
 */

#include <mutex>

class Window
{
public:
    u_char *buf;       // Buffer array
    u_short n;             // Maximum number of slots
    u_short front;         // buf[front%n] is first item
    u_short rear;          // buf[(rear-1)%n] is last item
    u_short size;      // remaining number of bytes of the buffer
    std::mutex mutex;  // Protects accesses to buf

    Window();
    ~Window();
};