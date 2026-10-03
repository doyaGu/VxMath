#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <io.h>
#include <fcntl.h>

// Standalone Win32 ABI adapter; no reconstructed math code is linked.
struct Quaternion { float x, y, z, w; };
struct Vector { float x, y, z; };
struct Matrix { float v[16]; };
struct Input { uint32_t operation; int unit, restore; float value[16]; };
using FromRotation = void (__thiscall *)(Quaternion *, const Vector *, float);
using FromEuler = void (__thiscall *)(Quaternion *, float, float, float);
using ToEuler = void (__thiscall *)(const Quaternion *, float *, float *, float *);
using FromMatrix = void (__thiscall *)(Quaternion *, const Matrix *, int, int);
using MatrixQuaternion = Quaternion *(__cdecl *)(Quaternion *, const Matrix *);
using ToMatrix = void (__thiscall *)(const Quaternion *, Matrix *);
using Snuggle = Quaternion *(__cdecl *)(Quaternion *, Quaternion *, Vector *);
using MatrixRotation = void (__cdecl *)(Matrix *, const Vector *, float);
using MatrixRotationOrigin = void (__cdecl *)(Matrix *, const Vector *, const Vector *, float);
using MatrixEuler = void (__cdecl *)(Matrix *, float, float, float);
using MatrixToEuler = void (__cdecl *)(const Matrix *, float *, float *, float *);

