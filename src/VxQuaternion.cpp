#include "VxQuaternion.h"
#include "VxMatrix.h"

// Global lookup tables used by VxQuaternionSnuggle
static VxQuaternion g_SnuggleLookup[8] = {
    {0.0f, 0.0f, 0.0f, 1.0f}, // Identity
    {0.5f, 0.5f, 0.5f, 0.5f}, // 120-degree rotations
    {0.5f, -0.5f, 0.5f, 0.5f},
    {-0.5f, 0.5f, 0.5f, 0.5f},
    {0.5f, 0.5f, -0.5f, 0.5f},
    {-0.5f, -0.5f, 0.5f, 0.5f},
    {0.5f, -0.5f, -0.5f, 0.5f},
    {1.0f, 0.0f, 0.0f, 0.0f} // 180-degree rotation
};

VxQuaternion Vx3DQuaternionSnuggle(VxQuaternion *Quat, VxVector *Scale) {
    // It handles "snuggling" quaternions to remove scale artifacts
    if (!Quat || !Scale)
        return VxQuaternion(0, 0, 0, 1);

    float x = Scale->x;
    float y = Scale->y;
    float z = Scale->z;

    // Check if scales are nearly equal
    if (x == y && y == z) {
        // Uniform scale - return conjugate
        return Vx3DQuaternionConjugate(*Quat);
    }

    // Determine which scale component is different
    int caseIndex = 0;
    if (x == z) caseIndex = 3;      // y is different
    else if (y == z) caseIndex = 1; // x is different
    else if (x == y) caseIndex = 2; // z is different

    if (caseIndex == 0) {
        // All different - complex case
        VxQuaternion tempQuat = *Quat;

        // Find largest scale component
        float maxScale = XMax(x, XMax(y, z));
        float scales[4] = {tempQuat.x, tempQuat.y, tempQuat.z, tempQuat.w};

        // This involves checking signs and magnitudes of components
        bool signs[4];
        for (int i = 0; i < 4; i++) {
            signs[i] = scales[i] < 0.0f;
            if (signs[i]) scales[i] = -scales[i];
        }

        // Find dominant components and apply appropriate transformation
        float avgScale = (x + y + z + scales[3]) * 0.5f;
        float threshold = sqrtf((scales[0] + scales[1]) * 0.70710677f);

        if (avgScale < threshold) {
            // Use half-angle rotations
            VxQuaternion result(0, 0, 0, 1);
            for (int i = 0; i < 3; i++) {
                result[i] = signs[i] ? -0.5f : 0.5f;
            }

            // Swap scale components based on sign pattern
            bool parity = signs[0] ^ signs[1] ^ signs[2] ^ signs[3];
            if (parity) {
                float temp = x;
                x = y;
                y = z;
                z = temp;
            } else {
                float temp = y;
                y = x;
                x = z;
                z = temp;
            }

            Scale->x = x;
            Scale->y = y;
            Scale->z = z;

            return VxQuaternion(-result.x, -result.y, -result.z, result.w);
        } else {
            // Use simpler transformations
            int maxIndex = 0;
            if (scales[1] > scales[maxIndex]) maxIndex = 1;
            if (scales[2] > scales[maxIndex]) maxIndex = 2;
            if (scales[3] > scales[maxIndex]) maxIndex = 3;

            VxQuaternion result(0, 0, 0, 1);
            result[maxIndex] = signs[maxIndex] ? -1.0f : 1.0f;

            return VxQuaternion(-result.x, -result.y, -result.z, result.w);
        }
    } else {
        // Two components are equal - simpler case
        *Quat = Vx3DQuaternionConjugate(*Quat);

        // Apply appropriate lookup table transformation
        VxQuaternion transform = g_SnuggleLookup[caseIndex];
        VxQuaternion result = Vx3DQuaternionMultiply(*Quat, transform);

        // Swap scale components as needed
        if (caseIndex == 1) {
            float temp = x;
            x = z;
            z = y;
            y = temp;
        } else if (caseIndex == 2) {
            float temp = z;
            z = x;
            x = y;
            y = temp;
        }

        Scale->x = x;
        Scale->y = y;
        Scale->z = z;

        return VxQuaternion(-result.x, -result.y, -result.z, result.w);
    }
}
