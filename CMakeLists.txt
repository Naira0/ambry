cmake_minimum_required(VERSION 3.23)
project(ambry)

set(CMAKE_CXX_STANDARD 20)

add_executable(ambry
        main.cpp
        lib/db.cpp lib/db.cpp
        lib/cache.hpp lib/cache.cpp
        lib/io_manager.hpp lib/io_manager.cpp
        lib/types.hpp
        lib/util.hpp lib/util.cpp
        lib/transaction.hpp lib/transaction.cpp)
