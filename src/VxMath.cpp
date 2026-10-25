#include <stdio.h>
#include <atomic>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

#include "VxMath.h"
#include "VxEigenMatrix.h"
#include "VxAtomic.h"
#include "VxSIMDDispatchInternal.h"

extern void InitializeTables();

#if defined(_WIN32)
HINSTANCE g_hinstDLL;
#endif

#if defined(VX_SIMD_SSE)
static inline void VxSIMDProjectPointToAxes3(
    const float *point,
    __m128 axisX,
    __m128 axisY,
    __m128 axisZ,
    float &outX,
    float &outY,
    float &outZ
) {
    const __m128 v = VxSIMDLoadFloat3(point);
    outX = _mm_cvtss_f32(VxSIMDDotProduct3(v, axisX));
    outY = _mm_cvtss_f32(VxSIMDDotProduct3(v, axisY));
    outZ = _mm_cvtss_f32(VxSIMDDotProduct3(v, axisZ));
}
#endif

static XBOOL FillStructureScalarImpl(int Count, void *Dst, XDWORD Stride, XDWORD SizeSrc, const void *Src);
static XBOOL CopyStructureScalarImpl(int Count, void *Dst, XDWORD OutStride, XDWORD SizeSrc, const void *Src, XDWORD InStride);
static XBOOL IndexedCopyScalarImpl(const VxStridedData &Dst, const VxStridedData &Src, XDWORD SizeSrc, const int *Indices, int IndexCount);
static XBOOL PtInRectScalarImpl(CKRECT *rect, CKPOINT *pt);
static XBOOL TransformBox2DScalarCore(const VxMatrix &World_ProjectionMat, const VxBbox &box, VxRect *ScreenSize, VxRect *Extents, VXCLIP_FLAGS &OrClipFlags, VXCLIP_FLAGS &AndClipFlags);
static void ProjectBoxZExtentsScalarCore(const VxMatrix &World_ProjectionMat, const VxBbox &box, float &ZhMin, float &ZhMax);
static void ProjectPointsToAxesExtentsScalar(const XBYTE *Points, XDWORD Stride, int Count, const VxMatrix &BBoxMatrix, float &minX, float &maxX, float &minY, float &maxY, float &minZ, float &maxZ);

static inline float VxScreenReciprocalAbs(float w) {
#if defined(VX_SIMD_SSE)
    __m128 reciprocal = _mm_rcp_ss(_mm_set_ss(w));
    reciprocal = _mm_and_ps(reciprocal, VX_SIMD_ABS_MASK);
    return _mm_cvtss_f32(reciprocal);
#else
    return 1.0f / XAbs(w);
#endif
}

#if defined(VX_SIMD_SSE)
static XBOOL FillStructureSIMDImpl(int Count, void *Dst, XDWORD Stride, XDWORD SizeSrc, const void *Src) {
    return VxSIMDFillStructure(Count, Dst, Stride, SizeSrc, Src);
}

static XBOOL CopyStructureSIMDImpl(int Count, void *Dst, XDWORD OutStride, XDWORD SizeSrc, const void *Src, XDWORD InStride) {
    return VxSIMDCopyStructure(Count, Dst, OutStride, SizeSrc, Src, InStride);
}

static XBOOL IndexedCopySIMDImpl(const VxStridedData &Dst, const VxStridedData &Src, XDWORD SizeSrc, const int *Indices, int IndexCount) {
    return VxSIMDIndexedCopy(Dst, Src, SizeSrc, Indices, IndexCount);
}

static XBOOL PtInRectSIMDImpl(CKRECT *rect, CKPOINT *pt) {
    return VxSIMDPtInRect(rect, pt);
}

static XBOOL TransformBox2DSIMDCore(const VxMatrix &World_ProjectionMat, const VxBbox &box, VxRect *ScreenSize, VxRect *Extents, VXCLIP_FLAGS &OrClipFlags, VXCLIP_FLAGS &AndClipFlags) {
    return VxSIMDTransformBox2D(
        &World_ProjectionMat,
        &box,
        Extents,
        ScreenSize,
        &OrClipFlags,
        &AndClipFlags
    );
}

static void ProjectBoxZExtentsSIMDCore(const VxMatrix &World_ProjectionMat, const VxBbox &box, float &ZhMin, float &ZhMax) {
    if (!box.IsValid()) {
        return;
    }

    VxSIMDProjectBoxZExtents(&World_ProjectionMat, &box, &ZhMin, &ZhMax);
}

