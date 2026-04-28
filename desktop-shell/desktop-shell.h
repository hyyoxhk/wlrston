// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#ifndef WLRSTON_DESKTOP_SHELL_H
#define WLRSTON_DESKTOP_SHELL_H

#include <wlrston-plugin.h>

struct desktop_shell {
	struct wlrston_server *server;
	const struct wlrston_plugin_api *api;
	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener new_xdg_surface;
};

int wlrston_shell_init(struct wlrston_server *server,
		       const struct wlrston_plugin_api *api,
		       int *argc, char *argv[]);

void handle_new_xdg_surface(struct wl_listener *listener, void *data);

#endif
