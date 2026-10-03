"""Capture synthetic quaternion operations from the original Win32 VxMath.dll."""
import argparse
import hashlib
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
dll = args.dll.resolve(strict=True)
runner = args.runner.resolve(strict=True)
output = args.output_dir.resolve()
output.mkdir(parents=True, exist_ok=True)
digest = hashlib.sha256(dll.read_bytes()).hexdigest()
if digest != 'bd17dddb747c943e6e090471305aeda7f87cfbca401b3fada36be400285973a2':
    raise RuntimeError('VxMath.dll does not match the IDA reference')

identity = [0, 0, 0, 1]
zero = [0, 0, 0, 0]
quaternions = [identity, zero, [.6, 0, 0, .8], [0, .8, 0, .6], [0, 0, -.6, -.8],
               [.2, -.3, .4, -.5], [1, -2, 3, -4], [0, 0, 0, -1],
               [1e-12, -2e-12, 3e-12, 4e-12], [1e-8, 0, 0, -1]]
rng = random.Random(0x2429D650)
for _ in range(16):
    q = [rng.uniform(-1, 1) for _ in range(4)]
    length = math.sqrt(sum(x*x for x in q))
    quaternions.append([x/length for x in q])
rows = []
for op in range(10):
    if op in (6, 7):
        pairs = [(quaternions[2], quaternions[3]), (quaternions[2], quaternions[4]),
                 (identity, [0, 0, .01, math.sqrt(1-.0001)]), (identity, [0, 0, .2, math.sqrt(.96)]),
                 (identity, [0, 0, 0, -1])]
        pairs += list(zip(quaternions[10:18], quaternions[18:26]))
        for a, b in pairs:
            for t in [-.5, 0, .125, .5, .875, 1, 1.5]:
                rows.append((op, t, a, b, quaternions[3], quaternions[4]))
    else:
        for i, a in enumerate(quaternions):
            b = quaternions[(i + 3) % len(quaternions)]
            rows.append((op, .375, a, b, identity, identity))
        if op in (1, 2, 5, 9):
            for b in [zero, identity, [0, 1.6, 0, 1.2], [0, 1e-12, 0, 1e-12]]:
                rows.append((op, .375, quaternions[2], b, identity, identity))
        if op == 8:
            for scale in [1e-30, 1e-20, 1e20, 1e30]:
                rows.append((op, 0, [scale, -2*scale, 3*scale, -4*scale], identity, identity, identity))
        if op == 3:
            for scale in [1e-30, 1e-20, 1e20, 1e30]:
                rows.append((op, 0, [scale, -2*scale, 3*scale, -4*scale], identity, identity, identity))
        if op == 4:
            for scale in [2**-24, 2**-23, 2**-22, 1e-6, 10, 100, 1000, 10000]:
                rows.append((op, 0, [scale, -.5*scale, .25*scale, 0], identity, identity, identity))

for a in quaternions:
    rows.append((10, 0, a, a, identity, identity))
# Native Slerp accumulates the dot product in z,y,x,w order, including
# cancellation before hemisphere and linear/spherical branch selection.
for a, b in [([1e10, 1e10, 1, 1], [1e10, -1e10, 1, 0]),
             ([1, 1e10, 1e10, 1], [1, 1e10, -1e10, 0]),
             ([1e10, 1e10, 1, 1], [-1e10, 1e10, -1, 0]),
             ([1, 1e10, 1e10, 1], [-1, -1e10, 1e10, 0])]:
    for t in [-2, .25, .5, 1, 2]:
        rows.append((6, t, a, b, identity, identity))
for dot in [.9899999, .99, .9900001, -.9899999, -.99, -.9900001]:
    for t in [-2, .25, .5, 1, 2]:
        rows.append((6, t, identity, [math.sqrt(1-dot*dot), 0, 0, dot], identity, identity))
# Unit quaternion pairs with a relative rotation near 180 degrees. Their dot
# products straddle zero depending on accumulation order (seed 20261002).
orthogonal_pairs = [
    ([.613120973, -.279877067, -.728119254, .124875493], [.358572572, .888067305, -.086475648, -.274379551]),
    ([-.218302429, -.269470304, .508761585, .787966669], [.0448374301, .914051294, -.101035178, .390245706]),
    ([.81518364, -.153842688, .158747986, -.535357058], [.338311702, -.628641427, -.393824935, .579013824]),
    ([-.603040218, -.782229185, .152499065, -.0346991532], [.00583435176, -.197414249, -.902412295, .382943422]),
]
for a, b in orthogonal_pairs:
    for t in [.25, .5, .75]:
        for op in (6, 7):
            rows.append((op, t, a, b, quaternions[3], quaternions[4]))

