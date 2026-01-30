#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define NSTARS 1000

const float focalx = 200, focaly = 180;
#define WIDTH 500
#define HEIGHT 500
#define DEPTH 500
// where stars spawn from
const int zspawn = DEPTH;
const int radmin = 20, radmax = 200;
const int speed = 2;
// how much to decay the blur buffer each frame 0..1
const float decay = 0.83f;

typedef struct Star {
    float r, g, b; // normalized color 0..1
    float x, y, z; // position
    float rad;     // maximum radius (when very close)
} Star;

typedef struct Rgb {
    float r, g, b;
} Rgb;

static inline void chroma_correct(Rgb* color) {
    // Luminance (Y) component of RGB to YUV conversion:
    // [Y; U; V] = [0.299 0.587 0.114; -0.14713 -0.28886 0.436; 0.615 -0.51499 -0.10001] * [R; G; B]
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

// 6-bit-per-channel palette entries encoded as 0xRRGGBB where each channel is 0..63.
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

static void palette_init(void) {
    for (size_t i = 0; i < (sizeof(palette_raw) / sizeof(palette_raw[0])); ++i) {
        uint32_t s = palette_raw[i];
        // 6 bits per channel -> divide by 2^6-1
        float R = ((s >> 16) & 0xFF)/63.0f;
        float G = ((s >> 8) & 0xFF)/63.0f;
        float B = (s & 0xFF)/63.0f;
        // intentionally skip gamma correction
        palette_lin[i].r = R;
        palette_lin[i].g = G;
        palette_lin[i].b = B;
    }
}

static inline float clamp01(float x) {    
    if (x <= 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}
#define PPM

#ifdef PPM
static int write_ppm_rgb(const char* path, const Rgb* img, int width, int height) {
    FILE* f = fopen(path, "wb");
    if (!f) {
        perror("fopen");
        return 0;
    }
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    for (int i = 0; i < width * height; ++i) {
        unsigned char r = (unsigned char)(clamp01(img[i].r) * 255.0f + 0.5f);
        unsigned char g = (unsigned char)(clamp01(img[i].g) * 255.0f + 0.5f);
        unsigned char b = (unsigned char)(clamp01(img[i].b) * 255.0f + 0.5f);
        fputc(r, f);
        fputc(g, f);
        fputc(b, f);
    }
    fclose(f);
    return 1;
}
#endif

// Linear congruential generator originally written by @Skeeto
enum { XRAND_MAX = 0x7fffffff };
static unsigned long long xrandom_state = 1234;
static void xsrandom(unsigned long long seed) {
    xrandom_state = seed;
}
static int xrandom(void) {
    xrandom_state = xrandom_state*0x3243f6a8885a308d + 1;
    return xrandom_state >> 33;
}
static float xrandom01(void) {
    enum { QUANT_LEVELS = 1000 };
    return (xrandom() % QUANT_LEVELS) / (float)QUANT_LEVELS;
}

Star stars[NSTARS];
Rgb blur[WIDTH * HEIGHT] = {};
Rgb new_color[WIDTH * HEIGHT] = {};

void star_init(Star* star) {
    star->x = xrandom() % WIDTH; 
    star->y = xrandom() % HEIGHT; 
    star->z = zspawn + xrandom() % DEPTH;
    // random color from the palette
    const size_t palette_count = sizeof(palette_raw) / sizeof(palette_raw[0]);
    size_t pi = (size_t)(xrandom() % (int)palette_count);
    star->r = palette_lin[pi].r;
    star->g = palette_lin[pi].g;
    star->b = palette_lin[pi].b;
    star->rad = radmin + xrandom() % (radmax - radmin);
}


int main() {
    int frames = 1000;
    palette_init();
    for (int i = 0; i < NSTARS; ++i)
        star_init(&stars[i]);
    for (int frame = 0; frame < frames; ++frame) {
        memcpy(new_color, blur, sizeof(blur));
        // additive lighting for later so no star is completely dark
        const float ambient = 2.0f/sqrt(NSTARS);
        for (int i = 0; i < NSTARS; ++i) {
            Star* star = &stars[i];
            star->z -= speed;
            // respawn if in the next step it would be past the viewer
            if (star->z < speed)
                star_init(star);
            //--------------------------------------------------------
            // persective projection
            //--------------------------------------------------------
            // treat star->x/y as screen-space spawn coords, centered at WIDTH/2, HEIGHT/2
            float projx = 2*focalx * (star->x - WIDTH/2) / star->z + WIDTH/2;
            float projy = 2*focaly * (star->y - HEIGHT/2) / star->z + HEIGHT/2;
            enum { MAX_PROJECT_RAD = 1800 };
            float projrad = MAX_PROJECT_RAD/MAX(speed, star->z - speed);
            //--------------------------------------------------------
            // additive decaying glow
            //--------------------------------------------------------
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
                        new_color[idx].r += star->r * falloff + ambient;
                        new_color[idx].g += star->g * falloff + ambient;
                        new_color[idx].b += star->b * falloff + ambient;
                    }
                }
            }
        }
        // chroma correction for pixels with overly bright components
        for (Rgb* pixel = new_color;  pixel < new_color + WIDTH * HEIGHT; ++pixel)
            chroma_correct(pixel);
        //--------------------------------------------------------
        // apply blur to current buffer
        //--------------------------------------------------------
        memcpy(blur, new_color, sizeof(blur));
        for (Rgb* pixel = blur;  pixel < blur + WIDTH * HEIGHT; ++pixel) {
            pixel->r *= decay;
            pixel->g *= decay;
            pixel->b *= decay;
        }

#ifdef PPM
        {
            char path[64];
            snprintf(path, sizeof(path), "blur_%03d.ppm", frame);
            (void)write_ppm_rgb(path, blur, WIDTH, HEIGHT);
        }
#endif

    }
    return 0;
}
