#!/usr/bin/env python3
"""Summarise a draw census captured by the Xenos Vulkan backend.

The census is the binary file the runtime writes when `gpu_census_capture` is
issued or `gpu_census_auto_swap` is reached (see draw_census.h in the patched
SDK). It holds a header, one record per shader seen, one per draw, one per
resolve copy and a trailer. This script reads it and prints what a native
renderer has to handle: render-target setups, resolves, shader pairs,
primitive and index types, textures and constant usage.

It reads only the census, which records GPU state (register values, hashes
and counts), never textures, vertices or other game content.

    tools/switch/census_report.py census_file.bin [--draws]
"""

import argparse
import collections
import struct
import sys

PREFIX = struct.Struct("<HHI")
KIND_HEADER, KIND_SHADER, KIND_DRAW, KIND_COPY, KIND_TRAILER = 1, 2, 3, 4, 5

# Offsets into DrawRecord (packed, see draw_census.h). The record is 3292 bytes.
DRAW = struct.Struct(
    "<8s"          # prefix
    "I Q I"        # draw_ordinal, vk_submission, flags
    "8B"           # prim types, vs type, index type/format/endian, edram_mode, msaa
    "4I"           # guest/host vertex count, index base, index bytes
    "Q"            # index_content_hash
    "Q Q Q Q Q"    # vs_hash, vs_mod, ps_hash, ps_mod, pipeline_desc_hash
    "I"            # render_pass_key
    "I I I I I"    # rb_surface_info, modecontrol, colorcontrol, depthcontrol, color_mask
    "4I"           # rb_color_info[4]
    "I"            # rb_depth_info
    "4I"           # rb_blendcontrol[4]
    "I I I"        # stencilrefmask, stencilrefmask_bf, alpha_ref_bits
    "I I I I I I"  # pa_su_sc_mode_cntl, pa_cl_vte_cntl, scissor tl/br, sq_program_cntl, sq_context_misc
)
COPY = struct.Struct("<8s I I I I I I I I I I I B3s")
SHADER = struct.Struct("<8s Q B B H I I 4Q 8I I 3I I Q")
TRAILER = struct.Struct("<8s 12I q Q Q")

DRAW_FLAGS = {
    0: "indexed", 1: "memexport_vs", 2: "memexport_ps", 3: "rasterization_done",
    4: "ps_needed", 5: "prim_reset", 6: "index_native", 7: "vb_overflow",
    8: "tex_overflow", 9: "sampler_overflow", 10: "viz_query_killed",
}
# xenos::ColorRenderTargetFormat
COLOR_FORMATS = {
    0: "8_8_8_8", 1: "8_8_8_8_GAMMA", 2: "2_10_10_10", 3: "2_10_10_10_FLOAT",
    4: "16_16", 5: "16_16_16_16", 6: "16_16_FLOAT", 7: "16_16_16_16_FLOAT",
    10: "2_10_10_10_AS_10_10_10_10", 12: "2_10_10_10_FLOAT_AS_16_16_16_16",
    14: "32_FLOAT", 15: "32_32_FLOAT",
}
DEPTH_FORMATS = {0: "D24S8", 1: "D24FS8"}
PRIMS = {1: "points", 2: "lines", 3: "line_strip", 4: "tris", 5: "tri_fan", 6: "tri_strip",
         8: "rect_list", 13: "quad_list", 17: "quad_strip", 12: "polygon"}
MSAA = {0: "1x", 1: "2x", 2: "4x"}


def records(data):
    offset = 0
    while offset + PREFIX.size <= len(data):
        kind, size, ordinal = PREFIX.unpack_from(data, offset)
        if size < PREFIX.size or offset + size > len(data):
            break
        yield kind, data[offset:offset + size]
        offset += size


def surface(info):
    return info & 0x3FFF, MSAA.get((info >> 16) & 3, "?")


