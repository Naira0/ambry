cmake_minimum_required(VERSION 3.23)
project(ambry)

set(CMAKE_CXX_STANDARD 20)

add_executable(ambry
        main.cpp
        lib/db.cpp lib/db.cpp
        lib/cache.hpp lib/cache.cpp)
