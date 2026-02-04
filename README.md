# starfield

Porting Bisqwit's starfield simulation from QB64 to C ([code](https://bisqwit.iki.fi/jutut/kuvat/programming_examples/sfield_qb64.bas), [video](https://www.youtube.com/watch?v=VL0oGct1S4Q))

### How it works

Blurring relies on the recurrence `blur[t+1] = decay * (blur[t] + ambience)`.

In more detail:

```
for each pixel p:
    blur[p] = (0,0,0)

for each frame t:
    new_color[:] = blur[:]
    # do the main rasterization work here
    # add frame contributions into new_color (rasterization)
    # ...
    blur[:] = new_color[:]
    for each pixel p:
        blur[p] *= decay
```

## Building

### Compile and run

You will need GNU make and to view it live you will need the X11 libraries.
Alternatively, you can run in headless mode by dumping the frames as PPM files.

The supported build options/flags you can pass to `CDEFS` are:

- `-DNO_X11` : disable linking/probing X11
- `-DWIDTH=<N>` and `-DHEIGHT=<N>` : set resolution
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

The program accepts a few command-line options (also shown by `-h` / `--help`):

- `--seed N` or `-s N` : set the RNG seed. Defaults to 1234.
- `--frames N` or `-f N` : number of frames to render. `0` means run forever. Defaults to 1000.
- `--fps N` or `-p N` : FPS cap. `0` means uncapped. Defaults to 60.

Example invocation:
```bash
./starfield --seed=42 --frames=500 --fps=30
```




