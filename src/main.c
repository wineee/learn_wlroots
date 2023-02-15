#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_output.h>

struct mcw_server {
	struct wl_display *wl_display;
        struct wl_event_loop *wl_event_loop;
	struct wlr_backend *backend;
	struct wl_listener new_output;
	struct wl_list outputs; // mcw_output::link
};

struct mcw_output {
        struct wlr_output *wlr_output;
        struct mcw_server *server;
        struct timespec last_frame;
	struct wl_listener destroy;
        struct wl_list link;
};

static void output_destroy_notify(struct wl_listener *listener, void *data) {
         struct mcw_output *output = wl_container_of(listener, output, destroy);
         wl_list_remove(&output->link);
         wl_list_remove(&output->destroy.link);
         // wl_list_remove(&output->frame.link);
         free(output);
}

static void new_output_notify(struct wl_listener *listener, void *data) {
        struct mcw_server *server = wl_container_of(
                        listener, server, new_output);
        struct wlr_output *wlr_output = data;

        if (!wl_list_empty(&wlr_output->modes)) {
                struct wlr_output_mode *mode =
                        wl_container_of(wlr_output->modes.prev, mode, link);
                wlr_output_set_mode(wlr_output, mode);
        }

        struct mcw_output *output = calloc(1, sizeof(struct mcw_output));
        clock_gettime(CLOCK_MONOTONIC, &output->last_frame);
        output->server = server;
        output->wlr_output = wlr_output;
        wl_list_insert(&server->outputs, &output->link);

	output->destroy.notify = output_destroy_notify;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);
}


int main(int argc, char **argv) {
	struct mcw_server server;

        server.wl_display = wl_display_create();
        assert(server.wl_display);
	server.wl_event_loop = wl_display_get_event_loop(server.wl_display);
        assert(server.wl_event_loop);
	server.backend = wlr_backend_autocreate(server.wl_display);
	assert(server.backend);

	wl_list_init(&server.outputs);
	server.new_output.notify = new_output_notify;
	wl_signal_add(&server.backend->events.new_output, &server.new_output);

        if (!wlr_backend_start(server.backend)) {
                fprintf(stderr, "Failed to start backend\n");
                wl_display_destroy(server.wl_display);
                return 1;
        }
        wl_display_run(server.wl_display);
        wl_display_destroy(server.wl_display);
        return 0;
 }
