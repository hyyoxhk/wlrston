#define _POSIX_C_SOURCE 200112L

#include "config.h"

#include <assert.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
#include <signal.h>
#include <linux/limits.h>
#include <dlfcn.h>

#include <wlrston.h>

static int on_term_signal(int signal_number, void *data)
{
	struct wl_display *display = data;

	wlr_log(WLR_ERROR, "caught signal %d", signal_number);
	wl_display_terminate(display);

	return 1;
}

static void sigint_helper(int sig)
{
	raise(SIGUSR2);
}

static size_t
module_path_from_env(const char *name, char *path, size_t path_len)
{
	const char *mapping = getenv("WLRSTON_MODULE_MAP");
	const char *end;
	const int name_len = strlen(name);

	if (!mapping)
		return 0;

	end = mapping + strlen(mapping);
	while (mapping < end && *mapping) {
		const char *filename, *next;

		/* early out: impossibly short string */
		if (end - mapping < name_len + 1)
			return 0;

		filename = &mapping[name_len + 1];
		next = strchrnul(mapping, ';');

		if (strncmp(mapping, name, name_len) == 0 &&
		    mapping[name_len] == '=') {
			size_t file_len = next - filename; /* no trailing NUL */
			if (file_len >= path_len)
				return 0;
			strncpy(path, filename, file_len);
			path[file_len] = '\0';
			return file_len;
		}

		mapping = next + 1;
	}

	return 0;
}


static void *
load_module_entrypoint(const char *name, const char *entrypoint, const char *module_dir)
{
	char path[PATH_MAX];
	void *module, *init;
	size_t len;

	if (name == NULL)
		return NULL;

	if (name[0] != '/') {
		len = module_path_from_env(name, path, sizeof path);
		if (len == 0)
			len = snprintf(path, sizeof path, "%s/%s",
				       module_dir, name);
	} else {
		len = snprintf(path, sizeof path, "%s", name);
	}

	/* snprintf returns the length of the string it would've written,
	 * _excluding_ the NUL byte. So even being equal to the size of
	 * our buffer is an error here. */
	if (len >= sizeof path)
		return NULL;

	module = dlopen(path, RTLD_NOW | RTLD_NOLOAD);
	if (module) {
		wlr_log(WLR_ERROR, "Module '%s' already loaded\n", path);
	} else {
		wlr_log(WLR_ERROR, "Loading module '%s'\n", path);
		module = dlopen(path, RTLD_NOW);
		if (!module) {
			wlr_log(WLR_ERROR, "Failed to load module: %s\n", dlerror());
			return NULL;
		}
	}

	init = dlsym(module, entrypoint);
	if (!init) {
		wlr_log(WLR_ERROR, "Failed to lookup init function: %s\n", dlerror());
		dlclose(module);
		return NULL;
	}

	return init;
}

static int
load_shell(struct wlrston_server *server, const char *name, int *argc, char *argv[])
{
	int (*shell_init)(struct wlrston_server *server,
			  int *argc, char *argv[]);

	shell_init = load_module_entrypoint(name, "wlrston_shell_init", SHELLDIR);
	if (!shell_init)
		return -1;
	if (shell_init(server, argc, argv) < 0)
		return -1;
	return 0;
}

int main(int argc, char *argv[])
{
	char *startup_cmd = NULL;
	struct wlrston_server *server;
	struct wl_display *display;
	struct wl_event_source *signals[2];
	struct wl_event_loop *loop;
	struct sigaction action;
	int i;
	int c;

	wlr_log_init(WLR_DEBUG, NULL);

	while ((c = getopt(argc, argv, "s:h")) != -1) {
		switch (c) {
		case 's':
			startup_cmd = optarg;
			break;
		default:
			printf("Usage: %s [-s startup command]\n", argv[0]);
			return 0;
		}
	}
	if (optind < argc) {
		printf("Usage: %s [-s startup command]\n", argv[0]);
		return 0;
	}

	display = wl_display_create();
	if (display == NULL) {
		wlr_log(WLR_ERROR,"fatal: failed to create display\n");
		goto out_display;
	}

	loop = wl_display_get_event_loop(display);
	signals[0] = wl_event_loop_add_signal(loop, SIGTERM, on_term_signal,
					      display);
	signals[1] = wl_event_loop_add_signal(loop, SIGUSR2, on_term_signal,
					      display);

	action.sa_handler = sigint_helper;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	sigaction(SIGINT, &action, NULL);
	if (!signals[0] || !signals[1])
		goto out_signals;

	server = server_create(display);
	if (!server) {
		goto out_signals;
	}

	if (!server_start(server))
		goto out;

	/* Add a Unix socket to the Wayland display. */
	const char *socket = wl_display_add_socket_auto(display);
	if (!socket)
		goto out;

	// load_shell
	load_shell(server, "desktop-shell.so", &argc, argv);

	setenv("WAYLAND_DISPLAY", socket, true);
	if (startup_cmd) {
		if (fork() == 0) {
			execl("/bin/sh", "/bin/sh", "-c", startup_cmd, (void *)NULL);
		}
	}

	wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s",
			socket);
	wl_display_run(display);

out:
	server_destroy(server);
	wl_display_destroy(display);

out_signals:
	for (i = 1; i >= 0; i--)
		if (signals[i])
			wl_event_source_remove(signals[i]);

out_display:
	return 0;
}
