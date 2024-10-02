// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#include <stdlib.h>

#include <wlrston.h>

void server_init(struct wlrston_server *server)
{
	/* The backend is a wlroots feature which abstracts the underlying input and
	 * output hardware. The autocreate option will choose the most suitable
	 * backend based on the current environment, such as opening an X11 window
	 * if an X11 server is running. */
	server->backend = wlr_backend_autocreate(server->wl_display);
	if (server->backend == NULL) {
		wlr_log(WLR_ERROR, "failed to create wlr_backend");
		exit(EXIT_FAILURE);
	}

	/* Autocreates a renderer, either Pixman, GLES2 or Vulkan for us. The user
	 * can also specify a renderer using the WLR_RENDERER env var.
	 * The renderer is responsible for defining the various pixel formats it
	 * supports for shared memory, this configures that for clients. */
	server->renderer = wlr_renderer_autocreate(server->backend);
	if (server->renderer == NULL) {
		wlr_log(WLR_ERROR, "failed to create wlr_renderer");
		exit(EXIT_FAILURE);
	}

	wlr_renderer_init_wl_display(server->renderer, server->wl_display);

	/* Autocreates an allocator for us.
	 * The allocator is the bridge between the renderer and the backend. It
	 * handles the buffer creation, allowing wlroots to render onto the
	 * screen */
	server->allocator = wlr_allocator_autocreate(server->backend, server->renderer);
	if (server->allocator == NULL) {
		wlr_log(WLR_ERROR, "failed to create wlr_allocator");
		exit(EXIT_FAILURE);
	}

	/* This creates some hands-off wlroots interfaces. The compositor is
	 * necessary for clients to allocate surfaces, the subcompositor allows to
	 * assign the role of subsurfaces to surfaces and the data device manager
	 * handles the clipboard. Each of these wlroots interfaces has room for you
	 * to dig your fingers in and play with their behavior if you want. Note that
	 * the clients cannot set the selection directly without compositor approval,
	 * see the handling of the request_set_selection event below.*/
	wlr_compositor_create(server->wl_display, server->renderer);
	wlr_subcompositor_create(server->wl_display);
	wlr_data_device_manager_create(server->wl_display);

	/* Creates an output layout, which a wlroots utility for working with an
	 * arrangement of screens in a physical layout. */
	server->output_layout = wlr_output_layout_create();

	/* Configure a listener to be notified when new outputs are available on the
	 * backend. */
	wl_list_init(&server->outputs);
	server->new_output.notify = server_new_output;
	wl_signal_add(&server->backend->events.new_output, &server->new_output);

	/* Create a scene graph. This is a wlroots abstraction that handles all
	 * rendering and damage tracking. All the compositor author needs to do
	 * is add things that should be rendered to the scene graph at the proper
	 * positions and then call wlr_scene_output_commit() to render a frame if
	 * necessary.
	 */
	server->scene = wlr_scene_create();
	wlr_scene_attach_output_layout(server->scene, server->output_layout);

	/* Set up xdg-shell version 3. The xdg-shell is a Wayland protocol which is
	 * used for application windows. For more detail on shells, refer to my
	 * article:
	 *
	 * https://drewdevault.com/2018/07/29/Wayland-shells.html
	 */
	wl_list_init(&server->views);
	server->xdg_shell = wlr_xdg_shell_create(server->wl_display, 3);
	server->new_xdg_surface.notify = server_new_xdg_surface;
	wl_signal_add(&server->xdg_shell->events.new_surface,
			&server->new_xdg_surface);

	cursor_init(server);
	/*
	 * Configures a seat, which is a single "seat" at which a user sits and
	 * operates the computer. This conceptually includes up to one keyboard,
	 * pointer, touch, and drawing tablet device. We also rig up a listener to
	 * let us know when new input devices are available on the backend.
	 */
	wl_list_init(&server->keyboards);
	server->new_input.notify = server_new_input;
	wl_signal_add(&server->backend->events.new_input, &server->new_input);
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
	/* Once wl_display_run returns, we shut down the server. */
	wl_display_destroy_clients(server->wl_display);
	cursor_finish(server);
	wlr_scene_node_destroy(&server->scene->tree.node);
	wlr_output_layout_destroy(server->output_layout);
	wlr_allocator_destroy(server->allocator);
	wlr_renderer_destroy(server->renderer);
	wlr_backend_destroy(server->backend);
	wl_display_destroy(server->wl_display);
}
