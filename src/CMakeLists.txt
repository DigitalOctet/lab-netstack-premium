project(netStack)
cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(ethernet)
add_subdirectory(ip)

add_executable(detectNIC tests/lab1-link-layer/detectNIC.cpp)
target_link_libraries(detectNIC PUBLIC ethernet)
target_link_libraries(detectNIC PUBLIC ip)

add_executable(send_frame tests/lab1-link-layer/send_frame.cpp)
target_link_libraries(send_frame PUBLIC ethernet)
target_link_libraries(send_frame PUBLIC ip)

add_executable(receive_frame tests/lab1-link-layer/receive_frame.cpp)
target_link_libraries(receive_frame PUBLIC ethernet)
target_link_libraries(receive_frame PUBLIC ip)

add_executable(epoll_test tests/lab1-link-layer/epoll_test.cpp)
target_link_libraries(epoll_test PUBLIC ethernet)
target_link_libraries(epoll_test PUBLIC ip)



add_executable(packet tests/lab2-network-layer/packet.cpp)
target_link_libraries(packet PUBLIC ethernet)
target_link_libraries(packet PUBLIC ip)

add_executable(arp tests/lab2-network-layer/packet.cpp)
target_link_libraries(arp PUBLIC ethernet)
target_link_libraries(arp PUBLIC ip)