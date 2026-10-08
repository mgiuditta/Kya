#!/usr/bin/env python3
"""Builds mods/textures from a KYA_TEXTURE_DUMP=1 dump with realesrgan-ncnn-vulkan.

Effect textures (smoke, light shafts, flames) are skipped: their soft alpha upscales into
stripes. A texture counts as an effect when more than FX_SOFT_ALPHA of its pixels have
alpha strictly between 0 and 0x80 (PS2 opaque).

Usage: make_texture_pack.py <dump dir> <realesrgan-ncnn-vulkan> <out dir> [model] [scale]
Needs Pillow.
"""
import os
import shutil
import subprocess
import sys
import tempfile

from PIL import Image

FX_SOFT_ALPHA = 0.10


def is_effect(path):
    alpha = Image.open(path).convert("RGBA").getchannel("A")
    histogram = alpha.histogram()
    soft = sum(histogram[1:0x80])
    return soft > FX_SOFT_ALPHA * alpha.width * alpha.height


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    dump, upscaler, out = sys.argv[1:4]
    model = sys.argv[4] if len(sys.argv) > 4 else "realesrgan-x4plus-anime"
    scale = sys.argv[5] if len(sys.argv) > 5 else "4"

    pngs = sorted(f for f in os.listdir(dump) if f.endswith(".png"))
    keep = [f for f in pngs if not is_effect(os.path.join(dump, f))]
    print(f"{len(pngs)} textures, {len(pngs) - len(keep)} effects skipped, {len(keep)} to upscale")

    os.makedirs(out, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        src = os.path.join(tmp, "in")
        os.makedirs(src)
        for f in keep:
            if not os.path.exists(os.path.join(out, f)):
                shutil.copy(os.path.join(dump, f), src)
        if os.listdir(src):
            subprocess.run([upscaler, "-i", src, "-o", out, "-n", model, "-s", scale, "-f", "png"],
                           cwd=os.path.dirname(os.path.abspath(upscaler)), check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    # A pack built before an effect was reclassified keeps no stale copy of it.
    for f in pngs:
        if f not in keep and os.path.exists(os.path.join(out, f)):
            os.remove(os.path.join(out, f))
    print(f"pack: {len(os.listdir(out))} files in {out}")


if __name__ == "__main__":
    main()
