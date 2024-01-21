/*
 * Copyright (C) 2023 Joseph M Vrba
 *
 * This file declares functions for initializing the socket layer. Windows has
 * functions that need to be run before using sockets, so its the primary
 * reason for this file.
 */
#ifndef SOCKET_LAYER_H
#define SOCKET_LAYER_H

#if WINDOWS
#include <winsock2.h>
#elif LINUX | MAC
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

/*
 * Initialize the socket layer for windows.
 */
int init_socket_layer(void) { return 0; }

/*
 * Cleanup the socket layer.
 */
void cleanup_socket_layer(void) {}

/*
 * Retrieve the error from the socket layer.
 */
int get_error(void) { return errno; }

#endif // SOCKET_LAYER_H
