#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>

// toggle to enable/disable windowed output in X
#define WINDOW
#ifdef WINDOW
#include <stdlib.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

// comment/uncomment to toggle below writing to file
//#define PPM
// comment/uncomment to toggle dithering (dithering may be slow)
//#define DO_DITHER
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define NSTARS 500
#define WIDTH 640
#define HEIGHT 480
#define DEPTH 450

typedef struct Rgb {
    float r, g, b;
} Rgb;

typedef struct Star {
    Rgb rgb;       // linear or gamma-corrected color
    float x, y, z; // position
    float rad;     // maximum radius (when very close)
} Star;

const float focalx = 200, focaly = 180;
// where stars spawn from
const int zspawn = 5*DEPTH/4;
const int radmin = 4, radmax = 40;
const int speed = 2;
// how much to decay the blur buffer each frame
const float decay = 0.83f;
const float gamma_ = 2.2f;
// 6-bit-per-channel luma-sorted palette entries encoded as 0xRRGGBB where each channel is 0..63.
static const uint32_t palette_raw[] = {
    0x000000,0x000001,0x010101,0x010102,0x020101,0x010201,0x010202,0x030102,0x020202,0x020203,
    0x030201,0x040104,0x020205,0x030302,0x010404,0x020402,0x060202,0x030305,0x050206,0x040304,
    0x030309,0x040402,0x020505,0x050404,0x040407,0x080303,0x050502,0x06030A,0x030603,0x070405,
    0x04040E,0x090307,0x040605,0x020707,0x060508,0x060605,0x060703,0x0B030B,0x0C0404,0x04070A,
    0x030903,0x070707,0x090609,0x050614,0x090705,0x08060D,0x0B0411,0x04090D,0x060A06,0x070909,
    0x0F0509,0x09080A,0x040B09,0x0B0807,0x10040F,0x090B03,0x090B07,0x0D080C,0x040E05,0x07081C,
    0x0F0517,0x150507,0x0F0904,0x090A10,0x080C0C,0x050D0F,0x0E0B08,0x0B0C0B,0x0F0910,0x11090B,
    0x0C0A14,0x080F09,0x14051E,0x140810,0x0A0928,0x0C0F05,0x080E16,0x0E0D0D,0x05120B,0x180617,
    0x09100E,0x0C0E11,0x130A16,0x1C070E,0x0E0C1A,0x140D06,0x061212,0x100F0A,0x120D11,0x081505,
    0x0F100F,0x1C0B07,0x150E0C,0x110F14,0x0C140A,0x110D21,0x101305,0x081220,0x1F0818,0x180D11,
    0x0F0B36,0x1B0826,0x0D1315,0x0B1512,0x081619,0x141210,0x190D1C,0x260A0C,0x161017,0x09132D,
    0x181306,0x13150A,0x101610,0x0A1A0D,0x1E100E,0x11141C,0x051C12,0x200930,0x260921,0x131515,
    0x170F2C,0x0F1B07,0x19150D,0x1A1316,0x0E1724,0x220F1A,0x0E1A18,0x2D0B16,0x171423,0x082108,
    0x171813,0x131B0F,0x0A1D1D,0x141919,0x2C0A2B,0x19171C,0x251313,0x231608,0x1F141F,0x191C07,
    0x231127,0x081F26,0x0D2110,0x131B1F,0x380C10,0x1D1A10,0x2A0B3C,0x2E130A,0x1F1817,0x0E1A3C,
    0x171F10,0x121C2D,0x1D172B,0x191D17,0x1E143B,0x122118,0x072817,0x132508,0x2E131F,0x102222,
    0x1E1C1D,0x1B1C25,0x3A0D23,0x25191E,0x271531,0x380D33,0x0B2D09,0x19221F,0x1F2211,0x2F1915,
    0x271A26,0x1F2408,0x291E0D,0x0C282A,0x261F16,0x172329,0x1F1E31,0x321629,0x182814,0x14291F,
    0x232122,0x3C1813,0x20251A,0x112738,0x20222A,0x112E15,0x083121,0x3D113E,0x231F3B,0x30183E,
    0x2C1E2B,0x3D1820,0x2E201F,0x1A2930,0x1B300A,0x1F2A24,0x242B13,0x3E192E,0x28281C,0x262531,
    0x38230C,0x0B3238,0x2F2714,0x1F283B,0x15302B,0x1C301C,0x312135,0x292827,0x322428,0x2A2E09,
    0x103C0C,0x0F3B1D,0x2D263D,0x232F2E,0x3E2327,0x17333B,0x0B3D2A,0x3D271A,0x193B16,0x3E213D,
    0x302E1F,0x263422,0x2B3416,0x2F2D30,0x223C0C,0x3B2832,0x3D2E0E,0x28303C,0x1D3C22,0x1A3B30,
    0x123E3D,0x30332A,0x3E2A3E,0x3A331B,0x33303D,0x273834,0x3D3026,0x293D1B,0x303C0F,0x263D29,
    0x213D3D,0x3A3433,0x323D24,0x3D3D0E,0x323D31,0x2B3F3E,0x3F363F,0x3D3F1A,0x3F3E23,0x353E3F,
    0x3E3E2D,0x3E3E36,0x3F3F3F,
};
static Rgb palette_lin[sizeof(palette_raw) / sizeof(palette_raw[0])];
Star stars[NSTARS];
Rgb frame_buffer[WIDTH * HEIGHT] = {0};
Rgb blurred[WIDTH * HEIGHT] = {0};
Rgb dithered[WIDTH * HEIGHT] = {0};

