#define _DEFAULT_SOURCE

#include <stdio.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <wayland-protocols/xdg-shell-enum.h>
#include <xdg-shell-client-protocol.h>

struct wl_compositor *compositor;
struct wl_shm *shm;
struct wl_seat *seat;
struct wl_shell *shell;
struct xdg_wm_base *xdg_wm_base;
struct wl_pointer *pointer;

void registry_global_handler(void *data, struct wl_registry *registry, uint32_t name, const char *interface,
                             uint32_t version) {
    (void)data;
    (void)version;
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 3);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, wl_shell_interface.name) == 0) {
        shell = wl_registry_bind(registry, name, &wl_shell_interface, 1);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
    } else {
        // printf("Unknown global: %s\n", interface);
    }
}

void registry_global_remove_handler(void *data, struct wl_registry *registry, uint32_t name) {
    (void)data;
    (void)registry;
    (void)name;
    // printf("removed: %u\n", name);
}

void xdg_toplevel_configure_handler(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height,
                                    struct wl_array *states) {
    (void)data;
    (void)xdg_toplevel;
    (void)states;
    (void)width;
    (void)height;
    // printf("configure: %dx%d\n", width, height);
}

void xdg_toplevel_close_handler(void *data, struct xdg_toplevel *xdg_toplevel) {
    (void)data;
    (void)xdg_toplevel;
    printf("close\n");
}

const struct xdg_toplevel_listener xdg_toplevel_listener = {.configure = xdg_toplevel_configure_handler,
                                                            .close     = xdg_toplevel_close_handler};

void xdg_surface_configure_handler(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
    (void)data;
    xdg_surface_ack_configure(xdg_surface, serial);
}

const struct xdg_surface_listener xdg_surface_listener = {.configure = xdg_surface_configure_handler};

const struct wl_registry_listener registry_listener = {.global        = registry_global_handler,
                                                       .global_remove = registry_global_remove_handler};

void xdg_wm_base_handler(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
    (void)data;
    xdg_wm_base_pong(xdg_wm_base, serial);
    // printf("ping-pong\n");
}

const struct xdg_wm_base_listener xdg_wm_base_listener = {.ping = xdg_wm_base_handler};

struct wl_surface *cursor_surface;
struct wl_cursor_image *cursor_image;

void pointer_enter_handler(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface,
                           wl_fixed_t x, wl_fixed_t y) {
    (void)data;
    (void)surface;
    (void)x;
    (void)y;
    if (cursor_surface) {
        wl_pointer_set_cursor(pointer, serial, cursor_surface, cursor_image->hotspot_x, cursor_image->hotspot_y);
    }
}

void pointer_leave_handler(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface) {
    (void)data;
    (void)pointer;
    (void)serial;
    (void)surface;
}

void pointer_motion_handler(void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t x, wl_fixed_t y) {
    (void)data;
    (void)pointer;
    (void)time;
    (void)x;
    (void)y;
}

void pointer_button_handler(void *data, struct wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t button,
                            uint32_t state) {
    (void)data;
    (void)pointer;
    (void)serial;
    (void)time;
    (void)button;
    (void)state;
}

void pointer_axis_handler(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t value) {
    (void)data;
    (void)pointer;
    (void)time;
    (void)axis;
    (void)value;
}

const struct wl_pointer_listener pointer_listener = {.enter  = pointer_enter_handler,
                                                     .leave  = pointer_leave_handler,
                                                     .motion = pointer_motion_handler,
                                                     .button = pointer_button_handler,
                                                     .axis   = pointer_axis_handler};

void draw(unsigned char *data, int width, int height, int stride) {
    // fill the buffer with a red square
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            struct {
                unsigned char b;
                unsigned char g;
                unsigned char r;
                unsigned char a;
            } *px = (void *)(data + y * stride + x * 4);
            if ((x + y) % 30 < 10) {
                // transparent
                px->a = 0;
            } else if ((x + y) % 30 < 20) {
                // yellow
                px->a = 255;
                px->r = 255;
                px->g = 255;
                px->b = 0;
            } else {
                // semitransparent red
                px->a = 128;
                px->r = 255;
                px->g = 0;
                px->b = 0;
            }
        }
    }
}

int main(void) {
    struct wl_display *display   = wl_display_connect(NULL);
    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);

    // wait for the "initial" set of globals to appear
    wl_display_roundtrip(display);

    // all our objects should be ready!
    if (!compositor) {
        fprintf(stderr, "wl_compositor not available\n");
        return 1;
    }
    if (!shm) {
        fprintf(stderr, "wl_shm not available\n");
        return 1;
    }

    struct wl_surface *surface = wl_compositor_create_surface(compositor);
    if (xdg_wm_base) {
        struct xdg_surface *xdg_surface   = xdg_wm_base_get_xdg_surface(xdg_wm_base, surface);
        struct xdg_toplevel *xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);
        xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);
        xdg_toplevel_set_title(xdg_toplevel, "Hello Wayland");
        xdg_toplevel_set_app_id(xdg_toplevel, "com.example.hellowayland");
        xdg_toplevel_add_listener(xdg_toplevel, &xdg_toplevel_listener, NULL);
        // signal that the surface is ready to be configured
        wl_surface_commit(surface);
    } else if (shell) {
        struct wl_shell_surface *shell_surface = wl_shell_get_shell_surface(shell, surface);
        wl_shell_surface_set_toplevel(shell_surface);
    } else {
        fprintf(stderr, "No wl_shell or xdg_wm_base available\n");
        return 1;
    }

    int width  = 200;
    int height = 200;
    int stride = width * 4;
    int size   = stride * height; // bytes
    // open an anonymous file and write some zero bytes to it
    int fd     = syscall(SYS_memfd_create, "buffer", 0);
    ftruncate(fd, size);

    // map it to the memory
    unsigned char *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    draw(data, width, height, stride);

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);

    // allocate the buffer in that pool
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);

    wl_display_roundtrip(display);

    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_commit(surface);

    pointer = wl_seat_get_pointer(seat);
    if (pointer) {
        wl_pointer_add_listener(pointer, &pointer_listener, NULL);

        struct wl_cursor_theme *cursor_theme = wl_cursor_theme_load(NULL, 24, shm);
        struct wl_cursor *cursor             = wl_cursor_theme_get_cursor(cursor_theme, "left_ptr");
        cursor_image                         = cursor->images[0];
        struct wl_buffer *cursor_buffer      = wl_cursor_image_get_buffer(cursor_image);

        cursor_surface = wl_compositor_create_surface(compositor);
        wl_surface_attach(cursor_surface, cursor_buffer, 0, 0);
        wl_surface_commit(cursor_surface);
    }

    if (xdg_wm_base) {
        wl_display_roundtrip(display);
        xdg_wm_base_add_listener(xdg_wm_base, &xdg_wm_base_listener, NULL);
    } else if (shell) {
        // TODO
    }

    while (1) {
        wl_display_dispatch(display);
    }
}
