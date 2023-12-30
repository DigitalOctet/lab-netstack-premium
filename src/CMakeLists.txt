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

# Add subdirectories
add_subdirectory(ethernet)
add_subdirectory(ip)
add_subdirectory(tcp)

# Function to simplify target linking
function(link_targets TARGET)
    target_link_libraries(${TARGET} PRIVATE ethernet ip tcp ethernet ip)
    foreach(FUNC_NAME ${WRAP_FUNC})
        target_link_options(${TARGET} PRIVATE -Wl,--wrap,${FUNC_NAME})
    endforeach()
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