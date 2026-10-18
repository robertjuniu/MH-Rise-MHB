"""Build the in-game NVN HUD addon; SDK calls are game imports, not Android libs."""
import hashlib
import json
import os
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
OUT = ROOT / "build/renderer"
OUT.mkdir(parents=True, exist_ok=True)
from toolchain import llvm_root, tool
LLVM = llvm_root()
XENO = ROOT / "vendor/imgui-xeno"
temporary = OUT / "tmp"
temporary.mkdir(exist_ok=True)
environment = dict(os.environ, TMP=str(temporary), TEMP=str(temporary), TMPDIR=str(temporary))
for script in ("generate_capture.py", "generate_player_life.py", "generate_render_assets.py"):
    subprocess.run([sys.executable, str(ROOT / script)], check=True)
common = ["-ffreestanding", "-fPIC", "-fvisibility=hidden", "-fno-stack-protector",
          "-fno-unwind-tables", "-fno-asynchronous-unwind-tables", "-ffunction-sections",
          "-fdata-sections", "-O2"]
includes = ["src", "include", "src_framework", "src_framework/lib", "src_framework/nvn",
            "src_framework/nn", "extern/imgui", "user_config", "shaders/precompiled", "fonts/precompiled"]
cppflags = ["--target=aarch64-linux-android21", "--sysroot=" + str(LLVM / "sysroot"),
            *common, "-fno-rtti", "-fno-exceptions", "-fno-threadsafe-statics",
            "-fno-c++-static-destructors", "-std=c++20", "-DIMGUI_XENO_FORCE_PRECOMPILED_SHADERS=1",
            "-DIMGUI_XENO_INPUT_TOUCH=0", "-DIMGUI_XENO_INPUT_KBM=0", "-DIMGUI_XENO_INPUT_PAD=0",
            "-DIMGUI_XENO_LOAD_DEFAULT_FONT=0", "-DIMGUI_DISABLE_FILE_FUNCTIONS",
            "-DIMGUI_DISABLE_DEBUG_TOOLS", "-DIMGUI_USER_CONFIG=\"imgui_user_config.h\"",
            "-I" + str(ROOT / "src"), *["-I" + str(XENO / folder) for folder in includes]]
cflags = ["--target=aarch64-none-elf", *common, "-DMHR_CAPTURE=1", "-DMHR_RENDER=1", "-Wall", "-Wextra", "-Werror"]
sources = [ROOT / "src" / name for name in ("module.c", "capture.c", "player_life.c", "entry.S", "renderer.cpp", "renderer_support.cpp")]
sources.append(ROOT / "build/monster-icons.S")
sources.extend(XENO / "src/imgui_backend" / name for name in
               ("imgui_nvn.cpp", "imgui_impl_nvn.cpp", "MemoryBuffer.cpp", "MemoryPoolMaker.cpp"))
sources.append(XENO / "src/imgui_xeno.cpp")
sources.extend(XENO / "extern/imgui" / name for name in ("imgui.cpp", "imgui_draw.cpp", "imgui_widgets.cpp", "imgui_tables.cpp"))
objects = []
headers = list((ROOT / "src").glob("*.h"))
for folder in includes:
    headers.extend((XENO / folder).rglob("*.h"))
    headers.extend((XENO / folder).rglob("*.hpp"))
header_time = max(path.stat().st_mtime for path in headers)
for source in sources:
    target = OUT / (source.name + ".o")
    flags = cppflags if source.suffix == ".cpp" else cflags
    compiler = "clang++" if source.suffix == ".cpp" else "clang"
    if not target.exists() or max(source.stat().st_mtime, header_time) > target.stat().st_mtime:
        print("Compiling", source.name, flush=True)
        subprocess.run([str(tool(compiler)), *flags, "-c", str(source), "-o", str(target)], check=True, env=environment)
    objects.append(str(target))
elf = OUT / "mhr-hud.elf"
subprocess.run([str(tool("ld.lld")), "-shared", "--hash-style=sysv", "-Bsymbolic",
                "--gc-sections", "-init=mhr_module_init", "-T", str(ROOT / "src/module.ld"),
                *objects, "-o", str(elf)], check=True, env=environment)
data = elf.read_bytes()
section_offset = struct.unpack_from("<Q", data, 40)[0]
section_size, section_count = struct.unpack_from("<HH", data, 58)
sections = [struct.unpack_from("<IIQQQQIIQQ", data, section_offset + i * section_size) for i in range(section_count)]
imports = []
for section in sections:
    if section[1] != 11: continue
    strings = sections[section[6]]
    pool = data[strings[4]:strings[4] + strings[5]]
    for offset in range(section[4], section[4] + section[5], 24):
        name, info, _, defined, _, _ = struct.unpack_from("<IBBHQQ", data, offset)
        if name and not defined:
            imports.append(pool[name:pool.index(0, name)].decode())
available = set(json.loads((ROOT / "build/main-symbol-names.json").read_text()))
missing = sorted(set(imports) - available)
(OUT / "imports.json").write_text(json.dumps({"imports": imports, "not_seen_in_main": missing}, indent=2))
if missing:
    raise RuntimeError("Unvalidated game imports: " + ", ".join(missing))
loads = []
phoff = struct.unpack_from("<Q", data, 32)[0]
phsize, count = struct.unpack_from("<HH", data, 54)
for i in range(count):
    kind, flags, offset, address, _, size, memory_size, _ = struct.unpack_from("<IIQQQQQQ", data, phoff + i * phsize)
    if kind == 1: loads.append((flags, address, data[offset:offset + size], memory_size))
assert [p for p, *_ in loads] == [5, 4, 6]
header = bytearray(256)
header[:4] = b"NSO0"
struct.pack_into("<I", header, 12, 0x38)
header[0x40:0x60] = hashlib.sha256(data).digest()
payload = bytearray()
for i, (_, address, segment, _) in enumerate(loads):
    struct.pack_into("<III", header, 0x10 + i * 16, 256 + len(payload), address, len(segment))
    struct.pack_into("<I", header, 0x60 + i * 4, len(segment))
    header[0xa0 + i * 32:0xc0 + i * 32] = hashlib.sha256(segment).digest()
    payload.extend(segment)
struct.pack_into("<I", header, 0x3c, loads[2][3] - len(loads[2][2]))
destination = ROOT / "dist/MH Rise MHB/exefs/subsdk9"
destination.parent.mkdir(parents=True, exist_ok=True)
destination.write_bytes(header + payload)
manifest = {"phase": "NVN HUD experimental", "sha256": hashlib.sha256(header + payload).hexdigest(),
            "imports_checked_against_main": len(imports), "title_id": "0100B04011742000", "version": "16.0.2"}
(ROOT / "dist/renderer-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
print(json.dumps(manifest, indent=2))
