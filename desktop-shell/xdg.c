// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#include <assert.h>
#include <stdlib.h>

#include "desktop-shell.h"

#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>

static void
xdg_toplevel_map(struct wl_listener *listener, void *data)
{
	struct wlrston_view *view = wl_container_of(listener, view, map);

	view->api->map_view(view);
	view->api->focus_view(view, view->xdg_toplevel->base->surface);
}

static void
xdg_toplevel_unmap(struct wl_listener *listener, void *data)
{
	struct wlrston_view *view = wl_container_of(listener, view, unmap);

	view->api->unmap_view(view);
}

static void
xdg_toplevel_destroy(struct wl_listener *listener, void *data)
{
	struct wlrston_view *view = wl_container_of(listener, view, destroy);

	wl_list_remove(&view->map.link);
	wl_list_remove(&view->unmap.link);
	wl_list_remove(&view->destroy.link);
	wl_list_remove(&view->request_move.link);
	wl_list_remove(&view->request_resize.link);
	wl_list_remove(&view->request_maximize.link);
	wl_list_remove(&view->request_fullscreen.link);
	free(view);
}

static void
xdg_toplevel_request_move(struct wl_listener *listener, void *data)
{
	struct wlrston_view *view = wl_container_of(listener, view, request_move);

	view->api->begin_interactive(view, WLRSTON_CURSOR_MOVE, 0);
}

static void
xdg_toplevel_request_resize(struct wl_listener *listener, void *data)
{
	struct wlrston_view *view = wl_container_of(listener, view, request_resize);
	struct wlr_xdg_toplevel_resize_event *event = data;

	view->api->begin_interactive(view, WLRSTON_CURSOR_RESIZE, event->edges);
}

static void
xdg_toplevel_request_maximize(struct wl_listener *listener, void *data)
{
	struct wlrston_view *view = wl_container_of(listener, view, request_maximize);

	wlr_xdg_surface_schedule_configure(view->xdg_toplevel->base);
}

static void
xdg_toplevel_request_fullscreen(struct wl_listener *listener, void *data)
{
	struct wlrston_view *view = wl_container_of(listener, view, request_fullscreen);

	wlr_xdg_surface_schedule_configure(view->xdg_toplevel->base);
}

void handle_new_xdg_surface(struct wl_listener *listener, void *data)
{
	struct desktop_shell *shell = wl_container_of(listener, shell, new_xdg_surface);
	struct wlr_xdg_surface *xdg_surface = data;
	struct wlr_xdg_surface *parent;
	struct wlr_scene_tree *parent_tree;
	struct wlr_xdg_toplevel *toplevel;
	struct wlrston_view *view;

	if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
		parent = wlr_xdg_surface_from_wlr_surface(xdg_surface->popup->parent);
		parent_tree = parent->data;
		xdg_surface->data = wlr_scene_xdg_surface_create(parent_tree, xdg_surface);
		return;
	}
	assert(xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL);

	view = calloc(1, sizeof(*view));
	if (view == NULL) {
		return;
	}

	view->server = shell->server;
	view->api = shell->api;
	view->xdg_toplevel = xdg_surface->toplevel;
	view->scene_tree = wlr_scene_xdg_surface_create(
		shell->app_tree,
		view->xdg_toplevel->base);
	if (view->scene_tree == NULL) {
		free(view);
		return;
	}
	view->scene_tree->node.data = view;
	xdg_surface->data = view->scene_tree;

	view->map.notify = xdg_toplevel_map;
	wl_signal_add(&xdg_surface->events.map, &view->map);
	view->unmap.notify = xdg_toplevel_unmap;
	wl_signal_add(&xdg_surface->events.unmap, &view->unmap);
	view->destroy.notify = xdg_toplevel_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &view->destroy);

	toplevel = xdg_surface->toplevel;
	view->request_move.notify = xdg_toplevel_request_move;
	wl_signal_add(&toplevel->events.request_move, &view->request_move);
	view->request_resize.notify = xdg_toplevel_request_resize;
	wl_signal_add(&toplevel->events.request_resize, &view->request_resize);
	view->request_maximize.notify = xdg_toplevel_request_maximize;
	wl_signal_add(&toplevel->events.request_maximize,
		      &view->request_maximize);
	view->request_fullscreen.notify = xdg_toplevel_request_fullscreen;
	wl_signal_add(&toplevel->events.request_fullscreen,
		      &view->request_fullscreen);
}
