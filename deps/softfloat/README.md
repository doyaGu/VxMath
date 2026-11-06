# Extended-precision arithmetic dependency

This directory contains the portable numeric part of Bochs at commit
`0e850b793a8b89b349d3221945c3109978a39f3c`: Berkeley SoftFloat 3e arithmetic,
the x87 transcendental functions, and their integer polynomial helpers.
It does not contain a CPU, instruction decoder, or device emulator.

The imported files are unchanged. `UPSTREAM.json` records each file's SHA256.
The local CMake adapter provides the integer types and byte order, selects
portable 64-bit integer primitives, and builds a private static target. It does
not wrap the source in a namespace, use the host's `long double` representation,
or require native x87 instructions.

Each operation receives its rounding/precision/exception state explicitly.
The 80-bit arithmetic supports 24-, 53-, and 64-bit significands; conversion
to binary32 or binary64 is a separate operation. The transcendental sources
also model the internal precision used by the x87 polynomial algorithms.

The independent arithmetic tests and benchmark can be built from
`tests/numerics`. The benchmark reports arithmetic throughput and a checksum;
it does not measure the cost of a complete simulation step.

Upstream sources and license notices:

- [Bochs numerical source](https://github.com/bochs-emu/Bochs/tree/0e850b793a8b89b349d3221945c3109978a39f3c/bochs/cpu)
- [Berkeley SoftFloat](https://www.jhauser.us/arithmetic/SoftFloat.html)
- `cpu/softfloat3e/COPYING.txt`, `BOCHS-COPYING`, `BOCHS-LICENSE`, and the notices
  retained in each imported source file.
