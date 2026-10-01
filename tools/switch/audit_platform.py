#!/usr/bin/env python3
"""Index platform-sensitive source lines; never scan game/generated directories.

Decisions are migration boundaries, not a claim the listed files were ported.
Third-party submodule implementation details require a later dependency audit.
"""
import argparse
import pathlib
import re

PATTERN = re.compile(r"WIN32|APPLE|linux|\bUNIX\b|POSIX|dlopen|\bmmap(?:64)?\b|pthread|SDL|\bX11\b|X11_|_X11|Wayland", re.I)


def treatment(path, text):
    if "gpu_plugin" in path:
        return "Switch-only static factory registration; preserve desktop ABI checks"
    if "d3d12/" in path or "math_msvc" in path or "win_math_compat" in path or "msvc_compat" in path:
        return "Windows-only; exclude from Horizon target"
    if "supported_dumps" in path or "guest_path" in path or "map_parser" in path or "xtypes" in path:
        return "No OS branch: guest data/path/error-code conventions or parser text"
    if "guest_ring_wait" in path or "guest_swap_wait" in path or "guest_vertex_unpack" in path or "guest_u8x4" in path:
        return "Existing ELF alias branch; verify Clang Horizon generated-symbol linkage later"
    if "dump_gate" in path or "input_record" in path or "parity_capture" in path:
        return "Audit non-Windows file/diagnostic APIs; preserve dump gate without override"
    if "launcher/" in path or "identify/" in path:
        return "Desktop launcher/worker path; separate from Switch probe and future entrypoint"
    if "xam_net" in path or "xsocket" in path:
        return "Switch socket-service initialization and newlib socket API branch required"
    if "user_module" in path:
        return "Replace file OpenMapped with tested buffered reads; unrelated to guest aliases"
    if "bytecode/" in path or "vulkan_spirv/" in path:
        return "No OS branch: incidental shader-bytecode/comment match"
    if "shaders/" in path or path.endswith("export_table.inc"):
        return "No host API inferred from shader/export-table text"
    if "/codegen/" in path or "/rexglue/" in path or "cmake/ppc_test" in path:
        return "Keep native host-tool path; audit generated target flags separately"
    if "win32" in path or "_win." in path or "_windows" in path or "surface_mac" in path or "surface_gnulinux" in path:
        return "Keep desktop implementation; exclude on Switch and provide own backend"
    if "memory" in path or "mmio" in path or "exception" in path or "seh" in path:
        return "Switch memory/fault branch required; probe semantics before implementation"
    if "thread" in path or "fiber" in path or "atomic_posix" in path:
        return "Switch threading/context adaptation required; pthread name is insufficient"
    if "dynlib" in path or "vulkan_instance" in path or "renderdoc" in path:
        return "Switch static lookup or explicit feature exclusion; no fake dlopen"
    if "CMake" in path or path.endswith(".cmake") or "cmake" in path:
        return "Explicit Horizon source/dependency selection; keep desktop conditions"
    if "vulkan" in path or "surface" in path or "window" in path:
        return "Explicit VI surface/window integration; retain portable Vulkan logic"
    if "sdl" in path.lower() or "SDL" in text:
        return "SDL3/libnx integration needs build and hardware verification"
    if "posix" in path or "filesystem" in path or "file.cpp" in path:
        return "Audit concrete newlib/libnx APIs; no blanket POSIX enablement"
    if "/platform" in path or path.endswith("assert.h"):
        return "Explicit Horizon platform branch or verified compiler capability"
    return "Keep guarded desktop code excluded; verify non-Windows fallback before runtime enablement"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sdk", type=pathlib.Path)
    args = parser.parse_args()
    project = pathlib.Path(__file__).resolve().parents[2]
    print("repository\tfile\tline\tmatched_terms\ttreatment")
    for label, root, starts in [
        ("ReXGlue", args.sdk, ["CMakeLists.txt", "CMakePresets.json", "cmake", "include", "src", "resources", "tests", "scripts", "thirdparty/CMakeLists.txt"]),
        ("Project8", project, ["src", "tools/cross"]),
    ]:
        files = set()
        for start in starts:
            p = root / start
            files.update(p.rglob("*") if p.is_dir() else [p])
        for p in sorted(files):
            if not p.is_file() or any(part in {"generated", "assets"} for part in p.relative_to(root).parts):
                continue
            try:
                lines = p.read_text().splitlines()
            except (UnicodeError, OSError):
                continue
            name = p.relative_to(root).as_posix()
            for number, line in enumerate(lines, 1):
                terms = sorted(set(PATTERN.findall(line)))
                if terms:
                    print(f"{label}\t{name}\t{number}\t{','.join(terms)}\t{treatment(name, line)}")


if __name__ == "__main__":
    main()
