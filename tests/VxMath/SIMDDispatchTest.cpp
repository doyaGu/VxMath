#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstring>

#include "VxMath.h"

namespace {

TEST(SIMDDispatchTest, InterpolateFloatArrayMatchesScalarWithTail) {
    constexpr int kCount = 7;
    float a[kCount] = {0.0f, -1.0f, 2.0f, 3.5f, 1000.0f, -1.0e-4f, 42.0f};
    float b[kCount] = {1.0f, 3.0f, -2.0f, 7.5f, -1000.0f, 2.0e-4f, -42.0f};
    float result[kCount] = {};
    float expected[kCount] = {};
    const float t = 0.37f;

    for (int i = 0; i < kCount; ++i) {
        expected[i] = a[i] + (b[i] - a[i]) * t;
    }

    InterpolateFloatArray(result, a, b, t, kCount);

    for (int i = 0; i < kCount; ++i) {
        EXPECT_NEAR(result[i], expected[i], 1.0e-6f);
    }
}

TEST(SIMDDispatchTest, InterpolateVectorArrayMatchesScalarWithStride) {
    struct PackedVector {
        VxVector v;
        float padding;
    };

    constexpr int kCount = 5;
    PackedVector inA[kCount];
    PackedVector inB[kCount];
    PackedVector out[kCount];
    PackedVector expected[kCount];

    for (int i = 0; i < kCount; ++i) {
        inA[i].v = VxVector(static_cast<float>(i), static_cast<float>(2 * i), static_cast<float>(-i));
        inB[i].v = VxVector(static_cast<float>(i + 10), static_cast<float>(-i), static_cast<float>(i * 3));
        inA[i].padding = inB[i].padding = out[i].padding = expected[i].padding = -999.0f;
    }

    const float t = 0.625f;
    for (int i = 0; i < kCount; ++i) {
        expected[i].v.x = inA[i].v.x + (inB[i].v.x - inA[i].v.x) * t;
        expected[i].v.y = inA[i].v.y + (inB[i].v.y - inA[i].v.y) * t;
        expected[i].v.z = inA[i].v.z + (inB[i].v.z - inA[i].v.z) * t;
    }

    InterpolateVectorArray(
        out,
        inA,
        inB,
        t,
        kCount,
        static_cast<XDWORD>(sizeof(PackedVector)),
        static_cast<XDWORD>(sizeof(PackedVector))
    );

    for (int i = 0; i < kCount; ++i) {
        EXPECT_NEAR(out[i].v.x, expected[i].v.x, 1.0e-6f);
        EXPECT_NEAR(out[i].v.y, expected[i].v.y, 1.0e-6f);
        EXPECT_NEAR(out[i].v.z, expected[i].v.z, 1.0e-6f);
        EXPECT_FLOAT_EQ(out[i].padding, -999.0f);
    }
}

TEST(SIMDDispatchTest, TransformBox2DInvalidBoxSetsClipFlags) {
    VxMatrix worldProjection;
    worldProjection.SetIdentity();

    VxBbox invalidBox; // default ctor is invalid (Min > Max)
    VxRect screen(0.0f, 0.0f, 800.0f, 600.0f);
    VxRect extents;
    VXCLIP_FLAGS orFlags = static_cast<VXCLIP_FLAGS>(0);
    VXCLIP_FLAGS andFlags = static_cast<VXCLIP_FLAGS>(0);

    const XBOOL visible = VxTransformBox2D(worldProjection, invalidBox, &screen, &extents, orFlags, andFlags);

    EXPECT_FALSE(visible);
    EXPECT_EQ(orFlags, VXCLIP_ALL);
    EXPECT_EQ(andFlags, VXCLIP_ALL);
}

TEST(SIMDDispatchTest, TransformBox2DFlatBoxProjectsToScreenBounds) {
    VxMatrix worldProjection;
    worldProjection.SetIdentity();

    VxBbox box(VxVector(-1.0f, -1.0f, 0.5f), VxVector(1.0f, 1.0f, 0.5f));
    VxRect screen(0.0f, 0.0f, 800.0f, 600.0f);
    VxRect extents;
    VXCLIP_FLAGS orFlags = static_cast<VXCLIP_FLAGS>(0);
    VXCLIP_FLAGS andFlags = static_cast<VXCLIP_FLAGS>(0);

    const XBOOL visible = VxTransformBox2D(worldProjection, box, &screen, &extents, orFlags, andFlags);

    EXPECT_TRUE(visible);
    EXPECT_NEAR(extents.left, 0.0f, 1.0e-4f);
    EXPECT_NEAR(extents.top, 0.0f, 1.0e-4f);
    EXPECT_NEAR(extents.right, 800.0f, 1.0e-4f);
    EXPECT_NEAR(extents.bottom, 600.0f, 1.0e-4f);
}

TEST(SIMDDispatchTest, ProjectBoxZExtentsInvalidBoxKeepsLegacyDefaults) {
    VxMatrix worldProjection;
    worldProjection.SetIdentity();

    VxBbox invalidBox;
    float zhMin = 0.0f;
    float zhMax = 0.0f;

    VxProjectBoxZExtents(worldProjection, invalidBox, zhMin, zhMax);

    EXPECT_FLOAT_EQ(zhMin, 1.0e10f);
    EXPECT_FLOAT_EQ(zhMax, -1.0e10f);
}

TEST(SIMDDispatchTest, FillStructureGenericSizeAndStrideMatchesPattern) {
    struct Pattern20 {
        uint32_t data[5];
    };

    Pattern20 pattern = {{0x10203040u, 0x11223344u, 0x55667788u, 0x99AABBCCu, 0xDDEEFF00u}};
    alignas(16) unsigned char buffer[96] = {};

    ASSERT_TRUE(VxFillStructure(3, buffer, 32, sizeof(Pattern20), &pattern));

    for (int i = 0; i < 3; ++i) {
        const void *dst = buffer + i * 32;
        EXPECT_EQ(std::memcmp(dst, &pattern, sizeof(Pattern20)), 0);
        EXPECT_EQ(buffer[i * 32 + 20], 0u);
    }
}

TEST(SIMDDispatchTest, CopyStructureGenericSizeWithDifferentStrideMatchesSource) {
    struct SrcElement {
        uint32_t data[7];
        uint32_t pad;
    };

    SrcElement src[3] = {
        {{1u, 2u, 3u, 4u, 5u, 6u, 7u}, 0xAAAAAAAAu},
        {{8u, 9u, 10u, 11u, 12u, 13u, 14u}, 0xBBBBBBBBu},
        {{15u, 16u, 17u, 18u, 19u, 20u, 21u}, 0xCCCCCCCCu},
    };
    alignas(16) unsigned char dst[120] = {};

    ASSERT_TRUE(VxCopyStructure(3, dst, 40, 28, src, sizeof(SrcElement)));

    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(std::memcmp(dst + i * 40, src[i].data, 28), 0);
        EXPECT_EQ(dst[i * 40 + 28], 0u);
    }
}

