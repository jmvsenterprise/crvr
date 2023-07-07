cmake_minimum_required(VERSION 3.16)
project(server CXX C)

add_executable(server "")
add_compile_definitions(
	UNIX=1
)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall -Wextra -Werror -Wconversion")
target_sources(server
	PRIVATE
		server.cpp
		pool.c
)
