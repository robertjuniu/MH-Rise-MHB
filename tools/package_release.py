"""Package the validated addon and the tracked corresponding sources."""
import hashlib
import json
import subprocess
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
version = ROOT.joinpath("VERSION").read_text().strip()
OUT = ROOT / "dist"
OUT.mkdir(exist_ok=True)
module = ROOT / "addon/MH Rise MHB/exefs/subsdk9"
digest = hashlib.sha256(module.read_bytes()).hexdigest()
expected = ROOT.joinpath("addon/SHA256SUMS.txt").read_text().split()[0]
assert digest == expected
manifest = {"name": "MH Rise MHB (Monster Health Bar)", "version": version,
    "target_emulator": "Eden", "title_id": "0100B04011742000", "game_version": "16.0.2",
    "game_build": "C9A3DD7702075ECD90101F45553895822804D38F", "module_sha256": digest}
binary = OUT / f"MH-Rise-MHB-v{version}.zip"
with zipfile.ZipFile(binary, "w", zipfile.ZIP_DEFLATED) as archive:
    archive.write(module, "MH Rise MHB/exefs/subsdk9")
    archive.write(ROOT / "README.md", "LEIA-ME.md")
    archive.write(ROOT / "CREDITS.md", "CREDITS.md")
    archive.writestr("manifest.json", json.dumps(manifest, indent=2) + "\n")
    for path, name in ((ROOT / "LICENSE.txt", "GPL-2.0.txt"),
        (ROOT / "standalone/vendor/imgui-xeno/LICENSE", "imgui-xeno-GPL-2.0.txt"),
        (ROOT / "standalone/vendor/imgui-xeno/extern/imgui/LICENSE.txt", "Dear-ImGui-MIT.txt"),
        (ROOT / "standalone/assets/fonts/LICENSE.txt", "Roboto-Apache-2.0.txt"),
        (ROOT / "references/monster-db-LICENSE.txt", "catalogo-MIT.txt"),
        (ROOT / "references/cursey-LICENSE.txt", "cursey-MIT.txt")):
        archive.write(path, "licencas/" + name)
source = OUT / f"MH-Rise-MHB-v{version}-fontes.zip"
files = subprocess.check_output(["git", "-C", str(ROOT), "ls-files", "-z"]).decode().split("\0")
assert "README.md" in files and "standalone/src/renderer.cpp" in files
with zipfile.ZipFile(source, "w", zipfile.ZIP_DEFLATED) as archive:
    for name in files:
        if name: archive.write(ROOT / name, f"MH-Rise-MHB-v{version}/" + name)
for path in (binary, source):
    with zipfile.ZipFile(path) as archive:
        assert archive.testzip() is None
        assert not any(s in name for name in archive.namelist() for s in ("device-analysis", "test-runs", "/.git/", "main.nso", ".xci", ".nsp"))
OUT.joinpath("SHA256SUMS.txt").write_text("".join(hashlib.sha256(path.read_bytes()).hexdigest()+"  "+path.name+"\n" for path in (binary, source)))
print(json.dumps({"module": manifest, "packages": [str(binary), str(source)]}, indent=2))
