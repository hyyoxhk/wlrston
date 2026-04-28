srcs_shell_desktop = [
	'desktop-shell.c',
	'xdg.c',
	xdg_shell_protocol_h,
	xdg_shell_protocol_c,
]

deps_shell_desktop = [
	dep_wayland_server,
	dep_wlroots,
]

shared_module(
	'desktop-shell',
	srcs_shell_desktop,
	include_directories: inc_wlrston,
	dependencies: deps_shell_desktop,
	name_prefix: '',
	install: true,
	install_dir: dir_shell,
)
