// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#ifndef WLRSTON_SERVER_H
#define WLRSTON_SERVER_H

#include <wlrston.h>

struct wl_listener;

/* Internal-only declarations shared across modules */

void seat_init(struct wlrston_server *server);
void seat_finish(struct wlrston_server *server);

void cursor_init(struct wlrston_seat *seat);
void cursor_finish(struct wlrston_seat *seat);

void handle_new_output(struct wl_listener *listener, void *data);
void handle_new_xdg_surface(struct wl_listener *listener, void *data);

void handle_keyboard_modifiers(struct wl_listener *listener, void *data);
void handle_keyboard_key(struct wl_listener *listener, void *data);

void reset_cursor_mode(struct wlrston_server *server);

#endif
