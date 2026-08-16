#include <windows.h>
#include <cstdint>
#include <cstring>
#include <vector>
#include <iostream>
#include "Console.h"

extern uintptr_t GA;
extern const IMAGE_NT_HEADERS* NtHeaders;

namespace RR::Methods::ImageSignature {
    extern uint32_t Outer;
    extern uint32_t Verifier;
}

namespace MemoryPatch {
    inline bool Same(const uint8_t* a, const std::vector<uint8_t>& b) {
        for (size_t i = 0; i < b.size(); ++i) {
            if (a[i] != b[i]) return false;
        }
        return true;
    }

    inline bool Apply(uintptr_t address, const std::vector<uint8_t>& expected,
        const std::vector<uint8_t>& patch) {
        if (expected.size() != patch.size() || address == 0) return false;

        uint8_t* target = reinterpret_cast<uint8_t*>(address);
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(target, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT)
            return false;

        if (Same(target, patch)) return true;
        if (!Same(target, expected)) return false;

        DWORD oldProtect = 0;
        if (!VirtualProtect(target, patch.size(), PAGE_EXECUTE_READWRITE, &oldProtect))
            return false;

        memcpy(target, patch.data(), patch.size());
        DWORD ignored = 0;
        VirtualProtect(target, patch.size(), oldProtect, &ignored);
        FlushInstructionCache(GetCurrentProcess(), target, patch.size());
        return true;
    }
}

namespace RR::Patches::ImageSignature {
    constexpr size_t ScanBytes = 0x10000;  // INCREASED from 0x800 to find the call

    inline bool Committed(const void* p) {
        MEMORY_BASIC_INFORMATION mbi{};
        return VirtualQuery(p, &mbi, sizeof(mbi)) && mbi.State == MEM_COMMIT;
    }

    bool Apply() {
        std::cout << "[IMAGES] " << "[*] Installing ImageSignature patch...\n";

        if (!GA || !NtHeaders) {
            std::cout << "[IMAGES] " << "[!] Invalid base\n";
            return false;
        }

        uintptr_t outerAddr = GA + RR::Methods::ImageSignature::Outer;
        uintptr_t verifierAddr = GA + RR::Methods::ImageSignature::Verifier;

        std::cout << "[IMAGES] " << "[*] Outer: 0x" << std::hex << outerAddr << std::dec << "\n";
        std::cout << "[IMAGES] " << "[*] Verifier: 0x" << std::hex << verifierAddr << std::dec << "\n";

        if (!Committed(reinterpret_cast<const void*>(outerAddr)) ||
            !Committed(reinterpret_cast<const void*>(verifierAddr))) {
            std::cout << "[IMAGES] " << "[!] Outer or Verifier not committed\n";
            return false;
        }

        const uint8_t* code = reinterpret_cast<const uint8_t*>(outerAddr);

        size_t callSite = 0;
        int hits = 0;
        for (size_t i = 0; i + 5 <= ScanBytes; ++i) {
            if (code[i] != 0xE8) continue;
            int32_t rel;
            memcpy(&rel, code + i + 1, sizeof(rel));
            uintptr_t target = outerAddr + i + 5 + rel;

            if (target < GA || target > GA + NtHeaders->OptionalHeader.SizeOfImage)
                continue;

            if (target == verifierAddr) {
                callSite = i;
                ++hits;
                std::cout << "[IMAGES] " << "[*] Found call to verifier at offset 0x" << std::hex << i << std::dec << "\n";
            }
        }

        std::cout << "[IMAGES] " << "[*] Found " << hits << " call sites to verifier\n";

        if (hits != 1) {
            std::cout << "[IMAGES] " << "[!] Expected 1 call site, got " << hits << "\n";
            return false;
        }

        size_t after = callSite + 5;
        if (after + 8 >= ScanBytes) {
            std::cout << "[IMAGES] " << "[!] Not enough bytes after call site\n";
            return false;
        }

        if (!(code[after] == 0x84 && code[after + 1] == 0xC0)) {
            std::cout << "[IMAGES] " << "[!] Expected test eax, eax (84 C0) not found\n";
            return false;
        }

        size_t jcc = after + 2;
        size_t jccLen = 0;

        if (code[jcc] == 0x74) {
            jccLen = 2;
        }
        else if (code[jcc] == 0x0F && code[jcc + 1] == 0x84) {
            jccLen = 6;
        }
        else if (code[jcc] == 0x75 || (code[jcc] == 0x0F && code[jcc + 1] == 0x85)) {
            std::cout << "[IMAGES] " << "[!] Found JNE instead of JE\n";
            return false;
        }
        else {
            std::cout << "[IMAGES] " << "[!] Unknown instruction at JCC: 0x" << std::hex << (int)code[jcc] << std::dec << "\n";
            return false;
        }

        size_t fall = jcc + jccLen;
        if (!(code[fall] == 0x33 && code[fall + 1] == 0xC0)) {
            std::cout << "[IMAGES] " << "[!] Expected xor eax, eax (33 C0) not found\n";
            return false;
        }

        std::vector<uint8_t> expected(code + jcc, code + jcc + jccLen);
        std::vector<uint8_t> patch(jccLen, 0x90);

        std::cout << "[IMAGES] " << "[*] Patching JE to NOPs at offset 0x" << std::hex << jcc << std::dec << "\n";

        if (MemoryPatch::Apply(outerAddr + jcc, expected, patch)) {
            std::cout << "[IMAGES] " << "[+] JE patched successfully\n";
        }
        else {
            std::cout << "[IMAGES] " << "[!] Failed to patch JE\n";
            return false;
        }

        std::cout << "[IMAGES] " << "[+] ImageSignature patch applied successfully\n";
        return true;
    }
}