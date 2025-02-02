/*
 * Copyright (C) Joseph M Vrba 2025
 * To get plugins working:
 * - Create a configuration file that tells the server what plugins to load
 *   - Just a text file of .so names and a URI page that will cause the server
 *     to send GETs and POSTs to the plugin.
 *   - ex: "asl.html:asl.so" or maybe "asl.html -> asl.so"
 *   - Like asl.h each plugin must have an load, unload, get_handler and
 *    post_handler functions available.
 * - Server loads plugins.
 * - When request comes in, see if the request is to the URI for the plugin. If
 *   it is send it to the plugin to handle completely.
 */
#include <errno.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "plugins.h"
#include "utils.h"

static int load_plugin(struct plugin *p);

int
load_plugins(struct plugin *plugins, size_t count)
{
	int result = 0;

	for (size_t i = 0; i < count; ++i) {
		result |= load_plugin(&plugins[i]);
	}
	return result;
}

static int load_plugin(struct plugin *p)
{
	int error = 0;
	char path[1024] = {0};

	if (p->handle) return 0;

	snprintf(path, STRMAX(path), "%s.%s", p->library_name, DYLIB_EXT);

	printf("Loading plugin %s...\n", path);

	p->handle = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
	if (!p->handle) {
		printf("Failed to load plugin %s: %s\n", path, strerror(errno));
		/* If we have a better error from the system use that.
		 * otherwise just return 1. */
		return errno? errno: 1;
	}

	p->load_plugin = dlsym(p->handle, "load_plugin");
	if (!p->load_plugin) {
		printf("Failed to find \"load_plugin\" in %s: %s\n",
			p->library_name, strerror(errno));
		error = EINVAL;
	}

	p->unload_plugin = dlsym(p->handle, "unload_plugin");
	if (!p->unload_plugin) {
		printf("Failed to find \"unload_plugin\" in %s: %s\n",
			p->library_name, strerror(errno));
		error = EINVAL;
	}

	p->handle_post = dlsym(p->handle, "handle_post");
	if (!p->handle_post) {
		printf("Failed to find \"handle_post\" in %s: %s\n",
			p->library_name, strerror(errno));
		error = EINVAL;
	}

	p->handle_get = dlsym(p->handle, "handle_get");
	if (!p->handle_get) {
		printf("Failed to find \"handle_get\" in %s: %s\n",
			p->library_name, strerror(errno));
		error = EINVAL;
	}

	if (error) return error;

	error = p->load_plugin();
	if (error) {
		printf("load_plugin failed for %s: %i\n", p->library_name,
			error);
	}

	return error;
}
