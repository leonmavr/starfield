# starfield

Porting Bisqwit's starfield simulation from QB64 to C ([code](https://bisqwit.iki.fi/jutut/kuvat/programming_examples/sfield_qb64.bas), [video](https://www.youtube.com/watch?v=VL0oGct1S4Q))

### Blurring

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
