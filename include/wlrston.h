// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#ifndef WLRSTON_H
#define WLRSTON_H

#include <wayland-server-core.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>

/* For brevity's sake, struct members are annotated where they are used. */
enum wlrston_cursor_mode {
	WLRSTON_CURSOR_PASSTHROUGH,
	WLRSTON_CURSOR_MOVE,
	WLRSTON_CURSOR_RESIZE,
};

struct wlrston_input {
	struct wlr_input_device *device;
	struct wlrston_seat *seat;
	struct wl_listener destroy;
	struct wl_list link; /* seat::input_list */
};

struct wlrston_seat {
	struct wlrston_server *server;
	struct wlr_seat *seat;
	struct wlr_keyboard_group *keyboard_group;

	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *xcursor_mgr;

	struct wl_list input_list;
	struct wl_listener new_input;

	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

	struct wl_listener request_set_cursor;
	struct wl_listener request_set_selection;
};

struct wlrston_server {
	struct wl_display *wl_display;
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_scene *scene;

	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener new_xdg_surface;
	struct wl_list view_list;

	struct wlrston_seat seat;

	enum wlrston_cursor_mode cursor_mode;
	struct wlrston_view *grabbed_view;
	double grab_x, grab_y;
	struct wlr_box grab_geobox;
	uint32_t resize_edges;

	struct wlr_output_layout *output_layout;
	struct wl_list output_list;
	struct wl_listener new_output;
};

struct wlrston_output {
	struct wl_list link;
	struct wlrston_server *server;
	struct wlr_output *wlr_output;
	struct wl_listener frame;
	struct wl_listener destroy;
};

struct wlrston_keyboard {
	struct wlrston_input base;
	struct wlr_keyboard *wlr_keyboard;

	struct wl_listener modifiers;
	struct wl_listener key;
};

struct wlrston_server *server_create(struct wl_display *display);

void server_destroy(struct wlrston_server *server);

bool server_start(struct wlrston_server *server);

/* internal APIs moved to include/server.h */

/* internal APIs moved to include/server.h */

void keyboard_init(struct wlrston_seat *seat);

void keyboard_finish(struct wlrston_seat *seat);

int wlrston_shell_init(struct wlrston_server *server, int *argc, char *argv[]);

#endif
