# sudo apt install libxcb1-dev
# gcc day4.c -o day4 -lxcb

#include <xcb/xcb.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// Handmade Hero-style type redefines
typedef int32_t int32;
typedef uint8_t uint8;
typedef uint32_t uint32;

typedef struct {
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
} linux_offscreen_buffer;

static linux_offscreen_buffer GlobalBackbuffer;
static bool Running;

// Equivalent to RenderWeirdGradient
static void
LinuxRenderWeirdGradient(linux_offscreen_buffer *Buffer,
                         int BlueOffset, int GreenOffset)
{
    uint8 *Row = (uint8 *)Buffer->Memory;

    for(int Y = 0; Y < Buffer->Height; ++Y) {
        uint32 *Pixel = (uint32 *)Row;

        for(int X = 0; X < Buffer->Width; ++X) {
            uint8 Blue = (X + BlueOffset);
            uint8 Green = (Y + GreenOffset);

            *Pixel++ = ((Green << 8) | Blue);
        }

        Row += Buffer->Pitch;
    }
}

static void
LinuxResizeDIBSection(linux_offscreen_buffer *Buffer, int Width, int Height)
{
    if (Buffer->Memory) {
        free(Buffer->Memory);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;
    Buffer->BytesPerPixel = 4;

    Buffer->Pitch = Width * Buffer->BytesPerPixel;
    int BitmapMemorySize = Buffer->Pitch * Height;

    Buffer->Memory = malloc(BitmapMemorySize);

    if (!Buffer->Memory) {
        fprintf(stderr, "Failed to allocate backbuffer\n");
        exit(1);
    }
}

static void
LinuxDisplayBuffer(xcb_connection_t *Connection,
                   xcb_window_t Window,
                   xcb_gcontext_t GC,
                   xcb_screen_t *Screen,
                   linux_offscreen_buffer *Buffer)
{
    xcb_put_image(Connection,
                  XCB_IMAGE_FORMAT_Z_PIXMAP,
                  Window,
                  GC,
                  Buffer->Width,
                  Buffer->Height,
                  0, 0,
                  0,
                  Screen->root_depth,
                  Buffer->Pitch * Buffer->Height,
                  (uint8_t *)Buffer->Memory);
}

int main() {
    // 1. Setup Connection and Screen
    xcb_connection_t *Connection = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(Connection)) {
      fprintf(stderr, "Cannot connect to X server\n");
      return -1;
    }
    const xcb_setup_t *Setup = xcb_get_setup(Connection);
    xcb_screen_t *Screen = xcb_setup_roots_iterator(Setup).data;

    // 2. Create Window
    xcb_window_t Window = xcb_generate_id(Connection);
    uint32_t Mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t Values[2] = {
        Screen->black_pixel,
        XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY
    };

    int Width = 800; // Initial size
    int Height = 600;

    xcb_create_window(Connection,
                      XCB_COPY_FROM_PARENT,
                      Window,
                      Screen->root,
                      0, 0,
                      Width, Height,
                      0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      Screen->root_visual,
                      Mask, Values);

    xcb_map_window(Connection, Window);
    xcb_gcontext_t GraphicsContext = xcb_generate_id(Connection);
    xcb_create_gc(Connection, GraphicsContext, Window, 0, NULL);
    xcb_flush(Connection);

    // Initialize backbuffer
    LinuxResizeDIBSection(&GlobalBackbuffer, Width, Height);

    int XOffset = 0, YOffset = 0;
    Running = true;
    while(Running) {
        // Non-blocking event loop (Equivalent to PeekMessage)
        xcb_generic_event_t *Event;
        while((Event = xcb_poll_for_event(Connection))) {
            switch(Event->response_type & ~0x80) {
                case XCB_DESTROY_NOTIFY: { Running = false; } break;
                case XCB_CONFIGURE_NOTIFY: { // Equivalent to WM_SIZE
                    xcb_configure_notify_event_t *Cfg = (xcb_configure_notify_event_t *)Event;
                    LinuxResizeDIBSection(&GlobalBackbuffer, Cfg->width, Cfg->height);
                } break;
            }
            free(Event);
        }

        LinuxRenderWeirdGradient(&GlobalBackbuffer, XOffset, YOffset);

        // Display
        LinuxDisplayBuffer(Connection, Window, GraphicsContext,
            Screen, &GlobalBackbuffer);

        xcb_flush(Connection);

       // Animate
        XOffset++;
        YOffset += 2;
    }

    if (GlobalBackbuffer.Memory) {
        free(GlobalBackbuffer.Memory);
    }

    xcb_disconnect(Connection);
    return 0;

    /*
        // 4. Push pixels to screen (Equivalent to StretchDIBits)
        xcb_image_t *Image = xcb_image_create_native(Connection, BitmapWidth, BitmapHeight,
                                                   XCB_IMAGE_FORMAT_Z_PIXMAP, Screen->root_depth,
                                                   NULL, BitmapWidth * BitmapHeight * 4, NULL);
        memcpy(Image->data, BitmapMemory, BitmapWidth * BitmapHeight * 4);
        xcb_image_put(Connection, Window, GraphicsContext, Image, 0, 0, 0);
        xcb_image_destroy(Image); // Just destroys the wrapper, not the memory
        xcb_flush(Connection);

        XOffset++;
        YOffset += 2;
    }

    xcb_disconnect(Connection);
    return 0;
    */
}
