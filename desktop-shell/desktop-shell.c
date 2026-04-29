// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#include <stdlib.h>

#include "desktop-shell.h"

#include <wayland-server-core.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>
#include <wlr/util/box.h>

#include "weston-desktop-shell-protocol.h"

static const struct wlr_surface_role desktop_shell_surface_role = {
	.name = "weston_desktop_shell_surface",
};

static void
desktop_shell_surface_handle_scene_tree_destroy(struct wl_listener *listener,
						  void *data)
{
	struct desktop_shell_surface *surface =
		wl_container_of(listener, surface, scene_tree_destroy);

	wl_list_remove(&surface->scene_tree_destroy.link);
	surface->scene_tree = NULL;
	surface->scene_surface = NULL;
}

static void
desktop_shell_surface_destroy(struct wl_listener *listener, void *data)
{
	struct desktop_shell_surface *surface =
		wl_container_of(listener, surface, surface_destroy);

	if (surface->scene_tree != NULL) {
		wlr_scene_node_destroy(&surface->scene_tree->node);
	}
	wl_list_remove(&surface->surface_destroy.link);
	wl_list_remove(&surface->link);
	free(surface);
}

static struct desktop_shell_surface *
desktop_shell_surface_from_wlr_surface(struct desktop_shell *shell,
					 struct wlr_surface *wlr_surface)
{
	struct desktop_shell_surface *surface;

	wl_list_for_each(surface, &shell->surfaces, link) {
		if (surface->surface == wlr_surface) {
			return surface;
		}
	}

	return NULL;
}

static struct desktop_shell_surface *
desktop_shell_ensure_surface(struct desktop_shell *shell,
			       struct wl_resource *resource,
			       struct wlr_surface *wlr_surface)
{
	struct desktop_shell_surface *surface;

	surface = desktop_shell_surface_from_wlr_surface(shell, wlr_surface);
	if (surface != NULL) {
		return surface;
	}

	surface = calloc(1, sizeof(*surface));
	if (surface == NULL) {
		wl_client_post_no_memory(wl_resource_get_client(resource));
		return NULL;
	}

	if (!wlr_surface_set_role(wlr_surface, &desktop_shell_surface_role,
				  surface, resource,
				  WESTON_DESKTOP_SHELL_ERROR_INVALID_ARGUMENT)) {
		free(surface);
		return NULL;
	}

	surface->shell = shell;
	surface->surface = wlr_surface;
	surface->surface_destroy.notify = desktop_shell_surface_destroy;
	wl_signal_add(&wlr_surface->events.destroy, &surface->surface_destroy);
	wl_list_insert(&shell->surfaces, &surface->link);

	return surface;
}

static void
desktop_shell_detach_surface(struct desktop_shell_surface *surface)
{
	if (surface->scene_tree != NULL) {
		wlr_scene_node_destroy(&surface->scene_tree->node);
		surface->scene_tree = NULL;
		surface->scene_surface = NULL;
	}
}

static void
desktop_shell_get_output_box(struct desktop_shell *shell,
			       struct wlr_output *output,
			       struct wlr_box *box)
{
	struct wlr_output_layout *output_layout;

	output_layout = shell->api->get_output_layout(shell->server);
	wlr_output_layout_get_box(output_layout, output, box);
}

void
desktop_shell_update_surface_layout(struct desktop_shell_surface *surface)
{
	struct desktop_shell *shell = surface->shell;
	struct wlr_box box = {0};
	int x, y;

	if (surface->scene_tree == NULL) {
		return;
	}

	desktop_shell_get_output_box(shell, surface->output, &box);
	x = box.x;
	y = box.y;

	if (surface->kind == DESKTOP_SHELL_SURFACE_PANEL) {
		int width = surface->surface->current.width;
		int height = surface->surface->current.height;

		switch (shell->panel_position) {
		case WESTON_DESKTOP_SHELL_PANEL_POSITION_BOTTOM:
			y = box.y + box.height - height;
			break;
		case WESTON_DESKTOP_SHELL_PANEL_POSITION_RIGHT:
			x = box.x + box.width - width;
			break;
		case WESTON_DESKTOP_SHELL_PANEL_POSITION_LEFT:
			break;
		case WESTON_DESKTOP_SHELL_PANEL_POSITION_TOP:
		default:
			break;
		}
	}

	wlr_scene_node_set_position(&surface->scene_tree->node, x, y);
}

static void
desktop_shell_attach_surface(struct desktop_shell_surface *surface,
			       struct wlr_scene_tree *parent,
			       struct wlr_output *output,
			       enum desktop_shell_surface_kind kind)
{
	surface->kind = kind;
	surface->output = output;

	desktop_shell_detach_surface(surface);

	surface->scene_tree = wlr_scene_tree_create(parent);
	if (surface->scene_tree == NULL) {
		return;
	}
	surface->scene_tree_destroy.notify =
		desktop_shell_surface_handle_scene_tree_destroy;
	wl_signal_add(&surface->scene_tree->node.events.destroy,
		      &surface->scene_tree_destroy);

