cmake_minimum_required(VERSION 3.24)
project(ambry-server)

set(CMAKE_CXX_STANDARD 20)

# add_library(command-interpreter
# 	aci.hpp aci.cpp)
	
add_executable(ambry-server
	main.cpp
	server.hpp server.cpp
	log.hpp log.cpp
	socket_util.hpp socket_util.cpp)

include_directories(../shared)
# add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../lib ${CMAKE_CURRENT_BINARY_DIR}/lib)

# target_link_libraries(ambry-server PUBLIC ambry_lib)
# target_include_directories(ambry-server PUBLIC ../lib)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../aci ${CMAKE_CURRENT_BINARY_DIR}/aci)

target_link_libraries(ambry-server PUBLIC command-interpreter)
target_include_directories(ambry-server PUBLIC ../aci)