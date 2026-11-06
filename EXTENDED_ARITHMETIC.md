# Extended arithmetic

`VXMATH_ENABLE_X87_EMULATION` enables the original evaluation and storage rules
with a portable integer numerical backend. It defaults to `ON`. The backend
does not use the host's `long double` format or native x87 arithmetic.

```sh
cmake -S . -B build -DVXMATH_ENABLE_X87_EMULATION=ON
cmake --build build --config Release
```

The numerical sources are pinned in `deps/softfloat/UPSTREAM.json`, with their
licenses retained alongside the sources. VxMath owns its dependency and builds
independently of IVP. The IVP and VxMath build options remain separate; both
must be enabled for the original physics replay checks.

## Interface and state

`VxFloat80.h` provides plain data structures and functions. `VxFloat80` holds
the significand and combined sign/exponent field. `VxFloatContext` holds precision, rounding
and accumulated exception flags. Neither structure owns resources or allocates
memory. Initialize a context before using it:

```cpp
#include "VxFloat80.h"
#include "VxFPUControlWord.h"

VxFloatContext context;
VxFloatInitContext(context, VxGetFPUControlWord());
const VxFloat80 first = VxFloat80Load(context, 1.0f);
const VxFloat80 second = VxFloat80Load(context, 3.0f);
const VxFloat80 quotient = VxFloat80Divide(context, first, second);
const float result = VxFloat80StoreFloat(context, quotient);
```

Alternatively, initialize with an explicit `VxFloatPrecision` and
`VxFloatRounding`. The supported significand precisions are 24, 53 and 64 bits,
with nearest-even, downward, upward and toward-zero rounding. Operations
accumulate the six masked arithmetic exception flags; they do not deliver
hardware exception traps. `VxFloatClearExceptionFlags` clears only those flags.

Loads from binary32, binary64 and signed 32-bit integers are separate from
rounded stores to binary32 and binary64. Register-width values remain
`VxFloat80` between operations. The interface also provides comparison,
square root, signs, integer rounding, scaling, sine, cosine, arctangent,
logarithmic and exponential operations. Transcendental range failures are
reported through the corresponding output argument.

Use `VxFloat80FromBytes` and `VxFloat80ToBytes` for the ten-byte little-endian
extended encoding. Object padding is not part of the encoding. Do not serialize
`sizeof(VxFloat80)` bytes.

The existing `VxGetFPUControlWord`, `VxSetFPUControlWord` and
`VxSetBaseFPUControlWord` API owns thread control. On processors without x87,
the same API maintains a software control word. Native-host IVP uses this API
as well, so precision changes remain consistent across the engine and physics.

## Integration and performance

Normal matrix, free-vector and OBB query APIs select their implementation at
build time. Callers continue using `Vx3DMultiplyMatrix`, `Vx3DInverseMatrix`,
`Vx3DRotateVector`, `Vx3DMultiplyMatrixVector`, `Magnitude`, `Normalize` and
`VxIntersect::OBBOBB`. Their original float stores and operation order are part
of the implementation, including zero, overflow and contact boundaries.

Binary32 matrix operations use the existing VxMath SIMD abstraction where it
is available, including its SIMDe provider. The scalar implementation evaluates
the same binary32 operations in software. Extended operations always use the
integer backend. Floating-point contraction is disabled, and supported Release
builds enable interprocedural optimization across the numerical sources.

The CMake target exports the selected mode to consumers. Rebuild all consumers
when changing it: emulated matrix and free-vector functions are library entry
points; disabling emulation retains their existing inline implementations.

VxMath headers and inline files use matching `#ifndef` / `#define` include
guards. New interfaces follow the existing global function naming style.

## Independent verification

The numerical tests do not require SDL or game assets:

```sh
cmake -S tests/numerics -B build/numerics
cmake --build build/numerics --config Release
ctest --test-dir build/numerics -C Release --output-on-failure
```

The same cases run with SIMD enabled and disabled. They check precision,
rounding, exception state, register encoding, original matrix and vector
snapshots, and original OBB query outputs at all three precisions.
`Float80Benchmark` and `Float80ScalarBenchmark` report throughput and checksums.
These measure numerical work; whole-game performance is checked separately.
