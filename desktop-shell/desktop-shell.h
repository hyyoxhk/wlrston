// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#ifndef WLRSTON_DESKTOP_SHELL_H
#define WLRSTON_DESKTOP_SHELL_H

#include <stdbool.h>

#include <wlrston-plugin.h>

struct wl_global;
struct wl_resource;

enum desktop_shell_surface_kind {
	DESKTOP_SHELL_SURFACE_BACKGROUND,
	DESKTOP_SHELL_SURFACE_PANEL,
	DESKTOP_SHELL_SURFACE_LOCK,
	DESKTOP_SHELL_SURFACE_SCREENSAVER,
};

struct desktop_shell_surface {
	struct wl_list link;
	struct desktop_shell *shell;
	struct wlr_surface *surface;
	struct wlr_output *output;
	struct wlr_scene_tree *scene_tree;
	struct wlr_scene_surface *scene_surface;
	struct wl_listener scene_tree_destroy;
	struct wl_listener surface_destroy;
	enum desktop_shell_surface_kind kind;
};

struct desktop_shell {
	struct wlrston_server *server;
	const struct wlrston_plugin_api *api;
	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener new_xdg_surface;
	struct wlr_scene_tree *background_tree;
	struct wlr_scene_tree *app_tree;
	struct wlr_scene_tree *panel_tree;
	struct wlr_scene_tree *screensaver_tree;
	struct wlr_scene_tree *lock_tree;
	struct wl_global *desktop_shell_global;
	struct wl_global *screensaver_global;
	struct wl_list surfaces;
	struct wlr_surface *grab_surface;
	bool desktop_ready;
	bool locked;
	uint32_t panel_position;
};

int wlrston_shell_init(struct wlrston_server *server,
		       const struct wlrston_plugin_api *api,
		       int *argc, char *argv[]);

void desktop_shell_update_surface_layout(struct desktop_shell_surface *surface);
void handle_new_xdg_surface(struct wl_listener *listener, void *data);

#endif
