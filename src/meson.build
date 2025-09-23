srcs_wlrston = [
	'main.c',
]

deps_wlrston = [
	dep_wayland_server,
	dep_wlroots,
	dep_xkbcommon,
	dep_pixman,
]

lib_srcs_wlrston = [
	'server.c',
	'output.c',
	'seat.c',
	'keyboard.c',
	'cursor.c',
	'view.c',
	xdg_shell_protocol_h,
	xdg_shell_protocol_c,
]

lib_wlrston = shared_library(
	'wlrston',
	lib_srcs_wlrston,
	include_directories: inc_wlrston,
	dependencies: deps_wlrston,
	install: true,
)

executable(
	'wlrston',
	sources: srcs_wlrston,
	include_directories: inc_wlrston,
	dependencies: deps_wlrston,
	link_with: lib_wlrston,
)