static void ProjectPointsToAxesExtentsSIMD(const XBYTE *Points, XDWORD Stride, int Count, const VxMatrix &BBoxMatrix, float &minX, float &maxX, float &minY, float &maxY, float &minZ, float &maxZ) {
    const __m128 axisX = VxSIMDLoadFloat3(&BBoxMatrix[0][0]);
    const __m128 axisY = VxSIMDLoadFloat3(&BBoxMatrix[1][0]);
    const __m128 axisZ = VxSIMDLoadFloat3(&BBoxMatrix[2][0]);

    const float *firstPoint = reinterpret_cast<const float *>(Points);
    float projX;
    float projY;
    float projZ;
    VxSIMDProjectPointToAxes3(firstPoint, axisX, axisY, axisZ, projX, projY, projZ);

    minX = maxX = projX;
    minY = maxY = projY;
    minZ = maxZ = projZ;

    // Keep scalar-style comparisons so NaN handling and floating-point order stay consistent.
    for (int i = 0; i < Count; ++i) {
        const float *p = reinterpret_cast<const float *>(Points + i * Stride);
        float x, y, z;
        VxSIMDProjectPointToAxes3(p, axisX, axisY, axisZ, x, y, z);

        if (x >= maxX)
            maxX = x;
        else if (x < minX)
            minX = x;

        if (y >= maxY)
            maxY = y;
        else if (y < minY)
            minY = y;

        if (z >= maxZ)
            maxZ = z;
        else if (z < minZ)
            minZ = z;
    }
}
#endif

static void InterpolateFloatArray_ScalarImpl(void *Res, void *array1, void *array2, float factor, int count) {
    float *r = static_cast<float *>(Res);
    const float *a = static_cast<const float *>(array1);
    const float *b = static_cast<const float *>(array2);
    if (factor == 0.0f || factor == 1.0f) {
        const float *source = factor == 0.0f ? a : b;
        for (int i = 0; i < count; ++i) r[i] = source[i];
        return;
    }
    const double t = factor;
    for (int i = 0; i < count; ++i) {
        const double x = a[i], y = b[i];
        r[i] = static_cast<float>(x + (y - x)*t);
    }
}

static void InterpolateVectorArray_ScalarImpl(void *Res, void *Inarray1, void *Inarray2, float factor, int count, XDWORD StrideRes, XDWORD StrideIn) {
    if (count <= 0) return;
    const char *srcA = static_cast<const char *>(Inarray1);
    const char *srcB = static_cast<const char *>(Inarray2);
    char *dst = static_cast<char *>(Res);
    if (factor == 0.0f || factor == 1.0f) {
        const char *src = factor == 0.0f ? srcA : srcB;
        for (;;) {
            float value[3];
            memcpy(value, src, 12);
            memcpy(dst + 8, value + 2, 4);
            memcpy(dst + 4, value + 1, 4);
            memcpy(dst, value, 4);
            if (--count == 0) return;
            src += StrideIn;
            dst += StrideRes;
        }
    }
    const double t = factor;
    for (;;) {
        float a[3], b[3], value[3];
        memcpy(a, srcA, 12);
        memcpy(b, srcB, 12);
        for (int k = 0; k < 3; ++k) {
            const double x = a[k], y = b[k];
            value[k] = static_cast<float>(x + (y - x)*t);
        }
        memcpy(dst + 8, value + 2, 4);
        memcpy(dst + 4, value + 1, 4);
        memcpy(dst, value, 4);
        if (--count == 0) return;
        srcA += StrideIn;
        srcB += StrideIn;
        dst += StrideRes;
    }
}

