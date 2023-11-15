add_library(ethernet STATIC device_manager.cpp
                            device.cpp
                            endian.cpp
                            epoll_server.cpp)
target_link_libraries(ethernet PRIVATE ip)
target_link_libraries(ethernet PRIVATE pcap)
target_include_directories(ethernet PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
