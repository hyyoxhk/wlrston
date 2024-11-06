// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#include <stdlib.h>

#include <wlrston.h>

static void
input_device_destroy(struct wl_listener *listener, void *data)
{
	struct wlrston_input *input = wl_container_of(listener, input, destroy);
	wl_list_remove(&input->link);
	wl_list_remove(&input->destroy.link);

	/* `struct keyboard` is derived and has some extra clean up to do */
	if (input->device->type == WLR_INPUT_DEVICE_KEYBOARD) {
		struct wlrston_keyboard *keyboard = (struct wlrston_keyboard *)input;
		wl_list_remove(&keyboard->key.link);
		wl_list_remove(&keyboard->modifiers.link);
	}
	free(input);
}

static void
seat_update_capabilities(struct wlrston_seat *seat)
{
	struct wlrston_input *input = NULL;
	uint32_t caps = 0;

	wl_list_for_each(input, &seat->input_list, link) {
		switch (input->device->type) {
		case WLR_INPUT_DEVICE_KEYBOARD:
			caps |= WL_SEAT_CAPABILITY_KEYBOARD;
			break;
		case WLR_INPUT_DEVICE_POINTER:
			caps |= WL_SEAT_CAPABILITY_POINTER;
			break;
		default:
			break;
		}
	}
	wlr_seat_set_capabilities(seat->seat, caps);
}

static void
seat_add_device(struct wlrston_seat *seat, struct wlrston_input *input)
{
	input->seat = seat;
	input->destroy.notify = input_device_destroy;
	wl_signal_add(&input->device->events.destroy, &input->destroy);
	wl_list_insert(&seat->input_list, &input->link);

	seat_update_capabilities(seat);
}

static struct wlrston_input *
server_new_keyboard(struct wlrston_seat *seat, struct wlr_input_device *device) {
	struct wlr_keyboard *wlr_keyboard = wlr_keyboard_from_input_device(device);

	struct wlrston_keyboard *keyboard =
		calloc(1, sizeof(struct wlrston_keyboard));
	// keyboard->server = server;
	keyboard->base.device = device;
	keyboard->wlr_keyboard = wlr_keyboard;

	/* We need to prepare an XKB keymap and assign it to the keyboard. This
	 * assumes the defaults (e.g. layout = "us"). */
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, NULL,
		XKB_KEYMAP_COMPILE_NO_FLAGS);

	wlr_keyboard_set_keymap(wlr_keyboard, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);

	/* Here we set up listeners for keyboard events. */
	keyboard->modifiers.notify = keyboard_handle_modifiers;
	wl_signal_add(&wlr_keyboard->events.modifiers, &keyboard->modifiers);
	keyboard->key.notify = keyboard_handle_key;
	wl_signal_add(&wlr_keyboard->events.key, &keyboard->key);

	// keyboard->destroy.notify = keyboard_handle_destroy;
	// wl_signal_add(&device->events.destroy, &keyboard->destroy);

	wlr_seat_set_keyboard(seat->seat, keyboard->wlr_keyboard);

	/* And add the keyboard to our list of keyboards */
	// wl_list_insert(&seat->keyboards, &keyboard->link);

	return (struct wlrston_input *)keyboard;
}

static struct wlrston_input *
server_new_pointer(struct wlrston_seat *seat, struct wlr_input_device *device) {
	struct wlrston_input *input =
		calloc(1, sizeof(struct wlrston_input));

	input->device = device;

	/* We don't do anything special with pointers. All of our pointer handling
	 * is proxied through wlr_cursor. On another compositor, you might take this
	 * opportunity to do libinput configuration on the device to set
	 * acceleration, etc. */
	wlr_cursor_attach_input_device(seat->cursor, device);
	return input;
}

static void server_new_input(struct wl_listener *listener, void *data) {
	/* This event is raised by the backend when a new input device becomes
	 * available. */
	struct wlrston_seat *seat =
		wl_container_of(listener, seat, new_input);
	struct wlr_input_device *device = data;
	struct wlrston_input *input = NULL;

	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		input = server_new_keyboard(seat, device);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		input = server_new_pointer(seat, device);
		break;
	default:
		break;
	}
	seat_add_device(seat, input);
}

void seat_request_cursor(struct wl_listener *listener, void *data) {
	struct wlrston_seat *seat = wl_container_of(
			listener, seat, request_cursor);
	/* This event is raised by the seat when a client provides a cursor image */
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	struct wlr_seat_client *focused_client =
		seat->seat->pointer_state.focused_client;
	/* This can be sent by any client, so we check to make sure this one is
	 * actually has pointer focus first. */
	if (focused_client == event->seat_client) {
		/* Once we've vetted the client, we can tell the cursor to use the
		 * provided surface as the cursor image. It will set the hardware cursor
		 * on the output that it's currently on and continue to do so as the
		 * cursor moves between outputs. */
		wlr_cursor_set_surface(seat->cursor, event->surface,
				event->hotspot_x, event->hotspot_y);
	}
}

void seat_request_set_selection(struct wl_listener *listener, void *data) {
	/* This event is raised by the seat when a client wants to set the selection,
	 * usually when the user copies something. wlroots allows compositors to
	 * ignore such requests if they so choose, but in tinywl we always honor
	 */
	struct wlrston_seat *seat = wl_container_of(
			listener, seat, request_set_selection);
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(seat->seat, event->source, event->serial);
}

void seat_init(struct wlrston_server *server)
{
	struct wlrston_seat *seat = &server->seat;
	seat->server = server;

	seat->seat = wlr_seat_create(server->wl_display, "seat0");

	wl_list_init(&seat->input_list);

	seat->new_input.notify = server_new_input;
	wl_signal_add(&server->backend->events.new_input, &seat->new_input);

	/*
	 * Creates a cursor, which is a wlroots utility for tracking the cursor
	 * image shown on screen.
	 */
	seat->cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(seat->cursor, server->output_layout);

	cursor_init(seat);
}

void seat_finish(struct wlrston_server *server)
{
	struct wlrston_seat *seat = &server->seat;
	wl_list_remove(&seat->new_input.link);

	// struct input *input, *next;
	// wl_list_for_each_safe(input, next, &seat->inputs, link) {
	// 	input_device_destroy(&input->destroy, NULL);
	// }


	cursor_finish(seat);

	wlr_cursor_destroy(seat->cursor);
}
