#include <stdio.h>
#include <string.h>
#include <math.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define NSTARS 1000

const float focalx = 200, focaly = 180;
#define WIDTH 500
#define HEIGHT 500
#define DEPTH 500
// where stars spawn from
const int zspawn = DEPTH;
const int radmin = 10, radmax = 70;
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

enum { XRAND_MAX = 0x7fffffff };
static unsigned long long xrandom_state = 1;
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
Rgb blur[WIDTH * HEIGHT] = {0};
Rgb new_color[WIDTH * HEIGHT] = {0};

void star_init(Star* star) {
    star->x = xrandom() % WIDTH; 
    star->y = xrandom() % HEIGHT; 
    star->z = zspawn + xrandom() % DEPTH;
    star->r = xrandom01() + 0.01;
    star->g = xrandom01() + 0.01;
    star->b = xrandom01() + 0.01;
    float m = MAX(star->r, MAX(star->g, star->b));
    star->r /= m, star->g /= m, star->b /= m;
    star->rad = radmin + xrandom() % (radmax - radmin);
}


int main() {
    int frames = 1000;
    for (int i = 0; i < NSTARS; ++i)
        star_init(&stars[i]);
    for (int frame = 0; frame < frames; ++frame) {
        memcpy(new_color, blur, sizeof(blur));
        // additive lighting for later so no star is completely dark
        const float ambient = 1/sqrt(NSTARS);
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
        // Luminance (Y) component of RGB to YUV conversion:
        // [Y; U; V] = [0.299 0.587 0.114; -0.14713 -0.28886 0.436; 0.615 -0.51499 -0.10001] * [R; G; B]
        float* r = &new_color[0].r, *g = &new_color[0].g, *b = &new_color[0].b;
        float luminance = 0.299* (*r) + 0.587* (*g) + 0.114* (*b);
        if (luminance >= 1.0f) {
            *r = *g = *b = 1.0f;
        } else if (luminance <= 0.0f) {
            *r = *g = *b = 0.0f;
        } else {
            float sat = 1.0f;
            if (*r > 1.0f) sat = MIN(sat, (luminance - 1.0f) / (luminance - *r));
            else if (*r < 0.0f) sat = MIN(sat, luminance / (luminance - *r));
            if (*g > 1.0f) sat = MIN(sat, (luminance - 1.0f) / (luminance - *g));
            else if (*g < 0.0f) sat = MIN(sat, luminance / (luminance - *g));
            if (*b > 1.0f) sat = MIN(sat, (luminance - 1.0f) / (luminance - *b));
            else if (*b < 0.0f) sat = MIN(sat, luminance / (luminance - *b));
            if (sat < 1.0f) {
                *r = (*r - luminance) * sat + luminance;
                *g = (*g - luminance) * sat + luminance;
                *b = (*b - luminance) * sat + luminance;
            }
        }
        //--------------------------------------------------------
        // apply blur to current buffer
        //--------------------------------------------------------
        memcpy(blur, new_color, sizeof(blur));
        for (int i = 0; i < WIDTH * HEIGHT; ++i) {
            blur[i].r *= decay;
            blur[i].g *= decay;
            blur[i].b *= decay;
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