	surface->scene_surface = wlr_scene_surface_create(surface->scene_tree,
							      surface->surface);
	if (surface->scene_surface == NULL) {
		wlr_scene_node_destroy(&surface->scene_tree->node);
		surface->scene_tree = NULL;
		return;
	}

	desktop_shell_update_surface_layout(surface);
}

static struct desktop_shell_surface *
desktop_shell_get_surface_from_resource(struct desktop_shell *shell,
					 struct wl_resource *resource,
					 struct wl_resource *surface_resource)
{
	struct wlr_surface *wlr_surface;

	wlr_surface = wlr_surface_from_resource(surface_resource);
	if (wlr_surface == NULL) {
		wl_resource_post_error(resource,
				       WESTON_DESKTOP_SHELL_ERROR_INVALID_ARGUMENT,
				       "invalid wl_surface");
		return NULL;
	}

	return desktop_shell_ensure_surface(shell, resource, wlr_surface);
}

static struct wlr_output *
desktop_shell_get_output_from_resource(struct wl_resource *resource,
					 struct wl_resource *output_resource)
{
	struct wlr_output *output;

	output = wlr_output_from_resource(output_resource);
	if (output == NULL) {
		wl_resource_post_error(resource,
				       WESTON_DESKTOP_SHELL_ERROR_INVALID_ARGUMENT,
				       "invalid wl_output");
	}

	return output;
}

static void
desktop_shell_handle_set_background(struct wl_client *client,
				       struct wl_resource *resource,
				       struct wl_resource *output_resource,
				       struct wl_resource *surface_resource)
{
	struct desktop_shell *shell = wl_resource_get_user_data(resource);
	struct desktop_shell_surface *surface;
	struct wlr_output *output;

	output = desktop_shell_get_output_from_resource(resource, output_resource);
	if (output == NULL) {
		return;
	}

	surface = desktop_shell_get_surface_from_resource(shell, resource,
							  surface_resource);
	if (surface == NULL) {
		return;
	}

	desktop_shell_attach_surface(surface, shell->background_tree, output,
				       DESKTOP_SHELL_SURFACE_BACKGROUND);
}

static void
desktop_shell_handle_set_panel(struct wl_client *client,
				struct wl_resource *resource,
				struct wl_resource *output_resource,
				struct wl_resource *surface_resource)
{
	struct desktop_shell *shell = wl_resource_get_user_data(resource);
	struct desktop_shell_surface *surface;
	struct wlr_output *output;

	output = desktop_shell_get_output_from_resource(resource, output_resource);
	if (output == NULL) {
		return;
	}

	surface = desktop_shell_get_surface_from_resource(shell, resource,
							  surface_resource);
	if (surface == NULL) {
		return;
	}

	desktop_shell_attach_surface(surface, shell->panel_tree, output,
				       DESKTOP_SHELL_SURFACE_PANEL);
}

static void
desktop_shell_handle_set_lock_surface(struct wl_client *client,
				       struct wl_resource *resource,
				       struct wl_resource *surface_resource)
{
	struct desktop_shell *shell = wl_resource_get_user_data(resource);
	struct desktop_shell_surface *surface;

	surface = desktop_shell_get_surface_from_resource(shell, resource,
							  surface_resource);
	if (surface == NULL) {
		return;
	}

	shell->locked = true;
	wlr_scene_node_set_enabled(&shell->lock_tree->node, true);
	desktop_shell_attach_surface(surface, shell->lock_tree, NULL,
				       DESKTOP_SHELL_SURFACE_LOCK);
}

static void
desktop_shell_handle_unlock(struct wl_client *client,
			      struct wl_resource *resource)
{
	struct desktop_shell *shell = wl_resource_get_user_data(resource);

	shell->locked = false;
	wlr_scene_node_set_enabled(&shell->lock_tree->node, false);
}

static void
desktop_shell_handle_set_grab_surface(struct wl_client *client,
					struct wl_resource *resource,
					struct wl_resource *surface_resource)
{
	struct desktop_shell *shell = wl_resource_get_user_data(resource);
	struct wlr_surface *wlr_surface;

	wlr_surface = wlr_surface_from_resource(surface_resource);
	if (wlr_surface == NULL) {
		wl_resource_post_error(resource,
				       WESTON_DESKTOP_SHELL_ERROR_INVALID_ARGUMENT,
				       "invalid grab wl_surface");
		return;
	}

	shell->grab_surface = wlr_surface;
}

static void
desktop_shell_handle_desktop_ready(struct wl_client *client,
				     struct wl_resource *resource)
{
	struct desktop_shell *shell = wl_resource_get_user_data(resource);

	shell->desktop_ready = true;
}

