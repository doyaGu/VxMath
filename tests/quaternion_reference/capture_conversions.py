"""Capture original quaternion conversions, Snuggle and their observable side effects."""
import argparse
import hashlib
import itertools
import json
import math
from pathlib import Path
import random
import struct
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--dll', type=Path, required=True)
parser.add_argument('--runner', type=Path, required=True)
parser.add_argument('--output-dir', type=Path, required=True)
args = parser.parse_args()
dll, runner = args.dll.resolve(strict=True), args.runner.resolve(strict=True)
output = args.output_dir.resolve()
output.mkdir(parents=True, exist_ok=True)
digest = hashlib.sha256(dll.read_bytes()).hexdigest()
if digest != 'bd17dddb747c943e6e090471305aeda7f87cfbca401b3fada36be400285973a2':
    raise RuntimeError('VxMath.dll does not match the IDA reference')
rows = []


def add(op, value, unit=1, restore=1, tolerance=3e-6):
    rows.append((op, unit, restore, value + [0]*(16-len(value)), tolerance))


axes = [[1, 0, 0], [0, 1, 0], [0, 0, 1], [1, -2, 3], [0, 0, 0],
        [1e-5, -2e-5, 3e-5], [1e-12, -2e-12, 3e-12]]
for axis, angle in itertools.product(axes, [0, 1e-8, -.3, .7, math.pi/2, math.pi, 4.2]):
    add(0, axis + [angle])
    add(7, axis + [angle])
eulers = [[0, 0, 0], [.3, -.7, 1.2], [-2, 1, -3], [0, math.pi/2, 0],
          [.3, math.pi/2-1e-6, .9], [.3, -math.pi/2+1e-6, .9]]
for x in [5e-9, 1e-8, 2e-8, 1e-7]:
    for axis in range(3):
        v = [0, 0, 0]
        v[axis] = x
        eulers.append(v)
for v in eulers:
    for op in (1, 8):
        add(op, v, tolerance=1e-10 if max(map(abs, v)) < 1e-6 else 3e-6)

