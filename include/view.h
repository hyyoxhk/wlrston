// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2024 He Yong <hyyoxhk@163.com>
 */

#ifndef VIEW_H
#define VIEW_H

#include <wlrston-plugin.h>

void map_view(struct wlrston_view *view);
void unmap_view(struct wlrston_view *view);
void focus_view(struct wlrston_view *view, struct wlr_surface *surface);
void begin_interactive_view(struct wlrston_view *view,
			    enum wlrston_cursor_mode mode, uint32_t edges);

#endif
