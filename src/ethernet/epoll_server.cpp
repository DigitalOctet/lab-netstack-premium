/**
 * @file epoll_server.cpp
 */

#include "epoll_server.h"
#include <unistd.h>
#include <iostream>

/**
 * @brief Constructor of `EpollServer`. Call `epoll_create()` to create 
 * a new epoll(7) instance and restore the file descriptor referring to 
 * the new epoll instance.
 * 
 * @see epoll_create
 */
EpollServer::EpollServer(): events()
{
    if((epfd = epoll_create(1)) == -1){
        std::cerr << "Epoll creation failed!" << std::endl;
        return;
    }
}

/**
 * @brief Destructor of `EpollServer`. Close the descriptor successfully 
 * created by `epoll_create` .
 */
EpollServer::~EpollServer()
{
    if(epfd >= 0){
        if(close(epfd) == -1){
            std::cerr << "Close epfd error!" << std::endl;
        }
    }
}

/**
 * @brief Add an entry to the interest list of the epoll file descriptor, epfd.
 * 
 * @param fd File descriptor to read from.
 * @param device Device related to FD.
 * @return 0 on success, -1 on error.
 */
int 
EpollServer::addRead(int fd, Device *device)
{
    struct epoll_event event;  // Used for register an event.
    event.data.fd = fd;
    event.events = EPOLLIN;

    if(epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event) == -1){
        std::cerr << "Epoll control interface add error!" << std::endl;
        return -1;
    }

    auto it = fd2device.find(fd);
    if(it != fd2device.end()){
        std::cerr << "FD " << fd << " already exists!" << std::endl;
    }else{
        fd2device[fd] = device;
    }
    return 0;
}

/**
 * @brief Waits for events on the epoll(7) instance referred to by the file 
 * descriptor `epfd`. The buffer pointed to by events is used to return 
 * information from the ready list about file descriptors in the interest list 
 * that have some events available. Up to MAX_EVENTS are returned.
 * 
 * @return 0 on success, -1 on error.
 */
int 
EpollServer::waitRead()
{
    int n_events = epoll_wait(epfd, events, MAX_EVENTS, TIMEOUT);
    if(n_events == -1){
        std::cerr << "Epoll wait error!" << std::endl;
        return -1;
    }

    for(int i = 0; i < n_events; i++){
        auto it = fd2device.find(events[i].data.fd);
        if(it != fd2device.end()){
            it->second->capBuf();
        }else{
            std::cerr << "File descriptor not found!" << std::endl;
        }
    }
    return 0;
}