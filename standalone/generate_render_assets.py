"""Prepare HUD atlas and relocation metadata using existing local inputs only."""
import json
import struct
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parent
OUT = ROOT / "build"
OUT.mkdir(exist_ok=True)
monsters = json.loads((ROOT / "assets/monsters.json").read_text())
icons = ROOT / "assets/icons"
pixels = bytearray()
lines = ["// SPDX-License-Identifier: GPL-2.0-or-later", "struct MonsterInfo { unsigned id; const char *name, *kind; };",
         "static const MonsterInfo mhr_monsters[] = {"]
for monster in monsters:
    path = icons / f"monster_{monster['id']:08x}.png"
    picture = Image.open(path).convert("RGBA").resize((64, 64), Image.Resampling.LANCZOS)
    pixels.extend(picture.tobytes())
    lines.append(f"    {{{monster['id']}, {json.dumps(monster['name'])}, {json.dumps(monster['kind'])}}},")
lines.append("};\n")
(ROOT / "src/monster_assets.h").write_text("\n".join(lines))
(OUT / "monster-icons.rgba").write_bytes(pixels)
(OUT / "monster-icons.S").write_text(
    '.section .rodata.monster_icons,"a"\n.balign 16\n.global mhr_icons_start\n'
    'mhr_icons_start:\n.incbin "' + (OUT / "monster-icons.rgba").as_posix() + '"\n')
with (OUT / "monster-icons.S").open("a") as assembly:
    for weight in ("Regular", "Bold"):
        symbol = "mhr_font_" + weight.lower()
        font = ROOT / "assets/fonts" / ("Roboto-" + weight + ".ttf")
        assert font.read_bytes()[:4] == b"\x00\x01\x00\x00"
        assembly.write('.balign 16\n.global ' + symbol + '_start\n.global ' + symbol + '_end\n' +
                       symbol + '_start:\n.incbin "' + font.as_posix() + '"\n' + symbol + '_end:\n')

# Build-specific metadata is committed; no game dump is needed to build.
(OUT / "main-symbol-names.json").write_text((ROOT / "assets/game-imports.json").read_text())
assert "0x1267f9f0" in (ROOT / "src/renderer_offsets.h").read_text()
print("Prepared", len(monsters), "64px icons for Rise Switch 16.0.2")
