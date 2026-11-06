#ifndef VXOBBOVERLAP_H
#define VXOBBOVERLAP_H

#include <math.h>

#include "VxOBB.h"
#include "VxFloat80.h"
#include "VxFPUControlWord.h"

static VxFloat80 VxOBBProductSum(VxFloatContext &context, const float *pairs, int count, bool paired = false) {
    VxFloat80 sum = VxFloat80Multiply(context, VxFloat80Load(context, pairs[0]), VxFloat80Load(context, pairs[1]));
    if (paired) {
        const VxFloat80 next =
            VxFloat80Multiply(context, VxFloat80Load(context, pairs[2]), VxFloat80Load(context, pairs[3]));
        sum = VxFloat80Add(context, sum, next);
        const VxFloat80 third =
            VxFloat80Multiply(context, VxFloat80Load(context, pairs[4]), VxFloat80Load(context, pairs[5]));
        const VxFloat80 fourth =
            VxFloat80Multiply(context, VxFloat80Load(context, pairs[6]), VxFloat80Load(context, pairs[7]));
        sum = VxFloat80Add(context, sum, VxFloat80Add(context, third, fourth));
    } else {
        for (int i = 1; i < count; ++i) {
            const VxFloat80 next = VxFloat80Multiply(context, VxFloat80Load(context, pairs[i * 2]),
                                                     VxFloat80Load(context, pairs[i * 2 + 1]));
            sum = VxFloat80Add(context, sum, next);
        }
    }
    return sum;
}

static float VxOBBDot3(VxFloatContext &context, const VxVector &a, const VxVector &b, int first, int second,
                       int third) {
    const float pairs[] = {a[first], b[first], a[second], b[second], a[third], b[third]};
    return VxFloat80StoreFloat(context, VxOBBProductSum(context, pairs, 3));
}

static bool VxOBBRadiusContains(VxFloatContext &context, const float *radiusPairs, const float *projectionPairs,
                                int projectionTerms, bool paired = false) {
    const VxFloat80 radius = VxOBBProductSum(context, radiusPairs, 4, paired);
    const VxFloat80 projection = VxOBBProductSum(context, projectionPairs, projectionTerms);
    // All fifteen original comparisons accept unordered results.
    return VxFloat80Compare(context, radius, VxFloat80Absolute(context, projection)) != VXFLOAT_LESS;
}

static XBOOL VxOBBOverlap(const VxOBB &a, const VxOBB &b) {
    VxFloatContext context;
    VxFloatInitContext(context, VxGetFPUControlWord());
    // Original queries use orthonormal SAT identities for saved box axes,
    // including slightly skewed frames. Preserve each float storage boundary
    // and the summation order at contact and overflow boundaries.
    const VxVector delta(b.m_Center.x - a.m_Center.x, b.m_Center.y - a.m_Center.y, b.m_Center.z - a.m_Center.z);
    float rotation[3][3], absolute[3][3], translation[3];
    for (int i = 0; i < 3; ++i) {
        translation[i] =
            i == 0 ? VxOBBDot3(context, delta, a.m_Axis[i], 1, 0, 2) : VxOBBDot3(context, delta, a.m_Axis[i], 2, 1, 0);
        for (int j = 0; j < 3; ++j) {
            if (j == 0)
                rotation[i][j] = i == 0 ? VxOBBDot3(context, a.m_Axis[i], b.m_Axis[j], 2, 0, 1)
                                        : VxOBBDot3(context, a.m_Axis[i], b.m_Axis[j], 1, 2, 0);
            else
                rotation[i][j] = VxOBBDot3(context, a.m_Axis[i], b.m_Axis[j], 2, 1, 0);
            absolute[i][j] = fabsf(rotation[i][j]);
        }
        const int first = i == 2 ? 2 : 0;
        const int second = i == 2 ? 1 : 2;
        const int third = i == 2 ? 0 : 1;
        const float radius[] = {absolute[i][first], b.m_Extents[first], absolute[i][second], b.m_Extents[second],
                                absolute[i][third], b.m_Extents[third], a.m_Extents[i],      1};
        const float projection[] = {translation[i], 1};
        if (!VxOBBRadiusContains(context, radius, projection, 1))
            return FALSE;
    }
    for (int j = 0; j < 3; ++j) {
        const float radius[] = {absolute[2][j], a.m_Extents[2], absolute[0][j], a.m_Extents[0],
                                absolute[1][j], a.m_Extents[1], b.m_Extents[j], 1};
        const int second = j == 0 ? 0 : 1, third = j == 0 ? 1 : 0;
        const float projection[] = {delta.z,      b.m_Axis[j].z,     delta[second], b.m_Axis[j][second],
                                    delta[third], b.m_Axis[j][third]};
        if (!VxOBBRadiusContains(context, radius, projection, 3))
            return FALSE;
    }
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            const int u = (i + 1) % 3, v = (i + 2) % 3;
            const int k = (j + 1) % 3, l = (j + 2) % 3;
            float radius[] = {absolute[u][j], a.m_Extents[v], absolute[v][j], a.m_Extents[u],
                              absolute[i][l], b.m_Extents[k], absolute[i][k], b.m_Extents[l]};
            if (i == 0 && j == 0) {
                for (int n = 0; n < 4; ++n) {
                    const float saved = radius[n];
                    radius[n] = radius[n + 4];
                    radius[n + 4] = saved;
                }
            } else if (i < 2 && j > 0) {
                const float c = radius[4], e = radius[5];
                radius[4] = radius[6];
                radius[5] = radius[7];
                radius[6] = c;
                radius[7] = e;
            }
            const float projection[] = {translation[v], rotation[u][j], -rotation[v][j], translation[u]};
            if (!VxOBBRadiusContains(context, radius, projection, 2, true))
                return FALSE;
        }
    return TRUE;
}

#endif // VXOBBOVERLAP_H