namespace {

typedef void (*VxInterpolateFloatArrayDispatchFn)(void *, void *, void *, float, int);
typedef void (*VxInterpolateVectorArrayDispatchFn)(void *, void *, void *, float, int, XDWORD, XDWORD);
typedef XBOOL (*VxFillStructureDispatchFn)(int, void *, XDWORD, XDWORD, const void *);
typedef XBOOL (*VxCopyStructureDispatchFn)(int, void *, XDWORD, XDWORD, const void *, XDWORD);
typedef XBOOL (*VxIndexedCopyDispatchFn)(const VxStridedData &, const VxStridedData &, XDWORD, const int *, int);
typedef XBOOL (*VxPtInRectDispatchFn)(CKRECT *, CKPOINT *);
typedef XBOOL (*VxTransformBox2DCoreDispatchFn)(const VxMatrix &, const VxBbox &, VxRect *, VxRect *, VXCLIP_FLAGS &, VXCLIP_FLAGS &);
typedef void (*VxProjectBoxZExtentsCoreDispatchFn)(const VxMatrix &, const VxBbox &, float &, float &);
typedef void (*VxProjectPointsToAxesExtentsDispatchFn)(const XBYTE *, XDWORD, int, const VxMatrix &, float &, float &, float &, float &, float &, float &);

#if defined(VX_SIMD_SSE)
void InterpolateFloatArray_SIMDImpl(void *Res, void *array1, void *array2, float factor, int count) {
    VxSIMDInterpolateFloatArray(
        static_cast<float *>(Res),
        static_cast<const float *>(array1),
        static_cast<const float *>(array2),
        factor,
        count);
}

void InterpolateVectorArray_SIMDImpl(void *Res, void *Inarray1, void *Inarray2, float factor, int count, XDWORD StrideRes, XDWORD StrideIn) {
    VxSIMDInterpolateVectorArray(
        Res,
        Inarray1,
        Inarray2,
        factor,
        count,
        StrideRes,
        StrideIn);
}
#endif

struct VxMathDispatchTable {
    VxInterpolateFloatArrayDispatchFn interpolateFloatArray;
    VxInterpolateVectorArrayDispatchFn interpolateVectorArray;
    VxFillStructureDispatchFn fillStructure;
    VxCopyStructureDispatchFn copyStructure;
    VxIndexedCopyDispatchFn indexedCopy;
    VxPtInRectDispatchFn ptInRect;
    VxTransformBox2DCoreDispatchFn transformBox2DCore;
    VxProjectBoxZExtentsCoreDispatchFn projectBoxZExtentsCore;
    VxProjectPointsToAxesExtentsDispatchFn projectPointsToAxesExtents;
};

const VxMathDispatchTable kVxMathDispatchScalar = {
    InterpolateFloatArray_ScalarImpl,
    InterpolateVectorArray_ScalarImpl,
    FillStructureScalarImpl,
    CopyStructureScalarImpl,
    IndexedCopyScalarImpl,
    PtInRectScalarImpl,
    TransformBox2DScalarCore,
    ProjectBoxZExtentsScalarCore,
    ProjectPointsToAxesExtentsScalar
};

#if defined(VX_SIMD_SSE)
const VxMathDispatchTable kVxMathDispatchSIMD = {
    InterpolateFloatArray_SIMDImpl,
    InterpolateVectorArray_SIMDImpl,
    FillStructureSIMDImpl,
    CopyStructureSIMDImpl,
    IndexedCopySIMDImpl,
    PtInRectSIMDImpl,
    TransformBox2DSIMDCore,
    ProjectBoxZExtentsSIMDCore,
    ProjectPointsToAxesExtentsSIMD
};
#endif

std::atomic<const VxMathDispatchTable *> g_VxMathDispatch(&kVxMathDispatchScalar);

const VxMathDispatchTable *GetVxMathDispatchTable() {
    return g_VxMathDispatch.load(std::memory_order_acquire);
}
}

void VxMathDispatchRebuild(int effectiveMode) {
    const bool useSIMD = (effectiveMode != VX_SIMD_MODE_NONE);
#if defined(VX_SIMD_SSE)
    const VxMathDispatchTable *next = useSIMD ? &kVxMathDispatchSIMD : &kVxMathDispatchScalar;
#else
    (void) useSIMD;
    const VxMathDispatchTable *next = &kVxMathDispatchScalar;
#endif
    g_VxMathDispatch.store(next, std::memory_order_release);
}

void InitVxMath() {
    VxDetectProcessor();
    InitializeTables();
    VxSIMDDispatchInitialize();
}

void InterpolateFloatArray(void *Res, void *array1, void *array2, float factor, int count) {
    if (!Res || !array1 || !array2 || count <= 0)
        return;

    GetVxMathDispatchTable()->interpolateFloatArray(Res, array1, array2, factor, count);
}

void InterpolateVectorArray(void *Res, void *Inarray1, void *Inarray2, float factor, int count, XDWORD StrideRes, XDWORD StrideIn) {
    if (!Res || !Inarray1 || !Inarray2 || count <= 0)
        return;

    GetVxMathDispatchTable()->interpolateVectorArray(Res, Inarray1, Inarray2, factor, count, StrideRes, StrideIn);
}

