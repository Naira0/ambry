cmake_minimum_required(VERSION 3.24)
project(ambry-command-interpreter)

set(CMAKE_CXX_STANDARD 20)

add_library(command-interpreter
	aci.hpp aci.cpp)
	
# add_executable(command-interpreter
# 	main.cpp
# 	aci.hpp aci.cpp) a

include_directories(../shared)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../lib ${CMAKE_CURRENT_BINARY_DIR}/lib)

target_link_libraries(command-interpreter PUBLIC ambry_lib)
target_include_directories(command-interpreter PUBLIC ../lib)
