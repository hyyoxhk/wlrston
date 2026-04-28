// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#include <assert.h>

#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/edges.h>

#include <wlrston.h>
#include <server.h>
#include <view.h>

void map_view(struct wlrston_view *view)
{
	wl_list_insert(&view->server->view_list, &view->link);
}

void unmap_view(struct wlrston_view *view)
{
	if (view == view->server->grabbed_view) {
		reset_cursor_mode(view->server);
	}
	wl_list_remove(&view->link);
}

void focus_view(struct wlrston_view *view, struct wlr_surface *surface)
{
	struct wlr_xdg_surface *previous;
	struct wlr_surface *prev_surface;
	struct wlrston_server *server;
	struct wlr_keyboard *keyboard;
	struct wlrston_seat *seat;
	struct wlr_seat *wlr_seat;

	if (view == NULL) {
		return;
	}

	server = view->server;
	seat = &server->seat;
	wlr_seat = seat->seat;

	prev_surface = wlr_seat->keyboard_state.focused_surface;
	if (prev_surface == surface) {
		return;
	}
	if (prev_surface) {
		previous = wlr_xdg_surface_from_wlr_surface(wlr_seat->keyboard_state.focused_surface);
		if (previous && previous->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
		wlr_xdg_toplevel_set_activated(previous->toplevel, false);
		}
	}
	keyboard = wlr_seat_get_keyboard(wlr_seat);

	wlr_scene_node_raise_to_top(&view->scene_tree->node);
	wl_list_remove(&view->link);
	wl_list_insert(&server->view_list, &view->link);

	wlr_xdg_toplevel_set_activated(view->xdg_toplevel, true);

	if (keyboard != NULL) {
		wlr_seat_keyboard_notify_enter(wlr_seat, view->xdg_toplevel->base->surface,
					       keyboard->keycodes, keyboard->num_keycodes,
					       &keyboard->modifiers);
	}
}

void
begin_interactive_view(struct wlrston_view *view,
		       enum wlrston_cursor_mode mode, uint32_t edges)
{
	struct wlrston_server *server = view->server;
	struct wlrston_seat *seat = &server->seat;
	struct wlr_surface *focused_surface =
		seat->seat->pointer_state.focused_surface;

	if (focused_surface == NULL) {
		return;
	}
	if (view->xdg_toplevel->base->surface !=
	    wlr_surface_get_root_surface(focused_surface)) {
		return;
	}
	server->grabbed_view = view;
	server->cursor_mode = mode;

	if (mode == WLRSTON_CURSOR_MOVE) {
		server->grab_x = seat->cursor->x - view->x;
		server->grab_y = seat->cursor->y - view->y;
	} else {
		struct wlr_box geo_box;
		double border_x, border_y;

		wlr_xdg_surface_get_geometry(view->xdg_toplevel->base, &geo_box);

		border_x = (view->x + geo_box.x) +
			((edges & WLR_EDGE_RIGHT) ? geo_box.width : 0);
		border_y = (view->y + geo_box.y) +
			((edges & WLR_EDGE_BOTTOM) ? geo_box.height : 0);
		server->grab_x = seat->cursor->x - border_x;
		server->grab_y = seat->cursor->y - border_y;

		server->grab_geobox = geo_box;
		server->grab_geobox.x += view->x;
		server->grab_geobox.y += view->y;

		server->resize_edges = edges;
	}
}
