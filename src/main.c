#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_output.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_idle.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_seat.h>

struct mcw_server {
	struct wl_display *wl_display;
        struct wl_event_loop *wl_event_loop;
	struct wlr_backend *backend;
	struct wlr_compositor *compositor;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_scene *scene;

	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener new_xdg_surface;

	struct wlr_seat *seat;

	struct wlr_output_layout *output_layout;
	struct wl_listener new_output;
	struct wl_list outputs; // mcw_output::link
};

struct mcw_output {
        struct wlr_output *wlr_output;
        struct mcw_server *server;
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
        //struct wlr_output *wlr_output = data;
	struct mcw_server *server = output->server;
	//struct wlr_renderer *renderer = server->renderer;
	struct wlr_scene *scene = server->scene;

	/* Render the scene if needed and commit the output */
	struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(scene, output->wlr_output);
	wlr_scene_output_commit(scene_output);

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(scene_output, &now);
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
		wlr_output_enable(wlr_output, true);
		if (!wlr_output_commit(wlr_output)) {
			return;
		}
        }

        struct mcw_output *output = calloc(1, sizeof(struct mcw_output));
        output->server = server;
        output->wlr_output = wlr_output;
        wl_list_insert(&server->outputs, &output->link);

	output->destroy.notify = output_destroy_notify;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);

	output->frame.notify = output_frame_notify;
	wl_signal_add(&wlr_output->events.frame, &output->frame);

	wlr_output_create_global(wlr_output); // ?
	wlr_output_layout_add_auto(server->output_layout, wlr_output);

}

struct mcw_view {
	struct wl_list link;
	struct mcw_server *server;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;
	struct wl_listener destroy;
	//int x, y;
};

static void xdg_toplevel_destroy(struct wl_listener *listener, void *data) {
	/* Called when the surface is destroyed and should never be shown again. */
	struct mcw_view *view = wl_container_of(listener, view, destroy);
	wl_list_remove(&view->destroy.link);
	free(view);
}

static void server_new_xdg_surface(struct wl_listener *listener, void *data) {
	/* This event is raised when wlr_xdg_shell receives a new xdg surface from a
	 * client, either a toplevel (application window) or popup. */
	struct mcw_server *server =
		wl_container_of(listener, server, new_xdg_surface);
	struct wlr_xdg_surface *xdg_surface = data;

	/* We must add xdg popups to the scene graph so they get rendered. The
	 * wlroots scene graph provides a helper for this, but to use it we must
	 * provide the proper parent scene node of the xdg popup. To enable this,
	 * we always set the user data field of xdg_surfaces to the corresponding
	 * scene node. */
	if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
		struct wlr_xdg_surface *parent = wlr_xdg_surface_from_wlr_surface(
			xdg_surface->popup->parent);
		struct wlr_scene_tree *parent_tree = parent->data;
		xdg_surface->data = wlr_scene_xdg_surface_create(
			parent_tree, xdg_surface);
		return;
	}
	assert(xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL);

	/* Allocate a tinywl_view for this surface */
	struct mcw_view *view =
		calloc(1, sizeof(struct mcw_view));
	view->server = server;
	view->xdg_toplevel = xdg_surface->toplevel;
	view->scene_tree = wlr_scene_xdg_surface_create(
			&view->server->scene->tree, view->xdg_toplevel->base);
	view->scene_tree->node.data = view;
	xdg_surface->data = view->scene_tree;

	/* Listen to the various events it can emit */
	view->destroy.notify = xdg_toplevel_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &view->destroy);
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

	server.output_layout = wlr_output_layout_create();
	assert(server.output_layout);
	
	server.scene = wlr_scene_create();
	assert(server.scene);
	wlr_scene_attach_output_layout(server.scene, server.output_layout);

	wl_list_init(&server.outputs);
	server.new_output.notify = new_output_notify;
	wl_signal_add(&server.backend->events.new_output, &server.new_output);

	server.seat = wlr_seat_create(server.wl_display, "seat0");

	const char *socket = wl_display_add_socket_auto(server.wl_display);
	assert(socket);

        if (!wlr_backend_start(server.backend)) {
                fprintf(stderr, "Failed to start backend\n");
		wlr_backend_destroy(server.backend);
                wl_display_destroy(server.wl_display);
                return 1;
        }

        printf("Running compositor on wayland display '%s'\n", socket);
        setenv("WAYLAND_DISPLAY", socket, true);

        wl_display_init_shm(server.wl_display);
        wlr_gamma_control_manager_v1_create(server.wl_display);
        wlr_screencopy_manager_v1_create(server.wl_display);
        wlr_primary_selection_v1_device_manager_create(server.wl_display);
        wlr_idle_create(server.wl_display);
	
        server.compositor = wlr_compositor_create(server.wl_display, server.renderer);
	wlr_subcompositor_create(server.wl_display);
	wlr_data_device_manager_create(server.wl_display);

        server.xdg_shell = wlr_xdg_shell_create(server.wl_display, 3u);
	server.new_xdg_surface.notify = server_new_xdg_surface;
	wl_signal_add(&server.xdg_shell->events.new_surface,
			&server.new_xdg_surface);

	wl_display_run(server.wl_display);
	/* Once wl_display_run returns, we shut down the server. */
	wl_display_destroy_clients(server.wl_display);
        wl_display_destroy(server.wl_display);
        return 0;
 }
