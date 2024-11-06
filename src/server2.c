#include <stdlib.h>

#include <wlrston.h>

struct wlrston_server *server_create(struct wl_display *display)
{
	struct wlrston_server *server;

	server = calloc(1, sizeof *server);
	if (!server)
		return NULL;

	server->wl_display = display;

	server->backend = wlr_backend_autocreate(server->wl_display);
	if (!server->backend) {
		wlr_log(WLR_ERROR, "failed to create backend");
		goto failed;
	}

	server->renderer = wlr_renderer_autocreate(server->backend);
	if (!server->renderer) {
		wlr_log(WLR_ERROR, "failed to create renderer\n");
		goto failed_destroy_backend;
	}

	wlr_renderer_init_wl_display(server->renderer, server->wl_display);

	server->allocator = wlr_allocator_autocreate(server->backend, server->renderer);
	if (!server->allocator) {
		wlr_log(WLR_ERROR, "failed to create allocator\n");
		goto failed_destroy_renderer;
	}

	server->scene = wlr_scene_create();
	if (!server->scene) {
		wlr_log(WLR_ERROR, "failed to create scene\n");
		goto failed_destroy_allocator;
	}

	if (wlr_compositor_create(server->wl_display, server->renderer)) {
		wlr_log(WLR_ERROR, "failed to create the wlroots compositor\n");
		goto failed_destroy_scene;
	}

	if (wlr_subcompositor_create(server->wl_display)) {
		wlr_log(WLR_ERROR, "failed to create the wlroots subcompositor\n");
		goto failed_destroy_scene;
	}

	server->output_layout = wlr_output_layout_create();
	if (!server->output_layout) {
		wlr_log(WLR_ERROR, "failed to create output_layout\n");
		goto failed_destroy_scene;
	}
	
	if (wlr_scene_attach_output_layout(server->scene, server->output_layout)) {
		wlr_log(WLR_ERROR, "failed to attach output layout\n");
		goto failed_destroy_output_layout;
	}

	if (wlr_data_device_manager_create(server->wl_display)) {
		wlr_log(WLR_ERROR, "unable to create data device manager");
		goto failed_destroy_output_layout;
	}

	seat_init(server);

	server->new_output.notify = new_output_notify;
	wl_signal_add(&server->backend->events.new_output, &server->new_output);

	wl_list_init(&server->output_list);
	wl_list_init(&server->view_list);

	return server;

failed_destroy_output_layout:
	wlr_output_layout_destroy(server->output_layout);
failed_destroy_scene:
	wlr_scene_node_destroy(&server->scene->tree.node);
failed_destroy_allocator:
	wlr_allocator_destroy(server->allocator);
failed_destroy_renderer:
	wlr_renderer_destroy(server->renderer);
failed_destroy_backend:
	wlr_backend_destroy(server->backend);
failed:
	free(server);
	return NULL;
}

void
server_destory(struct wlrston_server *server)
{
	free(server);
}
