# SDL libm subset

These unmodified math sources come from SDL 3.4.8, commit
`d9d5536704d585616d4db3c8ba3c4ff6fc2757e1`, `src/libm/`:
https://github.com/libsdl-org/SDL/tree/d9d5536704d585616d4db3c8ba3c4ff6fc2757e1/src/libm

They provide fdlibm/uClibc sine and cosine for MSVC x86, whose UCRT loses
large-angle phase accuracy. Other platforms retain their system math library.
The original Sun Microsystems and SDL license notices remain in each file.
`UPSTREAM.json` records the source URLs and SHA-256 hashes.

`SDL_internal.h` is a local build shim, not SDL's original internal header.
It supplies the required public SDL types/macros and prefixes private symbols
with `VxLibm` to avoid collisions when linking SDL statically. No numerical
algorithm or coefficient is modified. When updating, replace upstream files
together and rerun the independent full-range trigonometric tests.
