project(netStack)
cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 17)

set(WRAP_FUNC
    socket
    bind
    listen
    connect
    accept
    read
    write
    close
    getaddrinfo)
# List of targets
set(TARGETS_LAB1
    detectNIC
    send_frame
    receive_frame
    epoll_test)
set(TARGETS_LAB2
    packet
    arp)
set(TARGETS_LAB3
    client
    server)

# Add subdirectories
add_subdirectory(ethernet)
add_subdirectory(ip)
add_subdirectory(tcp)

# Function to simplify target linking
function(link_targets TARGET)
    foreach(FUNC_NAME ${WRAP_FUNC})
        target_link_options(${TARGET} PUBLIC -Wl,--wrap,${FUNC_NAME})
    endforeach()
    target_link_libraries(${TARGET} PUBLIC tcp)
    target_link_libraries(${TARGET} PUBLIC ip)
    target_link_libraries(${TARGET} PUBLIC ethernet)
endfunction()

# Create and link executables
foreach(TARGET_NAME ${TARGETS_LAB1})
    add_executable(${TARGET_NAME} tests/lab1-link-layer/${TARGET_NAME}.cpp)
    link_targets(${TARGET_NAME})
endforeach()

foreach(TARGET_NAME ${TARGETS_LAB2})
    add_executable(${TARGET_NAME} tests/lab2-network-layer/${TARGET_NAME}.cpp)
    link_targets(${TARGET_NAME})
endforeach()

foreach(TARGET_NAME ${TARGETS_LAB3})
    add_executable(${TARGET_NAME} tests/lab3-transport-layer/${TARGET_NAME}.c
                                  tests/lab3-transport-layer/util.c)
    link_targets(${TARGET_NAME})
endforeach()

add_executable(send_segment tests/lab3-transport-layer/send_segment.cpp)
link_targets(send_segment)