quaternions = [[0, 0, 0, 1], [0, 0, 0, -1], [0, 0, 0, 0], [.6, 0, 0, .8],
               [0, .8, 0, .6], [.5, .5, .5, .5], [-.5, .5, -.5, .5],
               [1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [1, -2, 3, -4]]
rng = random.Random(0x2429CAE0)
for _ in range(48):
    q = [rng.uniform(-1, 1) for _ in range(4)]
    length = math.sqrt(sum(x*x for x in q))
    quaternions.append([x/length for x in q])
# Sign parity and equal-component ties select different Snuggle permutations.
quaternions += [list(q) for q in itertools.product([-.5, .5], repeat=4)]
for a, b in itertools.combinations(range(4), 2):
    for sa, sb in itertools.product([-1, 1], repeat=2):
        q = [0]*4
        q[a], q[b] = sa*math.sqrt(.5), sb*math.sqrt(.5)
        quaternions.append(q)
for q in quaternions:
    for op in (2, 5):
        add(op, q)
    for scale in [[1, 1, 1], [2, 1, 1], [1, 2, 1], [1, 1, 2],
                  [1, 2, 3], [-3, 1, 2], [0, 0, 2]]:
        add(6, q + scale)
for scale in [1e-5, 1e-10]:
    add(5, [scale*x for x in [.6, 0, 0, .8]])
for scale in [1e-30, 1e-20, 1e20, 1e30]:
    for q in [[.6, 0, 0, .8], [.2, -.3, .4, -.5]]:
        for op in (2, 5):
            add(op, [scale*x for x in q])


def matrix(q):
    x, y, z, w = q
    return [1-2*(y*y+z*z), 2*(x*y-w*z), 2*(x*z+w*y), 4,
            2*(x*y+w*z), 1-2*(x*x+z*z), 2*(y*z-w*x), 5,
            2*(x*z-w*y), 2*(y*z+w*x), 1-2*(x*x+y*y), 6, 7, 8, 9, 10]


matrices = [matrix(q) for q in quaternions[:2] + quaternions[3:]]
matrices += [[0]*16, [0, -1, 0, 4, 0, 0, -1, 5, 1, 0, 0, 6, 7, 8, 9, 10]]
for m in matrices[:8]:
    scaled = m[:]
    for i, s in enumerate([2, 3, -4]):
        for j in range(3):
            scaled[4*i+j] *= s
    matrices.append(scaled)
matrices.append([2, .3, -.2, 4, .1, 3, .5, 5, -.4, .2, 4, 6, 7, 8, 9, 10])
for m in matrices:
    for unit, restore in [(1, 1), (1, 0), (0, 1), (0, 0)]:
        add(3, m, unit, restore)
    add(4, m)
    add(9, m)
# Direct matrices around the original Euler singularity cutoff (16*FLT_EPSILON).
for magnitude in [1e-8, 1e-7, 1e-6, 2**-19, 3e-6]:
    add(9, [magnitude, 0, -1, 0, 0, .8, .2, 0, 0, -.6, .4, 0, 0, 0, 0, 1])

# Operation 10 reuses unit as alias mode: 0 separate, 1 origin in row 3,
# 2 axis in row 0. These row references are supported by the public matrix API.
for axis, angle, origin, alias in itertools.product(
        [[0, 1, 0], [1, -2, 3], [1e-12, -2e-12, 3e-12], [0, 0, 0]],
        [0, .0001, -.3, 1.2, math.pi],
        [[0, 0, 0], [3, -5, 7], [1e6, -2e6, 3e6]], range(3)):
    add(10, axis + [angle] + origin, unit=alias)

# Direct x87 trigonometry has a different large-angle reduction and leaves
# finite operands outside (-2**63, 2**63) unchanged. Exercise both signs and
# propagate the native matrix results through the quaternion constructors.
large_angles = [sign*angle for sign in (-1, 1) for angle in
                [1e5, 1e8, 1e12, 1e16, 2**62, 2**63-2**39,
                 2**63, 2**63+2**40, 1e20, 1e30]]
for angle in large_angles + [math.inf, -math.inf, math.nan]:
    for axis in [[0, 1, 0], [1, -2, 3]]:
        for op in (0, 7):
            add(op, axis + [angle])
    for alias in range(3):
        add(10, [1, -2, 3, angle, 3, -5, 7], unit=alias)
    for component in range(3):
        for base in [[0, 0, 0], [.3, -.7, 1.2]]:
            v = base[:]
            v[component] = angle
            for op in (1, 8):
                add(op, v)
for angle in [2**63, -2**63, 1e20, -1e20, 1e30]:
    for v in [[angle, angle, 0], [0, angle, angle], [angle, 0, angle],
              [angle, angle, .3], [.3, angle, angle], [angle, .3, angle],
              [angle, angle, angle]]:
        for op in (1, 8):
            add(op, v)
angle_rng = random.Random(0x2429B280)
for _ in range(64):
    v = [angle_rng.uniform(-1, 1)*10**angle_rng.uniform(4, 18.9) for _ in range(3)]
    for op in (1, 8):
        add(op, v)

# ToMatrix has distinct scalar stores and SSE reciprocal arithmetic.
# Include mixed component magnitudes, subnormal squares and nonfinite inputs.
smallest = struct.unpack('<f', bytes.fromhex('01000000'))[0]
largest = struct.unpack('<f', bytes.fromhex('ffff7f7f'))[0]
matrix_quaternions = [[largest]*4, [largest, -largest, largest, -largest]]
for scale in [smallest, 2**-126, 1e-30, 2**-64, 2**-63, 2**-62,
              1e-10, 1, 1e10, 2**62, 2**63, 2**64, 1e30, 1e38]:
    matrix_quaternions += [[scale]*4, [scale, -2*scale, 3*scale, -scale],
                           [scale, 0, 0, 1]]
for special in [math.inf, -math.inf, math.nan]:
    for component in range(4):
        for base in [[1, -2, 3, -4], [0, 0, 0, 0]]:
            q = base[:]
            q[component] = special
            matrix_quaternions.append(q)
matrix_rng = random.Random(0x242836E0)
for _ in range(64):
    matrix_quaternions.append([matrix_rng.choice([-1, 1])*10**matrix_rng.uniform(-44, 38) for _ in range(4)])
for q in matrix_quaternions:
    add(5, q)
    add(2, q)

# Trace order and the negative-trace radicand are observable with cancellation.
# Preserve non-rotation entries to check the full RestoreMat side effect.
boundary_matrices = []
for scale in [2**24, 1e20, 1e30, largest]:
    for diagonal in itertools.permutations([scale, -scale, 1]):
        m = [diagonal[0], .25, -.5, 4, -.75, diagonal[1], 1, 5,
             1.25, -1.5, diagonal[2], 6, 7, 8, 9, 10]
        boundary_matrices.append(m)
for diagonal in [[largest]*3, [-largest]*3, [largest, -largest, -largest],
                 [-1, -1, -1], [math.nan, 1, 2], [1, math.nan, 2],
                 [1, 2, math.nan], [math.inf, 1, 2], [-math.inf, 1, 2]]:
    m = [diagonal[0], .25, -.5, 4, -.75, diagonal[1], 1, 5,
         1.25, -1.5, diagonal[2], 6, 7, 8, 9, 10]
    boundary_matrices.append(m)
for _ in range(64):
    m = [matrix_rng.uniform(-3, 3) for _ in range(16)]
    boundary_matrices.append(m)
for m in boundary_matrices:
    for unit, restore in [(1, 1), (1, 0), (0, 1), (0, 0)]:
        add(3, m, unit, restore)
    add(4, m)

# Snuggle chooses between one-, two- and four-component permutations.
# Adjacent floats at score ties can change the returned basis and Scale order.
def adjacent_positive(value, offset):
    bits = struct.unpack('<I', struct.pack('<f', value))[0]
    return struct.unpack('<f', struct.pack('<I', bits + offset))[0]


snuggle_inputs = [[0]*4, [-0.0]*4, [largest]*4, [largest, -largest, largest, -largest]]
for first, second in itertools.combinations(range(4), 2):
    for signs, offset in itertools.product(itertools.product([-1, 1], repeat=2), [-2, -1, 0, 1, 2]):
        q = [0]*4
        q[first] = signs[0]*math.cos(math.pi/8)
        q[second] = signs[1]*adjacent_positive(math.sin(math.pi/8), offset)
        snuggle_inputs.append(q)
for base in [[math.sqrt(.75)] + [math.sqrt(1/12)]*3,
             [math.cos(math.pi/8)/math.sqrt(2)]*2 + [math.sin(math.pi/8)/math.sqrt(2)]*2]:
    for component, offset in itertools.product(range(4), [-2, -1, 0, 1, 2]):
        q = base[:]
        q[component] = adjacent_positive(q[component], offset)
        snuggle_inputs.append(q)
for scale in [smallest, 2**-126, 1e-30, 1e-20, 1e-10, 1e10, 1e20, 1e30, 1e38]:
    snuggle_inputs += [[scale]*4, [scale, -scale, scale, -scale]]
for special, component in itertools.product([math.inf, -math.inf, math.nan], range(4)):
    q = [.1, -.2, .3, -.4]
    q[component] = special
    snuggle_inputs.append(q)
snuggle_rng = random.Random(0x2429CFDA)
for _ in range(64):
    q = [snuggle_rng.uniform(-1, 1) for _ in range(4)]
    norm = math.sqrt(sum(x*x for x in q))
    snuggle_inputs.append([x/norm for x in q])
for q in snuggle_inputs:
    for scale in [[1, 1, 1], [2, 1, 1], [1, 2, 1], [1, 1, 2], [1, 2, 3]]:
        add(6, q + scale)
for scale in [[-1, -1, 2], [-1, 2, -1], [2, -1, -1], [0, -0.0, 2],
              [math.inf]*3, [math.inf, math.inf, 1], [math.nan, 1, 1],
              [1, math.nan, 1], [1, 1, math.nan], [1, math.nan, 2]]:
    for q in [[.1, -.2, .3, -.4], [.5]*4]:
        add(6, q + scale)

# Row normalization exposes the axis helper without trigonometric error.
# Scalar x87 retains wide exponents at 24-bit precision; SSE deliberately uses
# the original unrefined RSQRTSS, including its zero/overflow behavior.
normalization_axes = [[0]*3, [-0.0]*3, [largest]*3, [largest, -largest, largest]]
for scale in [smallest, 2**-140, 2**-126, 1e-30, 2**-75, 2**-64, 2**-63,
              1e-10, 1, 1e10, 2**62, 2**63, 2**64, 1e30, 1e38]:
    normalization_axes += [[scale, -2*scale, 3*scale],
                           [scale, smallest, -smallest], [scale, 1, -2**-126]]
for special, component in itertools.product([math.inf, -math.inf, math.nan], range(3)):
    axis = [1, -2, 3]
    axis[component] = special
    normalization_axes.append(axis)
axis_rng = random.Random(0x24282C00)
for _ in range(128):
    normalization_axes.append([axis_rng.choice([-1, 1])*10**axis_rng.uniform(-44, 38) for _ in range(3)])
normalization_pairs = [(axis, normalization_axes[(index*37+11) % len(normalization_axes)])
                       for index, axis in enumerate(normalization_axes)]
# Tiny cross products can survive subtraction before the final float store.
for tiny, offset, component in itertools.product([2**-74, 2**-75, 2**-76, 2**-126], [-1, 0, 1], range(3)):
    a, b = [tiny, tiny, tiny], [tiny, adjacent_positive(tiny, offset), -tiny]
    a[component] = b[component] = 1
    normalization_pairs.append((a, b))
for a, b in normalization_pairs:
    m = a + [4] + b + [5, 1.25, -1.5, .75, 6, 7, 8, 9, 10]
    for restore in (0, 1):
        add(3, m, unit=0, restore=restore)
for axis, angle in itertools.product(normalization_axes, [-.3, .7, math.pi]):
    add(0, axis + [angle])
    add(7, axis + [angle])

raw = struct.pack('<I', len(rows)) + b''.join(struct.pack('<Iii16f', op, unit, restore, *v) for op, unit, restore, v, _ in rows)
input_path = output/'conversion-input.bin'
input_path.write_bytes(raw)
run = subprocess.run([str(runner), str(dll), str(input_path)], capture_output=True, timeout=30)
if run.returncode or len(run.stdout) != len(rows)*80:
    raise RuntimeError((run.returncode, len(run.stdout), run.stderr))
values = list(struct.iter_unpack('<20f', run.stdout))
scalar_run = subprocess.run([str(runner), str(dll), str(input_path), '--scalar'], capture_output=True, timeout=30)
if scalar_run.returncode or len(scalar_run.stdout) != len(rows)*80:
    raise RuntimeError((scalar_run.returncode, len(scalar_run.stdout), scalar_run.stderr))
scalar_values = list(struct.iter_unpack('<20f', scalar_run.stdout))
runtime = run.stderr.decode('ascii').strip()
scalar_runtime = scalar_run.stderr.decode('ascii').strip()
if runtime != 'x87 control word: 0x007f' or scalar_runtime != runtime:
    raise RuntimeError(f'Unexpected reference FPU mode: {runtime}, {scalar_runtime}')


def f(v):
    v = struct.unpack('<f', struct.pack('<f', v))[0]
    if math.isnan(v):
        return 'std::numeric_limits<float>::quiet_NaN()'
    if math.isinf(v):
        return ('-' if v < 0 else '') + 'std::numeric_limits<float>::infinity()'
    s = format(v, '.9g')
    return s + ('.0f' if '.' not in s and 'e' not in s else 'f')


header = ['// Synthetic original-DLL outputs, including input mutations. No game assets.',
          '// Generated by tests/quaternion_reference/capture_conversions.py.', '#pragma once', '#include <limits>',
          'namespace QuaternionConversionReference {',
          'struct Sample { int operation, unit, restore; float tolerance, input[16], expected[20], scalar[20]; };',
          'static const Sample Samples[] = {']
for (op, unit, restore, v, tolerance), expected, scalar in zip(rows, values, scalar_values):
    # Matrix/quaternion conversions and scoring require equal numeric floats,
    # retaining classification checks for NaN/Inf.
    if op in (3, 4, 5, 6):
        tolerance = 0
    header.append('    {%d, %d, %d, %s, {%s}, {%s}, {%s}},' % (op, unit, restore, f(tolerance), ', '.join(map(f, v)), ', '.join(map(f, expected)), ', '.join(map(f, scalar))))
header += ['};', '} // namespace QuaternionConversionReference', '']
(output/'QuaternionConversionSamples.h').write_text('\n'.join(header), encoding='utf-8')
(output/'conversion-reference.json').write_text(json.dumps({'sha256': digest, 'count': len(rows), 'runtime': runtime, 'scalar_runtime': scalar_runtime, 'values': values, 'scalar': scalar_values}, indent=2), encoding='utf-8')
print(f'Captured {len(rows)} conversion results across 11 operations')
