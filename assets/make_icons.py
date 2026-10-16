"""Builds every app icon from the one master image, assets/icon.png (1024x1024, transparent corners).

Edit or replace icon.png, then run this from anywhere (needs Pillow: pip install pillow):

    python assets/make_icons.py [windows] [macos] [android]

With no argument it builds every OS's icons; name one or more OSes to build only theirs. web/icon.png (the
GUI's tab icon) is always built, since every OS's app serves the GUI.

Outputs (git ignores them: CI runs this before building, see .github/actions/make-icons):
    assets/keymapper.ico                          Windows exe and installer
    assets/keymapper.icns                         macOS Keymapper.app
    android/app/src/main/res/mipmap-*/ic_launcher.png   Android launcher
    web/icon.png                                  the GUI's browser tab
"""
import io
import struct
import sys
from pathlib import Path

from PIL import Image

root = Path(__file__).resolve().parent.parent
master = Image.open(root / "assets" / "icon.png").convert("RGBA")


targets = set(sys.argv[1:]) or {"windows", "macos", "android"}
unknown = targets - {"windows", "macos", "android"}
if unknown:
    sys.exit(f"unknown OS: {', '.join(sorted(unknown))} (use windows, macos, android)")


def at(size):
    return master.resize((size, size), Image.LANCZOS)


if "windows" in targets:  # sizes 16..256 in one .ico
    ico_sizes = [16, 24, 32, 48, 64, 128, 256]
    at(256).save(root / "assets" / "keymapper.ico", format="ICO",
                 sizes=[(s, s) for s in ico_sizes], append_images=[at(s) for s in ico_sizes[:-1]])

if "macos" in targets:  # .icns is a list of PNG chunks (ic07 128, ic08 256, ic09 512, ic10 1024)
    chunks = b""
    for tag, size in [(b"ic07", 128), (b"ic08", 256), (b"ic09", 512), (b"ic10", 1024)]:
        buf = io.BytesIO()
        at(size).save(buf, "PNG")
        chunks += tag + struct.pack(">I", len(buf.getvalue()) + 8) + buf.getvalue()
    (root / "assets" / "keymapper.icns").write_bytes(b"icns" + struct.pack(">I", len(chunks) + 8) + chunks)

if "android" in targets:  # launcher icons, one per density
    for density, size in [("mdpi", 48), ("hdpi", 72), ("xhdpi", 96), ("xxhdpi", 144), ("xxxhdpi", 192)]:
        out = root / "android" / "app" / "src" / "main" / "res" / f"mipmap-{density}"
        out.mkdir(parents=True, exist_ok=True)
        at(size).save(out / "ic_launcher.png")

# The GUI's tab icon
at(128).save(root / "web" / "icon.png")
