/*
 * Copyright (C) 2023 Joseph M Vrba
 *
 * This file declares functions for initializing the socket layer. Windows has
 * functions that need to be run before using sockets, so its the primary
 * reason for this file.
 */
#ifndef SOCKET_LAYER_H
#define SOCKET_LAYER_H

/*
 * Initialize the socket layer for windows.
 */
static int init_socket_layer(void);

/*
 * Cleanup the socket layer.
 */
static void cleanup_socket_layer(void);

/*
 * Retrieve the error from the socket layer.
 */
static int get_error(void);

#endif // SOCKET_LAYER_H
