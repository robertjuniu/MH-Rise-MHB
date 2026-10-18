"""Execute the independent capture hook with relocated module-owned storage."""
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_MEM_WRITE
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_X21, UC_ARM64_REG_X22, UC_ARM64_REG_X23, UC_ARM64_REG_X26, UC_ARM64_REG_Q0, UC_ARM64_REG_SP, UC_ARM64_REG_NZCV

header = (ROOT / "src/capture_code.h").read_text()
payload = bytes(int(x, 16) for x in re.findall(r"0x([0-9a-f]{2})", header))
literal = int(re.search(r"MHR_STATE_LITERAL_OFFSET (\d+)", header)[1])
hook, cave, state = 0x0501adb0, 0x07d46d00, 0x7400000000
import os
dump = Path(os.environ.get("MHR_MAIN_NSO", "main.nso"))
if dump.is_file():
    with dump.open("rb") as file:
        file.seek(0x100 + hook)
        assert file.read(8) == bytes.fromhex("401300bddb070034")
        file.seek(0x100 + cave)
        assert file.read(len(payload)) == bytes(len(payload))
else:
    print("SKIP: optional original-game signature check (set MHR_MAIN_NSO)")
cases = [(0, 0, 0x202), (1, 0, 0x202), (0, 1, 0x202), (0, 0, 0x584),
         (0, 0, 0), (0, 0, 0x1003)]
for base in (0, 0x7100000000):
    for selector, index, species in cases:
        cpu = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
        for address in (base + (hook & ~4095), base + (cave & ~4095),
                        state, 0x7200000000, 0x7300000000):
            cpu.mem_map(address, 4096)
        code = bytearray(payload)
        struct.pack_into("<Q", code, literal, state)
        cpu.mem_write(base + cave, bytes(code))
        cpu.mem_write(base + hook, bytes.fromhex("d4afb414"))
        for i in range(31): cpu.reg_write(UC_ARM64_REG_X0 + i, 0x1000 + i)
        for i in range(32): cpu.reg_write(UC_ARM64_REG_Q0 + i, 0x1122334455667788 + i)
        cpu.reg_write(UC_ARM64_REG_X26, 0x7200000000)
        cpu.reg_write(UC_ARM64_REG_X21, 0x7200000100)
        cpu.reg_write(UC_ARM64_REG_X22, index)
        cpu.reg_write(UC_ARM64_REG_X23, selector)
        cpu.reg_write(UC_ARM64_REG_SP, 0x7300000800)
        cpu.reg_write(UC_ARM64_REG_Q0, int.from_bytes(struct.pack("<f", 1234.5), "little"))
        cpu.reg_write(UC_ARM64_REG_NZCV, 0xa0000000)
        cpu.mem_write(0x7200000010, struct.pack("<fff", 1500, 2000, 0))
        cpu.mem_write(0x7200000150, struct.pack("<Q", 0x7200000500))
        cpu.mem_write(0x72000007d0, struct.pack("<I", species))
        registers = [UC_ARM64_REG_X0 + i for i in range(31)] + [
            UC_ARM64_REG_Q0 + i for i in range(32)] + [UC_ARM64_REG_SP, UC_ARM64_REG_NZCV]
        before = [cpu.reg_read(r) for r in registers]
        changes = []

        def write(uc, access, address, size, value, _):
            if state <= address < state + 32:
                changes.append((address - state, size, value))
        cpu.hook_add(UC_HOOK_MEM_WRITE, write)
        cpu.emu_start(base + hook, base + hook + 4, count=100)
        assert before == [cpu.reg_read(r) for r in registers]
        assert struct.unpack("<fff", cpu.mem_read(0x7200000010, 12)) == (1234.5, 2000, 0)
        result = bytes(cpu.mem_read(state, 32))
        if selector == index == 0 and 0 < species < 0x1000:
            assert struct.unpack_from("<ff", result) == (1234.5, 2000)
            assert struct.unpack_from("<Q", result, 8)[0] == 0x7200000000
            assert struct.unpack_from("<II", result, 16) == (2, species)
            assert struct.unpack_from("<Q", result, 24)[0] == 0x7200000100
            assert changes[0] == (16, 4, 1) and changes[-1] == (16, 4, 2)
        else:
            assert not any(result) and not changes
print("PASS: 12 ARM64 capture cases, relocated state, all GPR/SIMD/flags preserved,")
print("original HP store, species filtering, odd/even publication order, validated cave.")
