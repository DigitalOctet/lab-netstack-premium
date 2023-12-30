/**
 * @file window.cpp
 */

#include <tcp/tcp.h>

Window::Window(): size(1 << 15), n(1 << 15), front(0), rear(0)
{
    buf = new u_char[size];
}

Window::~Window()
{
    delete[] buf;
}