static inline float clamp01(float x) {    
    if (x <= 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

//--------------------------------------------------------------------
// Window (X11) utilities
//--------------------------------------------------------------------
#ifdef WINDOW
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
    uint32_t maxv = (0x1 << nones) - 0x1;
    // round to nearest before shifting it into a mask
    uint32_t v = (c * maxv + 0x7F) / 255;
    return v << nzeros;
}

static inline uint32_t x11_pack_rgb(WindowCtx* ctx, uint8_t r, uint8_t g, uint8_t b) {
    return ctx->r_lut[r] | ctx->g_lut[g] | ctx->b_lut[b];
}

static int x11_init(WindowCtx* ctx, int width, int height) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->disp = XOpenDisplay(NULL);
    if (!ctx->disp) {
        fprintf(stderr, "ERROR: XOpenDisplay failed.\\n");
        return 0;
    }

    ctx->screen = DefaultScreen(ctx->disp);
    ctx->visual = DefaultVisual(ctx->disp, ctx->screen);
    ctx->depth = DefaultDepth(ctx->disp, ctx->screen);
    uint32_t black = BlackPixel(ctx->disp, ctx->screen);
    uint32_t white = WhitePixel(ctx->disp, ctx->screen);
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
        fprintf(stderr, "ERROR: calloc failed for XImage buffer.\\n");
        return 0;
    }
    ctx->img = XCreateImage(ctx->disp, ctx->visual, (unsigned)ctx->depth, ZPixmap, 0,
                            data, (unsigned)width, (unsigned)height, 32, 0);
    if (!ctx->img) {
        fprintf(stderr, "ERROR: XCreateImage failed.\\n");
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
            uint8_t r = clamp01(px.r) * 255.0f + 0.5f;
            uint8_t g = clamp01(px.g) * 255.0f + 0.5f;
            uint8_t b = clamp01(px.b) * 255.0f + 0.5f;
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

//--------------------------------------------------------------------
// Random number generator utilities
//--------------------------------------------------------------------
// Linear congruential generator originally written by @Skeeto
enum { XRAND_MAX = 0x7fffffff };
static uint64_t xrandom_state = 1234;
static void xrandom_seed(uint64_t seed) {
    xrandom_state = seed;
}
static int xrandom(void) {
    xrandom_state = xrandom_state*0x3243f6a8885a308d + 1;
    return xrandom_state >> 33;
}

static void usage(const char* argv0) {
    fprintf(stderr,
            "Usage: %s [--seed N] [--frames N] [--fps N]\n"
            "  --seed|-s N     RNG seed (positive integer)\n"
            "  --frames|-f N   Number of frames; 0 = infinite\n"
            "  --fps|-p N      FPS cap; 0 = uncapped (default 60)\n"
            "  -h, --help   Show this help\n",
            argv0);
}

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void sleep_ns(uint64_t ns) {
    struct timespec req;
    req.tv_sec = (time_t)(ns / 1000000000ull);
    req.tv_nsec = (long)(ns % 1000000000ull);
    while (nanosleep(&req, &req) != 0 && errno == EINTR) {
        // retry with remaining time
    }
}

static int parse_u64(const char* s, uint64_t* out) {
    if (!s || !*s)
        return 0;
    uint64_t v = 0;
    for (const unsigned char* p = (const unsigned char*)s; *p; ++p) {
        if (*p < '0' || *p > '9')
            return 0;
        uint64_t prev = v;
        // ASCII char to int
        v = v * 10 + (uint64_t)(*p - '0');
        if (v < prev)
            return 0;
    }
    *out = v;
    return 1;
}

//--------------------------------------------------------------------
// Star flight
//--------------------------------------------------------------------
static inline void chroma_correct(Rgb* color) {
    // Luminance (Y) component of RGB to YUV conversion:
    // [Y; U; V] = [0.299    0.587    0.114;
    //              -0.14713 -0.28886 0.436;
    //              0.615    -0.51499 -0.10001] * [R; G; B]
    float* r = &color->r, *g = &color->g, *b = &color->b;
    float luminance = 0.299*(*r) + 0.587*(*g) + 0.114*(*b);
    // After the blur computation, r, g, b may have exceeded [0..1],
    // leading to luminance values outside [0..1].
    if (luminance > 1.0f) {
        *r = *g = *b = 1.0f;
    } else if (luminance <= 0.0f) {
        *r = *g = *b = 0.0f;
    } else { // luminance in (0..1), r, g, b may be out of range
        // sat = 1 fully preserves the color, while sat = 0 desaturates to gray
        float sat = 1.0f;
        // If not in 0..1, we clip each component (let it be C' = R, G, or B) to 0 or 1
        // Set C' = 1, or C' = 0 and from linear interpolation between luminance and C',
        // solve for saturation:
        // C' = (C - luminance) * sat + luminance =>
        // sat = (C' - luminance) / (C - luminance), C = 0 or 1
        // Then, find the minimum saturation that keeps all components in [0..1]
        if (*r > 1.0f) sat = MIN(sat, (luminance - 1.0f) / (luminance - *r));
        else if (*r < 0.0f) sat = MIN(sat, luminance / (luminance - *r));
        if (*g > 1.0f) sat = MIN(sat, (luminance - 1.0f) / (luminance - *g));
        else if (*g < 0.0f) sat = MIN(sat, luminance / (luminance - *g));
        if (*b > 1.0f) sat = MIN(sat, (luminance - 1.0f) / (luminance - *b));
        else if (*b < 0.0f) sat = MIN(sat, luminance / (luminance - *b));
        // compress oversaturated colors (desaturate) back into RGB cube
        // while preserving luminance
        if (sat < 1.0f) {
            *r = (*r - luminance) * sat + luminance;
            *g = (*g - luminance) * sat + luminance;
            *b = (*b - luminance) * sat + luminance;
        }
    }
}

static void palette_init(void) {
    size_t sz = sizeof(palette_raw) / sizeof(palette_raw[0]);
    for (size_t i = 0; i < sz; ++i) {
        uint32_t p = palette_raw[i];
        // 6 bits per channel -> divide by 2^6-1 to normalize
        float r = ((p >> 16) & 0xFF) / 63.0f;
        float g = ((p >> 8) & 0xFF) / 63.0f;
        float b = (p & 0xFF) / 63.0f;
        palette_lin[i].r = r;
        palette_lin[i].g = g;
        palette_lin[i].b = b;
    }
}

static inline int lin_closest_idx(Rgb rgb) {
    int idx_best = 0;
    float err_min = 0.0f;
    const int palette_count = sizeof(palette_lin)/
                              sizeof(palette_lin[0]);
    for (int p = 0; p < palette_count; ++p) {
        float dr = palette_lin[p].r - rgb.r;
        float dg = palette_lin[p].g - rgb.g;
        float db = palette_lin[p].b - rgb.b;
        float err = dr*dr + dg*dg + db*db;
        if (p == 0 || err < err_min) {
            err_min = err;
            idx_best = p;
        }
    }
    return idx_best;
}

static inline int ykdither_palette_idx(Rgb lin, int x, int y) {
    // Gamma-aware positional dithering (Knoll-Yliluoma algorithm):
    // - generate a set of candidate palette entries using an error
    //   term in gamma space
    // - select one based on an 8x8 Bayer threshold.
    enum { CANDCOUNT = 64 };
    static const uint8_t bayer8[8][8] = {
        {  0, 48, 12, 60,  3, 51, 15, 63 },
        { 32, 16, 44, 28, 35, 19, 47, 31 },
        {  8, 56,  4, 52, 11, 59,  7, 55 },
        { 40, 24, 36, 20, 43, 27, 39, 23 },
        {  2, 50, 14, 62,  1, 49, 13, 61 },
        { 34, 18, 46, 30, 33, 17, 45, 29 },
        { 10, 58,  6, 54,  9, 57,  5, 53 },
        { 42, 26, 38, 22, 41, 25, 37, 21 },
    };
    const float ungamma = 1.0f / gamma_;
    Rgb lin_gamma = {
        powf(clamp01(lin.r), gamma_),
        powf(clamp01(lin.g), gamma_),
        powf(clamp01(lin.b), gamma_)
    };
    Rgb err = {0.0f, 0.0f, 0.0f};
    int candidates[CANDCOUNT];
    for (int c = 0; c < CANDCOUNT; ++c) {
        Rgb try = {
            powf(clamp01(lin_gamma.r + err.r), ungamma),
            powf(clamp01(lin_gamma.g + err.g), ungamma),
            powf(clamp01(lin_gamma.b + err.b), ungamma)
        };
        int idx = lin_closest_idx(try);
        candidates[c] = idx;
        Rgb palette_gamma = {
            powf(palette_lin[idx].r, gamma_),
            powf(palette_lin[idx].g, gamma_),
            powf(palette_lin[idx].b, gamma_)
        };
        // error in gamma space between target and the selected
        // palette entry
        err = (Rgb) {
            lin_gamma.r - palette_gamma.r,
            lin_gamma.g - palette_gamma.g,
            lin_gamma.b - palette_gamma.b
        };
    }
    // palette entries are luma-sorted (ascending),
    // so sorting by index (insertion sort) sorts by luma
    for (int j = 1; j < CANDCOUNT; ++j) {
        int k = candidates[j], i = j;
        for (; i > 0; --i) {
            if (candidates[i - 1] <= k)
                break;
            candidates[i] = candidates[i - 1];
        }
        candidates[i] = k;
    }
    // Bayer matrix selects the index (0..63) so that nearby pixels
    // have significantly different luma to reduce the "banding" effect
    uint8_t b = bayer8[y & 7][x & 7];
    return candidates[b];
}

#ifdef PPM
static void ppm_write(const char* path, const Rgb* img, int width, int height) {
    FILE* f = fopen(path, "wb");
    if (!f)
        perror("ERROR: Cannot open file to write.");
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    for (int i = 0; i < width * height; ++i) {
        uint8_t r = clamp01(img[i].r) * 255.0f + 0.5f;
        uint8_t g = clamp01(img[i].g) * 255.0f + 0.5f;
        uint8_t b = clamp01(img[i].b) * 255.0f + 0.5f;
        fputc(r, f);
        fputc(g, f);
        fputc(b, f);
    }
    fclose(f);
}
#endif

void star_init(Star* star) {
    star->x = xrandom() % WIDTH; 
    star->y = xrandom() % HEIGHT; 
    star->z = DEPTH/2 + xrandom() % DEPTH;
    // random color from the palette
    const size_t palette_count = sizeof(palette_raw) /
                                 sizeof(palette_raw[0]);
    size_t pi = (size_t)(xrandom() % palette_count);
    star->rgb.r = palette_lin[pi].r;
    star->rgb.g = palette_lin[pi].g;
    star->rgb.b = palette_lin[pi].b;
    star->rad = radmin + xrandom() % (radmax - radmin);
}


int main(int argc, char** argv) {
    uint64_t frames = 1000;
    uint64_t seed = 1234;
    uint64_t fps = 60;

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            usage(argv[0]);
            return 0;
        }
        const char* val = NULL;
        if (!strcmp(arg, "-s") || !strcmp(arg, "--seed")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "ERROR: %s expects a value.\n", arg);
                return 1;
            }
            val = argv[++i];
            if (!parse_u64(val, &seed)) {
                fprintf(stderr, "ERROR: invalid seed: %s\n", val);
                return 1;
            }
            continue;
        }
        if (!strcmp(arg, "-f") || !strcmp(arg, "--frames")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "ERROR: %s expects a value.\n", arg);
                return 1;
            }
            val = argv[++i];
            if (!parse_u64(val, &frames)) {
                fprintf(stderr, "ERROR: invalid frames: %s\n", val);
                return 1;
            }
            continue;
        }
        if (!strcmp(arg, "-p") || !strcmp(arg, "--fps")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "ERROR: %s expects a value.\n", arg);
                return 1;
            }
            val = argv[++i];
            if (!parse_u64(val, &fps)) {
                fprintf(stderr, "ERROR: invalid fps: %s\n", val);
                return 1;
            }
            continue;
        }
        if (!strncmp(arg, "--seed=", strlen("--seed="))) {
            val = arg + strlen("--seed=");
            if (!parse_u64(val, &seed)) {
                fprintf(stderr, "ERROR: invalid seed: %s\n", val);
                return 1;
            }
            continue;
        }
        if (!strncmp(arg, "--frames=", strlen("--frames="))) {
            val = arg + strlen("--frames=");
            if (!parse_u64(val, &frames)) {
                fprintf(stderr, "ERROR: invalid frames: %s\n", val);
                return 1;
            }
            continue;
        }
        if (!strncmp(arg, "--fps=", strlen("--fps="))) {
            val = arg + strlen("--fps=");
            if (!parse_u64(val, &fps)) {
                fprintf(stderr, "ERROR: invalid fps: %s\n", val);
                return 1;
            }
            continue;
        }

        fprintf(stderr, "ERROR: unknown argument: %s\n", arg);
        usage(argv[0]);
        return 1;
    }

    xrandom_seed(seed);
    palette_init();
    for (int i = 0; i < NSTARS; ++i)
        star_init(&stars[i]);

