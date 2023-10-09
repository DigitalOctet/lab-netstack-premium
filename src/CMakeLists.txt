project(netStack)
cmake_minimum_required(VERSION 3.10)

add_executable(detectNIC tests/lab1-link-layer/detectNIC.cpp 
                         ethernet/device_manager.cpp
                         ethernet/device.cpp
                         ethernet/endian.cpp)
target_link_libraries(detectNIC PUBLIC pcap)

add_executable(send_frame tests/lab1-link-layer/send_frame.cpp 
                          ethernet/device_manager.cpp
                          ethernet/device.cpp
                          ethernet/endian.cpp)
target_link_libraries(send_frame PUBLIC pcap)

add_executable(receive_frame tests/lab1-link-layer/receive_frame.cpp 
                          ethernet/device_manager.cpp
                          ethernet/device.cpp
                          ethernet/endian.cpp)
target_link_libraries(receive_frame PUBLIC pcap)