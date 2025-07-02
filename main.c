// SPDX-License-Identifier: GPL-2.0 OR BSL-1.0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <wayland-client-protocol.h>
#include <wayland-client.h>
#include <wayland-cursor.h>

#include "cursor-shape-v1-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

struct SimpleClient {
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct wl_seat *seat;
    struct xdg_wm_base *xdg_wm_base;
    struct wp_cursor_shape_manager_v1 *wp_cursor_shape_manager_v1;
    struct zxdg_decoration_manager_v1 *zxdg_decoration_manager_v1;
    struct wl_surface *cursor_surface;
    struct wl_cursor_image *cursor_image;
    struct wl_surface *surface;
    struct wl_buffer *buffer;
    struct xdg_toplevel *xdg_toplevel;
    struct xdg_surface *xdg_surface;
    struct wl_shm_pool *shm_pool;

    unsigned char *shm_data;

    float pointer_pos_x;
    float pointer_pos_y;
    int use_server_side_decoration;
    int should_close;
};

static void registry_global_handler(void *data, struct wl_registry *registry, uint32_t name, const char *interface,
                                    uint32_t version) {
    (void)version;
    struct SimpleClient *client = data;
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        client->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 3);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        client->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        client->xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        client->seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
    } else if (strcmp(interface, wp_cursor_shape_manager_v1_interface.name) == 0) {
        client->wp_cursor_shape_manager_v1 = wl_registry_bind(registry, name, &wp_cursor_shape_manager_v1_interface, 1);
    } else if (strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0) {
        client->zxdg_decoration_manager_v1 = wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface, 1);
    } else {
        // printf("Unknown global: %s\n", interface);
    }
    // printf("interface: %s, name: %d, version: %d\n", interface, name, version);
}

static void registry_global_remove_handler(void *data, struct wl_registry *registry, uint32_t name) {
    (void)data;
    (void)registry;
    (void)name;
    // printf("removed: %u\n", name);
}

static void xdg_toplevel_configure_handler(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height,
                                           struct wl_array *states) {
    (void)data;
    (void)xdg_toplevel;
    (void)states;
    (void)width;
    (void)height;
    // printf("configure: %dx%d\n", width, height);
}

static void xdg_toplevel_close_handler(void *data, struct xdg_toplevel *xdg_toplevel) {
    (void)data;
    (void)xdg_toplevel;
    // printf("close\n");

    struct SimpleClient *client = data;
    client->should_close        = 1;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_configure_handler,
    .close     = xdg_toplevel_close_handler,
};

static void xdg_surface_configure_handler(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
    (void)data;
    xdg_surface_ack_configure(xdg_surface, serial);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure_handler,
};

static const struct wl_registry_listener registry_listener = {
    .global        = registry_global_handler,
    .global_remove = registry_global_remove_handler,
};

static void xdg_wm_base_handler(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
    (void)data;
    xdg_wm_base_pong(xdg_wm_base, serial);
    // printf("ping-pong\n");
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_handler,
};

static void pointer_enter_handler(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface,
                                  wl_fixed_t x, wl_fixed_t y) {
    (void)surface;

    struct SimpleClient *client = data;
    client->pointer_pos_x       = wl_fixed_to_double(x);
    client->pointer_pos_y       = wl_fixed_to_double(y);
    if (client->wp_cursor_shape_manager_v1) {
        struct wp_cursor_shape_device_v1 *device =
            wp_cursor_shape_manager_v1_get_pointer(client->wp_cursor_shape_manager_v1, pointer);
        wp_cursor_shape_device_v1_set_shape(device, serial, WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT);
    } else if (client->cursor_surface) {
        wl_pointer_set_cursor(pointer, serial, client->cursor_surface, client->cursor_image->hotspot_x,
                              client->cursor_image->hotspot_y);
    }
}

static void pointer_leave_handler(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface) {
    (void)pointer;
    (void)serial;
    (void)surface;

    struct SimpleClient *client = data;
    client->pointer_pos_x       = -1.0f;
    client->pointer_pos_y       = -1.0f;
}

static void pointer_motion_handler(void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t x, wl_fixed_t y) {
    (void)pointer;
    (void)time;

    struct SimpleClient *client = data;
    client->pointer_pos_x       = wl_fixed_to_double(x);
    client->pointer_pos_y       = wl_fixed_to_double(y);
}

#ifndef BTN_LEFT
#define BTN_LEFT 0x110
#endif

const int width  = 400;
const int height = 400;

struct rect_t {
    float x;
    float y;
    float width;
    float height;
};

static const struct rect_t close_button_area = {width - 20, 0.0f, 20, 20};
static const struct rect_t title_bar_area    = {0.0f, 0.0f, width, 20};

static int in_rect(const struct rect_t *rect, float x, float y) {
    return (rect->x <= x && x < rect->x + rect->width && rect->y <= y && y < rect->y + rect->height);
}

