#include <stdio.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define NSTARS 1000

const float focalx = 200, focaly = 180;
const int width = 500, height = 500, depth = 500;
// where stars spawn from
const int zspawn = -depth;
const int radmin = 40, radmax = 60;

typedef struct Star {
    float r, g, b; // normalized color 0..1
    float x, y, z; // position
    float rad;     // maximum radius (when very close)
} Star;

static inline float clamp01(float x) {    
    if (x <= 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

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

void star_init(Star* star) {
    star->x = xrandom() % width - 2*width; 
    star->y = xrandom() % height - 2*height; 
    star->z = xrandom() % depth + 1;
    star->r = xrandom01() + 0.01;
    star->g = xrandom01() + 0.01;
    star->b = xrandom01() + 0.01;
    float m = MAX(star->r, MAX(star->g, star->b));
    star->r /= m, star->g /= m, star->b /= m;
    star->rad = radmin + xrandom() % (radmax - radmin);
}

int main()
{
    for (int i = 0; i < NSTARS; ++i)
        star_init(&stars[i]);

    return 0;
}