static void
desktop_shell_handle_set_panel_position(struct wl_client *client,
					  struct wl_resource *resource,
					  uint32_t position)
{
	struct desktop_shell *shell = wl_resource_get_user_data(resource);
	struct desktop_shell_surface *surface;

	switch (position) {
	case WESTON_DESKTOP_SHELL_PANEL_POSITION_TOP:
	case WESTON_DESKTOP_SHELL_PANEL_POSITION_BOTTOM:
	case WESTON_DESKTOP_SHELL_PANEL_POSITION_LEFT:
	case WESTON_DESKTOP_SHELL_PANEL_POSITION_RIGHT:
		break;
	default:
		wl_resource_post_error(resource,
				       WESTON_DESKTOP_SHELL_ERROR_INVALID_ARGUMENT,
				       "invalid panel position %u", position);
		return;
	}

	shell->panel_position = position;
	wl_list_for_each(surface, &shell->surfaces, link) {
		if (surface->kind == DESKTOP_SHELL_SURFACE_PANEL) {
			desktop_shell_update_surface_layout(surface);
		}
	}
}

static const struct weston_desktop_shell_interface desktop_shell_implementation = {
	.set_background = desktop_shell_handle_set_background,
	.set_panel = desktop_shell_handle_set_panel,
	.set_lock_surface = desktop_shell_handle_set_lock_surface,
	.unlock = desktop_shell_handle_unlock,
	.set_grab_surface = desktop_shell_handle_set_grab_surface,
	.desktop_ready = desktop_shell_handle_desktop_ready,
	.set_panel_position = desktop_shell_handle_set_panel_position,
};

static void
desktop_shell_bind(struct wl_client *client, void *data,
		     uint32_t version, uint32_t id)
{
	struct desktop_shell *shell = data;
	struct wl_resource *resource;

	resource = wl_resource_create(client, &weston_desktop_shell_interface,
				      version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, &desktop_shell_implementation,
				       shell, NULL);
}

static void
desktop_shell_handle_screensaver_set_surface(struct wl_client *client,
					       struct wl_resource *resource,
					       struct wl_resource *surface_resource,
					       struct wl_resource *output_resource)
{
	struct desktop_shell *shell = wl_resource_get_user_data(resource);
	struct desktop_shell_surface *surface;
	struct wlr_output *output;

	output = desktop_shell_get_output_from_resource(resource, output_resource);
	if (output == NULL) {
		return;
	}

	surface = desktop_shell_get_surface_from_resource(shell, resource,
							  surface_resource);
	if (surface == NULL) {
		return;
	}

	desktop_shell_attach_surface(surface, shell->screensaver_tree, output,
				       DESKTOP_SHELL_SURFACE_SCREENSAVER);
}

static const struct weston_screensaver_interface screensaver_implementation = {
	.set_surface = desktop_shell_handle_screensaver_set_surface,
};

static void
screensaver_bind(struct wl_client *client, void *data,
		   uint32_t version, uint32_t id)
{
	struct desktop_shell *shell = data;
	struct wl_resource *resource;

	resource = wl_resource_create(client, &weston_screensaver_interface,
				      version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, &screensaver_implementation,
				       shell, NULL);
}

WL_EXPORT int
wlrston_shell_init(struct wlrston_server *server,
		   const struct wlrston_plugin_api *api,
		   int *argc, char *argv[])
{
	struct desktop_shell *shell;

	if (api == NULL || api->abi_version != WLRSTON_PLUGIN_API_VERSION) {
		wlr_log(WLR_ERROR, "desktop-shell: incompatible plugin API");
		return -1;
	}

	shell = calloc(1, sizeof *shell);
	if (!shell)
		return -1;

	shell->server = server;
	shell->api = api;
	shell->xdg_shell = api->get_xdg_shell(server);
	shell->panel_position = WESTON_DESKTOP_SHELL_PANEL_POSITION_TOP;
	wl_list_init(&shell->surfaces);

	shell->background_tree = wlr_scene_tree_create(&api->get_scene(server)->tree);
	shell->app_tree = wlr_scene_tree_create(&api->get_scene(server)->tree);
	shell->panel_tree = wlr_scene_tree_create(&api->get_scene(server)->tree);
	shell->screensaver_tree = wlr_scene_tree_create(&api->get_scene(server)->tree);
	shell->lock_tree = wlr_scene_tree_create(&api->get_scene(server)->tree);
	if (shell->background_tree == NULL || shell->app_tree == NULL ||
	    shell->panel_tree == NULL || shell->screensaver_tree == NULL ||
	    shell->lock_tree == NULL) {
		free(shell);
		return -1;
	}
	wlr_scene_node_set_enabled(&shell->lock_tree->node, false);

	shell->desktop_shell_global = wl_global_create(
		api->get_display(server), &weston_desktop_shell_interface, 1,
		shell, desktop_shell_bind);
	shell->screensaver_global = wl_global_create(
		api->get_display(server), &weston_screensaver_interface, 1,
		shell, screensaver_bind);
	if (shell->desktop_shell_global == NULL ||
	    shell->screensaver_global == NULL) {
		free(shell);
		return -1;
	}

	shell->new_xdg_surface.notify = handle_new_xdg_surface;
	wl_signal_add(&shell->xdg_shell->events.new_surface,
		      &shell->new_xdg_surface);

	return 0;
}