TEST(SIMDDispatchTest, IndexedCopyGenericSizeFollowsIndices) {
    struct SrcElement {
        uint32_t data[5];
        uint32_t pad;
    };
    struct DstElement {
        uint32_t data[5];
    };

    SrcElement srcData[4] = {
        {{11u, 12u, 13u, 14u, 15u}, 0xAAAAAAA1u},
        {{21u, 22u, 23u, 24u, 25u}, 0xAAAAAAA2u},
        {{31u, 32u, 33u, 34u, 35u}, 0xAAAAAAA3u},
        {{41u, 42u, 43u, 44u, 45u}, 0xAAAAAAA4u},
    };
    DstElement dstData[4] = {};
    int indices[4] = {2, 0, 3, 1};

    VxStridedData src(srcData, sizeof(SrcElement));
    VxStridedData dst(dstData, sizeof(DstElement));

    ASSERT_TRUE(VxIndexedCopy(dst, src, sizeof(DstElement), indices, 4));

    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(std::memcmp(dstData[i].data, srcData[indices[i]].data, sizeof(DstElement)), 0);
    }
}

TEST(SIMDDispatchTest, PtInRectHandlesNegativeAndBoundaryValues) {
    CKRECT rect = {-50, -20, 80, 60};
    CKPOINT inside = {0, 0};
    CKPOINT onEdge = {-50, 60};
    CKPOINT outside = {-51, 0};

    EXPECT_TRUE(VxPtInRect(&rect, &inside));
    EXPECT_TRUE(VxPtInRect(&rect, &onEdge));
    EXPECT_FALSE(VxPtInRect(&rect, &outside));
}

TEST(SIMDDispatchTest, ComputeBestFitBBoxContainsInputPoints) {
    struct PointWithPad {
        float x, y, z, pad;
    };

    PointWithPad points[] = {
        {-3.0f, -1.0f, 2.0f, 0.0f},
        {4.0f, -2.0f, 1.0f, 0.0f},
        {1.0f, 5.0f, -4.0f, 0.0f},
        {-2.0f, 3.0f, 6.0f, 0.0f},
        {0.5f, -4.0f, -1.0f, 0.0f},
    };

    VxMatrix bbox;
    ASSERT_TRUE(VxComputeBestFitBBox(
        reinterpret_cast<const XBYTE *>(points),
        static_cast<XDWORD>(sizeof(PointWithPad)),
        static_cast<int>(sizeof(points) / sizeof(points[0])),
        bbox,
        0.05f
    ));

    const VxVector center(bbox[3][0], bbox[3][1], bbox[3][2]);
    for (const PointWithPad &p : points) {
        const VxVector d(p.x - center.x, p.y - center.y, p.z - center.z);
        for (int axis = 0; axis < 3; ++axis) {
            const VxVector scaledAxis(bbox[axis][0], bbox[axis][1], bbox[axis][2]);
            const float denom = DotProduct(scaledAxis, scaledAxis);
            ASSERT_GT(denom, 0.0f);
            const float local = DotProduct(d, scaledAxis) / denom;
            EXPECT_LE(std::fabs(local), 1.01f);
        }
    }
}

} // namespace
