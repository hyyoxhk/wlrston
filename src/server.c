// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#include <stdlib.h>

#include <wlrston.h>

void server_init(struct wlrston_server *server)
{
	server->backend = wlr_backend_autocreate(server->wl_display);
	if (server->backend == NULL) {
		wlr_log(WLR_ERROR, "failed to create wlr_backend");
		exit(EXIT_FAILURE);
	}

	server->renderer = wlr_renderer_autocreate(server->backend);
	if (server->renderer == NULL) {
		wlr_log(WLR_ERROR, "failed to create wlr_renderer");
		exit(EXIT_FAILURE);
	}

	wlr_renderer_init_wl_display(server->renderer, server->wl_display);

	server->allocator = wlr_allocator_autocreate(server->backend, server->renderer);
	if (server->allocator == NULL) {
		wlr_log(WLR_ERROR, "failed to create wlr_allocator");
		exit(EXIT_FAILURE);
	}

	wlr_compositor_create(server->wl_display, server->renderer);
	wlr_subcompositor_create(server->wl_display);
	wlr_data_device_manager_create(server->wl_display);

	server->output_layout = wlr_output_layout_create();

	wl_list_init(&server->output_list);
	server->new_output.notify = server_new_output;
	wl_signal_add(&server->backend->events.new_output,
		      &server->new_output);

	server->scene = wlr_scene_create();
	wlr_scene_attach_output_layout(server->scene, server->output_layout);

	wl_list_init(&server->view_list);
	server->xdg_shell = wlr_xdg_shell_create(server->wl_display, 3);
	server->new_xdg_surface.notify = server_new_xdg_surface;
	wl_signal_add(&server->xdg_shell->events.new_surface,
		      &server->new_xdg_surface);

	cursor_init(server);

	wl_list_init(&server->keyboard_list);
	server->new_input.notify = server_new_input;
	wl_signal_add(&server->backend->events.new_input,
		      &server->new_input);
	server->seat = wlr_seat_create(server->wl_display, "seat0");
	server->request_cursor.notify = seat_request_cursor;
	wl_signal_add(&server->seat->events.request_set_cursor,
		      &server->request_cursor);
	server->request_set_selection.notify = seat_request_set_selection;
	wl_signal_add(&server->seat->events.request_set_selection,
		      &server->request_set_selection);
}

void server_finish(struct wlrston_server *server)
{
	wl_display_destroy_clients(server->wl_display);
	cursor_finish(server);
	wlr_scene_node_destroy(&server->scene->tree.node);
	wlr_output_layout_destroy(server->output_layout);
	wlr_allocator_destroy(server->allocator);
	wlr_renderer_destroy(server->renderer);
	wlr_backend_destroy(server->backend);
	wl_display_destroy(server->wl_display);
}
