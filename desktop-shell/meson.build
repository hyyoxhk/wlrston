if get_option('shell-desktop')
	config_h.set_quoted('WESTON_SHELL_CLIENT', get_option('desktop-shell-client-default'))

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
	shared_library(
		'desktop-shell',
		srcs_shell_desktop,
		include_directories: inc_wlrston,
		dependencies: deps_shell_desktop,
		link_with: lib_wlrston,
		name_prefix: '',
	install: true,
	install_dir: dir_shell,
	)
endif
