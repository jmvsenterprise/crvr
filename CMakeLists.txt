cmake_minimum_required(VERSION 3.16)
project(Server CXX)

add_executable(server "")
target_sources(server
	PUBLIC
		Server.cpp
)
target_link_libraries(server
	PUBLIC
		Ws2_32
)