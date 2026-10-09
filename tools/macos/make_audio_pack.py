#!/usr/bin/env python3
"""Builds mods/audio from a KYA_AUDIO_DUMP=1 dump with AudioSR (48 kHz output).

Each channel is upscaled on its own (AudioSR is mono), in 10.24 s chunks joined with a short
crossfade, then trimmed to the original length scaled to 48 kHz so loop points and cutscene
timing stay put. Already built files are kept, so the run can be stopped and resumed.

Needs a Python 3.10 venv with audiosr==0.0.7, torch/torchaudio 2.4.1, numpy<2.

Usage: make_audio_pack.py <dump dir> <out dir> [kind ...]
  kinds default to: stream sample music
"""
import os
import sys
import tempfile
import time
import wave

import numpy as np

OUT_RATE = 48000
FADE = 0.25           # seconds of crossfade between chunks
CHUNK = 10.24 - FADE  # seconds per AudioSR pass, overlap included
MIN_SECONDS = 0.05


def read_wav(path):
    with wave.open(path, "rb") as f:
        assert f.getsampwidth() == 2, path
        rate, channels = f.getframerate(), f.getnchannels()
        pcm = np.frombuffer(f.readframes(f.getnframes()), dtype="<i2")
    return pcm.reshape(-1, channels).astype(np.float32) / 32768.0, rate


def write_wav(path, audio):
    pcm = (np.clip(audio, -1.0, 1.0) * 32767.0).round().astype("<i2")
    tmp = path + ".tmp"
    with wave.open(tmp, "wb") as f:
        f.setnchannels(pcm.shape[1])
        f.setsampwidth(2)
        f.setframerate(OUT_RATE)
        f.writeframes(pcm.tobytes())
    os.replace(tmp, path)


def resample(mono, rate, out_rate, length):
    # Linear resample for the fallback path; AudioSR output is already at 48 kHz.
    x = np.linspace(0.0, len(mono) - 1, length)
    return np.interp(x, np.arange(len(mono)), mono).astype(np.float32)


def upscale_channel(model, super_resolution, mono, rate, tmpdir):
    """Returns the channel at 48 kHz, exactly round(len * 48000 / rate) frames."""
    out_len = round(len(mono) * OUT_RATE / rate)
    step = int(CHUNK * rate)
    fade_in = int(FADE * rate)
    out = np.zeros(out_len + int(CHUNK * OUT_RATE), dtype=np.float32)
    weight = np.zeros_like(out)

    start = 0
    while start < len(mono):
        piece = mono[max(start - fade_in, 0):start + step]
        src = os.path.join(tmpdir, "chunk.wav")
        write_mono(src, piece, rate)
        piece_len = round(len(piece) * OUT_RATE / rate)
        try:
            result = super_resolution(model, src, seed=42, guidance_scale=3.5, ddim_steps=50)
            result = np.asarray(result, dtype=np.float32).reshape(-1)
        except (ValueError, RuntimeError):
            # AudioSR fails on some near-silent, full-band or very short chunks; keep those as they are.
            result = resample(piece, rate, OUT_RATE, piece_len)
        result = result[:piece_len]
        if len(result) < piece_len:
            result = np.pad(result, (0, piece_len - len(result)))

        # Triangular weights at the overlap, flat elsewhere.
        w = np.ones(piece_len, dtype=np.float32)
        overlap = round(min(start, fade_in) * OUT_RATE / rate)
        if overlap > 0:
            w[:overlap] = np.linspace(0.0, 1.0, overlap)
        begin = round(max(start - fade_in, 0) * OUT_RATE / rate)
        out[begin:begin + piece_len] += result * w
        weight[begin:begin + piece_len] += w
        start += step

    weight[weight == 0] = 1.0
    return (out / weight)[:out_len]


def write_mono(path, mono, rate):
    pcm = (np.clip(mono, -1.0, 1.0) * 32767.0).round().astype("<i2")
    with wave.open(path, "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(rate)
        f.writeframes(pcm.tobytes())


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    dump, out = (os.path.abspath(a) for a in sys.argv[1:3])
    kinds = sys.argv[3:] or ["stream", "sample", "music"]

    jobs = []
    for kind in kinds:
        folder = os.path.join(dump, kind)
        if not os.path.isdir(folder):
            continue
        os.makedirs(os.path.join(out, kind), exist_ok=True)
        for name in sorted(os.listdir(folder)):
            if name.endswith(".wav") and not os.path.exists(os.path.join(out, kind, name)):
                jobs.append((kind, name))
    print(f"{len(jobs)} files to build", flush=True)
    if not jobs:
        return

    os.environ.setdefault("TQDM_DISABLE", "1")
    from audiosr import build_model, super_resolution
    model = build_model(model_name="basic", device="auto")

    began = time.time()
    with tempfile.TemporaryDirectory() as tmpdir:
        for i, (kind, name) in enumerate(jobs):
            audio, rate = read_wav(os.path.join(dump, kind, name))
            out_len = round(len(audio) * OUT_RATE / rate)
            if len(audio) < MIN_SECONDS * rate or not np.any(audio):
                # Too short or silent for AudioSR: plain resample keeps the timing.
                channels = [resample(audio[:, c], rate, OUT_RATE, out_len) for c in range(audio.shape[1])]
            else:
                channels = [upscale_channel(model, super_resolution, audio[:, c], rate, tmpdir) for c in range(audio.shape[1])]
            write_wav(os.path.join(out, kind, name), np.stack(channels, axis=1))
            print(f"[{i + 1}/{len(jobs)}] {kind}/{name} {len(audio) / rate:.1f}s ({time.time() - began:.0f}s elapsed)", flush=True)


if __name__ == "__main__":
    main()
