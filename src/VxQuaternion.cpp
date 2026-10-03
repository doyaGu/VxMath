#include "VxQuaternion.h"
#include "VxMatrix.h"

VxQuaternion Vx3DQuaternionSnuggle(VxQuaternion *Quat, VxVector *Scale) {
    if (!Quat || !Scale)
        return VxQuaternion();

    // Original 0x2429CAE0: choose an equivalent scale-axis representation
    // with the smallest rotation, permuting scales along with their axes.
    const float halfRoot = 0.70710677f;
    int distinct = -1;
    if (Scale->x == Scale->y) {
        if (Scale->x == Scale->z) return Vx3DQuaternionConjugate(*Quat);
        distinct = 2;
    } else if (Scale->x == Scale->z) distinct = 1;
    else if (Scale->y == Scale->z) distinct = 0;

    if (distinct >= 0) {
        VxQuaternion toZ;
        if (distinct == 0) toZ = VxQuaternion(0, halfRoot, 0, halfRoot);
        if (distinct == 1) toZ = VxQuaternion(halfRoot, 0, 0, halfRoot);
        if (distinct != 2) {
            *Quat = Vx3DQuaternionMultiply(*Quat, toZ);
            const float temp = (*Scale)[distinct];
            (*Scale)[distinct] = Scale->z; Scale->z = temp;
        }
        *Quat = Vx3DQuaternionConjugate(*Quat);
        const double x = Quat->x, y = Quat->y, z = Quat->z, w = Quat->w;
        double magnitude[3] = {
            z*z + w*w - 0.5,
            z*x - w*y,
            w*x + z*y
        };
        bool negative[3];
        for (int i = 0; i < 3; ++i) {
            negative[i] = magnitude[i] < 0;
            if (negative[i]) magnitude[i] = -magnitude[i];
        }
        const int winner = magnitude[0] > magnitude[1]
            ? (magnitude[0] > magnitude[2] ? 0 : 2)
            : (magnitude[1] > magnitude[2] ? 1 : 2);
        VxQuaternion permutation;
        if (winner == 0) {
            if (negative[0]) permutation = VxQuaternion(1, 0, 0, 0);
        } else if (winner == 1) {
            permutation = negative[1] ? VxQuaternion(.5f, .5f, -.5f, -.5f)
                                      : VxQuaternion(.5f, .5f, .5f, .5f);
            *Scale = VxVector(Scale->z, Scale->x, Scale->y);
        } else {
            permutation = negative[2] ? VxQuaternion(-.5f, .5f, -.5f, -.5f)
                                      : VxQuaternion(.5f, .5f, .5f, -.5f);
            *Scale = VxVector(Scale->y, Scale->z, Scale->x);
        }
        const VxQuaternion aligned = Vx3DQuaternionMultiply(*Quat, permutation);
        const double length = std::sqrt(magnitude[winner] + 0.5);
        const VxQuaternion freeRotation(0, 0,
            static_cast<float>(-aligned.z / length),
            static_cast<float>(aligned.w / length));
        permutation = Vx3DQuaternionMultiply(permutation, freeRotation);
        return Vx3DQuaternionMultiply(toZ, Vx3DQuaternionConjugate(permutation));
    }

    float magnitude[4];
    bool negative[4], odd = false;
    for (int i = 0; i < 4; ++i) {
        negative[i] = (*Quat)[i] < 0;
        odd ^= negative[i];
        magnitude[i] = std::fabs((*Quat)[i]);
    }
    // Keep the original strict comparisons: ties choose the later component.
    int low = magnitude[0] > magnitude[1] ? 0 : 1;
    int high = magnitude[2] > magnitude[3] ? 2 : 3;
    if (magnitude[low] > magnitude[high]) {
        const int other = low ^ 1;
        if (magnitude[other] > magnitude[high]) { high = low; low = other; }
        else { const int temp = low; low = high; high = temp; }
    } else if (magnitude[high ^ 1] > magnitude[low]) low = high ^ 1;

    // Wide scores avoid overflow; equivalent permutations may tie.
    const double all = (static_cast<double>(magnitude[3]) + magnitude[2] + magnitude[1] + magnitude[0]) * 0.5;
    const double two = (static_cast<double>(magnitude[low]) + magnitude[high]) * halfRoot;
    const double one = magnitude[high];
    VxQuaternion permutation(0, 0, 0, 0);
    if (all > two && all > one) {
        for (int i = 0; i < 4; ++i) permutation[i] = negative[i] ? -.5f : .5f;
        *Scale = odd ? VxVector(Scale->y, Scale->z, Scale->x)
                     : VxVector(Scale->z, Scale->x, Scale->y);
    } else if (!(all > two) && two > one) {
        permutation[low] = negative[low] ? -halfRoot : halfRoot;
        permutation[high] = negative[high] ? -halfRoot : halfRoot;
        if (low > high) { const int temp = low; low = high; high = temp; }
        if (high == 3) {
            static const int next[3] = {1, 2, 0};
            high = next[low];
            low = 3 - high - low;
        }
        const float temp = (*Scale)[low];
        (*Scale)[low] = (*Scale)[high]; (*Scale)[high] = temp;
    } else {
        permutation[high] = negative[high] ? -1.0f : 1.0f;
    }
    return Vx3DQuaternionConjugate(permutation);
}