static XBOOL TransformBox2DScalarCore(const VxMatrix &World_ProjectionMat, const VxBbox &box, VxRect *ScreenSize, VxRect *Extents, VXCLIP_FLAGS &OrClipFlags, VXCLIP_FLAGS &AndClipFlags) {
    const double low[3] = {box.Min.x, box.Min.y, box.Min.z};
    const double high[3] = {box.Max.x, box.Max.y, box.Max.z};
    const bool flat = static_cast<float>(high[2] - low[2]) == 0;
    const int count = flat ? 4 : 8;
    const int xMask = flat ? 1 : 4, yMask = 2, zMask = flat ? 0 : 1;
    double terms[4][3][2], translation[4];
    for (int col = 0; col < 4; ++col) {
        translation[col] = World_ProjectionMat[3][col];
        for (int row = 0; row < 3; ++row) {
            terms[col][row][0] = low[row] * World_ProjectionMat[row][col];
            terms[col][row][1] = high[row] * World_ProjectionMat[row][col];
        }
    }
    double vertices[8][3];
    unsigned combined = 0, common = 0xffffffffu;
    for (int i = 0; i < count; ++i) {
        double point[4];
        for (int col = 0; col < 4; ++col) {
            const double components[4] = {terms[col][0][(i & xMask) != 0], terms[col][1][(i & yMask) != 0], terms[col][2][(i & zMask) != 0], translation[col]};
            double sum = ((components[0] + components[1]) + components[2]) + components[3];
            const double magnitude = fabs(components[0]) + fabs(components[1]) + fabs(components[2]) + fabs(components[3]);
            if (fabs(sum) < magnitude * (1.0 / 16777216.0)) {
                sum = components[0];
                double correction = 0;
                for (int row = 1; row < 4; ++row) {
                    const double next = sum + components[row];
                    correction += fabs(sum) >= fabs(components[row]) ? (sum - next) + components[row] : (components[row] - next) + sum;
                    sum = next;
                }
                sum += correction;
            }
            point[col] = sum;
        }
        const double x = point[0], y = point[1], z = point[2], w = point[3];
        unsigned flags = 0;
        if (x < -w) flags |= VXCLIP_LEFT;
        if (x > w) flags |= VXCLIP_RIGHT;
        if (y < -w) flags |= VXCLIP_BOTTOM;
        if (y > w) flags |= VXCLIP_TOP;
        if (z < 0) flags |= VXCLIP_FRONT;
        if (z > w) flags |= VXCLIP_BACK;
        combined |= flags; common &= flags;
        vertices[i][0] = x; vertices[i][1] = y; vertices[i][2] = w;
    }
    if (Extents && ScreenSize) {
        // Cache the viewport before an aliased output rectangle can overwrite it.
        const double halfX = (double(ScreenSize->right) - ScreenSize->left) * 0.5;
        const double halfY = (double(ScreenSize->bottom) - ScreenSize->top) * 0.5;
        const double centerX = ScreenSize->left + halfX, centerY = ScreenSize->top + halfY;
        float minX = 1.0e10f, minY = 1.0e10f, maxX = -1.0e10f, maxY = -1.0e10f;
        if (!common) for (int i = 0; i < count; ++i) {
            const double denominator = fabs(vertices[i][2]);
            const float x = static_cast<float>(vertices[i][0] / denominator * halfX + centerX);
            const float y = static_cast<float>(vertices[i][1] / denominator * (-halfY) + centerY);
            // Match original SSE min/max selection, including ties and NaNs.
            minX = minX < x ? minX : x; maxX = maxX > x ? maxX : x;
            minY = minY < y ? minY : y; maxY = maxY > y ? maxY : y;
        }
        Extents->left = minX; Extents->right = maxX; Extents->top = minY; Extents->bottom = maxY;
        if (combined & VXCLIP_FRONT) {
            if (combined & VXCLIP_LEFT) Extents->left = ScreenSize->left;
            if (combined & VXCLIP_RIGHT) Extents->right = ScreenSize->right;
            if (combined & VXCLIP_TOP) Extents->top = ScreenSize->top;
            if (combined & VXCLIP_BOTTOM) Extents->bottom = ScreenSize->bottom;
        }
    }
    OrClipFlags = static_cast<VXCLIP_FLAGS>(combined);
    AndClipFlags = static_cast<VXCLIP_FLAGS>(common);
    return common == 0;
}

XBOOL VxTransformBox2D(const VxMatrix &World_ProjectionMat, const VxBbox &box, VxRect *ScreenSize, VxRect *Extents, VXCLIP_FLAGS &OrClipFlags, VXCLIP_FLAGS &AndClipFlags) {
    // Validate input
    if (!box.IsValid()) {
        OrClipFlags = VXCLIP_ALL;
        AndClipFlags = VXCLIP_ALL;
        return FALSE;
    }

    return GetVxMathDispatchTable()->transformBox2DCore(
        World_ProjectionMat,
        box,
        ScreenSize,
        Extents,
        OrClipFlags,
        AndClipFlags
    );
}

