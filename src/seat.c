// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#include <stdlib.h>

#include <wlrston.h>

static void new_keyboard(struct wlrston_server *server, struct wlr_input_device *device)
{
	struct wlr_keyboard *wlr_keyboard;
	struct wlrston_keyboard *keyboard;
	struct xkb_context *context;
	struct xkb_keymap *keymap;

	wlr_keyboard = wlr_keyboard_from_input_device(device);

	keyboard = calloc(1, sizeof(struct wlrston_keyboard));
	keyboard->server = server;
	keyboard->wlr_keyboard = wlr_keyboard;

	context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	keymap = xkb_keymap_new_from_names(context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);

	wlr_keyboard_set_keymap(wlr_keyboard, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);

	keyboard->modifiers.notify = keyboard_modifiers_notify;
	wl_signal_add(&wlr_keyboard->events.modifiers, &keyboard->modifiers);
	keyboard->key.notify = keyboard_key_notify;
	wl_signal_add(&wlr_keyboard->events.key, &keyboard->key);
	keyboard->destroy.notify = keyboard_destroy;
	wl_signal_add(&device->events.destroy, &keyboard->destroy);

	wlr_seat_set_keyboard(server->seat, keyboard->wlr_keyboard);

	wl_list_insert(&server->keyboard_list, &keyboard->link);
}

static void new_pointer(struct wlrston_server *server, struct wlr_input_device *device)
{
	wlr_cursor_attach_input_device(server->cursor, device);
}

void new_input_notify(struct wl_listener *listener, void *data)
{
	struct wlrston_server *server = wl_container_of(listener, server, new_input);
	struct wlr_input_device *device = data;
	uint32_t caps = WL_SEAT_CAPABILITY_POINTER;

	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		new_keyboard(server, device);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		new_pointer(server, device);
		break;
	default:
		break;
	}

	if (!wl_list_empty(&server->keyboard_list)) {
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	}
	wlr_seat_set_capabilities(server->seat, caps);
}

void request_set_cursor_notify(struct wl_listener *listener, void *data)
{
	struct wlrston_server *server = wl_container_of(listener, server, request_set_cursor);
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	struct wlr_seat_client *focused_client =
		server->seat->pointer_state.focused_client;

	if (focused_client == event->seat_client) {
		wlr_cursor_set_surface(server->cursor, event->surface,
				       event->hotspot_x, event->hotspot_y);
	}
}

void request_set_selection_notify(struct wl_listener *listener, void *data)
{
	struct wlrston_server *server =
		wl_container_of(listener, server, request_set_selection);
	struct wlr_seat_request_set_selection_event *event = data;

	wlr_seat_set_selection(server->seat, event->source, event->serial);
}