def color_target(info):
    return info & 0xFFF, COLOR_FORMATS.get((info >> 16) & 0xF, str((info >> 16) & 0xF))


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("census")
    parser.add_argument("--draws", action="store_true", help="list every draw")
    args = parser.parse_args()
    data = open(args.census, "rb").read()

    shaders = {}
    draws = []
    copies = []
    trailer = None
    header = None
    for kind, raw in records(data):
        if kind == KIND_HEADER:
            header = raw
        elif kind == KIND_SHADER and len(raw) >= SHADER.size:
            f = SHADER.unpack_from(raw)
            shaders[f[1]] = {"type": "vs" if f[2] == 0 else "ps", "memexport": f[3],
                             "dwords": f[5], "floats": f[6], "textures": bin(f[-2]).count("1")}
        elif kind == KIND_DRAW and len(raw) >= DRAW.size:
            draws.append(DRAW.unpack_from(raw))
        elif kind == KIND_COPY and len(raw) >= COPY.size:
            copies.append(COPY.unpack_from(raw))
        elif kind == KIND_TRAILER and len(raw) >= TRAILER.size:
            trailer = TRAILER.unpack_from(raw)

    if header:
        swap = struct.unpack_from("<Q", header, 8 + 8 + 4 + 4 + 64 + 64 + 4 + 4 + 8)[0]
        print(f"census from guest swap {swap}")
    if trailer:
        (_, recorded, skipped, ncopies, nshaders, *_rest) = trailer
        print(f"draws recorded {recorded}, skipped {skipped}, copies {ncopies}, shaders {nshaders}")
    print(f"{len(draws)} draws, {len(copies)} resolves, {len(shaders)} shaders\n")

    flag_counts = collections.Counter()
    prims = collections.Counter()
    passes = collections.Counter()
    pairs = collections.Counter()
    vertices = 0
    for d in draws:
        flags = d[3]
        for bit, name in DRAW_FLAGS.items():
            if flags & (1 << bit):
                flag_counts[name] += 1
        prims[PRIMS.get(d[4], str(d[4]))] += 1
        vertices += d[12]
        edram_mode = d[10]
        pitch, msaa = surface(d[23])
        color = color_target(d[28])
        depth = (d[32] & 0xFFF, DEPTH_FORMATS.get((d[32] >> 16) & 1, "?"))
        passes[(pitch, msaa, color, depth, edram_mode)] += 1
        pairs[(d[17], d[19])] += 1

    print(f"guest vertices: {vertices}")
    print("primitive types:", dict(prims.most_common()))
    print("draw flags:", dict(flag_counts.most_common()))
    print("\nrender-target setups (pitch, msaa, color0 base/format, depth base/format, edram mode):")
    for key, n in passes.most_common(20):
        print(f"  {n:5d}  {key}")
    print(f"\n{len(pairs)} distinct VS/PS pairs; most used:")
    for (vs, ps), n in pairs.most_common(15):
        v = shaders.get(vs, {})
        p = shaders.get(ps, {})
        print(f"  {n:5d}  vs {vs:016x} ({v.get('dwords', '?')} dw)  ps {ps:016x} "
              f"({p.get('dwords', '?')} dw, {p.get('textures', '?')} tex)")
    if copies:
        print("\nresolves (draws before, dest base, pitch, written bytes, surface pitch/msaa, color0):")
        for c in copies:
            pitch, msaa = surface(c[8])
            print(f"  after {c[1]:5d}: dest 0x{c[4]:08x} pitch {c[5]:5d} bytes {c[7]:8d} "
                  f"surface {pitch}/{msaa} color0 {color_target(c[9])}")
    memexport = sum(1 for s in shaders.values() if s["memexport"])
    print(f"\nshaders using memexport: {memexport}")
    if args.draws:
        print("\nordinal prim verts vs ps surface color0")
        for d in draws:
            print(f"  {d[1]:5d} {PRIMS.get(d[4], d[4]):>10} {d[12]:6d} {d[17]:016x} {d[19]:016x} "
                  f"{surface(d[23])} {color_target(d[28])}")


if __name__ == "__main__":
    sys.exit(main())
