cmake_minimum_required(VERSION 3.10)
project(ReverseProxy)

set(CMAKE_CXX_STANDARD 17)

# Create an executable named 'proxy' from our main.cpp file
add_executable(proxy src/main.cpp src/server/server.cpp)

# If we are compiling on Windows, link the Winsock networking library
if(WIN32)
    target_link_libraries(proxy ws2_32)
endif()
