#include "VxMath.h"

#include <stdio.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#if defined(__GNUC__)
#include <cpuid.h>
#elif defined(_MSC_VER)
#include <intrin.h>
#endif

#include "VxEigenMatrix.h"

HINSTANCE g_hinstDLL;
CRITICAL_SECTION g_CriticalSection;

void InitVxMath() {
    VxDetectProcessor();
    ::InitializeCriticalSection(&g_CriticalSection);
}

XBOOL VxPtInRect(CKRECT *rect, CKPOINT *pt) {
    if (pt->x < rect->left)
        return FALSE;
    if (pt->x > rect->right)
        return FALSE;
    return pt->y <= rect->bottom && pt->y >= rect->top;
}

// Compute best fit oriented bounding box for a set of points
XBOOL VxComputeBestFitBBox(const XBYTE *Points, const XULONG Stride, const int Count,
                           VxMatrix &BBoxMatrix, const float AdditionalBorder) {
    // Check for valid input
    if (Count <= 0 || !Points) {
        BBoxMatrix.SetIdentity();
        return FALSE;
    }

    // Create and compute covariance matrix
    VxEigenMatrix eigenMat;
    eigenMat.Covariance((float *) Points, Stride, Count);
    eigenMat.EigenStuff3();

    // Copy eigenvectors to the bounding box matrix
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            BBoxMatrix[i][j] = eigenMat[i][j];
        }
    }

    // Normalize the first two axes
    ((VxVector *) &BBoxMatrix[0])->Normalize();
    ((VxVector *) &BBoxMatrix[1])->Normalize();

    // Compute the third axis using cross product for orthogonality
    float x = BBoxMatrix[1][1] * BBoxMatrix[0][0] - BBoxMatrix[0][1] * BBoxMatrix[1][0];
    float y = BBoxMatrix[1][0] * BBoxMatrix[0][2] - BBoxMatrix[1][2] * BBoxMatrix[0][0];
    float z = BBoxMatrix[0][1] * BBoxMatrix[1][2] - BBoxMatrix[1][1] * BBoxMatrix[0][2];

    BBoxMatrix[2][0] = x;
    BBoxMatrix[2][1] = y;
    BBoxMatrix[2][2] = z;

    // Project all points onto the principal axes to find min/max extents
    const float *firstPoint = (const float *) Points;

    // Initialize min/max with the first point
    float projX = firstPoint[0] * BBoxMatrix[0][0] +
        firstPoint[1] * BBoxMatrix[0][1] +
        firstPoint[2] * BBoxMatrix[0][2];
    float minX = projX, maxX = projX;

    float projY = firstPoint[0] * BBoxMatrix[1][0] +
        firstPoint[1] * BBoxMatrix[1][1] +
        firstPoint[2] * BBoxMatrix[1][2];
    float minY = projY, maxY = projY;

    float projZ = firstPoint[0] * BBoxMatrix[2][0] +
        firstPoint[1] * BBoxMatrix[2][1] +
        firstPoint[2] * BBoxMatrix[2][2];
    float minZ = projZ, maxZ = projZ;

    // Find min/max extents for all points
    for (int i = 1; i < Count; i++) {
        const float *point = (const float *) (Points + i * Stride);

        float x = point[0] * BBoxMatrix[0][0] +
            point[1] * BBoxMatrix[0][1] +
            point[2] * BBoxMatrix[0][2];

        float y = point[0] * BBoxMatrix[1][0] +
            point[1] * BBoxMatrix[1][1] +
            point[2] * BBoxMatrix[1][2];

        float z = point[0] * BBoxMatrix[2][0] +
            point[1] * BBoxMatrix[2][1] +
            point[2] * BBoxMatrix[2][2];

        // Update min/max values
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
        if (z < minZ) minZ = z;
        if (z > maxZ) maxZ = z;
    }

    // Calculate center point in principal axis space
    float centerX = (minX + maxX) * 0.5f;
    float centerY = (minY + maxY) * 0.5f;
    float centerZ = (minZ + maxZ) * 0.5f;

    // Store center temporarily
    BBoxMatrix[3][0] = centerX;
    BBoxMatrix[3][1] = centerY;
    BBoxMatrix[3][2] = centerZ;

    // Transform center from principal axis space to world space
    float cx = centerX * BBoxMatrix[0][0];
    float cy = centerX * BBoxMatrix[0][1];
    float cz = centerX * BBoxMatrix[0][2];

    float tx = centerY * BBoxMatrix[1][0];
    float ty = centerY * BBoxMatrix[1][1];
    float tz = centerY * BBoxMatrix[1][2];

    float rx = centerZ * BBoxMatrix[2][0];
    float ry = centerZ * BBoxMatrix[2][1];
    float rz = centerZ * BBoxMatrix[2][2];

    // Set transformed center position
    BBoxMatrix[3][0] = cx + tx + rx;
    BBoxMatrix[3][1] = cy + ty + ry;
    BBoxMatrix[3][2] = cz + tz + rz;

    // Calculate half-extents with additional border
    float halfExtentX = (maxX - minX) * 0.5f + AdditionalBorder;
    float halfExtentY = (maxY - minY) * 0.5f + AdditionalBorder;
    float halfExtentZ = (maxZ - minZ) * 0.5f + AdditionalBorder;

    // Create the extent vector for each axis
    VxVector extentVector;

    // Scale the axes by their respective half-extents
    for (int i = 0; i < 3; i++) {
        float extent = (i == 0) ? halfExtentX : (i == 1) ? halfExtentY : halfExtentZ;

        BBoxMatrix[i][0] *= extent;
        BBoxMatrix[i][1] *= extent;
        BBoxMatrix[i][2] *= extent;
        BBoxMatrix[i][3] = 0.0f; // Set homogeneous component to 0
    }

    // Set homogeneous component of translation to 1
    BBoxMatrix[3][3] = 1.0f;

    // Validate matrix for reasonable values
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 3; j++) {
            if (fabs(BBoxMatrix[i][j]) >= 1000000.0f)
                return FALSE;
        }
    }

    return TRUE;
}

BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            g_hinstDLL = hinstDLL;
            InitVxMath();
            break;
        case DLL_PROCESS_DETACH:
            DeleteCriticalSection(&g_CriticalSection);
            break;
        default:
            break;
    }
    return TRUE;
}