#ifdef WINDOW
    WindowCtx win;
    int window_ok = x11_init(&win, WIDTH, HEIGHT);
    int window_running = window_ok;
#endif

    const uint64_t target_ns = (fps > 0) ? (1000000000ull / fps) : 0;
    for (uint64_t frame = 0; frames == 0 || frame < frames; ++frame) {
        const uint64_t frame_start_ns = (target_ns > 0) ? now_ns() : 0;
        memcpy(frame_buffer, blurred, sizeof(blurred));
        // additive lighting for later so no star is completely dark
        const float ambient = 0.05f/sqrt(NSTARS);
        for (int i = 0; i < NSTARS; ++i) {
            Star* star = &stars[i];
            star->z -= speed;
            // respawn if in the next step it would be past the viewer
            if (star->z < speed) {
                star_init(star);
                star->z = zspawn + (xrandom() % (DEPTH/3));
            }
            //// 1. persective projection
            // treat star->x/y as screen-space spawn coords, centered at WIDTH/2, HEIGHT/2
            float projx = 2*focalx * (star->x - WIDTH/2) / star->z + WIDTH/2;
            float projy = 2*focaly * (star->y - HEIGHT/2) / star->z + HEIGHT/2;
            enum { MAX_PROJECT_RAD = 1800 };
            float projrad = MAX_PROJECT_RAD/MAX(speed, star->z - speed);
            //// 2. additive decaying glow
            // compute integer bounding box and clamp to screen
            int minx = MAX(0,      (int)(projx - projrad));
            int maxx = MIN(WIDTH,  (int)(projx + projrad) + 1);
            int miny = MAX(0,      (int)(projy - projrad));
            int maxy = MIN(HEIGHT, (int)(projy + projrad) + 1);
            // 2D rasterization
            for (int y = miny; y < maxy; ++y) {
                for (int x = minx; x < maxx; ++x) {
                    float dx = x - projx;
                    float dy = y - projy;
                    // decay (falloff) within star's radius
                    if (dx*dx + dy*dy < projrad*projrad) {
                        float falloff = 1.0f - (dx*dx + dy*dy)/(projrad*projrad);
                        int idx = y * WIDTH + x;
                        frame_buffer[idx].r += star->rgb.r * falloff + ambient;
                        frame_buffer[idx].g += star->rgb.g * falloff + ambient;
                        frame_buffer[idx].b += star->rgb.b * falloff + ambient;
                    }
                }
            }
        }
        //// 3. frame processing - blur, chroma correct and dither
        memcpy(blurred, frame_buffer, sizeof(blurred));
        for (Rgb* pixel = blurred;  pixel < blurred + WIDTH * HEIGHT; ++pixel) {
            pixel->r *= decay;
            pixel->g *= decay;
            pixel->b *= decay;
        }
#ifdef DO_DITHER
        for (int py = 0; py < HEIGHT; ++py) {
            for (int px = 0; px < WIDTH; ++px) {
                int i = py * WIDTH + px;
                Rgb lin = blurred[i];
                chroma_correct(&lin);
                int p = ykdither_palette_idx(lin, px, py);
                dithered[i] = palette_lin[p];
            }
        }
#endif

#ifdef WINDOW
        if (window_running) {
            window_running = x11_process_events(&win);
            if (window_running)
#ifdef DO_DITHER
                x11_image_show(&win, dithered, WIDTH, HEIGHT);
#else
                x11_image_show(&win, blurred, WIDTH, HEIGHT);
#endif
        }
#endif
#ifdef PPM
        // change mod below to write more/fewer frames
        if (frame % 7 == 0) {
            char path[64];
            snprintf(path, sizeof(path), "blur_%03d.ppm", frame);
            ppm_write(path, dithered, WIDTH, HEIGHT);
        }
#endif

        if (target_ns > 0) {
            const uint64_t elapsed_ns = now_ns() - frame_start_ns;
            if (elapsed_ns < target_ns)
                sleep_ns(target_ns - elapsed_ns);
        }
    }

#ifdef WINDOW
    if (window_ok)
        x11_shutdown(&win);
#endif
    return 0;
}
