"""Validate module layout, NCE boundaries and writable relocation destinations."""
import hashlib
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent
elf = (ROOT / "build/renderer/mhr-hud.elf").read_bytes()
nso = (ROOT / "dist/MH Rise MHB/exefs/subsdk9").read_bytes()
ranges = []
phoff = struct.unpack_from("<Q", elf, 32)[0]
phsize, count = struct.unpack_from("<HH", elf, 54)
for i in range(count):
    kind, flags, _, address, _, _, size, _ = struct.unpack_from("<IIQQQQQQ", elf, phoff + i * phsize)
    if kind == 1: ranges.append((flags, address, address + size))
for i in range(3):
    offset, address, size = struct.unpack_from("<III", nso, 0x10 + i * 16)
    assert hashlib.sha256(nso[offset:offset + size]).digest() == nso[0xa0 + i * 32:0xc0 + i * 32]
    assert address == ranges[i][1]
    if i == 0:
        for word in range(0, size, 4):
            instruction = struct.unpack_from("<I", nso, offset + word)[0]
            if instruction & 0xffe0001f == 0xd4000001:
                assert word >= 0x24
section_offset = struct.unpack_from("<Q", elf, 40)[0]
section_size, section_count = struct.unpack_from("<HH", elf, 58)
relocations = 0
for i in range(section_count):
    section = struct.unpack_from("<IIQQQQIIQQ", elf, section_offset + i * section_size)
    if section[1] != 4: continue
    for offset in range(section[4], section[4] + section[5], 24):
        target, _, _ = struct.unpack_from("<QQq", elf, offset)
        assert any(flags == 6 and begin <= target < end for flags, begin, end in ranges)
        relocations += 1
print("PASS NVN module: segment hashes, SVC scan boundary,", relocations, "writable relocations.")
