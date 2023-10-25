project(netStack)
cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 17)

add_executable(detectNIC tests/lab1-link-layer/detectNIC.cpp 
                         ethernet/endian.cpp
                         ethernet/device_manager.cpp
                         ethernet/device.cpp
                         ethernet/epoll_server.cpp
                         ip/ip.cpp
                         ip/packet.cpp
                         ip/routing_table.cpp)
target_link_libraries(detectNIC PUBLIC pcap)

add_executable(send_frame tests/lab1-link-layer/send_frame.cpp 
                          ethernet/endian.cpp
                          ethernet/device_manager.cpp
                          ethernet/device.cpp
                          ethernet/epoll_server.cpp
                          ip/ip.cpp
                          ip/packet.cpp
                          ip/routing_table.cpp)
target_link_libraries(send_frame PUBLIC pcap)

add_executable(receive_frame tests/lab1-link-layer/receive_frame.cpp 
                          ethernet/endian.cpp
                          ethernet/device_manager.cpp
                          ethernet/device.cpp
                          ethernet/epoll_server.cpp
                          ip/ip.cpp
                          ip/packet.cpp
                          ip/routing_table.cpp)
target_link_libraries(receive_frame PUBLIC pcap)

add_executable(epoll_test tests/lab1-link-layer/epoll_test.cpp 
                        ethernet/endian.cpp
                        ethernet/device_manager.cpp
                        ethernet/device.cpp
                        ethernet/epoll_server.cpp
                        ip/ip.cpp
                        ip/packet.cpp
                        ip/routing_table.cpp)
                        target_link_libraries(epoll_test PUBLIC pcap)