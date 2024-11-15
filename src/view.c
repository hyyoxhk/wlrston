// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#include <assert.h>

#include <wlrston.h>
#include <view.h>

void focus_view(struct wlrston_view *view, struct wlr_surface *surface)
{
	struct wlr_xdg_surface *previous;
	struct wlr_surface *prev_surface;
	struct wlrston_server *server;
	struct wlr_keyboard *keyboard;
	struct wlr_seat *seat;

	if (view == NULL) {
		return;
	}

	server = view->server;
	seat = server->seat;
	prev_surface = seat->keyboard_state.focused_surface;
	if (prev_surface == surface) {
		return;
	}
	if (prev_surface) {
		previous = wlr_xdg_surface_from_wlr_surface(seat->keyboard_state.focused_surface);
		assert(previous->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL);
		wlr_xdg_toplevel_set_activated(previous->toplevel, false);
	}
	keyboard = wlr_seat_get_keyboard(seat);

	wlr_scene_node_raise_to_top(&view->scene_tree->node);
	wl_list_remove(&view->link);
	wl_list_insert(&server->view_list, &view->link);

	wlr_xdg_toplevel_set_activated(view->xdg_toplevel, true);

	if (keyboard != NULL) {
		wlr_seat_keyboard_notify_enter(seat, view->xdg_toplevel->base->surface,
					       keyboard->keycodes, keyboard->num_keycodes,
					       &keyboard->modifiers);
	}
}
