#include "gtest/gtest.h"
#include "VxMath.h"

TEST(VxRectTest, TransformFromHomogeneousPointUsesTopForY) {
    const VxRect rect(10.0f, 20.0f, 110.0f, 220.0f);
    const Vx2DVector normalized(0.25f, 0.5f);

    Vx2DVector point;
    rect.TransformFromHomogeneous(point, normalized);

    EXPECT_NEAR(point.x, 35.0f, 1e-6f);
    EXPECT_NEAR(point.y, 120.0f, 1e-6f);
}

