#include <atomic>
#include <cstdio>
#include <thread>
#include <rex/exception_handler.h>
#include <rex/system/gpu_plugin.h>
#include <switch.h>
#include "guest_memory_switch.h"

namespace {
std::atomic<unsigned> watched_writes{0};
std::atomic<unsigned> failures{0};
uint8_t* watched_page = nullptr;

bool WatchWrite(rex::arch::Exception* ex, void*) {
    if (ex->code() != rex::arch::Exception::Code::kAccessViolation ||
        ex->access_violation_operation() != rex::arch::Exception::AccessViolationOperation::kWrite ||
        ex->fault_address() != reinterpret_cast<uint64_t>(watched_page)) return false;
    watched_writes.fetch_add(1, std::memory_order_relaxed);
    return true; // the Switch exception adapter writes through the shadow alias
}

bool Check(FILE* log, const char* name, bool okay) {
    std::fprintf(log, "%s %s\n", name, okay ? "PASS" : "FAIL");
    std::fflush(log);
    if (!okay) failures.fetch_add(1, std::memory_order_relaxed);
    return okay;
}
}

int main() {
    consoleInit(nullptr);
    FILE* log = std::fopen("sdmc:/switch/project8-rexglue-test.log", "w");
    if (!log) log = stdout;
    auto gpu = rex::system::LoadGpuPlugin("xenos", "vulkan");
    Check(log, "STATIC_XENOS_PLUGIN", gpu != nullptr);
    gpu.reset();
    auto* base = RexGmInit(0x120000000ull);
    if (!Check(log, "RESERVATION", base != nullptr)) return 1;
    constexpr size_t phys = 0x100400000ull;
    constexpr size_t page = 0x1000;
    constexpr size_t addresses[] = {0x7F400000, 0xA0400000, 0xC0400000,
                                    0xE0400000, phys};
    bool views = true;
    for (size_t at : addresses) views &= RexGmAddView(base + at, phys, page);
    Check(log, "FIVE_VIEWS", views);
    Check(log, "LAZY_BEFORE", RexGmCommittedBytes() == 0 && RexGmMappedBytes() == 0);
    Check(log, "COMMIT", RexGmCommit(phys, page, REX_GM_WRITE));
    Check(log, "LAZY_MIRRORS", RexGmMappedBytes() == 0);
    auto* shadow = static_cast<volatile uint8_t*>(RexGmShadowFor(reinterpret_cast<uint64_t>(base + addresses[0])));
    Check(log, "SHADOW", shadow != nullptr);
    if (shadow) shadow[0] = 0x51;
    bool mirrors = true;
    for (size_t at : addresses) {
        mirrors &= RexGmFaultIn(reinterpret_cast<uint64_t>(base + at));
        mirrors &= base[at] == 0x51;
    }
    Check(log, "MIRROR_COHERENCE", mirrors);
    watched_page = base + addresses[1];
    Check(log, "PROTECT_READ", RexGmProtect(watched_page, page, REX_GM_READ, nullptr));
    RexGmAccess logical = REX_GM_NONE;
    size_t length = page;
    Check(log, "LOGICAL_PROTECTION", RexGmQueryProtect(watched_page, &length, &logical) && logical == REX_GM_READ);
    Check(log, "WATCH_UNMAPPED", RexGmModoProteccion() == 2);
    rex::arch::ExceptionHandler::Install(WatchWrite, nullptr);
    volatile uint8_t recovered_read = watched_page[0];
    Check(log, "SHADOW_READ_EXCEPTION", recovered_read == 0x51);
    *reinterpret_cast<volatile uint8_t*>(watched_page) = 0x61;
    Check(log, "WATCHED_WRITE_EXCEPTION", watched_writes.load() == 1 && shadow && shadow[0] == 0x61);
    const unsigned failures_before_threads = failures.load();
    std::thread a([&] { for (int i = 0; i < 64; ++i) if (static_cast<volatile uint8_t*>(watched_page)[0] != 0x61) ++failures; });
    std::thread b([&] { for (int i = 0; i < 64; ++i) if (static_cast<volatile uint8_t*>(watched_page)[0] != 0x61) ++failures; });
    a.join(); b.join();
    Check(log, "CONCURRENT_EXCEPTION_READS", failures.load() == failures_before_threads);
    rex::arch::ExceptionHandler::Uninstall(WatchWrite, nullptr);
    Check(log, "RESTORE_WRITE", RexGmProtect(watched_page, page, REX_GM_WRITE, nullptr));
    Check(log, "RESTORED_ACCESS", watched_page[0] == 0x61);
    std::fprintf(log, "SUMMARY %s\n", failures.load() ? "FAIL" : "PASS");
    std::fflush(log);
    if (log != stdout) std::fclose(log);
    std::printf("ReXGlue guest memory: %s\nPress + to exit\n", failures.load() ? "FAIL" : "PASS");
    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
        svcSleepThread(16'000'000);
    }
    RexGmShutdown();
    consoleExit(nullptr);
    return failures.load() ? 1 : 0;
}
