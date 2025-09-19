// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#include <stdlib.h>

#include <wlr/types/wlr_seat.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_keyboard_group.h>

#include <wlrston.h>
#include <server.h>

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
new_keyboard(struct wlrston_seat *seat, struct wlr_input_device *device)
{
	struct wlr_keyboard *wlr_keyboard;
	struct wlrston_keyboard *keyboard;

	wlr_keyboard = wlr_keyboard_from_input_device(device);

	keyboard = calloc(1, sizeof(struct wlrston_keyboard));
	keyboard->base.device = device;
	keyboard->wlr_keyboard = wlr_keyboard;

	wlr_keyboard_set_keymap(wlr_keyboard, seat->keyboard_group->keyboard.keymap);

	keyboard->modifiers.notify = handle_keyboard_modifiers;
	wl_signal_add(&wlr_keyboard->events.modifiers, &keyboard->modifiers);
	keyboard->key.notify = handle_keyboard_key;
	wl_signal_add(&wlr_keyboard->events.key, &keyboard->key);

	wlr_seat_set_keyboard(seat->seat, keyboard->wlr_keyboard);

	return (struct wlrston_input *)keyboard;
}

static struct wlrston_input *
new_pointer(struct wlrston_seat *seat, struct wlr_input_device *device)
{
	struct wlrston_input *input =
		calloc(1, sizeof(struct wlrston_input));

	input->device = device;

	wlr_cursor_attach_input_device(seat->cursor, device);

	return input;
}

static void new_input_notify(struct wl_listener *listener, void *data)
{
	struct wlrston_seat *seat = wl_container_of(listener, seat, new_input);
	struct wlr_input_device *device = data;
	struct wlrston_input *input = NULL;

	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		input = new_keyboard(seat, device);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		input = new_pointer(seat, device);
		break;
	default:
		wlr_log(WLR_INFO, "unsupported input device");
		return;
	}
	seat_add_device(seat, input);
}

void seat_init(struct wlrston_server *server)
{
	struct wlrston_seat *seat = &server->seat;
	seat->server = server;

	seat->seat = wlr_seat_create(server->wl_display, "seat0");

	wl_list_init(&seat->input_list);

	seat->new_input.notify = new_input_notify;
	wl_signal_add(&server->backend->events.new_input,
		      &seat->new_input);

	seat->cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(seat->cursor, server->output_layout);

	keyboard_init(seat);
	cursor_init(seat);
}

void seat_finish(struct wlrston_server *server)
{
	struct wlrston_seat *seat = &server->seat;
	wl_list_remove(&seat->new_input.link);

	struct wlrston_input *input, *next;
	wl_list_for_each_safe(input, next, &seat->input_list, link) {
		input_device_destroy(&input->destroy, NULL);
	}

	keyboard_finish(seat);
	cursor_finish(seat);
	wlr_seat_destroy(seat->seat);
}
