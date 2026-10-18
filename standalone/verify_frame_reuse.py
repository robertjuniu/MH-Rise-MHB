"""Execute ARM64 frame-reuse protection with signaled, busy and failed GPU fences."""
import os
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_X1, UC_ARM64_REG_X2, UC_ARM64_REG_X3, UC_ARM64_REG_X19, UC_ARM64_REG_X30, UC_ARM64_REG_SP, UC_ARM64_REG_PC

from toolchain import tool
OUT = ROOT / "build/frame-reuse-test"
OUT.mkdir(parents=True, exist_ok=True)
environment = dict(os.environ, TMP=str(OUT), TEMP=str(OUT), TMPDIR=str(OUT))
source = OUT / "test.c"
source.write_text('#include "frame_reuse.h"\n'
    'int acquire_case(unsigned *p, void *c, MhrWaitForFrame w, unsigned *b) { return mhr_acquire_frame(p,c,w,b); }\n')
subprocess.run([str(tool("clang")), "--target=aarch64-none-elf", "-O2", "-ffreestanding",
    "-fno-stack-protector", "-I" + str(ROOT / "src"), "-c", str(source), "-o", str(OUT / "test.o")], check=True, env=environment)
subprocess.run([str(tool("ld.lld")), "-Ttext=0x1000", "--entry=acquire_case", str(OUT / "test.o"), "-o", str(OUT / "test.elf")], check=True, env=environment)
subprocess.run([str(tool("llvm-objcopy")), "-O", "binary", "--only-section=.text", str(OUT / "test.elf"), str(OUT / "test.bin")], check=True)
cases = [(0, [], 1, 0, 0), (1, [0], 1, 0, 0), (1, [1], 1, 0, 0),
         (1, [2, 0], 1, 0, 1), (1, [2, 1], 1, 0, 1),
         (1, [2, 2], 0, 1, 1), (1, [2, 3], 0, 1, 1),
         (1, [3], 0, 1, 0), (1, [99], 0, 1, 0)]
for pending, results, expected, remaining, blocked in cases:
    cpu = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
    cpu.mem_map(0x1000, 0x4000)
    cpu.mem_map(0x8000, 0x1000)
    cpu.mem_write(0x1000, (OUT / "test.bin").read_bytes())
    cpu.mem_write(0x3000, bytes.fromhex("c0035fd6"))
    cpu.mem_write(0x4000, struct.pack("<II", pending, 99))
    cpu.reg_write(UC_ARM64_REG_X0, 0x4000)
    cpu.reg_write(UC_ARM64_REG_X1, 0x4020)
    cpu.reg_write(UC_ARM64_REG_X2, 0x3000)
    cpu.reg_write(UC_ARM64_REG_X3, 0x4004)
    cpu.reg_write(UC_ARM64_REG_X30, 0x2000)
    cpu.reg_write(UC_ARM64_REG_SP, 0x8ff0)
    for i in range(10): cpu.reg_write(UC_ARM64_REG_X19 + i, 0x9000 + i)
    waits = []
    responses = iter(results)
    def callback(uc, address, size, user):
        if address != 0x3000: return
        assert uc.reg_read(UC_ARM64_REG_X0) == 0x4020
        waits.append(uc.reg_read(UC_ARM64_REG_X1))
        uc.reg_write(UC_ARM64_REG_X0, next(responses))
        uc.reg_write(UC_ARM64_REG_PC, uc.reg_read(UC_ARM64_REG_X30))
    cpu.hook_add(UC_HOOK_CODE, callback)
    cpu.emu_start(0x1000, 0x2000, count=1000)
    assert cpu.reg_read(UC_ARM64_REG_X0) == expected
    assert struct.unpack("<II", cpu.mem_read(0x4000, 8)) == (remaining, blocked)
    assert waits == ([0, 1000000000] if blocked else [0] if pending else [])
    assert cpu.reg_read(UC_ARM64_REG_SP) == 0x8ff0
    assert all(cpu.reg_read(UC_ARM64_REG_X19 + i) == 0x9000 + i for i in range(10))
print("PASS: 9 ARM64 frame reuse cases; in-flight/failed slots protected, bounded wait, ABI preserved.")
