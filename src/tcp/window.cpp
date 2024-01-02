/**
 * @file window.cpp
 */

#include <tcp/tcp.h>

Window::Window(): 
    size(MAX_BUFFER_SIZE), n(MAX_BUFFER_SIZE), front(0), rear(0), push(false)
{
    buf = new u_char[size];
}

Window::~Window()
{
    delete[] buf;
}