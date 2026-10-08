#!/usr/bin/env python3
"""Builds mods/textures from a KYA_TEXTURE_DUMP=1 dump with realesrgan-ncnn-vulkan.

Every texture is upscaled, effects and UI included; already upscaled files are kept.

Usage: make_texture_pack.py <dump dir> <realesrgan-ncnn-vulkan> <out dir> [model] [scale]
"""
import os
import shutil
import subprocess
import sys
import tempfile


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    # The upscaler runs from its own folder to find models/, so every path is made absolute.
    dump, upscaler, out = (os.path.abspath(a) for a in sys.argv[1:4])
    model = sys.argv[4] if len(sys.argv) > 4 else "realesrgan-x4plus-anime"
    scale = sys.argv[5] if len(sys.argv) > 5 else "4"

    pngs = sorted(f for f in os.listdir(dump) if f.endswith(".png"))
    print(f"{len(pngs)} textures")

    os.makedirs(out, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        src = os.path.join(tmp, "in")
        os.makedirs(src)
        for f in pngs:
            if not os.path.exists(os.path.join(out, f)):
                shutil.copy(os.path.join(dump, f), src)
        if os.listdir(src):
            subprocess.run([upscaler, "-i", src, "-o", out, "-n", model, "-s", scale, "-f", "png"],
                           cwd=os.path.dirname(os.path.abspath(upscaler)), check=True,
                           stdout=subprocess.DEVNULL)

    print(f"pack: {len(os.listdir(out))} files in {out}")


if __name__ == "__main__":
    main()
