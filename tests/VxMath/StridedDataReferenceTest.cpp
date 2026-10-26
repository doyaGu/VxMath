#include <limits.h>
#include <new>
#include <stddef.h>
#include <string.h>

#ifndef VX_STRIDED_DATA_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif

struct StridedDataChecks {
    int checks = 0;
    int errors = 0;
    int calls = 0;
    void Expect(bool condition) { ++checks; if (!condition) ++errors; }
};

typedef void (*StridedTransform)(VxStridedData *, VxStridedData *, const VxMatrix &, int);
typedef XBOOL (*StridedCopy)(const VxStridedData &, const VxStridedData &, XDWORD, const int *, int);

static StridedDataChecks StridedLayout() {
    StridedDataChecks result;
    VxStridedData value;
    const size_t size = sizeof(void *) == 8 ? 16 : 8;
    result.Expect(sizeof(VxStridedDataBase) == size);
    result.Expect(sizeof(VxStridedData) == size);
    result.Expect(alignof(VxStridedDataBase) == alignof(void *));
    result.Expect(alignof(VxStridedData) == alignof(void *));
    result.Expect(offsetof(VxStridedDataBase, Ptr) == 0);
    result.Expect(offsetof(VxStridedDataBase, CPtr) == 0);
    result.Expect(offsetof(VxStridedDataBase, Stride) == sizeof(void *));
    result.Expect(sizeof(value.Stride) == 4);
    result.Expect(static_cast<VxStridedDataBase *>(&value) == static_cast<void *>(&value));
    return result;
}

static StridedDataChecks StridedConstructors() {
    StridedDataChecks result;
    alignas(VxStridedData) unsigned char storage[sizeof(VxStridedData) + 2 * alignof(VxStridedData)];
    const size_t offset = alignof(VxStridedData);
    unsigned char data[8] = {};
    void *pointers[] = {nullptr, data, data + 3};
    const unsigned strides[] = {0, 1, 12, 16, 0x7fffffffu, 0x80000000u, UINT_MAX};
    memset(storage, 0xa5, sizeof(storage));
    VxStridedData *empty = new(storage + offset) VxStridedData;
    result.Expect(empty->Ptr == nullptr && empty->Stride == 0);
    empty->~VxStridedData();
    for (void *pointer : pointers) {
        for (unsigned stride : strides) {
            memset(storage, 0xa5, sizeof(storage));
            VxStridedData *value = new(storage + offset) VxStridedData(pointer, stride);
            result.Expect(value->Ptr == pointer && value->Stride == stride);
            for (size_t k = 0; k < sizeof(storage); ++k) {
                if (k < offset || k >= offset + sizeof(*value)) result.Expect(storage[k] == 0xa5);
            }
            value->~VxStridedData();
        }
    }
    return result;
}

static StridedDataChecks StridedAliases() {
    StridedDataChecks result;
    unsigned char data[32] = {};
    VxStridedDataBase base;
    base.Ptr = data;
    base.Stride = 12;
    result.Expect(base.CPtr == data);
    base.CPtr += 7;
    result.Expect(base.Ptr == data + 7);
    result.Expect(base.Stride == 12);
    base.CPtr[2] = 0x6d;
    result.Expect(data[9] == 0x6d);
    return result;
}

static StridedDataChecks StridedReferenceView() {
    StridedDataChecks result;
    unsigned char data[32] = {};
    VxStridedDataBase base;
    base.Ptr = data;
    base.Stride = 17;
    const VxStridedDataBase &constant = base;
    const VxStridedData &view = constant;
    result.Expect(static_cast<const void *>(&view) == static_cast<const void *>(&base));
    result.Expect(view.Ptr == data && view.Stride == 17);
    base.Ptr = data + 3;
    base.Stride = UINT_MAX;
    result.Expect(view.Ptr == data + 3 && view.Stride == UINT_MAX);
    const VxStridedData original(data, 23);
    VxStridedData copied(original), assigned;
    assigned = original;
    result.Expect(copied.Ptr == data && copied.Stride == 23);
    result.Expect(assigned.Ptr == data && assigned.Stride == 23);
    copied.CPtr += 1;
    copied.Stride = 29;
    result.Expect(original.Ptr == data && original.Stride == 23);
    result.Expect(assigned.Ptr == data && assigned.Stride == 23);
    return result;
}

static StridedDataChecks StridedCopyConsumer(StridedCopy copy) {
    StridedDataChecks result;
    unsigned char source[96], output[96], expected[96];
    for (unsigned k = 0; k < sizeof(source); ++k) source[k] = static_cast<unsigned char>(k * 7 + 3);
    const unsigned strides[] = {0, 8, 12, 20};
    const int indices[] = {2, 0, 1};
    for (unsigned stride : strides) {
        memset(output, 0xa5, sizeof(output));
        memcpy(expected, output, sizeof(expected));
        VxStridedDataBase sourceBase, destinationBase;
        sourceBase.Ptr = source + 1;
        sourceBase.Stride = stride;
        destinationBase.Ptr = output + 3;
        destinationBase.Stride = 16;
        unsigned char beforeSource[sizeof(sourceBase)], beforeDestination[sizeof(destinationBase)];
        memcpy(beforeSource, &sourceBase, sizeof(sourceBase));
        memcpy(beforeDestination, &destinationBase, sizeof(destinationBase));
        for (int k = 0; k < 3; ++k) memcpy(expected + 3 + k * 16, source + 1 + indices[k] * stride, 8);
        result.Expect(copy(destinationBase, sourceBase, 8, indices, 3) == TRUE);
        ++result.calls;
        result.Expect(memcmp(output, expected, sizeof(output)) == 0);
        result.Expect(memcmp(&sourceBase, beforeSource, sizeof(sourceBase)) == 0);
        result.Expect(memcmp(&destinationBase, beforeDestination, sizeof(destinationBase)) == 0);
    }
    return result;
}

