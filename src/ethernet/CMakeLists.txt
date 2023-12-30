add_library(ethernet STATIC device_manager.cpp
                            device.cpp
                            endian.cpp
                            epoll_server.cpp)
target_link_libraries(ethernet PRIVATE ip)
target_link_libraries(ethernet PRIVATE tcp)
target_link_libraries(ethernet PRIVATE pcap)
target_link_options(ethernet PRIVATE -Wl,--wrap,close)
target_include_directories(ethernet PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
