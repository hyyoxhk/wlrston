// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#include <wlrston.h>

struct desktop_shell {
        struct wlrston_server *server;
	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener new_xdg_surface;



	struct wlr_scene_tree *fullscreen_tree;
	struct wlr_scene_tree *panel_tree;
	struct wlr_scene_tree *background_tree;
	struct wlr_scene_tree *lock_tree;
	struct wlr_scene_tree *view_tree;
	struct wlr_scene_tree *minimized_tree;

};

void handle_new_xdg_surface(struct wl_listener *listener, void *data);