static StridedDataChecks StridedTransformConsumers(const StridedTransform functions[3]) {
    StridedDataChecks result;
    alignas(16) unsigned char source[160], output[160], expected[160];
    VxMatrix matrix;
    memset(&matrix, 0, sizeof(matrix));
    matrix[0][0] = 2; matrix[1][1] = 3; matrix[2][2] = 4;
    matrix[3][0] = 7; matrix[3][1] = -5; matrix[3][2] = 11;
    matrix[0][3] = 0.5f; matrix[1][3] = 1; matrix[2][3] = -2; matrix[3][3] = 3;
    const unsigned strides[] = {12, 16, 20, 32};
    const float points[3][3] = {{1, 2, 3}, {-4, 5, -6}, {7, -8, 9}};
    for (int kind = 0; kind < 3; ++kind) {
        for (unsigned sourceStride : strides) {
            for (unsigned destinationStride : strides) {
                if (kind == 1 && destinationStride < 16) continue;
                memset(source, 0x6d, sizeof(source));
                memset(output, 0xa5, sizeof(output));
                memcpy(expected, output, sizeof(expected));
                for (int k = 0; k < 3; ++k) {
                    memcpy(source + 4 + k * sourceStride, points[k], sizeof(points[k]));
                    const float x = points[k][0], y = points[k][1], z = points[k][2];
                    const float translated[4] = {2*x+7, 3*y-5, 4*z+11, 0.5f*x+y-2*z+3};
                    const float rotated[3] = {2*x, 3*y, 4*z};
                    memcpy(expected + 8 + k * destinationStride, kind == 2 ? rotated : translated, kind == 1 ? 16 : 12);
                }
                VxStridedData input(source + 4, sourceStride), destination(output + 8, destinationStride);
                unsigned char beforeInput[sizeof(input)], beforeDestination[sizeof(destination)];
                memcpy(beforeInput, &input, sizeof(input));
                memcpy(beforeDestination, &destination, sizeof(destination));
                functions[kind](&destination, &input, matrix, 3);
                ++result.calls;
                result.Expect(memcmp(output, expected, sizeof(output)) == 0);
                result.Expect(memcmp(&input, beforeInput, sizeof(input)) == 0);
                result.Expect(memcmp(&destination, beforeDestination, sizeof(destination)) == 0);
            }
        }
    }
    return result;
}

#ifndef VX_STRIDED_DATA_PROBE
TEST(StridedDataReferenceTest, NativeLayoutAndFieldOffsets) { EXPECT_EQ(StridedLayout().errors, 0); }
TEST(StridedDataReferenceTest, ConstructorsPreservePointerAndUnsignedStride) { EXPECT_EQ(StridedConstructors().errors, 0); }
TEST(StridedDataReferenceTest, PointerNamesShareStorage) { EXPECT_EQ(StridedAliases().errors, 0); }
TEST(StridedDataReferenceTest, ConstReferenceConversionKeepsOriginalDescriptor) { EXPECT_EQ(StridedReferenceView().errors, 0); }
TEST(StridedDataReferenceTest, BaseReferenceFeedsIndexedCopyWithoutDescriptorMutation) {
    EXPECT_EQ(StridedCopyConsumer(VxIndexedCopy).errors, 0);
}
TEST(StridedDataReferenceTest, MatrixConsumersRespectPointerStrideAndDescriptorStorage) {
    const StridedTransform functions[] = {Vx3DMultiplyMatrixVectorStrided, Vx3DMultiplyMatrixVector4Strided, Vx3DRotateVectorStrided};
    EXPECT_EQ(StridedTransformConsumers(functions).errors, 0);
}
#if defined(VX_SIMD_SSE)
static void StridedTransformSIMD(VxStridedData *d, VxStridedData *s, const VxMatrix &m, int n) {
    VxSIMDMultiplyMatrixVectorStrided(d, s, &m, n);
}
static void StridedTransform4SIMD(VxStridedData *d, VxStridedData *s, const VxMatrix &m, int n) {
    VxSIMDMultiplyMatrixVector4Strided(d, s, &m, n);
}
static void StridedRotateSIMD(VxStridedData *d, VxStridedData *s, const VxMatrix &m, int n) {
    VxSIMDRotateVectorStrided(d, s, &m, n);
}
TEST(StridedDataReferenceTest, SIMDConsumersRespectDescriptorContract) {
    const StridedTransform functions[] = {StridedTransformSIMD, StridedTransform4SIMD, StridedRotateSIMD};
    EXPECT_EQ(StridedTransformConsumers(functions).errors, 0);
    EXPECT_EQ(StridedCopyConsumer(VxSIMDIndexedCopy).errors, 0);
}
#endif
#endif