static void ProjectBoxZExtentsScalarCore(const VxMatrix &World_ProjectionMat, const VxBbox &box, float &ZhMin, float &ZhMax) {
    if (!box.IsValid()) return;
    const double low[3] = {box.Min.x, box.Min.y, box.Min.z};
    const double high[3] = {box.Max.x, box.Max.y, box.Max.z};
    int xMask = 4, yMask = 2, zMask = 1, count = 8;
    // Preserve the original rounded-float threshold and Z/X/Y priority.
    if (static_cast<float>(high[2] - low[2]) <= EPSILON) {
        xMask = 1; zMask = 0; count = 4;
    } else if (static_cast<float>(high[0] - low[0]) <= EPSILON) {
        xMask = 0; count = 4;
    } else if (static_cast<float>(high[1] - low[1]) <= EPSILON) {
        xMask = 2; yMask = 0; count = 4;
    }
    double z[3][2], w[3][2];
    for (int row = 0; row < 3; ++row) {
        z[row][0] = low[row] * World_ProjectionMat[row][2];
        z[row][1] = high[row] * World_ProjectionMat[row][2];
        w[row][0] = low[row] * World_ProjectionMat[row][3];
        w[row][1] = high[row] * World_ProjectionMat[row][3];
    }
    const double translationZ = World_ProjectionMat[3][2], translationW = World_ProjectionMat[3][3];
    float projected[8];
    for (int i = 0; i < count; ++i) {
        double values[2];
        const double components[2][4] = {
            {z[0][(i & xMask) != 0], z[1][(i & yMask) != 0], z[2][(i & zMask) != 0], translationZ},
            {w[0][(i & xMask) != 0], w[1][(i & yMask) != 0], w[2][(i & zMask) != 0], translationW}
        };
        for (int component = 0; component < 2; ++component) {
            const double *terms = components[component];
            double sum = ((terms[0] + terms[1]) + terms[2]) + terms[3];
            const double magnitude = fabs(terms[0]) + fabs(terms[1]) + fabs(terms[2]) + fabs(terms[3]);
            if (fabs(sum) < magnitude * (1.0 / 16777216.0)) {
                // Recover terms lost before large opposite terms cancel.
                sum = terms[0];
                double correction = 0;
                for (int row = 1; row < 4; ++row) {
                    const double next = sum + terms[row];
                    correction += fabs(sum) >= fabs(terms[row]) ? (sum - next) + terms[row] : (terms[row] - next) + sum;
                    sum = next;
                }
                sum += correction;
            }
            values[component] = sum;
        }
        const double valueZ = values[0], valueW = values[1];
        projected[i] = !(valueW < 0) ? static_cast<float>(valueZ / valueW) : 0;
    }
    // Inputs are fully consumed before these references can overwrite them.
    // Keep each comparison/write ordered when both outputs name the same float.
    if (&ZhMin == &ZhMax) {
        ZhMin = 1.0e10f;
        ZhMax = -1.0e10f;
        for (int i = 0; i < count; ++i) {
            if (projected[i] < ZhMin) ZhMin = projected[i];
            if (projected[i] > ZhMax) ZhMax = projected[i];
        }
        return;
    }
    float minimum = 1.0e10f, maximum = -1.0e10f;
    for (int i = 0; i < count; ++i) {
        if (projected[i] < minimum) minimum = projected[i];
        if (projected[i] > maximum) maximum = projected[i];
    }
    ZhMin = minimum;
    ZhMax = maximum;
}

void VxProjectBoxZExtents(const VxMatrix &World_ProjectionMat, const VxBbox &box, float &ZhMin, float &ZhMax) {
    if (!box.IsValid()) {
        ZhMin = 1.0e10f;
        ZhMax = -1.0e10f;
        return;
    }

    GetVxMathDispatchTable()->projectBoxZExtentsCore(World_ProjectionMat, box, ZhMin, ZhMax);
}


/**
 * Fills an array with copies of a structure
 */
static XBOOL FillStructureScalarImpl(int Count, void *Dst, XDWORD Stride, XDWORD SizeSrc, const void *Src) {
    if (!Src || !Dst || Count <= 0 || !SizeSrc || !Stride || (SizeSrc & 3) != 0)
        return FALSE;
    const XBYTE *src = static_cast<const XBYTE *>(Src);
    XBYTE *dst = static_cast<XBYTE *>(Dst);
    switch (SizeSrc) {
    case 4: {
        XDWORD value;
        memcpy(&value, src, 4);
        for (;;) {
            memcpy(dst, &value, 4);
            if (--Count == 0) return TRUE;
            dst += Stride;
        }
    }
    case 8: {
        XDWORD value[2];
        memcpy(value, src, 8);
        for (;;) {
            memcpy(dst, value, 8);
            if (--Count == 0) return TRUE;
            dst += Stride;
        }
    }
    case 12: {
        XDWORD value[3];
        memcpy(value, src, 12);
        for (;;) {
            memcpy(dst, value, 12);
            if (--Count == 0) return TRUE;
            dst += Stride;
        }
    }
    case 16: {
        XDWORD value[4];
        memcpy(value, src, 16);
        for (;;) {
            memcpy(dst, value, 16);
            if (--Count == 0) return TRUE;
            dst += Stride;
        }
    }
    default:
        for (;;) {
            // Generic sizes reread the current source one DWORD at a time.
            // An overlapping write can change a later source word or record.
            for (XDWORD offset = 0; offset < SizeSrc; offset += 4) {
                XDWORD value;
                memcpy(&value, src + offset, 4);
                memcpy(dst + offset, &value, 4);
            }
            if (--Count == 0) return TRUE;
            dst += Stride;
        }
    }
}

/**
 * Copies structures from one array to another
 */
