#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>

struct mcw_server {
	struct wl_display *wl_display;
        struct wl_event_loop *wl_event_loop;
	struct wlr_backend *backend;
};

int main(int argc, char **argv) {
	struct mcw_server server;

        server.wl_display = wl_display_create();
        assert(server.wl_display);
        server.wl_event_loop = wl_display_get_event_loop(server.wl_display);
        assert(server.wl_event_loop);
	server.backend = wlr_backend_autocreate(server.wl_display);
	assert(server.backend);
        return 0;
 }
