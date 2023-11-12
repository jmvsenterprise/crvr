cmake_minimum_required(VERSION 3.15)

project(sg LANGUAGES CXX)

enable_testing()

add_executable(cppserver "")
target_sources(cppserver
	PRIVATE
		asl.cpp
		cppserver.cpp
		http.cpp
		socket_layer.cpp
		utils.cpp
)

if (NOT DEFINED CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Debug")
endif()

# Enable all warnings and conform to standards. Save temporary files.
if (MSVC)
	target_compile_options(cppserver
		PRIVATE
			/W3
			/std:c++17
			/Zi # Enabled debug symbols for Release and Debug.
	)
	target_compile_definitions(cppserver
		PRIVATE
			-DWINDOWS=1
	)
else()
	target_compile_options(cppserver
		PRIVATE
			-Wall
			-Wextra
			-Wshadow
			-Wpedantic
			-Wconversion
			-std=c++17
	)

	if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
		target_compile_definitions(cppserver
			PRIVATE
				-DMAC=1
				-Wno-gnu-line-marker
		)
	elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
		target_compile_definitions(cppserver
			PRIVATE
				-DLINUX=1
		)
	endif()
endif()

message(STATUS "Build for ${CMAKE_BUILD_TYPE}")

# If debug, turn on symbols, if release make warnings errors.
if (CMAKE_BUILD_TYPE MATCHES Debug)
	message(STATUS "Adding debug symbols.")
	if (MSVC)
		# Enable spectre mitigations
		target_compile_options(cppserver
			PRIVATE
				/Qspectre
		)
	else()
		target_compile_options(cppserver
			PRIVATE
				-g
		)
	endif()
elseif(CMAKE_BUILD_TYPE MATCHES Release)
	message(STATUS "Building for release. Warnings are no errors.")
	# No warnings allowed.
	if (MSVC)
		target_compile_options(cppserver
			PRIVATE
				/O3
				/WX
		)
	else()
		target_compile_options(cppserver
			PRIVATE
				-O3
				-Werror
		)
	endif()
else()
	message(FATAL_ERROR "Unrecognized build type: ${CMAKE_BUILD_TYPE}")
endif()

# Try to find doxygen and build the documentation.
find_package(Doxygen OPTIONAL_COMPONENTS dot mscgen dia)
if (DOXYGEN_FOUND EQUAL TRUE)
	doxygen_add_docs(${CMAKE_CURRENT_SOURCE_DIR})
endif()

include(CPack)
set(CPACK_PACKAGE_NAME "cppserver")
set(CPACK_PACKAGE_VENDOR "Joseph Vrba")
set(CPACK_PACKAGE_VERSION "0.1")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/Copyright.txt")
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/Copyright.txt DESTINATION bin RENAME "License.txt")
include(InstallRequiredSystemLibraries)