static XBOOL CopyStructureScalarImpl(int Count, void *Dst, XDWORD OutStride, XDWORD SizeSrc, const void *Src, XDWORD InStride) {
    if (!Src || !Dst || Count <= 0 || !SizeSrc || !OutStride || !InStride || (SizeSrc & 3) != 0)
        return FALSE;
    const XBYTE *src = static_cast<const XBYTE *>(Src);
    XBYTE *dst = static_cast<XBYTE *>(Dst);
    switch (SizeSrc) {
    case 4:
        for (;;) {
            XDWORD value;
            memcpy(&value, src, 4);
            memcpy(dst, &value, 4);
            if (--Count == 0) return TRUE;
            src += InStride;
            dst += OutStride;
        }
    case 8:
        for (;;) {
            XDWORD pair[2];
            memcpy(pair, src, 8);
            memcpy(dst, pair, 8);
            if (--Count == 0) return TRUE;
            src += InStride;
            dst += OutStride;
        }
    case 12:
        for (;;) {
            XDWORD xy[2], z;
            memcpy(xy, src, 8);
            memcpy(dst, xy, 4);
            // The X write can change the source Z; Y remains snapshotted.
            memcpy(&z, src + 8, 4);
            memcpy(dst + 4, xy + 1, 4);
            memcpy(dst + 8, &z, 4);
            if (--Count == 0) return TRUE;
            src += InStride;
            dst += OutStride;
        }
    case 16:
        for (;;) {
            XDWORD pair[2];
            memcpy(pair, src, 8);
            memcpy(dst, pair, 8);
            memcpy(pair, src + 8, 8);
            memcpy(dst + 8, pair, 8);
            if (--Count == 0) return TRUE;
            src += InStride;
            dst += OutStride;
        }
    default:
        for (;;) {
            for (XDWORD offset = 0; offset < SizeSrc; offset += 4) {
                XDWORD value;
                memcpy(&value, src + offset, 4);
                memcpy(dst + offset, &value, 4);
            }
            if (--Count == 0) return TRUE;
            // Advancing the record origins avoids unsigned stride-size wrap.
            src += InStride;
            dst += OutStride;
        }
    }
}

/**
 * Copies structures from an array to another using an index array
 */
static XBOOL IndexedCopyScalarImpl(const VxStridedData &Dst, const VxStridedData &Src, XDWORD SizeSrc, const int *Indices, int IndexCount) {
    if (IndexCount <= 0 || SizeSrc == 0 || (SizeSrc & 3) != 0)
        return FALSE;
    if (Dst.Ptr == nullptr || Src.Ptr == nullptr || Indices == nullptr)
        return FALSE;

    XBYTE *out = static_cast<XBYTE *>(Dst.Ptr);
    const XDWORD outStride = Dst.Stride;
    const uintptr_t source = reinterpret_cast<uintptr_t>(Src.Ptr);
    const uintptr_t sourceStride = Src.Stride;
    const XBYTE *indexBytes = reinterpret_cast<const XBYTE *>(Indices);
    // Snapshot descriptors, but read each index only after earlier output writes.
    // Sign extension preserves valid negative gathers on both pointer widths.
    switch (SizeSrc) {
    case 4: {
        for (;;) {
            int index;
            memcpy(&index, indexBytes, sizeof(index));
            const XBYTE *in = reinterpret_cast<const XBYTE *>(source + static_cast<uintptr_t>(static_cast<intptr_t>(index)) * sourceStride);
            {
                XDWORD value[1];
                memcpy(value, in + 0, 4);
                memcpy(out + 0, value, 4);
            }
            if (--IndexCount == 0) return TRUE;
            out += outStride;
            indexBytes += sizeof(int);
        }
    }
    case 8: {
        for (;;) {
            int index;
            memcpy(&index, indexBytes, sizeof(index));
            const XBYTE *in = reinterpret_cast<const XBYTE *>(source + static_cast<uintptr_t>(static_cast<intptr_t>(index)) * sourceStride);
            {
                XDWORD value[2];
                memcpy(value, in + 0, 8);
                memcpy(out + 0, value, 8);
            }
            if (--IndexCount == 0) return TRUE;
            out += outStride;
            indexBytes += sizeof(int);
        }
    }
    case 12: {
        for (;;) {
            int index;
            memcpy(&index, indexBytes, sizeof(index));
            const XBYTE *in = reinterpret_cast<const XBYTE *>(source + static_cast<uintptr_t>(static_cast<intptr_t>(index)) * sourceStride);
            {
                XDWORD value[2];
                memcpy(value, in + 0, 8);
                memcpy(out + 0, value, 8);
            }
            {
                XDWORD value[1];
                memcpy(value, in + 8, 4);
                memcpy(out + 8, value, 4);
            }
            if (--IndexCount == 0) return TRUE;
            out += outStride;
            indexBytes += sizeof(int);
        }
    }
    case 16: {
        for (;;) {
            int index;
            memcpy(&index, indexBytes, sizeof(index));
            const XBYTE *in = reinterpret_cast<const XBYTE *>(source + static_cast<uintptr_t>(static_cast<intptr_t>(index)) * sourceStride);
            {
                XDWORD value[2];
                memcpy(value, in + 0, 8);
                memcpy(out + 0, value, 8);
            }
            {
                XDWORD value[2];
                memcpy(value, in + 8, 8);
                memcpy(out + 8, value, 8);
            }
            if (--IndexCount == 0) return TRUE;
            out += outStride;
            indexBytes += sizeof(int);
        }
    }
    case 20: {
        for (;;) {
            int index;
            memcpy(&index, indexBytes, sizeof(index));
            const XBYTE *in = reinterpret_cast<const XBYTE *>(source + static_cast<uintptr_t>(static_cast<intptr_t>(index)) * sourceStride);
            {
                XDWORD value[2];
                memcpy(value, in + 0, 8);
                memcpy(out + 0, value, 8);
            }
            {
                XDWORD value[2];
                memcpy(value, in + 8, 8);
                memcpy(out + 8, value, 8);
            }
            {
                XDWORD value[1];
                memcpy(value, in + 16, 4);
                memcpy(out + 16, value, 4);
            }
            if (--IndexCount == 0) return TRUE;
            out += outStride;
            indexBytes += sizeof(int);
        }
    }
    case 24: {
        for (;;) {
            int index;
            memcpy(&index, indexBytes, sizeof(index));
            const XBYTE *in = reinterpret_cast<const XBYTE *>(source + static_cast<uintptr_t>(static_cast<intptr_t>(index)) * sourceStride);
            {
                XDWORD value[2];
                memcpy(value, in + 0, 8);
                memcpy(out + 0, value, 8);
            }
            {
                XDWORD value[2];
                memcpy(value, in + 8, 8);
                memcpy(out + 8, value, 8);
            }
            {
                XDWORD value[2];
                memcpy(value, in + 16, 8);
                memcpy(out + 16, value, 8);
            }
            if (--IndexCount == 0) return TRUE;
            out += outStride;
            indexBytes += sizeof(int);
        }
    }
    default: {
        for (;;) {
            int index;
            memcpy(&index, indexBytes, sizeof(index));
            const XBYTE *in = reinterpret_cast<const XBYTE *>(source + static_cast<uintptr_t>(static_cast<intptr_t>(index)) * sourceStride);
            for (XDWORD offset = 0; offset < SizeSrc; offset += 4) {
                XDWORD value;
                memcpy(&value, in + offset, 4);
                memcpy(out + offset, &value, 4);
            }
            if (--IndexCount == 0) return TRUE;
            out += outStride;
            indexBytes += sizeof(int);
        }
    }
    }
}