# Exp executes x87 FSIN/FCOS directly; test large angles and the 2**63 limit.
for scale in [1e5, 1e8, 1e12, 1e16, 2**62, 2**63-2**39, 2**63, 1e20, 1e30, 1e38]:
    for direction in [[1, 0, 0], [1, -.5, .25], [.3, -.7, .2]]:
        rows.append((4, 0, [scale*x for x in direction]+[0], identity, identity, identity))
largest = struct.unpack('<f', bytes.fromhex('ffff7f7f'))[0]
rows.append((4, 0, [largest, largest, largest, 0], identity, identity, identity))
# Independent seeded large-angle coverage for the software x87 phase correction.
angle_rng = random.Random(0x2429C720)
for _ in range(64):
    scale = math.ldexp(angle_rng.uniform(.5, 1), angle_rng.randrange(20, 64))
    a = [scale*angle_rng.uniform(-1, 1) for _ in range(3)] + [0]
    rows.append((4, 0, a, identity, identity, identity))
for a in [[math.inf, 1, 2, 0], [-math.inf, 0, 0, 0], [math.nan, 1, 2, 0],
          [1, math.nan, 2, 0], [1, 2, 3, math.nan]]:
    rows.append((4, 0, a, identity, identity, identity))
for scale in [1e20, 1e30, 1e38]:
    for a, b in [([scale, scale, 1, 1], [scale, -scale, 1, 0]),
                 ([1, scale, scale, 1], [1, scale, -scale, 0]),
                 ([scale, scale, 1, 1], [-scale, scale, -1, 0]),
                 ([1, scale, scale, 1], [-1, -scale, scale, 0])]:
        for t in [-.5, .25, .5, 1, 1.5]:
            rows.append((6, t, a, b, identity, identity))
# The original tests the dot's sign before its float store underflows to zero.
for a, b in [([1e-30, 0, 0, 0], [-1e-30, 0, 0, 1]),
             ([0, -1e-30, 0, 0], [0, 1e-30, 0, 1])]:
    for t in [.25, .5, .75]:
        rows.append((6, t, a, b, identity, identity))

# Extrapolation reaches large x87 sine arguments even with unit endpoints.
# Keep both hemispheres and the linear branch, then check Squad's nested calls.
for a, b in [(identity, [.6, 0, 0, .8]),
             (identity, [-.6, 0, 0, -.8]),
             (identity, [1, 0, 0, 0]),
             (identity, [0, 0, .01, math.sqrt(1-.0001)]),
             ([1, -2, 3, -4], [2, -3, 4, -5])]:
    for t in [-1e30, -1e20, -2**63, -1e12, -1e8, -1e4, -10,
              10, 1e4, 1e8, 1e12, 2**63, 1e20, 1e30, largest]:
        for op in (6, 7):
            rows.append((op, t, a, b, quaternions[3], quaternions[4]))
# Selected nonfinite inputs distinguish the unordered dot-product branch
# from explicit invalid trigonometric arguments and overflowed Squad factors.
for t in [math.nan, math.inf, -math.inf]:
    for a, b in [(identity, [.6, 0, 0, .8]), (identity, identity)]:
        for op in (6, 7):
            rows.append((op, t, a, b, quaternions[3], quaternions[4]))
for a, b in [([math.nan, 1, 2, 3], identity), (identity, [math.nan, 1, 2, 3]),
             ([math.inf, 1, 2, 3], identity), (identity, [math.inf, 1, 2, 3]),
             ([largest]*4, [largest]*4)]:
    for t in [0, .25, .5, 1, 2]:
        rows.append((6, t, a, b, identity, identity))

