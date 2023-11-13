/*
 * Copyright (C) 2023 Joseph M Vrba
 *
 * Defines the routines that initialize or deinitialize the socket layer.
 */
#include "socket_layer.h"

#if WINDOWS
#include <winsock2.h>
#include <iostream>

namespace system {

// Not sure how to initialize the WSA layer whenever I need socket functions
// yet.
bool socketry::initialized{0};

socketry::socketry()
{
}
socketry::~socketry()
{
	WSACleanup();
}

int socketry::get_error()
{
	WSAGetLastError();
}

std::optional<socketry> socketry::init()
{
	static system::socketry socketry;

	// Startup winsock.
	struct WSAData wsa_data = {0};
	int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
	if (result != 0) {
		::std::cerr << "WSA Startup failed: " << result << '\n';
		return {};
	}
	return {socketry};
}

static void cleanup_socket_layer(void)
{
}

static int get_error(void)
{
}

} // End namespace system

#elif UNIX | LINUX
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#else

#error ("No build type specified")

#endif // WINDOWS/MAC
