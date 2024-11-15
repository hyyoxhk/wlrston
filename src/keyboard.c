// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#include <stdlib.h>
#include <wayland-util.h>

#include <wlrston.h>
#include <view.h>

void keyboard_modifiers_notify(struct wl_listener *listener, void *data)
{
	struct wlrston_keyboard *keyboard =
		wl_container_of(listener, keyboard, modifiers);
	struct wlrston_seat *seat = keyboard->base.seat;
	struct wlr_seat *wlr_seat = seat->seat;

	wlr_seat_set_keyboard(wlr_seat, keyboard->wlr_keyboard);
	wlr_seat_keyboard_notify_modifiers(wlr_seat,
					   &keyboard->wlr_keyboard->modifiers);
}

static bool handle_keybinding(struct wlrston_server *server, xkb_keysym_t sym)
{
	switch (sym) {
	case XKB_KEY_Escape:
		wl_display_terminate(server->wl_display);
		break;
	case XKB_KEY_F1:
		if (wl_list_length(&server->view_list) < 2) {
			break;
		}
		struct wlrston_view *next_view = wl_container_of(
			server->view_list.prev, next_view, link);
		focus_view(next_view, next_view->xdg_toplevel->base->surface);
		break;
	default:
		return false;
	}
	return true;
}

void keyboard_key_notify(struct wl_listener *listener, void *data)
{
	struct wlrston_keyboard *keyboard =
		wl_container_of(listener, keyboard, key);
	struct wlrston_seat *seat = keyboard->base.seat;
	struct wlr_seat *wlr_seat = seat->seat;
	struct wlrston_server *server = seat->server;
	struct wlr_keyboard_key_event *event = data;
	uint32_t keycode = event->keycode + 8;
	bool handled = false;
	const xkb_keysym_t *syms;
	uint32_t modifiers;
	int nsyms;
	int i;

	nsyms = xkb_state_key_get_syms(keyboard->wlr_keyboard->xkb_state, keycode, &syms);
	modifiers = wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);
	if ((modifiers & WLR_MODIFIER_ALT) && event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		for (i = 0; i < nsyms; i++) {
			handled = handle_keybinding(server, syms[i]);
		}
	}

	if (!handled) {
		wlr_seat_set_keyboard(wlr_seat, keyboard->wlr_keyboard);
		wlr_seat_keyboard_notify_key(wlr_seat, event->time_msec,
					     event->keycode, event->state);
	}
}