interpolation_rng = random.Random(0x2429C8F0)
for _ in range(64):
    controls = []
    for _ in range(4):
        q = [interpolation_rng.uniform(-1, 1) for _ in range(4)]
        norm = math.sqrt(sum(x*x for x in q))
        controls.append([x/norm for x in q])
    t = interpolation_rng.choice([-1, 1])*10**interpolation_rng.uniform(0, 12)
    for op in (6, 7):
        rows.append((op, t, *controls))
# A weight smaller than the least float may still have a visible product
# with a large non-unit endpoint; the original does not store weights as float.
smallest = struct.unpack('<f', bytes.fromhex('01000000'))[0]
for t in [smallest, -smallest, 2*smallest, -2*smallest]:
    for a, b in [([0, 0, 0, 1e-38], [1e38, 0, 0, 8e37]),
                 ([0, 0, 0, 1e-38], [-1e38, 0, 0, -8e37])]:
        rows.append((6, t, a, b, identity, identity))

# Multiplication and the relative-rotation quotient have distinct accumulation
# orders. Cancellation and products outside float range distinguish them.
product_pairs = []
for scale in [1e-30, 1e-20, 1e-10, 1, 1e10, 1e20, 1e30, 1e38]:
    product_pairs += [([scale]*4, [scale]*4),
                      ([scale, -scale, scale, -scale], [scale]*4),
                      ([scale, scale, 1, 1], [scale, -scale, 1, 0]),
                      ([1, scale, scale, 1], [1, scale, -scale, 0]),
                      ([scale, -2*scale, 3*scale, -scale], [.2, -.3, .4, -.5])]
product_pairs += [([largest]*4, [largest]*4),
                  ([math.inf, 1, 2, 3], [1, 2, 3, 4]),
                  ([math.nan, 1, 2, 3], [1, 2, 3, 4])]
product_rng = random.Random(0x2429D5C0)
for _ in range(64):
    a = [product_rng.choice([-1, 1])*10**product_rng.uniform(-30, 30) for _ in range(4)]
    b = [product_rng.choice([-1, 1])*10**product_rng.uniform(-30, 30) for _ in range(4)]
    product_pairs.append((a, b))
for a, b in product_pairs:
    for op in (1, 2, 5, 9):
        rows.append((op, 0, a, b, identity, identity))
    rows.append((10, 0, a, a, identity, identity))

# Normalization/logarithm retain x87's exponent range for every finite float.
# Include the normal/subnormal square boundary, all component positions, and
# zero vector parts with nonfinite scalar parts to exercise branch behavior.
norm_inputs = [[0, 0, 0, 0], [-0.0, -0.0, -0.0, -0.0],
               [largest]*4, [largest, -largest, largest, -largest]]
# The squares fit normal floats, but the smallest normalized component is
# subnormal. PC=24 multiplication followed by a float store rounds twice.
for component in range(4):
    for sign in [-1, 1]:
        q = [8e18]*4
        q[component] = sign*1.1e-19
        norm_inputs.append(q)
for scale in [smallest, 2**-126, 1e-30, 2**-64, 2**-63, 2**-62,
              1e-10, 1, 1e10, 2**62, 2**63, 2**64, 1e30, 1e38]:
    norm_inputs += [[scale, -scale, scale, -scale],
                    [scale, -2*scale, 3*scale, -scale],
                    [scale, 0, 0, -1], [scale, -scale, scale, 0]]
    for component in range(4):
        q = [0, 0, 0, 0]
        q[component] = scale
        norm_inputs.append(q)
for special in [math.inf, -math.inf, math.nan]:
    for component in range(4):
        for base in [[1, -2, 3, -4], [0, 0, 0, 0]]:
            q = base[:]
            q[component] = special
            norm_inputs.append(q)
norm_rng = random.Random(0x2429D6E0)
for _ in range(128):
    norm_inputs.append([norm_rng.choice([-1, 1])*10**norm_rng.uniform(-44, 38) for _ in range(4)])
for a in norm_inputs:
    for op in (3, 8):
        rows.append((op, 0, a, identity, identity, identity))

# Exp retains the sine in an x87 register until division by the length.
# Exercise the epsilon branch, ordinary angles and small vector components.
exp_inputs = [[0, 0, 0, 0], [-0.0, -0.0, -0.0, math.nan]]
epsilon = 2**-23
for scale in [smallest, 2**-126, 1e-30, 2**-64, 2**-63,
              epsilon-2**-47, epsilon, epsilon+2**-46,
              1e-5, .001, .1, 1, 3, 10, 100, 1000]:
    for direction in [[1, 0, 0], [1, -1, 1], [1, -.5, .25], [.3, -.7, .2]]:
        exp_inputs.append([scale*x for x in direction] + [math.nan])
