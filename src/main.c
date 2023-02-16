#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_output.h>
#include <wlr/render/allocator.h>

struct mcw_server {
	struct wl_display *wl_display;
        struct wl_event_loop *wl_event_loop;
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wl_listener new_output;
	struct wl_list outputs; // mcw_output::link
};

struct mcw_output {
        struct wlr_output *wlr_output;
        struct mcw_server *server;
        struct timespec last_frame;
	struct wl_listener destroy;
	struct wl_listener frame;
        struct wl_list link;
};

static void output_destroy_notify(struct wl_listener *listener, void *data) {
         struct mcw_output *output = wl_container_of(listener, output, destroy);
         wl_list_remove(&output->link);
         wl_list_remove(&output->destroy.link);
         wl_list_remove(&output->frame.link);
         free(output);
}

static void output_frame_notify(struct wl_listener *listener, void *data) {
        struct mcw_output *output = wl_container_of(listener, output, frame);
        struct wlr_output *wlr_output = data;
	struct wlr_renderer *renderer = output->server->renderer;

        wlr_output_attach_render(wlr_output, NULL);
	// makes the output’s OpenGL context “current”
        wlr_renderer_begin(renderer, wlr_output->width, wlr_output->height);

        float color[4] = {0.3, 0.3, 0.4, 1};
	wlr_renderer_clear(renderer, color);

        wlr_renderer_end(renderer);
        wlr_output_commit(wlr_output);
}

static void new_output_notify(struct wl_listener *listener, void *data) {
        struct mcw_server *server = wl_container_of(
                        listener, server, new_output);
        struct wlr_output *wlr_output = data;

	/* Configures the output created by the backend to use our allocator
     * and our renderer. Must be done once, before commiting the output */
	wlr_output_init_render(wlr_output, server->allocator, server->renderer);

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

	output->frame.notify = output_frame_notify;
	wl_signal_add(&wlr_output->events.frame, &output->frame);
}


int main(int argc, char **argv) {
	struct mcw_server server;

        server.wl_display = wl_display_create();
        assert(server.wl_display);
	server.wl_event_loop = wl_display_get_event_loop(server.wl_display);
        assert(server.wl_event_loop);
	server.backend = wlr_backend_autocreate(server.wl_display);
	assert(server.backend);
	server.renderer = wlr_renderer_autocreate(server.backend);
	assert(server.renderer);
	server.allocator = wlr_allocator_autocreate(server.backend, server.renderer);
	assert(server.allocator);

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
