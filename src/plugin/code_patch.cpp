#include "code_patch.h"

#include <Windows.h>

#include <cstring>

namespace {

// Executable memory within rel32 reach of site, holding `mov rax, target; jmp rax`.
void* AllocateJumpStub(uint8_t* site, void* target) {
    constexpr uintptr_t kStep = 0x10000;
    constexpr uintptr_t kReach = 0x70000000;
    const uintptr_t origin = reinterpret_cast<uintptr_t>(site) & ~(kStep - 1);
    for (uintptr_t distance = kStep; distance < kReach; distance += kStep) {
        for (const uintptr_t candidate : {origin - distance, origin + distance}) {
            void* stub = VirtualAlloc(reinterpret_cast<void*>(candidate), 0x1000,
                                      MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            if (stub == nullptr) {
                continue;
            }
            uint8_t code[12] = {0x48, 0xB8};
            std::memcpy(code + 2, &target, sizeof(target));
            code[10] = 0xFF;
            code[11] = 0xE0;
            std::memcpy(stub, code, sizeof(code));
            FlushInstructionCache(GetCurrentProcess(), stub, sizeof(code));
            return stub;
        }
    }
    return nullptr;
}

void WriteDisplacement(uint8_t* call, int32_t displacement) {
    DWORD oldProtect = 0;
    VirtualProtect(call + 1, sizeof(displacement), PAGE_EXECUTE_READWRITE, &oldProtect);
    std::memcpy(call + 1, &displacement, sizeof(displacement));
    VirtualProtect(call + 1, sizeof(displacement), oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), call, 5);
}

}  // namespace

std::span<const uint8_t> TextSection(uint8_t* module) {
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(module + dos->e_lfanew);
    const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
        if (std::memcmp(section->Name, ".text", 5) == 0) {
            return {module + section->VirtualAddress, section->Misc.VirtualSize};
        }
    }
    return {};
}

bool RedirectCall(uint8_t* call, void* replacement, int32_t& originalDisplacement) {
    void* stub = AllocateJumpStub(call, replacement);
    if (stub == nullptr) {
        return false;
    }
    std::memcpy(&originalDisplacement, call + 1, sizeof(originalDisplacement));
    WriteDisplacement(call, static_cast<int32_t>(reinterpret_cast<intptr_t>(stub) -
                                                 reinterpret_cast<intptr_t>(call + 5)));
    return true;
}

void RestoreCall(uint8_t* call, int32_t originalDisplacement) {
    WriteDisplacement(call, originalDisplacement);
}