static XBOOL PtInRectScalarImpl(CKRECT *rect, CKPOINT *pt) {
    if (pt->x < rect->left)
        return FALSE;
    if (pt->x > rect->right)
        return FALSE;
    return pt->y <= rect->bottom && pt->y >= rect->top;
}

static void ProjectPointsToAxesExtentsScalar(const XBYTE *Points, XDWORD Stride, int Count, const VxMatrix &BBoxMatrix, float &minX, float &maxX, float &minY, float &maxY, float &minZ, float &maxZ) {
    // Project all points onto the principal axes to find min/max extents
    const float *pPoint = reinterpret_cast<const float *>(Points);

    // Initialize min/max with the first point projection
    float projX = pPoint[0] * BBoxMatrix[0][0] + pPoint[1] * BBoxMatrix[0][1] + pPoint[2] * BBoxMatrix[0][2];
    minX = maxX = projX;

    float projY = pPoint[0] * BBoxMatrix[1][0] + pPoint[1] * BBoxMatrix[1][1] + pPoint[2] * BBoxMatrix[1][2];
    minY = maxY = projY;

    float projZ = pPoint[0] * BBoxMatrix[2][0] + pPoint[1] * BBoxMatrix[2][1] + pPoint[2] * BBoxMatrix[2][2];
    minZ = maxZ = projZ;

    // Find min/max extents for all points
    for (int i = 0; i < Count; ++i) {
        const float *p = reinterpret_cast<const float *>(Points + i * Stride);

        float x = p[0] * BBoxMatrix[0][0] + p[1] * BBoxMatrix[0][1] + p[2] * BBoxMatrix[0][2];
        if (x >= maxX)
            maxX = x;
        else if (x < minX)
            minX = x;

        float y = p[0] * BBoxMatrix[1][0] + p[1] * BBoxMatrix[1][1] + p[2] * BBoxMatrix[1][2];
        if (y >= maxY)
            maxY = y;
        else if (y < minY)
            minY = y;

        float z = p[0] * BBoxMatrix[2][0] + p[1] * BBoxMatrix[2][1] + p[2] * BBoxMatrix[2][2];
        if (z >= maxZ)
            maxZ = z;
        else if (z < minZ)
            minZ = z;
    }
}

XBOOL VxFillStructure(int Count, void *Dst, XDWORD Stride, XDWORD SizeSrc, const void *Src) {
    return GetVxMathDispatchTable()->fillStructure(Count, Dst, Stride, SizeSrc, Src);
}

XBOOL VxCopyStructure(int Count, void *Dst, XDWORD OutStride, XDWORD SizeSrc, const void *Src, XDWORD InStride) {
    return GetVxMathDispatchTable()->copyStructure(Count, Dst, OutStride, SizeSrc, Src, InStride);
}

