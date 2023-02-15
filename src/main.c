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

static void new_output_notify(struct wl_listener *listener, void *data) {
	
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
