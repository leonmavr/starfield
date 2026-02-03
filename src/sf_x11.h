#ifndef SF_X11_H
#define SF_X11_H

#ifndef NO_X11

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>


typedef struct WindowCtx {
    Display* disp;   /* connection wrapper */
    int screen;
    Window win;
    GC gc;           /* graphics context (fg/bg) */
    Visual* visual;  /* pixel format */
    int depth;
    XImage* img;
    Atom wm_delete;  /* deletion protocol return */
    // precomputed 8-bit channel LUTs for `visual -
    // this avoids recomputing mask shifts/scales per pixel
    uint32_t r_lut[256];
    uint32_t g_lut[256];
    uint32_t b_lut[256];
} WindowCtx;

static inline uint32_t x11_channel2mask(uint8_t c, uint32_t mask) {
    if (mask == 0)
        return 0;
    int nzeros = 0;
    // count trailing zeros
    while ((mask & 0x1) == 0x0) {
        mask >>= 1;
        ++nzeros;
    }
    int nones = 0;
    // count consecutive ones
    while ((mask & 0x1) == 0x1) {
        mask >>= 1;
        ++nones;
    }
    if (nones == 0)
        return 0;
    // shift the consecutive ones to the right place
    uint32_t maxv = (0x1u << nones) - 0x1u;
    // round to nearest before shifting it into a mask
    uint32_t v = (c * maxv + 0x7Fu) / 255u;
    return v << nzeros;
}

static inline uint32_t x11_pack_rgb(WindowCtx* ctx, uint8_t r, uint8_t g, uint8_t b) {
    return ctx->r_lut[r] | ctx->g_lut[g] | ctx->b_lut[b];
}

static int x11_init(WindowCtx* ctx, int width, int height) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->disp = XOpenDisplay(NULL);
    if (!ctx->disp) {
        fprintf(stderr, "ERROR: XOpenDisplay failed.\n");
        return 0;
    }

    ctx->screen = DefaultScreen(ctx->disp);
    ctx->visual = DefaultVisual(ctx->disp, ctx->screen);
    ctx->depth = DefaultDepth(ctx->disp, ctx->screen);
    uint32_t black = (uint32_t)BlackPixel(ctx->disp, ctx->screen);
    uint32_t white = (uint32_t)WhitePixel(ctx->disp, ctx->screen);
    ctx->win = XCreateSimpleWindow(ctx->disp, RootWindow(ctx->disp, ctx->screen),
                                   0, 0, (unsigned)width, (unsigned)height,
                                   1, white, black);
    XStoreName(ctx->disp, ctx->win, "Starfield - press any key to exit");
    // register repaint | key | resize event types
    XSelectInput(ctx->disp, ctx->win, ExposureMask | KeyPressMask | StructureNotifyMask);
    ctx->gc = XCreateGC(ctx->disp, ctx->win, 0, NULL);
    // delete when we receive a close event
    ctx->wm_delete = XInternAtom(ctx->disp, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(ctx->disp, ctx->win, &ctx->wm_delete, 1);

    // pre-compute mask LUTs for each channel for context's visual (pixels)
    for (int i = 0; i <= 255; ++i) {
        ctx->r_lut[i] = x11_channel2mask((uint8_t)i, ctx->visual->red_mask);
        ctx->g_lut[i] = x11_channel2mask((uint8_t)i, ctx->visual->green_mask);
        ctx->b_lut[i] = x11_channel2mask((uint8_t)i, ctx->visual->blue_mask);
    }

    size_t stride = (size_t)width * 4; // 4 bytes per pixel
    size_t size = stride * (size_t)height;
    char* data = (char*)calloc(1, size);
    if (!data) {
        fprintf(stderr, "ERROR: calloc failed for XImage buffer.\n");
        return 0;
    }
    ctx->img = XCreateImage(ctx->disp, ctx->visual, (unsigned)ctx->depth, ZPixmap, 0,
                            data, (unsigned)width, (unsigned)height, 32, 0);
    if (!ctx->img) {
        fprintf(stderr, "ERROR: XCreateImage failed.\n");
        free(data);
        return 0;
    }
    XMapWindow(ctx->disp, ctx->win);
    XFlush(ctx->disp);
    return 1;
}

static void x11_shutdown(WindowCtx* ctx) {
    if (!ctx || !ctx->disp)
        return;
    if (ctx->img) {
        XDestroyImage(ctx->img);
        ctx->img = NULL;
    }
    if (ctx->gc) {
        XFreeGC(ctx->disp, ctx->gc);
        ctx->gc = 0;
    }
    if (ctx->win) {
        XDestroyWindow(ctx->disp, ctx->win);
        ctx->win = 0;
    }
    XCloseDisplay(ctx->disp);
    ctx->disp = NULL;
}

static int x11_process_events(WindowCtx* ctx) {
    while (XPending(ctx->disp)) {
        XEvent ev;
        XNextEvent(ctx->disp, &ev);
        if (ev.type == ClientMessage) {
            if ((Atom)ev.xclient.data.l[0] == ctx->wm_delete)
                return 0;
        } else if (ev.type == KeyPress) {
            return 0;
        }
    }
    return 1;
}

static void x11_image_show(WindowCtx* ctx, const Rgb* img, int width, int height) {
    bool fast_unpack32 = (ctx->img && ctx->img->bits_per_pixel == 32);
    for (int y = 0; y < height; ++y) {
        uint8_t* row = NULL;
        if (fast_unpack32)
            row = (uint8_t*)ctx->img->data + y * ctx->img->bytes_per_line;
        for (int x = 0; x < width; ++x) {
            const Rgb px = img[y * width + x];
            uint8_t r = (uint8_t)(clamp01(px.r) * 255.0f + 0.5f);
            uint8_t g = (uint8_t)(clamp01(px.g) * 255.0f + 0.5f);
            uint8_t b = (uint8_t)(clamp01(px.b) * 255.0f + 0.5f);
            uint32_t p = x11_pack_rgb(ctx, r, g, b);
            if (fast_unpack32) {
                // load big-endian 32bpp
                if (ctx->img->byte_order == LSBFirst) {
                    ((uint32_t*)row)[x] = p;
                } else { // little-endian 32bpp
                    // indirectly write to image buffer in context
                    uint8_t* disp_row = row + (size_t)x * 4;
                    disp_row[0] = (uint8_t)((p >> 24) & 0xFF);
                    disp_row[1] = (uint8_t)((p >> 16) & 0xFF);
                    disp_row[2] = (uint8_t)((p >> 8) & 0xFF);
                    disp_row[3] = (uint8_t)(p & 0xFF);
                }
            } else {
                // arbitrary pixel format
                XPutPixel(ctx->img, x, y, (unsigned long)p);
            }
        }
    }
    XPutImage(ctx->disp, ctx->win, ctx->gc, ctx->img,
              0, 0, 0, 0, (unsigned)width, (unsigned)height);
    XFlush(ctx->disp);
}

#endif

#endif
