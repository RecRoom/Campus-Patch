#include <windows.h>
#include <cstdint>
#include <cstring>
#include <vector>
#include <iostream>
#include "Console.h"
#include "../../Protections/AntiLog.h"   // Protections::log — silent-in-release log sink

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

    // Pattern-scan fallback for when the hardcoded RVAs drift (game rebuild / update).
    // Looks for the idiom the ImageSignature verifier is compiled into:
    //     E8 ?? ?? ?? ??               ; call <verifier>
    //     84 C0                        ; test al, al
    //     74 ??   OR  0F 84 ?? ?? ?? ??; je short/near <fail>
    //     33 C0                        ; xor eax, eax   (return-false fall-through)
    // and NOPs the JE so the call-succeeded path always runs.
    //
    // Filters: the call target must land inside GameAssembly's own image, AND the whole
    // idiom must sit inside a code section. If the scan finds != 1 matching site we refuse
    // to patch — false-positive rate on this idiom in a 20MB text section is high enough
    // that blindly patching all matches would nuke unrelated `if (!func()) return 0;` sites.
    static int PatternFallback() {
        Protections::log << "[IMAGES] [*] Pattern-scan fallback (hardcoded RVAs failed)\n";

        auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(GA);
        auto nt  = reinterpret_cast<const IMAGE_NT_HEADERS*>((const uint8_t*)GA + dos->e_lfanew);
        auto sec = IMAGE_FIRST_SECTION(nt);
        uintptr_t patched = 0;
        int candidates = 0;

        for (WORD s = 0; s < nt->FileHeader.NumberOfSections; ++s) {
            if (!(sec[s].Characteristics & IMAGE_SCN_CNT_CODE)) continue;
            const uint8_t* code = reinterpret_cast<const uint8_t*>(GA + sec[s].VirtualAddress);
            size_t codeSize = sec[s].Misc.VirtualSize;

            for (size_t i = 0; i + 16 < codeSize; ++i) {
                if (code[i] != 0xE8) continue;                          // CALL rel32
                int32_t rel;
                memcpy(&rel, code + i + 1, sizeof(rel));
                uintptr_t target = (uintptr_t)(code + i + 5) + rel;
                if (target < GA || target >= GA + NtHeaders->OptionalHeader.SizeOfImage) continue;

                size_t p = i + 5;
                if (code[p] != 0x84 || code[p + 1] != 0xC0) continue;   // TEST AL, AL
                p += 2;

                size_t jccLen = 0;
                if (code[p] == 0x74) jccLen = 2;
                else if (code[p] == 0x0F && code[p + 1] == 0x84) jccLen = 6;
                else continue;

                size_t fall = p + jccLen;
                if (code[fall] != 0x33 || code[fall + 1] != 0xC0) continue;   // XOR EAX, EAX

                ++candidates;
                uintptr_t jccAddr = (uintptr_t)(code + p);
                Protections::log << "[IMAGES] [*] candidate JE @ 0x" << std::hex << jccAddr
                                 << " (call target 0x" << target << ")" << std::dec << "\n";

                if (candidates == 1) patched = jccAddr;   // remember, but keep scanning to count

                if (candidates > 8) {
                    Protections::log << "[IMAGES] [!] > 8 candidates - pattern is too generic, aborting\n";
                    return 0;
                }
            }
        }

        Protections::log << "[IMAGES] [*] scan found " << candidates << " candidate(s)\n";
        if (candidates != 1) {
            Protections::log << "[IMAGES] [!] Refusing to patch - need exactly 1 unambiguous match\n";
            return 0;
        }

        // Apply NOP patch to the single unambiguous match.
        size_t jccLen = (*(uint8_t*)patched == 0x74) ? 2 : 6;
        std::vector<uint8_t> expected((uint8_t*)patched, (uint8_t*)patched + jccLen);
        std::vector<uint8_t> patch(jccLen, 0x90);
        if (MemoryPatch::Apply(patched, expected, patch)) {
            Protections::log << "[IMAGES] [+] Patched JE @ 0x" << std::hex << patched << std::dec << " via pattern scan\n";
            return 1;
        }
        Protections::log << "[IMAGES] [!] MemoryPatch::Apply failed\n";
        return 0;
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