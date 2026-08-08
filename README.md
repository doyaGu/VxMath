# VxMath

VxMath is the Virtools-compatible math and platform utility library used by CK2 and the rest of Ballanced.

## Features

- Vectors, matrices, quaternions, planes, rays, bounds, frustums, and intersections
- Image conversion, blitting, resizing, and quantization
- Runtime SIMD dispatch with scalar and architecture-specific backends
- Containers, strings, allocators, paths, timing, threading, shared libraries, and memory-mapped files
- SDL3 and POSIX platform implementations used by the cross-platform runtime

## Support scope

The instructions in this document describe VxMath's `sdl` branch. That branch is continuously built through [Ballanced](https://github.com/doyaGu/Ballanced) on Windows, Linux, and macOS. The Ballanced root presets define the supported full-runtime architecture matrix.

## Building

### Recommended: Ballanced superproject

```bash
git clone --recurse-submodules https://github.com/doyaGu/Ballanced.git
cd Ballanced
cmake --preset macos-arm64-tests # choose the preset for your host
cmake --build --preset macos-arm64-tests-release
ctest --preset macos-arm64-tests-release
```

### Standalone

Requirements:

- CMake 3.16+
- A C/C++ toolchain
- SDL3 discoverable with `find_package(SDL3 CONFIG)`
- Initialized recursive submodules for SIMDe and stb

```bash
git clone --branch sdl --recurse-submodules https://github.com/doyaGu/VxMath.git
cd VxMath
cmake -S . -B build \
  -DVXMATH_BUILD_TESTS=ON \
  -DCMAKE_PREFIX_PATH=/path/to/SDL3
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

For Ninja and other single-configuration generators, add `-DCMAKE_BUILD_TYPE=Release`.

### CMake options

- `VXMATH_BUILD_SHARED` / `VXMATH_BUILD_STATIC`
- `VXMATH_BUILD_TESTS`
- `VXMATH_BUILD_PERF`
- `VXMATH_ENABLE_PERF_GATES`
- `VXMATH_ENABLE_SIMD`
- `VXMATH_INSTALL`

## Versioning

VxMath is versioned independently. Ballanced releases pin an exact VxMath commit and its nested dependency revisions.

## License

Apache License 2.0. See [LICENSE](LICENSE).
