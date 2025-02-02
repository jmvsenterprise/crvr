#ifndef PLUGINS_H
#define PLUGINS_H

#include "http.h"

/*
 * Copyright (C) Joseph M Vrba 2025
 */
struct plugin {
	char library_name[256];
	char uri_trigger[256];
	void *handle;
	int (*load_plugin)(void);
	int (*unload_plugin)(void);
	int (*handle_post)(struct request *r, int client);
	int (*handle_get)(struct request *r, int client);
};

/**
 * Loads the plugins described in the array provided.
 *
 * @param[in] plugins - The plugin array to load.
 * @param[in] count - The number of plugins in the array.
 *
 * @return Returns 0 if the plugins were successfully loaded. Otherwise returns
 *         an error.
 */
int load_plugins(struct plugin *plugins, size_t count);

#endif // PLUGINS_H
