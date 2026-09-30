# Disc asset facts for the remaster upscaling plan

Ticket: [Check the asset facts the upscaling plan needs against the disc](https://github.com/mgiuditta/Kya/issues/43).
Follows [asset-upscaling.md](https://github.com/mgiuditta/Kya/blob/research/remaster-assets/docs/research/remaster/asset-upscaling.md).
Measured on the EU disc (`CDEURO`), 2026-09-30. Scripts and full tables are in [`disc-check/`](disc-check/). No game data is committed: the sample images and contact sheets stay local.

## 1. Textures (full tables: [disc-check/textures.md](disc-check/textures.md))

Every bank (744), loose `.G2D` (175) and the font was parsed and decoded with emulated GS memory and swizzle, with no errors. There are 3,093 texture files, 9,732 bitmap instances and **3,158 unique images** (3,215 unique image+palette decodes). Level 0 totals 87 Mtexel: about **1.3 GB RGBA8 at 2x and 5.3 GB at 4x**, before compression.

| Format (unique images) | Share |
|---|---:|
| PSMT8 (8-bit palette) | 50.4% |
| PSMT4 (4-bit palette) | 43.0% |
| PSMCT32 | 6.5% |

- **Palettes:** all CT32 (CSM1, PA32); no CT16 anywhere. 83% of textures store a single palette, and the most stored is 8. Only **4.3% of images decode differently across palettes** (the most looks for one image is 11), so one replacement per image+palette key is cheap.
- **Sizes:** 128x128 is 54% and 64x64 is 14%. 64% are ≤128 px. The 512x512 images are almost all BONUS concept art (CT32).
- **Mips:** the header says 1, 2 or 3 levels (56/19/25%), but 451 images upload more levels than they declare. Replacements should regenerate mips rather than trust the header.
- **Alpha:** always in the PS2 0–0x80 range. Of the combinations, **41.8% are 1-bit cutouts**, 33.3% are opaque and 24.8% have graded alpha. 17 have alpha 0 everywhere (additive effects).
- **Tiling:** 6% of texture uses scroll their UVs, and many 128 px textures tile.
- **Upload:** 85% of palettised textures upload as PSMCT32, so a dumper needs the GS swizzle (done in `kya_tex.py`).

## 2. Audio sample rates (full histogram: [disc-check/audio-inventory.txt](disc-check/audio-inventory.txt))

| Category | Where | Rates | Amount |
|---|---|---|---|
| SFX (VAG in banks) | `LEVELIOP.BNK`, `FRNTDIOP.BNK`, cinematic banks | 22050 ×297, 11025 ×97, 44100 ×18, 16000 ×15, other ×7; mono | 433 samples, 11.4 min |
| Music instrument waves | one bank, identical in all 13 music levels | 32000 ×25, 44100 ×4, 22050 ×1, 26 at tuned rates (14602–44160) | 56 waves, 0.6 min |
| Long samples | `LEVEL/*/STREAMCH/*.VAG` | 22050 ×44, 11025 ×2; mono | 46 files, 5.4 min |
| Cinematic/voice streams | `LEVEL/*/STREAM/*.MIB` | **22050 stereo ×1010**, 44100 stereo ×2 (credits, pre-intro) | 1012 files, 241 min |
| FMV audio | `MOVIES/*.PSS` | 48000 stereo PCM16 (no ADPCM) | 17 files, 32.6 min |

- **MIB files have no header.** Their rate, channels, block size and block count live in a record in `LEVELIOP.BNK` (`src/b-witch/Audio.cpp:2124-2172`). Each file is padded to whole ~1 s blocks, with a median of 2.6 s of trailing silence. 14 MIBs are never referenced.
- **Music:** synthesised at a fixed 48 kHz. A wave's stored rate is its *tuning* (`edMusicSynth.cpp:166`), so a replacement wave must keep that rate or have its tuning updated.
- **SFX:** pitch is a ratio of the header rate. The port clamps pitch at 4x (`edSoundSampleService.cpp:64`).
- **FMVs:** MPEG-2 at 25 fps, 640x512 (640x384 for the `INT_PT*` intros). Their audio is already clean PCM, so upscaling them is a video problem, not an audio one.

## 3. Cinematic sync: the audio stream is the clock

- **The clock:** `CCinematic::IncrementCutsceneDelta` (`CinematicManager.cpp:2645-2688`) takes the cinematic time from `CBWCinSourceAudio::Func_0x1c` (`:6080-6161`). That is the stream playback time: `bytes × 3.5 / (rate × 2) + 0.04 s` (`edSoundPlay.cpp:893-895`). It falls back to the system timer after 5 s of stream failure. Cinematics without audio step per frame.
- **Scene end:** the scene ends at the cinematic's own authored length (`:2137`, `:2642`), not at the end of the audio. Voice lines exist only inside cinematics, and nothing waits for a voice stream to end.
- **Stubbed paths:** seek on time jump, loop and skip are behind `IMPLEMENTATION_GUARD` (`:485`, `:2180`, `:2194`).
- **Port bug:** on the PC port a stream never reports "finished". Voice positions stay at -2 (`_edSoundInit.cpp:37-44`), and only sample completions are drained (`edSoundPlay.cpp:43-60`). So a MIB that ends before its cinematic **freezes the timeline**, while PS2 would continue on the timer.

**Verdict:** cleaned or replaced voice audio does **not** need a sample-exact total length. It must:
- Keep its internal timing within about 1 frame (20 ms, with 40 ms the limit for lip sync), because the audio position drives animation and subtitles.
- Be at least as long as the original MIB (pad with silence) until the freeze bug is fixed.
- Keep 22050 Hz, or patch the bank record (rate, block size, block count), or have the port load the new file with its own rate.

Per-cinematic "MIB length ≥ cinematic length" was not checked; it needs the track-to-cinematic mapping.

## 4. Upscaler trial (39 textures, 4x, Apple M-series MPS via spandrel)

| Model | Licence | Time (39 textures) | Verdict |
|---|---|---:|---|
| 4x-PBRify_UpscalerV4 (DAT) | CC0 | 22.8 s | **Best for world textures.** Keeps the painted grain on moss, bark, stone and foliage without flattening. Adds some fake brush detail on smooth skin gradients. |
| RealESRGAN_x4plus | BSD-3 | 5.3 s | **Best for characters and smooth gradients** (face, clothes). Clean and faithful, but it smears fine terrain grain into flat paint. |
| RealESRGAN_x4plus_anime_6B | BSD-3 | 1.7 s | **Rejected.** It posterises shading and adds contrast highlights (foliage turns "cel"), which changes the art style. |
| Lanczos baseline | — | — | Enough for particles, glows, the font and small UI. The models add nothing on noisy particle textures. |

- **Tiling:** wrap-padding (8 px before the upscale, then cropped) gives seamless 2x2 tiles with every model.
- **Alpha:** upscaling alpha separately with lanczos softens the edges of 1-bit cutouts (41.8% of textures). The pipeline should re-threshold cutout alpha and clamp back to 0–0x80 on load.
- **Throughput:** PBRify runs at about 0.6 s per texture at mixed sizes, so the whole set of 3,215 takes roughly 30–40 min on one Mac. Batch cost is a non-issue; **review time is the real cost**.
- **Pipeline recommendation** (routing by category, with review on a side-by-side sheet):
  - world/terrain/foliage → PBRify V4
  - characters → Real-ESRGAN x4plus
  - FX/UI/font → lanczos or leave as is
  - BONUS art → either model

## Reproduce

```sh
python3 disc-check/kya_tex.py bin/MAC/CDEURO out        # survey.json, ~20 s
python3 disc-check/report.py > tables.md
KYA_CDEURO=bin/MAC/CDEURO python3 disc-check/export_samples.py
python3 disc-check/audio_rates.py bin/MAC/CDEURO records.json
# bake-off: venv with torch + spandrel + pillow, models in ./models next to bake.py
python bake.py samples/ out/
```
