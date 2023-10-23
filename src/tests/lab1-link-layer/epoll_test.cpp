/**
 * @file epoll_test.cpp
 * @brief Test my epoll server.
 */

#include "utils.h"
#include <iostream>
#include <thread>

void readLoop(EpollServer *epoll_server)
{
    while (true)
    {
        epoll_server->waitRead();
    }
}

int main()
{
    DeviceManager device_manager;
    device_manager.addAllDevice();
    device_manager.setFrameReceiveCallbackAll(test_callback);
    std::thread server(readLoop, device_manager.epoll_server);
    server.join();
}