XBOOL VxIndexedCopy(const VxStridedData &Dst, const VxStridedData &Src, XDWORD SizeSrc, const int *Indices, int IndexCount) {
    return GetVxMathDispatchTable()->indexedCopy(Dst, Src, SizeSrc, Indices, IndexCount);
}

XBOOL VxPtInRect(CKRECT *rect, CKPOINT *pt) {
    return GetVxMathDispatchTable()->ptInRect(rect, pt);
}

XBOOL VxComputeBestFitBBox(const XBYTE *Points, XDWORD Stride, int Count, VxMatrix &BBoxMatrix, float AdditionalBorder) {
    if (Count > 0 && Points) {
        // Create and compute covariance matrix
        VxEigenMatrix eigenMat;
        eigenMat.Covariance((float *) Points, Stride, Count);
        eigenMat.EigenStuff3();

        // Copy eigenvectors to the bounding box matrix (transposed)
        // eigenMat[j][i] -> BBoxMatrix[i][j]
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                BBoxMatrix[i][j] = eigenMat[j][i];
            }
            BBoxMatrix[i][3] = 0.0f;
        }
        BBoxMatrix[3][0] = 0.0f;
        BBoxMatrix[3][1] = 0.0f;
        BBoxMatrix[3][2] = 0.0f;
        BBoxMatrix[3][3] = 1.0f;

        // Normalize the first two axes
        BBoxMatrix[0].Normalize();
        BBoxMatrix[1].Normalize();

        // Compute the third axis using cross product for orthogonality
        // Cross = (A.y*B.z - A.z*B.y, A.z*B.x - A.x*B.z, A.x*B.y - A.y*B.x)
        float mx = BBoxMatrix[0][1] * BBoxMatrix[1][2] - BBoxMatrix[0][2] * BBoxMatrix[1][1];
        float my = BBoxMatrix[0][2] * BBoxMatrix[1][0] - BBoxMatrix[0][0] * BBoxMatrix[1][2];
        float mz = BBoxMatrix[0][0] * BBoxMatrix[1][1] - BBoxMatrix[0][1] * BBoxMatrix[1][0];

        BBoxMatrix[2][0] = mx;
        BBoxMatrix[2][1] = my;
        BBoxMatrix[2][2] = mz;
        BBoxMatrix[2][3] = 0.0f;

        float minX, maxX, minY, maxY, minZ, maxZ;
        GetVxMathDispatchTable()->projectPointsToAxesExtents(
            Points,
            Stride,
            Count,
            BBoxMatrix,
            minX,
            maxX,
            minY,
            maxY,
            minZ,
            maxZ
        );

        // Calculate center point in principal axis space
        float centerX = (minX + maxX) * 0.5f;
        float centerY = (minY + maxY) * 0.5f;
        float centerZ = (minZ + maxZ) * 0.5f;

        // Calculate half-extents with additional border
        float halfX = (maxX - minX) * 0.5f + AdditionalBorder;
        float halfY = (maxY - minY) * 0.5f + AdditionalBorder;
        float halfZ = (maxZ - minZ) * 0.5f + AdditionalBorder;

        // Transform center from principal axis space to world space (using unscaled axes)
        BBoxMatrix[3][0] = centerX * BBoxMatrix[0][0] + centerY * BBoxMatrix[1][0] + centerZ * BBoxMatrix[2][0];
        BBoxMatrix[3][1] = centerX * BBoxMatrix[0][1] + centerY * BBoxMatrix[1][1] + centerZ * BBoxMatrix[2][1];
        BBoxMatrix[3][2] = centerX * BBoxMatrix[0][2] + centerY * BBoxMatrix[1][2] + centerZ * BBoxMatrix[2][2];
        BBoxMatrix[3][3] = 1.0f;

        // Scale the axes by their respective half-extents
        for (int j = 0; j < 3; ++j) {
            BBoxMatrix[0][j] *= halfX;
        }
        BBoxMatrix[0][3] = 0.0f;

        for (int j = 0; j < 3; ++j) {
            BBoxMatrix[1][j] *= halfY;
        }
        BBoxMatrix[1][3] = 0.0f;

        for (int j = 0; j < 3; ++j) {
            BBoxMatrix[2][j] *= halfZ;
        }
        BBoxMatrix[2][3] = 0.0f;

        // Validate matrix for reasonable values
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 3; ++j) {
                if (fabs(BBoxMatrix[i][j]) >= 1.0e6f)
                    return FALSE;
            }
        }

        return TRUE;
    } else {
        BBoxMatrix.SetIdentity();
    }
    return FALSE;
}

#if defined(_WIN32) && !defined(VX_LIB)
BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            g_hinstDLL = hinstDLL;
            InitVxMath();
            break;
        case DLL_PROCESS_DETACH:
            break;
        default:
            break;
    }
    return TRUE;
}
#endif
