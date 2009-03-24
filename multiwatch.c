
#include <glib.h>
#include <ev.h>

#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <errno.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define UNUSED(x) ((void)(x))

#define PACKAGE_DESC (PACKAGE_NAME "-" PACKAGE_VERSION " - forks and watches multiple instances of a program in the same environment")

typedef struct {
	gchar **app;

	gint forks;

	/* how many times we try to spawn a child */
	gint retry;

	/* time within a dieing child is handled as "spawn failed"
	 * if it dies after the timeout, the retry counter is reset and
	 * we try to get it up again
	 */
	gint retry_timeout_ms;

	gboolean show_version;
} options;

struct data;
typedef struct data data;

struct child;
typedef struct child child;

struct child {
	data *d;
	int id;
	pid_t pid;
	gint tries;
	ev_tstamp last_spawn;
	ev_child watcher;
};

struct data {
	child *childs;
	guint running;
	gboolean shutdown;
	struct ev_loop *loop;
	ev_signal sigHUP, sigINT, sigQUIT, sigTERM, sigUSR1, sigUSR2;
	gint return_status;
};

static options opts = {
	NULL,
	1,
	3,
	10000,
	FALSE
};

static void forward_sig_cb(struct ev_loop *loop, ev_signal *w, int revents) {
	data *d = (data*) w->data;
	UNUSED(loop);
	UNUSED(revents);

	for (gint i = 0; i < opts.forks; i++) {
		if (d->childs[i].pid != -1) {
			kill(d->childs[i].pid, w->signum);
		}
	}
}

static void terminate_forward_sig_cb(struct ev_loop *loop, ev_signal *w, int revents) {
	data *d = (data*) w->data;
	UNUSED(loop);
	UNUSED(revents);

	d->shutdown = TRUE;

	for (gint i = 0; i < opts.forks; i++) {
		if (d->childs[i].pid != -1) {
			kill(d->childs[i].pid, w->signum);
		}
	}
}

static void spawn(child* c) {
	pid_t pid;

	if (c->tries++ > opts.retry) {
		g_printerr("Child[%i] died to often, not forking again\n", c->id);
		return;
	}

	switch (pid = fork()) {
	case -1:
		g_printerr("Fatal Error: Couldn't fork child[%i]: %s\n", c->id, g_strerror(errno));
		if (0 == c->d->running) {
			g_printerr("No child running and fork failed -> exit\n");
			c->d->return_status = -100;
			ev_unloop(c->d->loop, EVUNLOOP_ALL);
		}
		/* Do not retry... */
		break;
	case 0:
		/* child */
		execv(opts.app[0], opts.app);
		g_printerr("Exec failed: %s\n", g_strerror(errno));
		exit(errno);
		break;
	default:
		c->pid = pid;
		c->d->running++;
		c->last_spawn = ev_now(c->d->loop);
		ev_child_set(&c->watcher, c->pid, 0);
		ev_child_start(c->d->loop, &c->watcher);
		break;
	}
}

static void child_died(struct ev_loop *loop, ev_child *w, int revents) {
	child *c = (child*) w->data;
	UNUSED(revents);

	ev_child_stop(loop, w);
	c->d->running--;
	c->pid = -1;

	if (c->d->shutdown) return;

	if (ev_now(c->d->loop) - c->last_spawn > (opts.retry_timeout_ms / (ev_tstamp) 1000)) {
		g_printerr("Child[%i] died, respawn\n", c->id);
		c->tries = 0;
	} else {
		g_printerr("Spawing child[%i] failed, next try\n", c->id);
	}

	spawn(c);
}

static const GOptionEntry entries[] = {
	{ "forks", 'f', 0, G_OPTION_ARG_INT, &opts.forks, "Number of childs to fork and watch(default 1)", "childs" },
	{ "retry", 'r', 0, G_OPTION_ARG_INT, &opts.retry, "Number of retries to fork a single child", "retries" },
	{ "timeout", 't', 0, G_OPTION_ARG_INT, &opts.retry_timeout_ms, "Retry timeout in ms; if the child dies after the timeout the retry counter is reset", "ms" },
	{ "version", 'v', 0, G_OPTION_ARG_NONE, &opts.show_version, "Show version", NULL },
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &opts.app, "<application> [app arguments]", NULL },
	{ NULL, 0, 0, 0, NULL, NULL, NULL }
};

int main(int argc, char **argv) {
	GOptionContext *context;
	GError *error = NULL;
	gint res;

	context = g_option_context_new("<application> [app arguments]");
	g_option_context_add_main_entries(context, entries, NULL);
	g_option_context_set_summary(context, PACKAGE_DESC);

	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_printerr("Option parsing failed: %s\n", error->message);
		return -1;
	}

	if (opts.show_version) {
		g_printerr(PACKAGE_DESC);
		g_printerr("\nBuild-Date: " __DATE__ " " __TIME__ "\n");
		return 0;
	}

	if (!opts.app || !opts.app[0]) {
		g_printerr("Missing application\n");
		return -2;
	}

	if (opts.forks < 1) {
		g_printerr("Invalid forks argument: %i\n", opts.forks);
		return -3;
	}

	if (opts.retry < 1) {
		g_printerr("Invalid retry argument: %i\n", opts.retry);
		return -4;
	}

	if (opts.retry_timeout_ms < 0) {
		g_printerr("Invalid timeout argument: %i\n", opts.retry_timeout_ms);
		return -5;
	}

	data *d = g_slice_new0(data);
	d->childs = (child*) g_slice_alloc0(sizeof(child) * opts.forks);
	d->running = 0;
	d->shutdown = FALSE;
	d->return_status = 0;
	d->loop = ev_default_loop(0);

#define WATCH_SIG(x) do { ev_signal_init(&d->sig##x, forward_sig_cb, SIG##x); d->sig##x.data = d; ev_signal_start(d->loop, &d->sig##x); ev_unref(d->loop); } while (0)
#define WATCH_TERM_SIG(x) do { ev_signal_init(&d->sig##x, terminate_forward_sig_cb, SIG##x); d->sig##x.data = d; ev_signal_start(d->loop, &d->sig##x); ev_unref(d->loop); } while (0)
#define UNWATCH_SIG(x) do { ev_ref(d->loop); ev_signal_stop(d->loop, &d->sig##x); } while (0)

	WATCH_TERM_SIG(HUP);
	WATCH_TERM_SIG(INT);
	WATCH_TERM_SIG(QUIT);
	WATCH_TERM_SIG(TERM);
	WATCH_SIG(USR1);
	WATCH_SIG(USR2);

	for (gint i = 0; i < opts.forks; i++) {
		d->childs[i].d = d;
		d->childs[i].id = i;
		d->childs[i].pid = -1;
		d->childs[i].tries = 0;
		d->childs[i].watcher.data = &d->childs[i];
		ev_child_init(&d->childs[i].watcher, child_died, -1, 0);

		spawn(&d->childs[i]);
	}

	ev_loop(d->loop, 0);

	res = d->return_status;

	g_slice_free1(sizeof(child) * opts.forks, d->childs);
	g_slice_free(data, d);

	UNWATCH_SIG(HUP);
	UNWATCH_SIG(INT);
	UNWATCH_SIG(QUIT);
	UNWATCH_SIG(TERM);
	UNWATCH_SIG(USR1);
	UNWATCH_SIG(USR2);

	return res;
}
