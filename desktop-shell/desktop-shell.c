// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#include <stdlib.h>

#include "desktop-shell.h"

#include <wlr/util/log.h>

WL_EXPORT int
wlrston_shell_init(struct wlrston_server *server,
		   const struct wlrston_plugin_api *api,
		   int *argc, char *argv[])
{
	struct desktop_shell *shell;

	if (api == NULL || api->abi_version != WLRSTON_PLUGIN_API_VERSION) {
		wlr_log(WLR_ERROR, "desktop-shell: incompatible plugin API");
		return -1;
	}

	shell = calloc(1, sizeof *shell);
	if (!shell)
		return -1;

	shell->server = server;
	shell->api = api;
	shell->xdg_shell = api->get_xdg_shell(server);

	shell->new_xdg_surface.notify = handle_new_xdg_surface;
	wl_signal_add(&shell->xdg_shell->events.new_surface,
		      &shell->new_xdg_surface);

	return 0;
}