int wmain(int argc, wchar_t **argv) {
    static_assert(sizeof(void *) == 4 && sizeof(Input) == 76, "Win32 oracle ABI");
    if (argc != 3 && argc != 4) return 2;
    HMODULE dll = LoadLibraryExW(argv[1], nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!dll) return 3;
    using GetControlWord = unsigned short (__cdecl *)();
    auto getControlWord = reinterpret_cast<GetControlWord>(GetProcAddress(dll, "?VxGetFPUControlWord@@YAGXZ"));
    if (!getControlWord) return 4;
    std::fprintf(stderr, "x87 control word: 0x%04x\n", getControlWord());
    using Features = unsigned long (__cdecl *)();
    using ModifyFeatures = void (__cdecl *)(unsigned long, unsigned long);
    auto features = reinterpret_cast<Features>(GetProcAddress(dll, "?GetProcessorFeatures@@YAKXZ"));
    auto modifyFeatures = reinterpret_cast<ModifyFeatures>(GetProcAddress(dll, "?ModifyProcessorFeatures@@YAXKK@Z"));
    if (!features || !modifyFeatures) return 4;
    if (argc == 4) modifyFeatures(0, 0x02000000);
    else if (!(features() & 0x02000000)) return 9; // Capture the documented SSE branch.
    auto fromRotation = reinterpret_cast<FromRotation>(GetProcAddress(dll, "?FromRotation@VxQuaternion@@QAEXABUVxVector@@M@Z"));
    auto fromEuler = reinterpret_cast<FromEuler>(GetProcAddress(dll, "?FromEulerAngles@VxQuaternion@@QAEXMMM@Z"));
    auto toEuler = reinterpret_cast<ToEuler>(GetProcAddress(dll, "?ToEulerAngles@VxQuaternion@@QBEXPAM00@Z"));
    auto fromMatrix = reinterpret_cast<FromMatrix>(GetProcAddress(dll, "?FromMatrix@VxQuaternion@@QAEXABVVxMatrix@@HH@Z"));
    auto matrixQuaternion = reinterpret_cast<MatrixQuaternion>(GetProcAddress(dll, "?Vx3DQuaternionFromMatrix@@YA?AUVxQuaternion@@ABVVxMatrix@@@Z"));
    auto toMatrix = reinterpret_cast<ToMatrix>(GetProcAddress(dll, "?ToMatrix@VxQuaternion@@QBEXAAVVxMatrix@@@Z"));
    auto snuggle = reinterpret_cast<Snuggle>(GetProcAddress(dll, "?Vx3DQuaternionSnuggle@@YA?AUVxQuaternion@@PAU1@PAUVxVector@@@Z"));
    auto matrixRotation = reinterpret_cast<MatrixRotation>(GetProcAddress(dll, "?Vx3DMatrixFromRotation@@YAXAAVVxMatrix@@ABUVxVector@@M@Z"));
    auto matrixRotationOrigin = reinterpret_cast<MatrixRotationOrigin>(GetProcAddress(dll, "?Vx3DMatrixFromRotationAndOrigin@@YAXAAVVxMatrix@@ABUVxVector@@1M@Z"));
    auto matrixEuler = reinterpret_cast<MatrixEuler>(GetProcAddress(dll, "?Vx3DMatrixFromEulerAngles@@YAXAAVVxMatrix@@MMM@Z"));
    auto matrixToEuler = reinterpret_cast<MatrixToEuler>(GetProcAddress(dll, "?Vx3DMatrixToEulerAngles@@YAXABVVxMatrix@@PAM11@Z"));
    if (!fromRotation || !fromEuler || !toEuler || !fromMatrix || !matrixQuaternion ||
        !toMatrix || !snuggle || !matrixRotation || !matrixRotationOrigin || !matrixEuler || !matrixToEuler) return 4;
    FILE *input = nullptr;
    if (_wfopen_s(&input, argv[2], L"rb") || !input) return 5;
    uint32_t count = 0;
    if (std::fread(&count, 4, 1, input) != 1 || count > 100000) return 6;
    _setmode(_fileno(stdout), _O_BINARY);
    for (uint32_t i = 0; i < count; ++i) {
        Input row;
        if (std::fread(&row, sizeof(row), 1, input) != 1) return 6;
        float result[20] = {};
        Quaternion q = {row.value[0], row.value[1], row.value[2], row.value[3]};
        Quaternion returned = {0, 0, 0, 1};
        Vector axis = {q.x, q.y, q.z};
        Vector scale = {row.value[4], row.value[5], row.value[6]};
        Matrix matrix;
        std::memcpy(matrix.v, row.value, sizeof(matrix));
        switch (row.operation) {
        case 0: fromRotation(&returned, &axis, q.w); break;
        case 1: fromEuler(&returned, q.x, q.y, q.z); break;
        case 2: toEuler(&q, result, result + 1, result + 2); break;
        case 3: fromMatrix(&returned, &matrix, row.unit, row.restore); break;
        case 4: matrixQuaternion(&returned, &matrix); break;
        case 5: toMatrix(&q, &matrix); break;
        case 6:
            snuggle(&returned, &q, &scale);
            std::memcpy(result + 4, &q, sizeof(q));
            std::memcpy(result + 8, &scale, sizeof(scale));
            break;
        case 7: matrixRotation(&matrix, &axis, q.w); break;
        case 8: matrixEuler(&matrix, q.x, q.y, q.z); break;
        case 9: matrixToEuler(&matrix, result, result + 1, result + 2); break;
        case 10:
            std::memcpy(matrix.v + 12, &scale, sizeof(scale));
            matrixRotationOrigin(&matrix, row.unit == 2 ? reinterpret_cast<Vector *>(matrix.v) : &axis,
                                 row.unit == 1 ? reinterpret_cast<Vector *>(matrix.v + 12) : &scale, q.w);
            break;
        default: return 7;
        }
        if (row.operation == 0 || row.operation == 1 || row.operation == 3 || row.operation == 4 || row.operation == 6)
            std::memcpy(result, &returned, sizeof(returned));
        if (row.operation == 3 || row.operation == 4) std::memcpy(result + 4, &matrix, sizeof(matrix));
        if (row.operation == 5 || row.operation == 7 || row.operation == 8 || row.operation == 10) std::memcpy(result, &matrix, sizeof(matrix));
        if (std::fwrite(result, sizeof(result), 1, stdout) != 1) return 8;
    }
    std::fclose(input);
    return 0;
}
