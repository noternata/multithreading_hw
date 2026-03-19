cmake_minimum_required(VERSION 3.16)
project(hw_5_future)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(test_future
    test_future.cpp
)

add_executable(test_thread_pool
    test_thread_pool.cpp
)