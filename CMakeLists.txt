cmake_minimum_required(VERSION 3.10)
project(MultithreadedProxy CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# Build main reverse proxy
add_executable(main 
    src/main.cpp 
    src/server/server.cpp 
    src/http/request.cpp 
    src/concurrency/thread_pool.cpp
)
target_link_libraries(main ws2_32)

# Build fast_backend
add_executable(fast_backend backends/fast_backend.cpp)
target_link_libraries(fast_backend ws2_32)
