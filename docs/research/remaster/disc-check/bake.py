# Bake-off: upscale sample textures with each model, build side-by-side sheets.
# usage: bake.py <samples_dir> <out_dir>
import glob, os, sys, time
import numpy as np, torch, spandrel
from PIL import Image, ImageDraw

src, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
dev = torch.device("mps" if torch.backends.mps.is_available() else "cpu")
here = os.path.dirname(os.path.abspath(__file__))
models = {os.path.basename(f).split(".")[0]: spandrel.ModelLoader().load_from_file(f).to(dev).eval()
          for f in sorted(glob.glob(os.path.join(here, "models", "*")))}
PAD = 8  # wrap padding so tiling textures stay seamless

def run(model, rgb):
    a = np.pad(rgb, ((PAD, PAD), (PAD, PAD), (0, 0)), mode="wrap")
    t = torch.from_numpy(a).permute(2, 0, 1)[None].float().div(255).to(dev)
    with torch.no_grad():
        y = model(t).clamp(0, 1)[0].permute(1, 2, 0).cpu().numpy()
    s = model.scale
    y = y[PAD * s:-PAD * s, PAD * s:-PAD * s]
    return (y * 255 + 0.5).astype(np.uint8)

timings = {k: 0.0 for k in models}
for f in sorted(glob.glob(os.path.join(src, "*.png"))):
    im = Image.open(f).convert("RGBA")
    rgb, alpha = np.asarray(im)[..., :3], im.getchannel("A")
    w, h = im.size
    cols = [("nearest", im.resize((w * 4, h * 4), Image.NEAREST)),
            ("lanczos", im.resize((w * 4, h * 4), Image.LANCZOS))]
    a4 = alpha.resize((w * 4, h * 4), Image.LANCZOS)  # ponytail: alpha via lanczos, model alpha pass if edges look bad
    for name, m in models.items():
        t0 = time.time()
        up = Image.fromarray(run(m, rgb)); up.putalpha(a4)
        timings[name] += time.time() - t0
        up.save(os.path.join(out, f"{os.path.basename(f)[:-4]}__{name}.png"))
        cols.append((name, up))
    # sheet on checkerboard so alpha is visible
    W, H = w * 4, h * 4
    sheet = Image.new("RGBA", (W * len(cols), H + 14), (40, 40, 40, 255))
    d = ImageDraw.Draw(sheet)
    for i, (name, img) in enumerate(cols):
        bg = Image.new("RGBA", (W, H), (90, 90, 90, 255))
        for y in range(0, H, 16):
            for x in range((y // 16 % 2) * 16, W, 32):
                bg.paste((130, 130, 130, 255), (x, y, x + 16, y + 16))
        sheet.paste(Image.alpha_composite(bg, img), (i * W, 14))
        d.text((i * W + 2, 1), name[:40], fill=(255, 255, 0, 255))
    sheet.save(os.path.join(out, f"SHEET_{os.path.basename(f)}"))
    print("done", f, flush=True)
print("seconds per model:", {k: round(v, 1) for k, v in timings.items()})
