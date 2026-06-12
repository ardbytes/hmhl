// gcc points.c -o points -O2 -Wall -Wextra -lm -lxcb
// Original: https://gist.github.com/antirez/9a3dc1bc9749792fc036ab4c04669a06
// ChatGPT: https://chatgpt.com/share/6a2c3de3-2d98-83ea-b3da-4cf84810e01c

#include <xcb/xcb.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>

typedef struct p3 {
    float x, y, z;
} p3;

#define WIDTH 640
#define HEIGHT 480
#define NUMPOINTS 10000

static p3 Model[NUMPOINTS];
static p3 Rotated[NUMPOINTS];

/* 32-bit software framebuffer */
static uint32_t framebuffer[WIDTH * HEIGHT];

void clear(void)
{
    memset(framebuffer, 0, sizeof(framebuffer));
}

void fade(void)
{
    uint8_t *fb = (uint8_t *)framebuffer;

    for (size_t i = 0; i < sizeof(framebuffer); i++) {
        if (fb[i] > 15)
            fb[i] -= 15;
        else
            fb[i] = 0;
    }
}

void pixel(int x, int y, int r, int g, int b)
{
    if (x < 0 || x >= WIDTH)
        return;

    if (y < 0 || y >= HEIGHT)
        return;

    framebuffer[y * WIDTH + x] =
        ((uint32_t)r << 16) |
        ((uint32_t)g << 8)  |
        ((uint32_t)b);
}

void create_model(void)
{
#if 1
    for (int j = 0; j < NUMPOINTS; j++) {
        Model[j].x = -150 + rand() % 300;
        Model[j].y = -150 + rand() % 300;
        Model[j].z = -150 + rand() % 300;
    }
#else
    int j = 0;

    for (float x = -50; x < 50; x++) {
        for (float z = -50; z <= 50; z++) {

            float y =
                10 +
                (sinf(x / 100 * M_PI * 5) * 5) +
                (cosf(z / 100 * M_PI * 5) * 5);

            Model[j].x = x * 4;
            Model[j].y = y * 4;
            Model[j].z = z * 4;

            j++;

            if (j == NUMPOINTS)
                return;
        }
    }
#endif
}

void rotate_model(float time)
{
    float alpha =
        (time * ((float)(M_PI * 2.0) / 60.0f)) * 0.05f;

    float c = cosf(alpha);
    float s = sinf(alpha);

    for (int j = 0; j < NUMPOINTS; j++) {

        Rotated[j].x =
            Model[j].x * c +
            Model[j].z * s;

        Rotated[j].y =
            Model[j].y;

        Rotated[j].z =
            -Model[j].x * s +
             Model[j].z * c;
    }
}

void draw(float time)
{
    float cx = WIDTH / 2.0f;
    float cy = HEIGHT / 2.0f;

    rotate_model(time);

    clear();
    /* fade(); */

    for (int j = 0; j < NUMPOINTS; j++) {

        float zfactor =
            1.0f + (Rotated[j].z / 300.0f);

        float x =
            Rotated[j].x / zfactor;

        float y =
            Rotated[j].y / zfactor;

        pixel(
            (int)roundf(cx + x),
            (int)roundf(cy + y),
            255, 255, 255
        );
    }
}

void present(
    xcb_connection_t *conn,
    xcb_window_t window,
    xcb_gcontext_t gc,
    uint8_t depth)
{
    xcb_put_image(
        conn,
        XCB_IMAGE_FORMAT_Z_PIXMAP,
        window,
        gc,
        WIDTH,
        HEIGHT,
        0,
        0,
        0,
        depth,
        sizeof(framebuffer),
        (const uint8_t *)framebuffer);

    xcb_flush(conn);
}

int main(void)
{
    xcb_connection_t *conn =
        xcb_connect(NULL, NULL);

    if (xcb_connection_has_error(conn)) {
        fprintf(stderr,
                "Failed to connect to X server\n");
        return 1;
    }

    const xcb_setup_t *setup =
        xcb_get_setup(conn);

    xcb_screen_iterator_t iter =
        xcb_setup_roots_iterator(setup);

    xcb_screen_t *screen =
        iter.data;

    xcb_window_t window =
        xcb_generate_id(conn);

    uint32_t mask =
        XCB_CW_BACK_PIXEL |
        XCB_CW_EVENT_MASK;

    uint32_t values[] = {
        screen->black_pixel,
        XCB_EVENT_MASK_EXPOSURE |
        XCB_EVENT_MASK_KEY_PRESS |
        XCB_EVENT_MASK_STRUCTURE_NOTIFY
    };

    xcb_create_window(
        conn,
        screen->root_depth,
        window,
        screen->root,
        0,
        0,
        WIDTH,
        HEIGHT,
        0,
        XCB_WINDOW_CLASS_INPUT_OUTPUT,
        screen->root_visual,
        mask,
        values);

    xcb_change_property(
        conn,
        XCB_PROP_MODE_REPLACE,
        window,
        XCB_ATOM_WM_NAME,
        XCB_ATOM_STRING,
        8,
        strlen("XCB Point Cloud"),
        "XCB Point Cloud");

    xcb_map_window(conn, window);

    xcb_gcontext_t gc =
        xcb_generate_id(conn);

    xcb_create_gc(
        conn,
        gc,
        window,
        0,
        NULL);

    xcb_flush(conn);

    clear();
    create_model();

    int running = 1;
    float time = 0.0f;

    while (running) {

        draw(time);

        present(
            conn,
            window,
            gc,
            screen->root_depth);

        time += 1.0f;

        xcb_generic_event_t *event;

        while ((event =
                xcb_poll_for_event(conn)) != NULL)
        {
            uint8_t type =
                event->response_type & ~0x80;

            switch (type) {

            case XCB_KEY_PRESS:
                running = 0;
                break;

            case XCB_DESTROY_NOTIFY:
                running = 0;
                break;
            }

            free(event);
        }

        usleep(16000);
    }

    xcb_free_gc(conn, gc);
    xcb_disconnect(conn);

    return 0;
}
