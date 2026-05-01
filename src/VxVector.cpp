#include "VxVector.h"
#include "VxMatrix.h"

static constexpr int SIN_TABLE_SIZE = 4096;  // 16KB for sin table
static constexpr float SIN_TABLE_SCALE = SIN_TABLE_SIZE / (2.0f * PI);

static float g_SinTable[SIN_TABLE_SIZE + 1];
static float g_CosTable[SIN_TABLE_SIZE + 1];

static bool InitializeTrigTablesOnce() {
    for (int i = 0; i <= SIN_TABLE_SIZE; ++i) {
        const float angle = (2.0f * PI * static_cast<float>(i)) / SIN_TABLE_SIZE;
        g_SinTable[i] = sinf(angle);
        g_CosTable[i] = cosf(angle);
    }
    return true;
}

void InitializeTables() {
    static const bool initialized = InitializeTrigTablesOnce();

    (void) initialized;
}

inline float TableSin(float x) {
    InitializeTables();

    // Range reduction and table lookup
    const float scaled = x * SIN_TABLE_SCALE;
    const int baseIndex = static_cast<int>(floorf(scaled));
    const int index = baseIndex & (SIN_TABLE_SIZE - 1);
    const float frac = scaled - static_cast<float>(baseIndex);

    // Linear interpolation between table entries
    const float v0 = g_SinTable[index];
    const float v1 = g_SinTable[index + 1];
    return v0 + frac * (v1 - v0);
}

inline float TableCos(float x) {
    InitializeTables();

    const float scaled = x * SIN_TABLE_SCALE;
    const int baseIndex = static_cast<int>(floorf(scaled));
    const int index = baseIndex & (SIN_TABLE_SIZE - 1);
    const float frac = scaled - static_cast<float>(baseIndex);

    const float v0 = g_CosTable[index];
    const float v1 = g_CosTable[index + 1];
    return v0 + frac * (v1 - v0);
}

int radToAngle(float val) {
    return static_cast<int>(val * (180.0f / PI));
}

float Tsin(int angle) {
    const float angleRad = static_cast<float>(angle) * (PI / 180.0f);
    return TableSin(angleRad);
}

float Tcos(int angle) {
    const float angleRad = static_cast<float>(angle) * (PI / 180.0f);
    return TableCos(angleRad);
}

const VxVector VxVector::m_AxisX(1.0f, 0.0f, 0.0f);
const VxVector VxVector::m_AxisY(0.0f, 1.0f, 0.0f);
const VxVector VxVector::m_AxisZ(0.0f, 0.0f, 1.0f);
const VxVector VxVector::m_Axis0(0.0f, 0.0f, 0.0f);
const VxVector VxVector::m_Axis1(1.0f, 1.0f, 1.0f);

VxVector &VxVector::operator=(const VxCompressedVector &v) {
    const int yaAngle = static_cast<int>(v.ya);
    const int xaAngle = static_cast<int>(v.xa);

    const float sinYa = Tsin(yaAngle);
    const float cosYa = Tcos(yaAngle);
    const float sinXa = Tsin(xaAngle);
    const float cosXa = Tcos(xaAngle);

    x = sinYa * cosXa;
    y = -sinXa;
    z = cosYa * cosXa;

    return *this;
}

const VxVector &VxVector::axisX() { return m_AxisX; }
const VxVector &VxVector::axisY() { return m_AxisY; }
const VxVector &VxVector::axisZ() { return m_AxisZ; }
const VxVector &VxVector::axis0() { return m_Axis0; }
const VxVector &VxVector::axis1() { return m_Axis1; }
