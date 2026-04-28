// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_xcursor_manager.h>

#include <wlrston.h>
#include <server.h>
#include <view.h>

static struct wlrston_view *
desktop_view_at(struct wlrston_server *server, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy)
{
	struct wlr_scene_surface *scene_surface;
	struct wlr_scene_buffer *scene_buffer;
	struct wlr_scene_node *node;
	struct wlr_scene_tree *tree;

	node = wlr_scene_node_at(&server->scene->tree.node, lx, ly, sx, sy);
	if (node == NULL || node->type != WLR_SCENE_NODE_BUFFER) {
		return NULL;
	}
	scene_buffer = wlr_scene_buffer_from_node(node);
	scene_surface = wlr_scene_surface_from_buffer(scene_buffer);
	if (!scene_surface) {
		return NULL;
	}

	*surface = scene_surface->surface;

	tree = node->parent;
	while (tree != NULL && tree->node.data == NULL) {
		tree = tree->node.parent;
	}
	return tree != NULL ? tree->node.data : NULL;
}

void reset_cursor_mode(struct wlrston_server *server)
{
	server->cursor_mode = WLRSTON_CURSOR_PASSTHROUGH;
	server->grabbed_view = NULL;
	server->resize_edges = 0;
	server->grab_x = 0.0;
	server->grab_y = 0.0;
	server->grab_geobox.x = 0;
	server->grab_geobox.y = 0;
	server->grab_geobox.width = 0;
	server->grab_geobox.height = 0;
}

static void request_set_cursor_notify(struct wl_listener *listener, void *data)
{
	struct wlrston_seat *seat = wl_container_of(listener, seat, request_set_cursor);
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	struct wlr_seat_client *focused_client =
		seat->seat->pointer_state.focused_client;

	if (focused_client == event->seat_client) {
		wlr_cursor_set_surface(seat->cursor, event->surface,
				       event->hotspot_x, event->hotspot_y);
	}
}

static void request_set_selection_notify(struct wl_listener *listener, void *data)
{
	struct wlrston_seat *seat =
		wl_container_of(listener, seat, request_set_selection);
	struct wlr_seat_request_set_selection_event *event = data;

	wlr_seat_set_selection(seat->seat, event->source, event->serial);
}

static void process_cursor_move(struct wlrston_server *server)
{
	struct wlrston_seat *seat = &server->seat;
	struct wlrston_view *view = server->grabbed_view;

	view->x = seat->cursor->x - server->grab_x;
	view->y = seat->cursor->y - server->grab_y;
	wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
}

static void process_cursor_resize(struct wlrston_server *server)
{
	struct wlrston_seat *seat = &server->seat;
	struct wlrston_view *view = server->grabbed_view;
	double border_x = seat->cursor->x - server->grab_x;
	double border_y = seat->cursor->y - server->grab_y;
	int new_left = server->grab_geobox.x;
	int new_right = server->grab_geobox.x + server->grab_geobox.width;
	int new_top = server->grab_geobox.y;
	int new_bottom = server->grab_geobox.y + server->grab_geobox.height;
	int new_width, new_height;
	struct wlr_box geo_box;

	if (server->resize_edges & WLR_EDGE_TOP) {
		new_top = border_y;
		if (new_top >= new_bottom) {
			new_top = new_bottom - 1;
		}
	} else if (server->resize_edges & WLR_EDGE_BOTTOM) {
		new_bottom = border_y;
		if (new_bottom <= new_top) {
			new_bottom = new_top + 1;
		}
	}
	if (server->resize_edges & WLR_EDGE_LEFT) {
		new_left = border_x;
		if (new_left >= new_right) {
			new_left = new_right - 1;
		}
	} else if (server->resize_edges & WLR_EDGE_RIGHT) {
		new_right = border_x;
		if (new_right <= new_left) {
			new_right = new_left + 1;
		}
	}

	wlr_xdg_surface_get_geometry(view->xdg_toplevel->base, &geo_box);
	view->x = new_left - geo_box.x;
	view->y = new_top - geo_box.y;
	wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);

	new_width = new_right - new_left;
	new_height = new_bottom - new_top;
	wlr_xdg_toplevel_set_size(view->xdg_toplevel, new_width, new_height);
}