exp_rng = random.Random(0x2429C766)
for _ in range(128):
    scale = 10**exp_rng.uniform(-8, 3)
    exp_inputs.append([scale*exp_rng.uniform(-1, 1) for _ in range(3)] + [0])
for _ in range(48):
    tiny = struct.unpack('<f', struct.pack('<I', exp_rng.randrange(1, 0x00800001)))[0]
    angle = exp_rng.choice([.3, 1, 3, 10, 100, 1000])
    for a in [[angle, tiny, -tiny, 0], [tiny, angle, -tiny, 0], [tiny, -tiny, angle, 0]]:
        exp_inputs.append(a)
for a in exp_inputs:
    rows.append((4, 0, a, identity, identity, identity))

# Check the Squad blend factor at float range boundaries, as well as near
# zero and one. Its final Slerp call receives a stored float factor.
for t in [smallest, -smallest, 2**-126, -2**-126, 2**-24,
          1-2**-24, 1, 1+2**-23, 2**63-2**39, 2**63,
          2**64, -2**64, largest/2, largest]:
    for controls in [(identity, quaternions[2], quaternions[3], quaternions[4]),
                     (identity, identity, identity, identity),
                     ([1, -2, 3, -4], [2, -3, 4, -5], quaternions[2], quaternions[3])]:
        rows.append((7, t, *controls))

data = struct.pack('<I', len(rows))
for op, t, a, b, c, d in rows:
    data += struct.pack('<I17f', op, t, *a, *b, *c, *d)
input_path = output/'input.bin'
input_path.write_bytes(data)
run = subprocess.run([str(runner), str(dll), str(input_path)], capture_output=True, timeout=20)
if run.returncode or len(run.stdout) != len(rows)*16:
    raise RuntimeError((run.returncode, len(run.stdout), run.stderr))
runtime = run.stderr.decode('ascii').strip()
if runtime != 'x87 control word: 0x007f':
    raise RuntimeError(f'Unexpected reference FPU mode: {runtime}')
values = list(struct.iter_unpack('<4f', run.stdout))


def f(value):
    # Emit the exact float sent to the DLL, not a rounded Python double.
    value = struct.unpack('<f', struct.pack('<f', value))[0]
    if math.isnan(value): return 'std::numeric_limits<float>::quiet_NaN()'
    if math.isinf(value): return ('-' if value < 0 else '') + 'std::numeric_limits<float>::infinity()'
    text = format(value, '.9g')
    if '.' not in text and 'e' not in text:
        text += '.0'
    return text+'f'


header = ['// Synthetic samples from the original Win32 VxMath.dll.',
          '// Generated by tests/quaternion_reference/capture.py; no game assets.',
          '#pragma once', '#include <limits>', '', 'namespace QuaternionReference {',
          'struct Sample { int operation; float t, tolerance; float a[4], b[4], c[4], d[4], expected[4]; };',
          'static const Sample Samples[] = {']
for row, expected in zip(rows, values):
    op, t, *inputs = row
    arrays = ', '.join('{' + ', '.join(map(f, q)) + '}' for q in inputs + [expected])
    # Normalize/Ln and ordinary-angle Exp require equal numeric floats.
    # Signed zero and NaN payload bits are outside this comparison.
    exact = op in (3, 8) or (op == 4 and all(math.isfinite(x) and abs(x) <= 10000 for x in inputs[0][:3]))
    tolerance = 0 if exact else (1e-12 if op == 6 and 0 < abs(t) < 2**-126 else 2e-6)
    header.append(f'    {{{op}, {f(t)}, {f(tolerance)}, {arrays}}},')
header += ['};', '} // namespace QuaternionReference', '']
(output/'QuaternionReferenceSamples.h').write_text('\n'.join(header), encoding='utf-8')
(output/'reference.json').write_text(json.dumps({'sha256':digest,'count':len(rows),'runtime':runtime,'values':values}, indent=2), encoding='utf-8')
print(f'Captured {len(rows)} results across 11 quaternion operations')
