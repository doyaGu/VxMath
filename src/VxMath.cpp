#include "VxMath.h"

#include <stdio.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

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
XBOOL VxComputeBestFitBBox(const XBYTE *Points, XULONG Stride, int Count, VxMatrix &BBoxMatrix, float AdditionalBorder) {
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
    BBoxMatrix[0].Normalize();
    BBoxMatrix[1].Normalize();

    // Compute the third axis using cross product for orthogonality
    float mx = BBoxMatrix[0][1] * BBoxMatrix[1][2] - BBoxMatrix[0][2] * BBoxMatrix[1][1];  // A.y*B.z - A.z*B.y
    float my = BBoxMatrix[0][2] * BBoxMatrix[1][0] - BBoxMatrix[0][0] * BBoxMatrix[1][2];  // A.z*B.x - A.x*B.z
    float mz = BBoxMatrix[0][0] * BBoxMatrix[1][1] - BBoxMatrix[0][1] * BBoxMatrix[1][0];  // A.x*B.y - A.y*B.x

    BBoxMatrix[2][0] = mx;
    BBoxMatrix[2][1] = my;
    BBoxMatrix[2][2] = mz;

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
    // Fixed: Process all points including the first one for consistency with decompiled code
    for (int i = 0; i < Count; i++) {
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

    // Calculate half-extents with additional border
    float halfExtentX = (maxX - minX) * 0.5f + AdditionalBorder;
    float halfExtentY = (maxY - minY) * 0.5f + AdditionalBorder;
    float halfExtentZ = (maxZ - minZ) * 0.5f + AdditionalBorder;

    // Scale the axes by their respective half-extents
    for (int i = 0; i < 3; i++) {
        const float extent = (i == 0) ? halfExtentX : (i == 1) ? halfExtentY : halfExtentZ;

        BBoxMatrix[i][0] *= extent;
        BBoxMatrix[i][1] *= extent;
        BBoxMatrix[i][2] *= extent;
        BBoxMatrix[i][3] = 0.0f; // Set homogeneous component to 0
    }

    // Transform center from principal axis space to world space
    float worldX = centerX * BBoxMatrix[0][0] + centerY * BBoxMatrix[1][0] + centerZ * BBoxMatrix[2][0];
    float worldY = centerX * BBoxMatrix[0][1] + centerY * BBoxMatrix[1][1] + centerZ * BBoxMatrix[2][1];
    float worldZ = centerX * BBoxMatrix[0][2] + centerY * BBoxMatrix[1][2] + centerZ * BBoxMatrix[2][2];

    // Set transformed center position
    BBoxMatrix[3][0] = worldX;
    BBoxMatrix[3][1] = worldY;
    BBoxMatrix[3][2] = worldZ;
    BBoxMatrix[3][3] = 1.0f;

    // Validate matrix for reasonable values
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 3; j++) {
            if (fabs(BBoxMatrix[i][j]) > 1000000.0f)
                return FALSE;
        }
    }

    return TRUE;
}

#ifndef VX_LIB
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
#endif
