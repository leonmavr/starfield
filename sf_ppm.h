#ifndef SF_PPM_H
#define SF_PPM_H

#ifdef PPM

#include <stdint.h>
#include <stdio.h>

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

#endif
