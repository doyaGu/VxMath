# VxMath

VxMath provides Virtools-compatible math, containers, configuration, and platform utilities.

## Features

- Vectors, matrices, quaternions, planes, rays, bounds, frustums, and intersections
- Image conversion, blitting, resizing, and quantization
- Runtime SIMD dispatch with scalar and architecture-specific backends
- Containers, strings, allocators, paths, timing, threading, shared libraries, and memory-mapped files
- SDL3 and POSIX platform implementations used by the cross-platform runtime

## Support scope

The instructions in this document describe VxMath's `sdl` branch, which supports
Windows, Linux, and macOS. [NeMo2](https://github.com/doyaGu/NeMo2) assembles it
with the other Virtools runtime modules.

## Building

### NeMo2 engine

```bash
git clone --recurse-submodules https://github.com/doyaGu/NeMo2.git
cd NeMo2
cmake --preset macos-arm64-runtime # choose the preset for your host
cmake --build --preset macos-arm64-runtime-release
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
  -DVXMATH_BUILD_TESTS=OFF \
  -DCMAKE_PREFIX_PATH=/path/to/SDL3
cmake --build build --config Release
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

VxMath is versioned independently. Engine assemblies pin its commit and nested
dependency revisions.

## Application configuration

Include `VxUserConfig.h` and select the application's profile before using
`VxConfig` or starting worker threads:

```cpp
VxSetUserConfigProfile("SampleApplication", "SAMPLE_CONFIG_HOME");
```

`VxGetUserConfigPath(NULL, path)` and `VxConfig::GetRegistryFilePath` use that
profile. The named environment variable overrides the parent directory; the
application directory is appended to it. Without an override, the platform's
user configuration location is used. Names are copied and the environment is
read on each lookup. An explicit name passed to `VxGetUserConfigPath` overrides
the profile name.

The defaults are `NeMo2` and `NEMO_USER_CONFIG_HOME`. Pass null or empty arguments
to restore these defaults. `registry.ini` stores the logical registry roots
inside the selected application directory; selecting a profile does not migrate
existing files.

## License

Apache License 2.0. See [LICENSE](LICENSE).