static void process_cursor_motion(struct wlrston_seat *seat, uint32_t time)
{
	struct wlrston_server *server = seat->server;
	struct wlr_seat *wlr_seat = seat->seat;
	struct wlr_surface *surface = NULL;
	struct wlrston_view *view;
	double sx, sy;

	if (server->cursor_mode == WLRSTON_CURSOR_MOVE) {
		process_cursor_move(server);
		return;
	} else if (server->cursor_mode == WLRSTON_CURSOR_RESIZE) {
		process_cursor_resize(server);
		return;
	}


	view = desktop_view_at(server, seat->cursor->x, seat->cursor->y,
			       &surface, &sx, &sy);
	if (!view) {
		wlr_xcursor_manager_set_cursor_image(seat->xcursor_mgr, "left_ptr",
						     seat->cursor);
	}
	if (surface) {
		wlr_seat_pointer_notify_enter(wlr_seat, surface, sx, sy);
		wlr_seat_pointer_notify_motion(wlr_seat, time, sx, sy);
	} else {
		wlr_seat_pointer_clear_focus(wlr_seat);
	}
}

static void cursor_motion(struct wl_listener *listener, void *data)
{
	struct wlrston_seat *seat =
		wl_container_of(listener, seat, cursor_motion);
	struct wlr_pointer_motion_event *event = data;

	wlr_cursor_move(seat->cursor, &event->pointer->base,
			event->delta_x, event->delta_y);
	process_cursor_motion(seat, event->time_msec);
}

static void cursor_motion_absolute(struct wl_listener *listener, void *data)
{
	struct wlrston_seat *seat =
		wl_container_of(listener, seat, cursor_motion_absolute);
	struct wlr_pointer_motion_absolute_event *event = data;

	wlr_cursor_warp_absolute(seat->cursor, &event->pointer->base, event->x, event->y);
	process_cursor_motion(seat, event->time_msec);
}

static void cursor_button(struct wl_listener *listener, void *data)
{
	struct wlrston_seat *seat =
		wl_container_of(listener, seat, cursor_button);
	struct wlrston_server *server = seat->server;
	struct wlr_pointer_button_event *event = data;
	struct wlr_surface *surface = NULL;
	struct wlrston_view *view;
	double sx, sy;

	wlr_seat_pointer_notify_button(seat->seat, event->time_msec,
				       event->button, event->state);

	view = desktop_view_at(server, seat->cursor->x, seat->cursor->y,
			       &surface, &sx, &sy);
	if (event->state == WLR_BUTTON_RELEASED) {
		reset_cursor_mode(server);
	} else {
		focus_view(view, surface);
	}
}

static void cursor_axis(struct wl_listener *listener, void *data)
{
	struct wlrston_seat *seat =
		wl_container_of(listener, seat, cursor_axis);
	struct wlr_pointer_axis_event *event = data;

	wlr_seat_pointer_notify_axis(seat->seat, event->time_msec,
				     event->orientation, event->delta,
				     event->delta_discrete, event->source);
}

static void cursor_frame(struct wl_listener *listener, void *data)
{
	struct wlrston_seat *seat =
		wl_container_of(listener, seat, cursor_frame);

	wlr_seat_pointer_notify_frame(seat->seat);
}

void cursor_init(struct wlrston_seat *seat)
{
	seat->xcursor_mgr = wlr_xcursor_manager_create(NULL, 24);
	wlr_xcursor_manager_load(seat->xcursor_mgr, 1);

	seat->cursor_motion.notify = cursor_motion;
	wl_signal_add(&seat->cursor->events.motion,
		      &seat->cursor_motion);
	seat->cursor_motion_absolute.notify = cursor_motion_absolute;
	wl_signal_add(&seat->cursor->events.motion_absolute,
		      &seat->cursor_motion_absolute);
	seat->cursor_button.notify = cursor_button;
	wl_signal_add(&seat->cursor->events.button,
		      &seat->cursor_button);
	seat->cursor_axis.notify = cursor_axis;
	wl_signal_add(&seat->cursor->events.axis,
		      &seat->cursor_axis);
	seat->cursor_frame.notify = cursor_frame;
	wl_signal_add(&seat->cursor->events.frame,
		      &seat->cursor_frame);

	seat->request_set_cursor.notify = request_set_cursor_notify;
	wl_signal_add(&seat->seat->events.request_set_cursor,
		      &seat->request_set_cursor);
	seat->request_set_selection.notify = request_set_selection_notify;
	wl_signal_add(&seat->seat->events.request_set_selection,
		      &seat->request_set_selection);
}

void cursor_finish(struct wlrston_seat *seat)
{
	/* remove listeners before destroying objects */
	wl_list_remove(&seat->cursor_motion.link);
	wl_list_remove(&seat->cursor_motion_absolute.link);
	wl_list_remove(&seat->cursor_button.link);
	wl_list_remove(&seat->cursor_axis.link);
	wl_list_remove(&seat->cursor_frame.link);
	wl_list_remove(&seat->request_set_cursor.link);
	wl_list_remove(&seat->request_set_selection.link);

	wlr_xcursor_manager_destroy(seat->xcursor_mgr);
	wlr_cursor_destroy(seat->cursor);
}
