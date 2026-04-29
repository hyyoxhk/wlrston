// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#include <stdlib.h>
#include <time.h>

#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>

#include <wlrston.h>
#include <server.h>

static void output_frame(struct wl_listener *listener, void *data)
{
	struct wlrston_output *output = wl_container_of(listener, output, frame);
	struct wlr_scene *scene = output->server->scene;
	struct wlr_scene_output *scene_output;
	struct timespec now;

	scene_output = wlr_scene_get_scene_output(scene, output->wlr_output);
	wlr_scene_output_commit(scene_output);

	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(scene_output, &now);
}

static void output_destroy(struct wl_listener *listener, void *data)
{
	struct wlrston_output *output = wl_container_of(listener, output, destroy);
	struct wlrston_server *server = output->server;

	wl_list_remove(&output->frame.link);
	wl_list_remove(&output->destroy.link);
	wl_list_remove(&output->link);

	if (wl_list_empty(&server->output_list)) {
		wlr_log(WLR_INFO, "last output destroyed, terminating display");
		wl_display_terminate(server->wl_display);
	}

	free(output);
}

void handle_new_output(struct wl_listener *listener, void *data)
{
	struct wlrston_server *server =
		wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;
	struct wlr_output_mode *mode;
	struct wlrston_output *output;

	wlr_output_init_render(wlr_output, server->allocator, server->renderer);

	if (!wl_list_empty(&wlr_output->modes)) {
		mode = wlr_output_preferred_mode(wlr_output);
		wlr_output_set_mode(wlr_output, mode);
		wlr_output_enable(wlr_output, true);
		if (!wlr_output_commit(wlr_output)) {
			return;
		}
	}

	output = calloc(1, sizeof(struct wlrston_output));
	output->wlr_output = wlr_output;
	output->server = server;
	output->frame.notify = output_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);

	output->destroy.notify = output_destroy;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);

	wl_list_insert(&server->output_list, &output->link);

	wlr_output_layout_add_auto(server->output_layout, wlr_output);
}
