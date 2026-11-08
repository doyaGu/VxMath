# Extended arithmetic

`VXMATH_ENABLE_X87_EMULATION` enables the original evaluation and storage rules
for the covered routines, using a portable integer numerical backend. It defaults to `ON`. The backend
does not use the host's `long double` format or native x87 arithmetic.

```sh
cmake -S . -B build -DVXMATH_ENABLE_X87_EMULATION=ON
cmake --build build --config Release
```

The numerical sources are pinned in `deps/softfloat/UPSTREAM.json`, with their
licenses retained alongside the sources. VxMath owns its dependency and builds
independently of IVP. The IVP and VxMath build options remain separate; both
must be enabled for the original physics replay checks.

Coverage is intentionally limited to the scalar interface, the selected matrix
operations, free vector magnitude/normalization and OBB intersection routines
described below. Enabling the option does not rewrite ordinary floating-point
expressions, vector members or every other mathematical routine. Third-party
implementations are outside this coverage. IVP owns its independent arithmetic
and option; neither library selects the other's mode.

## Interface and state

Consumers compose scalar expressions through `VxScalar.h`: initialize a
`VxScalarContext`, load float, double or integer inputs, apply scalar operations,
and store at the required write boundaries. The value and context are plain
data. Callers do not inspect their representation, select a backend or test
the emulation build option. With emulation disabled the same interface uses
native double intermediates and does not link the numerical backend.
These evaluation rules apply to explicit scalar operations. Ordinary mixed
float/double expressions continue to use C++ promotion rules.
`VxScalarStore` and `VxScalarStoreDouble` provide explicit binary32/binary64
write boundaries in either mode.

```cpp
#include "VxScalar.h"

VxScalarContext arithmetic;
VxScalarInitContext(arithmetic);
const VxScalar product = VxScalarMultiply(arithmetic,
    VxScalarLoad(arithmetic, first), VxScalarLoad(arithmetic, second));
const float result = VxScalarStore(arithmetic,
    VxScalarAdd(arithmetic, product, VxScalarLoad(arithmetic, offset)));
```

Keep domain algorithms in their owning module. The scalar interface supplies
arithmetic primitives and explicit storage; it does not implement behaviors,
simulation steps or application formulas.

`Normalize(vector, signedLength)` computes and stores each scaled vector component.
Free normalization and magnitude functions remain library entry points
in both modes, so a caller's instruction set cannot select another implementation.
Supported Release builds retain interprocedural optimization in both modes.
`VxInitializeMathEnvironment` initializes the current thread before
runtime object construction and after external driver initialization. Hosts
call these ordinary interfaces without configuring precision registers.

`VxFloat80.h` is the fixed-format backend interface for numerical implementation
and verification. It is available in emulation builds and provides plain data
structures and functions. `VxFloat80` holds
the significand and combined sign/exponent field. `VxFloatContext` holds precision, rounding
and accumulated exception flags. Neither structure owns resources or allocates
memory.

Value and context alignment remain the same when these headers are included
under a host packing directive. The context keeps its native four-byte
alignment; the value keeps its eight-byte alignment. Header inclusion does
not change the host's packing setting.

Initialize a context before using it:

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
Each context snapshots its precision and rounding when initialized. Later
thread-control changes affect newly initialized contexts; existing sequences
retain their own settings and exception flags. Flags are local to the context,
not a replacement for the thread's hardware status register.

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
These control functions preserve thread-local precision, rounding and exception
settings in both build modes. Disabling emulation removes its numerical objects
and dependency; it does not remove the native environment API or Win32
floating-point ABI transfers.

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
when changing it: scalar representations and the matrix implementation depend
on that selection. Free magnitude and normalization functions always remain
library entry points.

`cmake/VxScalar.cmake` also provides `vx_add_scalar_arithmetic(target)` for
hosts that supply a separate vector/matrix ABI. This target shares the same
headers and backend source as full VxMath; it does not replace the host's other
math symbols. Its backend declarations use `VXMATH_SCALAR_STATIC` independently
of the host's existing DLL imports.

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
`ScalarNativeTest` links neither VxMath nor the numerical backend, so an
accidental dependency from the disabled scalar interface fails at link time.

`tests/scalar` exercises the scalar-only provider without SDL or GoogleTest.
Configure the same build directory with `VXMATH_ENABLE_X87_EMULATION` set to
`ON`, `OFF`, then `ON` again to verify consumer rebuilding and backend removal.
