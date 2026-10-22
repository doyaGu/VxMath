#ifndef SEGMENT_BOX_FACE_ORACLE_H
#define SEGMENT_BOX_FACE_ORACLE_H
#include "RayBoxFaceOracle.h"

typedef RayBoxFaceResult SegmentBoxFaceResult;

// The independent six-face line candidates are shared with the ray tests.
// Apply the separately verified segment contract, without a production call.
static SegmentBoxFaceResult SegmentBoxFaceOracle(const VxRay &segment, const VxBbox &box, bool detailed) {
    SegmentBoxFaceResult r = RayBoxFaceOracle(segment, box, detailed);
    if (!r.code) return r;
    if (!detailed) {
        if (r.nearAxis >= 0) {
            // Test whether the actual endpoint reaches the entry face, rather
            // than rounding (bound-origin)/direction to one. For example,
            // -FLT_MAX + FLT_MAX is zero and never reaches a positive face.
            const int axis = r.nearAxis;
            const double endpoint = double(segment.m_Origin[axis]) + segment.m_Direction[axis];
            if (segment.m_Direction[axis] > 0 ? endpoint < box.Min[axis] : endpoint > box.Max[axis]) r.code = 0;
        }
    } else {
        const double end = 1.0 + 0x1p-23;
        if (r.nearAxis < 0 || r.nearT > end || (r.nearT < 0x1p-23 && r.farT > end)) r.code = 0;
    }
    return r;
}
#endif
