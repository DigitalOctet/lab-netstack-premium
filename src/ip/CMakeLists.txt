add_library(ip STATIC ip.cpp
                      packet.cpp
                      routing_table.cpp)

target_link_libraries(ip PRIVATE ethernet)
target_link_libraries(ip PRIVATE pcap)
target_include_directories(ip PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)