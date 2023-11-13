/*
 * Copyright (C) 2023 Joseph M Vrba
 *
 * This file declares functions for initializing the socket layer. Windows has
 * functions that need to be run before using sockets, so its the primary
 * reason for this file.
 */
#ifndef SOCKET_LAYER_H
#define SOCKET_LAYER_H

#include <optional>

namespace system {

struct socketry {
	socketry();
	~socketry();
	// Prohibit copy and move, this should only be instantiated once.
	socketry(const socketry&) = delete;
	socketry(socketry&&) = delete;
	socketry& operator=(const socketry&) = delete;
	socketry& operator=(socketry&&) = delete;

	int get_error();

private:
	static bool initialized;
	
	/**
	 * @brief Initialize the socket layer.
	 * 
	 * @return Returns a valid socketry object if initialization was successful.
	 */
	static std::optional<socketry> init();
};

} // End namespace system

#endif // SOCKET_LAYER_H
