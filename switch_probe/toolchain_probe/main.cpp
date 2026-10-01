#include <switch.h>
#include <atomic>
#include <bit>
#include <cstdio>
#include <exception>
#include <thread>

thread_local int tls_value = 7;
int main() {
    static_assert(sizeof(void*) == 8 && std::endian::native == std::endian::little);
    static_assert(std::byteswap(0x12345678u) == 0x78563412u);
    consoleInit(nullptr);
    FILE* file = std::fopen("sdmc:/switch/project8-toolchain-probe.log", "w");
    if (file) { std::fputs("Starting C++23 jthread / atomic / TLS test\n", file); std::fflush(file); }
    std::atomic<bool> worker_ok{false};
    tls_value = 42;
    try {
        std::jthread worker([&] {
            worker_ok = tls_value == 7;
            tls_value = 99;
        });
    } catch (const std::exception& error) {
        std::printf("Thread test exception: %s\n", error.what());
        if (file) std::fprintf(file, "Thread test exception: %s\n", error.what());
    }
    bool ok = worker_ok && tls_value == 42;
    std::printf("C++23 jthread / atomic / TLS: %s\n", ok ? "PASS" : "FAIL");
    if (file) {
        std::fprintf(file, "C++23 jthread / atomic / TLS: %s\n", ok ? "PASS" : "FAIL");
        std::fclose(file);
    }
    for (unsigned frame = 0; frame < 300 && appletMainLoop(); ++frame) consoleUpdate(nullptr);
    consoleExit(nullptr);
    return ok ? 0 : 1;
}
