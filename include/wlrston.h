// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#ifndef WLRSTON_H
#define WLRSTON_H

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
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

	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *xcursor_mgr;

	struct wl_list input_list;
	struct wl_listener new_input;

	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

	struct wl_listener request_cursor;
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
	struct wl_list views;

	struct wlrston_seat seat;

	enum wlrston_cursor_mode cursor_mode;
	struct wlrston_view *grabbed_view;
	double grab_x, grab_y;
	struct wlr_box grab_geobox;
	uint32_t resize_edges;

	struct wlr_output_layout *output_layout;
	struct wl_list outputs;
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

void server_init(struct wlrston_server *server);

void server_finish(struct wlrston_server *server);

void reset_cursor_mode(struct wlrston_server *server);

void server_new_output(struct wl_listener *listener, void *data);

void server_new_xdg_surface(struct wl_listener *listener, void *data);

void seat_init(struct wlrston_server *server);

void seat_finish(struct wlrston_server *server);

void cursor_init(struct wlrston_seat *seat);

void cursor_finish(struct wlrston_seat *seat);

void seat_request_cursor(struct wl_listener *listener, void *data);

void seat_request_set_selection(struct wl_listener *listener, void *data);

void keyboard_handle_modifiers(struct wl_listener *listener, void *data);

void keyboard_handle_key(struct wl_listener *listener, void *data);

void keyboard_handle_destroy(struct wl_listener *listener, void *data);

#endif
