add_library(tcp STATIC bitmap.cpp
                       segment.cpp
                       socket.cpp
                       tcb.cpp
                       tcp.cpp
                       window.cpp)

target_link_libraries(tcp PRIVATE ethernet)
target_link_libraries(tcp PRIVATE ip)
target_include_directories(tcp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)