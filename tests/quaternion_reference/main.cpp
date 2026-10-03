#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <io.h>
#include <fcntl.h>

// Isolated oracle: no reconstructed VxMath headers or import library. The
// original Win32 struct-return ABI takes an explicit first output pointer.
struct Quaternion { float x, y, z, w; };
struct Input { uint32_t operation; float t; Quaternion a, b, c, d; };
using Unary = Quaternion *(__cdecl *)(Quaternion *, const Quaternion *);
using Binary = Quaternion *(__cdecl *)(Quaternion *, const Quaternion *, const Quaternion *);
using Interpolate = Quaternion *(__cdecl *)(Quaternion *, float, const Quaternion *, const Quaternion *);
using Quadrangle = Quaternion *(__cdecl *)(Quaternion *, float, const Quaternion *, const Quaternion *, const Quaternion *, const Quaternion *);
using Normalize = void (__thiscall *)(Quaternion *);
using Multiply = void (__thiscall *)(Quaternion *, const Quaternion *);

int wmain(int argc, wchar_t **argv) {
    static_assert(sizeof(void *) == 4 && sizeof(Input) == 72, "Win32 oracle ABI");
    if (argc != 3) return 2;
    HMODULE dll = LoadLibraryExW(argv[1], nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!dll) return 3;
    using GetControlWord = unsigned short (__cdecl *)();
    auto getControlWord = reinterpret_cast<GetControlWord>(GetProcAddress(dll, "?VxGetFPUControlWord@@YAGXZ"));
    if (!getControlWord) return 4;
    std::fprintf(stderr, "x87 control word: 0x%04x\n", getControlWord());
    auto conjugate = reinterpret_cast<Unary>(GetProcAddress(dll, "?Vx3DQuaternionConjugate@@YA?AUVxQuaternion@@ABU1@@Z"));
    auto multiply = reinterpret_cast<Binary>(GetProcAddress(dll, "?Vx3DQuaternionMultiply@@YA?AUVxQuaternion@@ABU1@0@Z"));
    auto divide = reinterpret_cast<Binary>(GetProcAddress(dll, "?Vx3DQuaternionDivide@@YA?AUVxQuaternion@@ABU1@0@Z"));
    auto ln = reinterpret_cast<Unary>(GetProcAddress(dll, "?Ln@@YA?AUVxQuaternion@@ABU1@@Z"));
    auto exp = reinterpret_cast<Unary>(GetProcAddress(dll, "?Exp@@YA?AUVxQuaternion@@ABU1@@Z"));
    auto lnDif = reinterpret_cast<Binary>(GetProcAddress(dll, "?LnDif@@YA?AUVxQuaternion@@ABU1@0@Z"));
    auto slerp = reinterpret_cast<Interpolate>(GetProcAddress(dll, "?Slerp@@YA?AUVxQuaternion@@MABU1@0@Z"));
    auto squad = reinterpret_cast<Quadrangle>(GetProcAddress(dll, "?Squad@@YA?AUVxQuaternion@@MABU1@000@Z"));
    auto normalize = reinterpret_cast<Normalize>(GetProcAddress(dll, "?Normalize@VxQuaternion@@QAEXXZ"));
    auto memberMultiply = reinterpret_cast<Multiply>(GetProcAddress(dll, "?Multiply@VxQuaternion@@QAEXABU1@@Z"));
    if (!conjugate || !multiply || !divide || !ln || !exp || !lnDif || !slerp || !squad || !normalize || !memberMultiply) return 4;
    FILE *input = nullptr;
    if (_wfopen_s(&input, argv[2], L"rb") || !input) return 5;
    uint32_t count = 0;
    if (std::fread(&count, 4, 1, input) != 1 || count > 100000) return 6;
    _setmode(_fileno(stdout), _O_BINARY);
    for (uint32_t i = 0; i < count; ++i) {
        Input row;
        if (std::fread(&row, sizeof(row), 1, input) != 1) return 6;
        Quaternion result = {};
        switch (row.operation) {
        case 0: conjugate(&result, &row.a); break;
        case 1: multiply(&result, &row.a, &row.b); break;
        case 2: divide(&result, &row.a, &row.b); break;
        case 3: ln(&result, &row.a); break;
        case 4: exp(&result, &row.a); break;
        case 5: lnDif(&result, &row.a, &row.b); break;
        case 6: slerp(&result, row.t, &row.a, &row.b); break;
        case 7: squad(&result, row.t, &row.a, &row.b, &row.c, &row.d); break;
        case 8: result = row.a; normalize(&result); break;
        case 9: result = row.a; memberMultiply(&result, &row.b); break;
        case 10: result = row.a; memberMultiply(&result, &result); break;
        default: return 7;
        }
        if (std::fwrite(&result, sizeof(result), 1, stdout) != 1) return 8;
    }
    std::fclose(input);
    return 0;
}
