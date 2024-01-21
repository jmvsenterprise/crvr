/*
 * Copyright (C) 2023 Joseph M Vrba
 *
 * Defines the routines that initialize or deinitialize the socket layer.
 */
#if WINDOWS
#include <winsock2.h>

static int init_socket_layer(void)
{
	// Startup winsock.
	struct WSAData wsa_data = {0};
	int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
	if (result != 0) {
		printf("WSA Startup failed: %d.\n", result);
		return -1;
	}
	return 0;
}

static void cleanup_socket_layer(void)
{
	WSACleanup();
}

static int get_error(void)
{
	WSAGetLastError();
}

#elif LINUX | MAC
// Nothing
#else

#error ("No build type specified")

#endif // WINDOWS/MAC
