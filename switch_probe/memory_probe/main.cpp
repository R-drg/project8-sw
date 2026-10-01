// Standalone feasibility experiment; no game or SDK-generated code.
#include <switch.h>
#include <array>
#include <bit>
#include <cinttypes>
#include <cstdio>

static_assert(sizeof(void*) == 8);
static_assert(std::endian::native == std::endian::little);
static_assert(std::byteswap(uint32_t{0x12345678}) == 0x78563412);
#if !defined(__aarch64__) || !defined(__SWITCH__) || !REX_PLATFORM_SWITCH
#error This probe requires AArch64 Horizon
#endif

namespace {
FILE* log_file;
void log(const char* message) {
    std::fputs(message, stdout);
    if (log_file) { std::fputs(message, log_file); std::fflush(log_file); }
}
template<typename... Args> void log(const char* format, Args... args) {
    std::printf(format, args...);
    if (log_file) { std::fprintf(log_file, format, args...); std::fflush(log_file); }
}
bool result(const char* operation, Result rc) {
    log("%s: 0x%08x\n", operation, unsigned(rc));
    return R_SUCCEEDED(rc);
}

// Each handle represents one backing page. A sparse implementation must also
// scale to guest allocations; this experiment does not establish that it will.
struct AliasResult {
    bool coherent = true;
    bool reprotected = true;
    bool sharedmem_unsupported = true;
    bool cleanup = true;
};
AliasResult aliases(uintptr_t base, const uintptr_t* offsets, size_t count) {
    AliasResult outcome;
    Handle handle = INVALID_HANDLE;
    if (!result("CreateSharedMemory(4096)",
                svcCreateSharedMemory(&handle, 4096, Perm_Rw, Perm_Rw))) return {false, false, false, true};
    size_t mapped = 0;
    bool ok = true;
    for (; mapped < count; ++mapped) {
        void* address = reinterpret_cast<void*>(base + offsets[mapped]);
        log("map offset=0x%" PRIxPTR " address=%p\n", offsets[mapped], address);
        if (!result("MapSharedMemory", svcMapSharedMemory(handle, address, 4096, Perm_Rw))) {
            ok = false; break;
        }
    }
    if (ok) {
        for (size_t writer = 0; writer < count; ++writer) {
            auto* p = reinterpret_cast<volatile uint32_t*>(base + offsets[writer]);
            *p = 0x81000000u + uint32_t(writer);
            for (size_t reader = 0; reader < count; ++reader) {
                auto* q = reinterpret_cast<volatile uint32_t*>(base + offsets[reader]);
                if (*q != *p) ok = false;
            }
        }
        log("simultaneous alias coherence: %s\n", ok ? "PASS" : "FAIL");
        void* first = reinterpret_cast<void*>(base + offsets[0]);
        // Do not intentionally fault: permission transitions and reported state
        // are only a prerequisite to the runtime's fault-driven write watches.
        for (u32 permission : {u32(Perm_R), u32(Perm_None), u32(Perm_Rw)}) {
            const Result rc = svcSetMemoryPermission(first, 4096, permission);
            bool changed = result("SetMemoryPermission", rc);
            outcome.reprotected &= changed;
            MemoryInfo info{}; u32 page{};
            if (result("QueryMemory", svcQueryMemory(&info, &page, uintptr_t(first)))) {
                log("requested=%u actual=%u state=0x%x\n", permission, info.perm, info.type);
                outcome.reprotected &= info.perm == permission;
                // Recognize only the exact hardware-observed SharedMem failure;
                // other errors must remain visible as unclassified failures.
                outcome.sharedmem_unsupported &= rc == 0xD401 && info.type == 0x6 && info.perm == Perm_Rw;
            } else {
                outcome.reprotected = false;
                outcome.sharedmem_unsupported = false;
            }
            for (size_t other = 1; other < count; ++other) {
                const uintptr_t address = base + offsets[other];
                if (result("QueryMemory(other alias)", svcQueryMemory(&info, &page, address))) {
                    // Check access first so a failed model produces a log.
                    log("other alias offset=0x%" PRIxPTR " permission=%u\n", offsets[other], info.perm);
                    if (info.perm != Perm_Rw) ok = false;
                    if (info.perm & Perm_R) {
                        ok &= *reinterpret_cast<volatile uint32_t*>(address) == 0x81000000u + uint32_t(count - 1);
                    }
                } else ok = false;
            }
        }
    }
    outcome.coherent = ok && mapped == count;
    if (!outcome.coherent) {
        outcome.reprotected = false;
        outcome.sharedmem_unsupported = false;
    }
    while (mapped) {
        --mapped;
        outcome.cleanup &= result("UnmapSharedMemory", svcUnmapSharedMemory(handle,
                     reinterpret_cast<void*>(base + offsets[mapped]), 4096));
    }
    outcome.cleanup &= result("CloseHandle", svcCloseHandle(handle));
    return outcome;
}
bool probe() {
    log("C++23 AArch64 LE64 memory probe; applet type=%u\n", unsigned(appletGetAppletType()));
    for (InfoType type : {InfoType_TotalMemorySize, InfoType_UsedMemorySize,
                          InfoType_AslrRegionAddress, InfoType_AslrRegionSize}) {
        u64 value{};
        Result rc = svcGetInfo(&value, type, CUR_PROCESS_HANDLE, 0);
        log("GetInfo(%u): rc=0x%08x value=0x%" PRIx64 "\n", unsigned(type), unsigned(rc), uint64_t(value));
    }
    for (unsigned call : {0x02u, 0x13u, 0x14u, 0x50u})
        log("syscall 0x%02x hinted=%d\n", call, envIsSyscallHinted(call));
    if (!envIsSyscallHinted(0x50)) {
        log("BLOCKED: shared-memory creation not available in loader hints.\n"
            "No alias test executed; this does not prove all Horizon approaches impossible.\n");
        return false;
    }
    constexpr size_t arena_size = 0x120000000ULL;
    virtmemLock();
    void* arena = virtmemFindAslr(arena_size, 4096);
    VirtmemReservation* reservation = arena ? virtmemAddReservation(arena, arena_size) : nullptr;
    virtmemUnlock();
    log("4.5 GiB virtual reservation (no physical allocation): %p\n", arena);
    if (!reservation) { log("BLOCKED: virtual reservation unavailable\n"); return false; }
    // Physical backing offset 0x1000 includes the E-view's +0x1000 bias.
    constexpr uintptr_t physical[] = {0x7f001000, 0xa0001000, 0xc0001000, 0xe0000000, 0x100001000};
    constexpr uintptr_t executable_data[] = {0x80000000, 0x90000000};
    auto physical_result = aliases(uintptr_t(arena), physical, std::size(physical));
    auto image_result = aliases(uintptr_t(arena), executable_data, std::size(executable_data));
    virtmemLock();
    virtmemRemoveReservation(reservation);
    virtmemUnlock();
    const bool alias_ok = physical_result.coherent && image_result.coherent;
    const bool cleanup_ok = physical_result.cleanup && image_result.cleanup;
    log("%s\n", alias_ok ? "ALIASING_PASS" : "ALIASING_FAIL");
    if (physical_result.reprotected && image_result.reprotected)
        log("REPROTECTION_PASS\n");
    else if (physical_result.sharedmem_unsupported && image_result.sharedmem_unsupported)
        log("REPROTECTION_UNSUPPORTED_WITH_SHAREDMEM\n");
    else log("REPROTECTION_FAILED_OR_UNCLASSIFIED\n");
    log("cleanup: %s\n", cleanup_ok ? "PASS" : "FAIL");
    log("NOT tested: fault recovery, sparse scaling, GPU access.\n"
        "Aliasing success is not a pass for the complete ReXGlue memory model.\n");
    // Independent Vulkan diagnostics remain useful when reprotection fails.
    return alias_ok && cleanup_ok;
}
}
bool run_memory_probe(FILE* destination) {
    log_file = destination;
    const bool ok = probe();
    log_file = nullptr;
    return ok;
}
#ifndef SWITCH_MEMORY_PROBE_LIBRARY
int main() {
    consoleInit(nullptr);
    log_file = std::fopen("sdmc:/switch/project8-memory-probe.log", "w");
    const bool ok = probe();
    if (log_file) { std::fclose(log_file); log_file = nullptr; }
    log("Press + to exit.\n");
    PadState pad{};
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
        consoleUpdate(nullptr);
    }
    consoleExit(nullptr);
    return ok ? 0 : 1;
}
#endif
