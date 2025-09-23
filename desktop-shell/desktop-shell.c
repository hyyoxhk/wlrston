// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#include "config.h"

//#include <stdio.h>

#include "desktop-shell.h"
#include "wlrston.h"
#include <server.h>
#include <view.h>

#include <assert.h>
#include <stdlib.h>

#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/util/edges.h>


WL_EXPORT int
wlrston_shell_init(struct wlrston_server *server, int *argc, char *argv[])
{
	struct desktop_shell *shell;

	shell = calloc(1, sizeof *shell);
	if (!shell)
		return -1;

	shell->server = server;
	shell->xdg_shell = server->xdg_shell;

	shell->new_xdg_surface.notify = handle_new_xdg_surface;
	wl_signal_add(&shell->xdg_shell->events.new_surface, &shell->new_xdg_surface);

	return 0;
}
