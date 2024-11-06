// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#include <stdlib.h>
#include <wayland-util.h>

#include <wlrston.h>
#include <view.h>

void keyboard_handle_modifiers(struct wl_listener *listener, void *data) {
	/* This event is raised when a modifier key, such as shift or alt, is
	 * pressed. We simply communicate this to the client. */
	struct wlrston_keyboard *keyboard =
		wl_container_of(listener, keyboard, modifiers);
	struct wlrston_seat *seat = keyboard->base.seat;
	struct wlr_seat *wlr_seat = seat->seat;

	/*
	 * A seat can only have one keyboard, but this is a limitation of the
	 * Wayland protocol - not wlroots. We assign all connected keyboards to the
	 * same seat. You can swap out the underlying wlr_keyboard like this and
	 * wlr_seat handles this transparently.
	 */
	wlr_seat_set_keyboard(wlr_seat, keyboard->wlr_keyboard);
	/* Send modifiers to the client. */
	wlr_seat_keyboard_notify_modifiers(wlr_seat,
		&keyboard->wlr_keyboard->modifiers);
}

static bool handle_keybinding(struct wlrston_server *server, xkb_keysym_t sym) {
	/*
	 * Here we handle compositor keybindings. This is when the compositor is
	 * processing keys, rather than passing them on to the client for its own
	 * processing.
	 *
	 * This function assumes Alt is held down.
	 */
	switch (sym) {
	case XKB_KEY_Escape:
		wl_display_terminate(server->wl_display);
		break;
	case XKB_KEY_F1:
		/* Cycle to the next view */
		if (wl_list_length(&server->views) < 2) {
			break;
		}
		struct wlrston_view *next_view = wl_container_of(
			server->views.prev, next_view, link);
		focus_view(next_view, next_view->xdg_toplevel->base->surface);
		break;
	default:
		return false;
	}
	return true;
}

void keyboard_handle_key(struct wl_listener *listener, void *data) {
	/* This event is raised when a key is pressed or released. */
	struct wlrston_keyboard *keyboard =
		wl_container_of(listener, keyboard, key);
	struct wlrston_seat *seat = keyboard->base.seat;
	struct wlr_seat *wlr_seat = seat->seat;
	struct wlr_keyboard_key_event *event = data;
	struct wlrston_server *server = seat->server;

	/* Translate libinput keycode -> xkbcommon */
	uint32_t keycode = event->keycode + 8;
	/* Get a list of keysyms based on the keymap for this keyboard */
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(
			keyboard->wlr_keyboard->xkb_state, keycode, &syms);

	bool handled = false;
	uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);
	if ((modifiers & WLR_MODIFIER_ALT) &&
			event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		/* If alt is held down and this button was _pressed_, we attempt to
		 * process it as a compositor keybinding. */
		for (int i = 0; i < nsyms; i++) {
			handled = handle_keybinding(server, syms[i]);
		}
	}

	if (!handled) {
		/* Otherwise, we pass it along to the client. */
		wlr_seat_set_keyboard(wlr_seat, keyboard->wlr_keyboard);
		wlr_seat_keyboard_notify_key(wlr_seat, event->time_msec,
			event->keycode, event->state);
	}
}

// void keyboard_handle_destroy(struct wl_listener *listener, void *data) {
// 	/* This event is raised by the keyboard base wlr_input_device to signal
// 	 * the destruction of the wlr_keyboard. It will no longer receive events
// 	 * and should be destroyed.
// 	 */
// 	struct wlrston_keyboard *keyboard =
// 		wl_container_of(listener, keyboard, destroy);
// 	wl_list_remove(&keyboard->modifiers.link);
// 	wl_list_remove(&keyboard->key.link);
// 	wl_list_remove(&keyboard->destroy.link);
// 	wl_list_remove(&keyboard->link);
// 	free(keyboard);
// }
