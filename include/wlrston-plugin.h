// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#ifndef WLRSTON_PLUGIN_H
#define WLRSTON_PLUGIN_H

#include <stdint.h>

#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>

#define WLRSTON_PLUGIN_API_VERSION 1

struct wlr_surface;
struct wlr_output_layout;
struct wlrston_plugin_api;
struct wlrston_server;

enum wlrston_cursor_mode {
	WLRSTON_CURSOR_PASSTHROUGH,
	WLRSTON_CURSOR_MOVE,
	WLRSTON_CURSOR_RESIZE,
};

struct wlrston_view {
	struct wl_list link;
	struct wlrston_server *server;
	const struct wlrston_plugin_api *api;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;
	int x, y;
};

struct wlrston_plugin_api {
	uint32_t abi_version;
	struct wl_display *(*get_display)(struct wlrston_server *server);
	struct wlr_xdg_shell *(*get_xdg_shell)(struct wlrston_server *server);
	struct wlr_scene *(*get_scene)(struct wlrston_server *server);
	struct wlr_output_layout *(*get_output_layout)(struct wlrston_server *server);
	void (*map_view)(struct wlrston_view *view);
	void (*unmap_view)(struct wlrston_view *view);
	void (*focus_view)(struct wlrston_view *view, struct wlr_surface *surface);
	void (*begin_interactive)(struct wlrston_view *view,
				 enum wlrston_cursor_mode mode, uint32_t edges);
};

typedef int (*wlrston_shell_init_func)(struct wlrston_server *server,
				       const struct wlrston_plugin_api *api,
				       int *argc, char *argv[]);

#endif
