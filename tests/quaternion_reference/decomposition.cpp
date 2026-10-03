#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <io.h>
#include <fcntl.h>

struct Q { float x, y, z, w; };
struct V { float x, y, z; };
struct M { float v[16]; };
struct Input { uint32_t operation, mask; float step; M a, b; };
using Polar = float (__cdecl *)(const M *, M *, M *);
using Spectral = V *(__cdecl *)(V *, const M *, M *);
using Decompose = void (__cdecl *)(const M *, Q *, V *, V *);
using Total = float (__cdecl *)(const M *, Q *, V *, V *, Q *);
using Interpolate = void (__cdecl *)(float, M *, const M *, const M *);

int wmain(int argc, wchar_t **argv) {
    static_assert(sizeof(void *) == 4 && sizeof(Input) == 140, "Original Win32 ABI");
    if (argc != 3 && argc != 4) return 2;
    HMODULE dll = LoadLibraryExW(argv[1], nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!dll) return 3;
    using GetControlWord = unsigned short (__cdecl *)();
    auto getControlWord = reinterpret_cast<GetControlWord>(GetProcAddress(dll, "?VxGetFPUControlWord@@YAGXZ"));
    if (getControlWord) std::fprintf(stderr, "x87 control word: 0x%04x\n", getControlWord());
    using ModifyFeatures = void (__cdecl *)(unsigned long, unsigned long);
    auto modify = reinterpret_cast<ModifyFeatures>(GetProcAddress(dll, "?ModifyProcessorFeatures@@YAXKK@Z"));
    // Internal helpers: RVAs verified in IDA. The capture script checks the
    // complete DLL hash before invoking this version-specific adapter.
    auto polar = reinterpret_cast<Polar>(reinterpret_cast<uintptr_t>(dll) + 0x1B990);
    auto spectral = reinterpret_cast<Spectral>(reinterpret_cast<uintptr_t>(dll) + 0x1BC30);
    auto decompose = reinterpret_cast<Decompose>(GetProcAddress(dll, "?Vx3DDecomposeMatrix@@YAXABVVxMatrix@@AAUVxQuaternion@@AAUVxVector@@2@Z"));
    auto total = reinterpret_cast<Total>(GetProcAddress(dll, "?Vx3DDecomposeMatrixTotal@@YAMABVVxMatrix@@AAUVxQuaternion@@AAUVxVector@@21@Z"));
    auto totalPtr = reinterpret_cast<Total>(GetProcAddress(dll, "?Vx3DDecomposeMatrixTotalPtr@@YAMABVVxMatrix@@PAUVxQuaternion@@PAUVxVector@@21@Z"));
    auto interpolate = reinterpret_cast<Interpolate>(GetProcAddress(dll, "?Vx3DInterpolateMatrix@@YAXMAAVVxMatrix@@ABV1@1@Z"));
    auto noScale = reinterpret_cast<Interpolate>(GetProcAddress(dll, "?Vx3DInterpolateMatrixNoScale@@YAXMAAVVxMatrix@@ABV1@1@Z"));
    if (!modify || !polar || !spectral || !decompose || !total || !totalPtr || !interpolate || !noScale) return 4;
    if (argc == 4) modify(0, 0x02000000);
    FILE *input = nullptr;
    if (_wfopen_s(&input, argv[2], L"rb") || !input) return 5;
    uint32_t count;
    if (std::fread(&count, 4, 1, input) != 1 || count > 100000) return 6;
    _setmode(_fileno(stdout), _O_BINARY);
    for (uint32_t i = 0; i < count; ++i) {
        Input row;
        if (std::fread(&row, sizeof(row), 1, input) != 1) return 6;
        float output[36] = {};
        Q q = {31, 32, 33, 34}, u = {41, 42, 43, 44};
        V pos = {51, 52, 53}, scale = {61, 62, 63};
        M a = {}, b = {};
        switch (row.operation) {
        case 0:
            output[0] = polar(&row.a, &a, &b);
            std::memcpy(output+1, &a, sizeof(a));
            std::memcpy(output+17, &b, sizeof(b));
            break;
        case 1:
            spectral(&scale, &row.a, &a);
            std::memcpy(output, &scale, sizeof(scale));
            std::memcpy(output+3, &a, sizeof(a));
            break;
        case 2:
            decompose(&row.a, &q, &pos, &scale);
            std::memcpy(output, &q, sizeof(q));
            std::memcpy(output+4, &pos, sizeof(pos));
            std::memcpy(output+7, &scale, sizeof(scale));
            break;
        case 3: output[0] = total(&row.a, &q, &pos, &scale, &u); break;
        case 4:
            output[0] = totalPtr(&row.a, row.mask & 1 ? &q : nullptr, row.mask & 2 ? &pos : nullptr,
                                row.mask & 4 ? &scale : nullptr, row.mask & 8 ? &u : nullptr);
            break;
        case 5: interpolate(row.step, &a, &row.a, &row.b); break;
        case 6: noScale(row.step, &a, &row.a, &row.b); break;
        default: return 7;
        }
        if (row.operation == 3 || row.operation == 4) {
            std::memcpy(output+1, &q, sizeof(q));
            std::memcpy(output+5, &pos, sizeof(pos));
            std::memcpy(output+8, &scale, sizeof(scale));
            std::memcpy(output+11, &u, sizeof(u));
        }
        if (row.operation == 5 || row.operation == 6) std::memcpy(output, &a, sizeof(a));
        if (std::fwrite(output, sizeof(output), 1, stdout) != 1) return 8;
    }
    std::fclose(input);
    return 0;
}