static void pointer_button_handler(void *data, struct wl_pointer *pointer, uint32_t serial, uint32_t time,
                                   uint32_t button, uint32_t state) {
    (void)pointer;
    (void)serial;
    (void)time;

    struct SimpleClient *client = data;
    float pointer_pos_x         = client->pointer_pos_x;
    float pointer_pos_y         = client->pointer_pos_y;

    if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED) {
        // printf("Left button pressed\n");
        if (!client->use_server_side_decoration) {
            if (in_rect(&close_button_area, pointer_pos_x, pointer_pos_y)) {
                // printf("click x\n");
                client->should_close = 1;
                return;
            } else if (in_rect(&title_bar_area, pointer_pos_x, pointer_pos_y)) {
                // printf("move window\n");
                xdg_toplevel_move(client->xdg_toplevel, client->seat, serial);
                return;
            }
        }
    }
}

static void pointer_axis_handler(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis,
                                 wl_fixed_t value) {
    (void)data;
    (void)pointer;
    (void)time;
    (void)axis;
    (void)value;
}

static const struct wl_pointer_listener pointer_listener = {
    .enter  = pointer_enter_handler,
    .leave  = pointer_leave_handler,
    .motion = pointer_motion_handler,
    .button = pointer_button_handler,
    .axis   = pointer_axis_handler,
};

struct pixel_t {
    unsigned char b;
    unsigned char g;
    unsigned char r;
    unsigned char a;
};

static void draw_rect(unsigned char *data, int width, int height, int stride, const struct rect_t *rect,
                      const struct pixel_t *color) {
    for (int y = rect->y; y < rect->y + rect->height && y < height; y++) {
        for (int x = rect->x; x < rect->x + rect->width && x < width; x++) {
            struct pixel_t *px = (struct pixel_t *)(data + y * stride + x * 4);
            if (x >= 0 && y >= 0 && x < width && y < height) {
                px->a = color->a;
                px->r = color->r;
                px->g = color->g;
                px->b = color->b;
            }
        }
    }
}

static void draw_decoration(unsigned char *data, int width, int height, int stride) {
    // gray title bar and red close button
    draw_rect(data, width, height, stride, &title_bar_area, &(struct pixel_t){.a = 255, .r = 120, .g = 120, .b = 120});
    draw_rect(data, width, height, stride, &close_button_area, &(struct pixel_t){.a = 255, .r = 255, .g = 0, .b = 0});
    // draw a white cross on the close button
    int w     = (int)close_button_area.width;
    int pad   = 2;
    int x_top = w - pad;
    int y_top = x_top;
    for (int y = 0; y < close_button_area.height; y++) {
        for (int x = 0; x < close_button_area.width; x++) {
            struct pixel_t *px =
                (struct pixel_t *)(data + (y + (int)close_button_area.y) * stride + (x + (int)close_button_area.x) * 4);
            if (pad < x && x < x_top && pad < y && y < y_top && (x == y || x + y == w)) {
                // white
                px->a = 255;
                px->r = 255;
                px->g = 255;
                px->b = 255;
            }
        }
    }
}

