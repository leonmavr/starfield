# starfield

Joel Yliluoma (nicknamed Bisqwit) once wrote a simulation of orbs in space, or stars, flying towards the viewer.
As they fly, they create a beautiful and computationally cheap effect of motion blur.
This fascinated me when I first saw it but the original code was written in QBasic64, so at least for me 
it was hard to read and even harder to run.

Therefore I ported it in C, sticking to the original code as much as possible, tweaking or un-hardcoding some parameters 
to make it more vibrant and seamless. I added basic window and frame storage support.

Note that the original code was written using only 256 colors in DOS by applying dithering.
I used the same 256 palette but dithering in this project is optional as it significantly slows down the rendering pipeline.

## How it works

The core of this algorithm is blurring, which creates the illusion of motion blur 
and relies on the recurrence:

```
blur[t+1] = decay * (blur[t] + ambience), with decay in 0..1
```

In more detail:

```
blur = {0 matrix}
frame_buffer = {0 matrix}
dithered = {0 matrix}

for each frame t:
    frame_buffer = blur
    blur = frame_buffer
    for star in stars:
        move star
        apply perspective transform and enlarge radius
        find bounding box and radially decay the glow away from its center 
        blur += glow(star)
    blur = decay(blur + ambience)
    // the rest is optional:
    chroma_correct(blur)
    dithered = yk_dithering(blur)
```

## Building

### Requirements

If you want to run the windowed version, you'll X window manager support.
You will also need GNU Make if you want to make the compilation easier.
If you don't want to use X, you can run in headless mode by dumping the frames as PPM files.

### Compile and run

The supported build options/flags you can pass to `CDEFS` are:

- `-DNO_X11` : disable linking/buffering to X11
- `-DWIDTH=<N>` and `-DHEIGHT=<N>` : set resolution. Defaults to `640x480`.
- `-DNSTARS=<N>` : set number of stars
- `-DPPM` : enable PPM output to files
- `-DDO_DITHER` : enable dithering

Examples:
```bash
# build with default parameters
make
# build without X11 window (headless) and set resolution
make CDEFS='-DNO_X11 -DWIDTH=800 -DHEIGHT=600'
# enable PPM output and dithering
make CDEFS='-DPPM -DDO_DITHER'
# clean
make clean
```

To run it:

```bash
./starfield
```

### Runtime options

The program accepts a few command-line options:

- `--seed N` or `-s N` : set the RNG seed. Defaults to 1234.
- `--frames N` or `-f N` : number of frames to render. `0` means run forever. Defaults to 1000.
- `--fps N` or `-p N` : FPS cap. `0` means uncapped. Defaults to 60.
- `--help` or `-h`: show the usage message.

Example invocation:
```bash
./starfield --seed=42 --frames=500 --fps=30
```

## Rendering example

**TODO**

## References

1. [original code](https://bisqwit.iki.fi/jutut/kuvat/programming_examples/sfield_qb64.bas)
2. [YouTube video](https://www.youtube.com/watch?v=VL0oGct1S4Q)
3. [Joel Yliluoma's arbitrary-palette positional dithering algorithm](https://bisqwit.iki.fi/story/howto/dither/jy/)

