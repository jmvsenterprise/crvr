cmake_minimum_required(VERSION 3.16)
project(server CXX)

add_executable(server "")
compile_definitions(MAC=1
)
target_sources(server
	PRIVATE
		server.cpp
)