static void draw(struct SimpleClient *client, int width, int height, int stride) {
    int64_t t = clock() / (CLOCKS_PER_SEC / 1000);
    // printf("t=%ld\n", t);

    unsigned char *data = client->shm_data;

    // fill the buffer with a red square
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            struct pixel_t *px = (struct pixel_t *)(data + y * stride + x * 4);
            if ((x + y + t) % 30 < 10) {
                // transparent
                px->a = 0;
            } else if ((x + y + t) % 30 < 20) {
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

    if (!client->use_server_side_decoration) {
        draw_decoration(data, width, height, stride);
    }
}

const int stride = width * 4;
const int size   = stride * height; // bytes

static void surface_request_frame(struct SimpleClient *client);

static void callback_done_handler(void *data, struct wl_callback *callback, uint32_t time) {
    (void)time;
    struct SimpleClient *client = data;
    // printf("done!\n");
    if (callback) {
        wl_callback_destroy(callback);
    }
    surface_request_frame(client);
    draw(client, width, height, stride);
    wl_surface_attach(client->surface, client->buffer, 0, 0);
    wl_surface_damage(client->surface, 0, 0, width, height);
    wl_surface_commit(client->surface);
}

static const struct wl_callback_listener callback_listener = {
    .done = callback_done_handler,
};

static void surface_request_frame(struct SimpleClient *client) {
    struct wl_callback *callback = wl_surface_frame(client->surface);
    wl_callback_add_listener(callback, &callback_listener, client);
}

static int args_match(int argc, char *argv[], const char *arg) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], arg) == 0) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    struct SimpleClient *client  = calloc(1, sizeof(struct SimpleClient));
    if (!client) {
        fprintf(stderr, "Failed to allocate memory for SimpleClient\n");
        return 1;
    }
    client->use_server_side_decoration = !args_match(argc, argv, "--no-server-side-decoration");

    struct wl_display *display   = wl_display_connect(NULL);
    struct wl_registry *registry = wl_display_get_registry(display);
    if (!display || !registry) {
        fprintf(stderr, "Failed to connect to Wayland display or registry\n");
        return 1;
    }
    wl_registry_add_listener(registry, &registry_listener, client);

    // wait for the "initial" set of globals to appear
    wl_display_roundtrip(display);

    // all our objects should be ready!
    if (!client->compositor) {
        fprintf(stderr, "wl_compositor not available\n");
        return 1;
    }
    if (!client->shm) {
        fprintf(stderr, "wl_shm not available\n");
        return 1;
    }

    if (client->xdg_wm_base) {
        wl_display_roundtrip(display);
        xdg_wm_base_add_listener(client->xdg_wm_base, &xdg_wm_base_listener, client);
    } else {
        fprintf(stderr, "xdg_wm_base not available\n");
        return 1;
    }

    if (!client->zxdg_decoration_manager_v1) {
        fprintf(stderr, "zxdg_decoration_manager_v1 not available, use client-side decoration\n");
        // fallback to client-side decoration
        client->use_server_side_decoration = 0;
    }

    client->surface = wl_compositor_create_surface(client->compositor);
    {
        client->xdg_surface  = xdg_wm_base_get_xdg_surface(client->xdg_wm_base, client->surface);
        client->xdg_toplevel = xdg_surface_get_toplevel(client->xdg_surface);
        xdg_surface_add_listener(client->xdg_surface, &xdg_surface_listener, client);
        xdg_toplevel_set_title(client->xdg_toplevel, "Hello Wayland");
        xdg_toplevel_set_app_id(client->xdg_toplevel, "com.example.hellowayland");
        xdg_toplevel_add_listener(client->xdg_toplevel, &xdg_toplevel_listener, client);
        // signal that the surface is ready to be configured
        wl_surface_commit(client->surface);
        if (client->use_server_side_decoration && client->zxdg_decoration_manager_v1) {
            struct zxdg_toplevel_decoration_v1 *decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(
                client->zxdg_decoration_manager_v1, client->xdg_toplevel);
            zxdg_toplevel_decoration_v1_set_mode(decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
        }
    }

    // open an anonymous file and write some zero bytes to it
    int fd;
#ifdef HAS_MEMFD
    fd = memfd_create("buffer", 0);
#else
#define SHMID "/wl_buffer"
    fd = shm_open(SHMID, O_RDWR | O_CREAT, 0600);
    if (fd >= 0) {
        if (shm_unlink(SHMID) < 0) {
            perror("shm_unlink failed");
            return 1;
        }
    } else {
        perror("shm_open failed");
        return 1;
    }
#endif

    if (ftruncate(fd, size) < 0) {
        perror("ftruncate failed");
        return 1;
    }

    // map it to the memory
    client->shm_data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (client->shm_data == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }

    client->shm_pool = wl_shm_create_pool(client->shm, fd, size);

    // allocate the buffer in that pool
    client->buffer = wl_shm_pool_create_buffer(client->shm_pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);

    wl_display_roundtrip(display);

    surface_request_frame(client);
    callback_done_handler(client, NULL, 0);

    struct wl_pointer *pointer = wl_seat_get_pointer(client->seat);
    if (pointer) {
        wl_pointer_add_listener(pointer, &pointer_listener, client);

        if (!client->wp_cursor_shape_manager_v1) {
            // Fallback to using a cursor theme
            struct wl_cursor_theme *cursor_theme = wl_cursor_theme_load(NULL, 24, client->shm);
            struct wl_cursor *cursor             = wl_cursor_theme_get_cursor(cursor_theme, "left_ptr");
            client->cursor_image                 = cursor->images[0];
            struct wl_buffer *cursor_buffer      = wl_cursor_image_get_buffer(client->cursor_image);

            client->cursor_surface = wl_compositor_create_surface(client->compositor);
            wl_surface_attach(client->cursor_surface, cursor_buffer, 0, 0);
            wl_surface_commit(client->cursor_surface);
        }
    }

    while (1) {
        wl_display_dispatch(display);
        if (client->should_close) {
            break;
        }
    }
    if (client->cursor_surface) {
        wl_surface_destroy(client->cursor_surface);
    }
    if (client->shm_data) {
        munmap(client->shm_data, size);
    }
    if (client->buffer) {
        wl_buffer_destroy(client->buffer);
    }
    if (client->shm_pool) {
        wl_shm_pool_destroy(client->shm_pool);
    }
    if (client->surface) {
        wl_surface_destroy(client->surface);
    }
    if (client->xdg_surface) {
        xdg_surface_destroy(client->xdg_surface);
    }
    if (client->xdg_toplevel) {
        xdg_toplevel_destroy(client->xdg_toplevel);
    }
    if (client->xdg_wm_base) {
        xdg_wm_base_destroy(client->xdg_wm_base);
    }
    if (client->wp_cursor_shape_manager_v1) {
        wp_cursor_shape_manager_v1_destroy(client->wp_cursor_shape_manager_v1);
    }
    if (client->zxdg_decoration_manager_v1) {
        zxdg_decoration_manager_v1_destroy(client->zxdg_decoration_manager_v1);
    }
    if (client->shm) {
        wl_shm_destroy(client->shm);
    }
    if (client->compositor) {
        wl_compositor_destroy(client->compositor);
    }
    if (client->seat) {
        wl_seat_destroy(client->seat);
    }
    if (client) {
        free(client);
    }
    wl_display_disconnect(display);
    return 0;
}
