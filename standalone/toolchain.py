"""Find an installed Android NDK LLVM toolchain without machine-specific paths."""
import os
import platform
from pathlib import Path

def llvm_root():
    configured = os.environ.get("ANDROID_NDK_HOME") or os.environ.get("ANDROID_NDK_ROOT")
    if configured:
        ndks = [Path(configured)]
    else:
        sdk = os.environ.get("ANDROID_HOME") or os.environ.get("ANDROID_SDK_ROOT")
        if not sdk and os.name == "nt":
            sdk = str(Path(os.environ.get("LOCALAPPDATA", "")) / "Android/Sdk")
        if not sdk:
            sdk = str(Path.home() / ("Library/Android/sdk" if platform.system() == "Darwin" else "Android/Sdk"))
        folder = Path(sdk) / "ndk"
        ndks = sorted(folder.glob("*"), reverse=True)
    host = {"Windows": "windows-x86_64", "Linux": "linux-x86_64", "Darwin": "darwin-x86_64"}[platform.system()]
    for ndk in ndks:
        root = ndk / "toolchains/llvm/prebuilt" / host
        if root.is_dir(): return root
    raise RuntimeError("Set ANDROID_NDK_HOME to an installed Android NDK (tested: 28.2.13676358)")

def tool(name):
    return llvm_root() / "bin" / (name + (".exe" if os.name == "nt" else ""))
