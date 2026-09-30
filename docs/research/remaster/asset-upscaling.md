# Texture and audio upscaling options for the PS2 assets

Research for [#40](https://github.com/mgiuditta/Kya/issues/40), part of the remaster feasibility map [#36](https://github.com/mgiuditta/Kya/issues/36).
Researched 2026-09-30. Code references are against `main` @ `2dfa7f0` (KyaTexture submodule @ `263737b`).

## Answer in short

- **Textures: the pipeline is mostly already in the port.** The renderer decodes every G2D texture (including 4-bit/8-bit palettised ones) into an RGBA8 buffer, keeps that buffer, and has an opt-in ONNX Runtime / ncnn upscaler that swaps a texture for a larger one at runtime (`TextureUpscale.cpp`, "Upscale" button in the debug texture viewer). The shader already handles a texture whose real size differs from its PS2 size, the same way PCSX2's hardware renderer does. What's missing is **offline** work: a dump step, a batch pipeline with manual review, and a **replacement-file loader** that reads reviewed PNGs from disk.
- **Model choice is limited by licence.** The model the port currently points at, `4x-UltraSharpV2`, is **CC BY-NC-SA 4.0**. That's usable in a free fan project if you give attribution and share alike, but it can't be bundled with anything commercial. The cleanest fits for 2000s game textures are Kim2091's **PBRify Upscaler** models (**CC0**, trained on old game textures) and Real-ESRGAN's official weights (**BSD-3-Clause**). A large set of **CC BY 4.0** (Nomos*) and **WTFPL** (NMKD, DitherDeleter) models is also open for any use.
- **Tools** (chaiNNer GPL-3.0, Upscayl AGPL-3.0, spandrel MIT, traiNNer-redux/neosr Apache-2.0, ncnn BSD-3, ONNX Runtime MIT) are all fine for a free project. The copyleft ones only matter if we ship or modify their code. They put no licence on the images they produce.
- **Audio has two separate problems.** (1) *Music* isn't streamed audio. It's Sony sequence + instrument-bank data that the port synthesises live at 48 kHz (`edMusicSynth`). Upscaling it means improving the **instrument samples**, not songs. Pre-rendering songs would lose the game's per-channel mixing and transitions. (2) *Streamed VAG* (voice, cinematics) is PS-ADPCM mono/stereo at the header's rate. It can be decoded losslessly (vgmstream, ISC-style licence) and cleaned or band-extended with AudioSR (MIT code, Apache-2.0 weights), Resemble Enhance (MIT) or DeepFilterNet (MIT/Apache-2.0), with manual A/B review.
- **Higher-quality masters: none found.** Wikipedia credits the music to Manuel Lauvernier and Thomas Colin, but no official soundtrack release turned up in any source I could reach. The disc data is the only master we have. Asking the composers directly would be the only way to find better sources.
- **Loading replacements fits the sequel's loose-file override (#28), with one caveat.** The override (`mods/<bank>/<entry>`) works at **bank-entry** level. A replaced `.g2d` entry would have to be a valid G2D with PS2 GS upload packets, and G2D is capped by PS2 texture sizes (TW/TH ≤ 1024 and GS memory budgets). So HD textures should **not** go through the bank override. They need their own **renderer-side replacement lookup** (a PCSX2-style `textures/replacements/<key>.png` directory), keyed on the G2D texture identity the port already has. Audio *can* use the existing paths: the stream service already probes `assets/<path>` for VAGs, and the #28 `edCFiler_CDVD::open` loose-file hook covers whole files.

## 1. What the port already does with textures

Primary source: the code.

- **Decode path.** `Renderer::Kya::TextureLibrary::AddTexture` is registered on the engine's texture-loaded delegate. For each G2D it walks materials → layers → textures and resolves the bitmap and, if `bHasPalette`, the palette chosen by `pLayer->paletteId` (`port/KyaTexture/src/Texture.cpp`, `G2D::Layer::ProcessTexture`). `SimpleTexture::CreateRenderer` then uploads the palette (`TextureUpload::UploadPalette`) and the bitmap through PCSX2-derived GS local-memory emulation (`TextureUpload::UploadTexture`), which **de-palettises into RGBA8** (`port/Windows/Renderer/Vulkan/src/Texture/TextureCache.cpp`). The CPU copy is kept in `GSSimpleTexture::pUploadBuffer`, documented as "Used for upscaling and reverting to original texture data" (`TextureCache.h`).
  - Consequence: **palettised textures need no special handling for upscaling**. You upscale the resolved RGBA image. But one bitmap used with *different* palettes (different `paletteId` per layer) becomes several `SimpleTexture`s, and each palette variant needs its own replacement. PCSX2 has the same constraint and puts a CLUT hash in its file names (see §3).
- **Alpha.** The RGBA8 buffer holds raw PS2 alpha, where `0x80` means opaque. The shader rescales it (`ps2.hlsl`, the `* 255.0f` fetch and the `>= 128.0f` AEM test). Replacement images must therefore **keep the PS2 0–128 alpha range**, not 0–255. The existing upscaler runs the model on RGB only and scales alpha with nearest-neighbour (`TextureUpscale.cpp`, `UpscaleAlpha`). That's safe for cut-out alpha but gives blocky edges on smooth alpha (smoke, glows, foliage fringes). An offline pipeline should upscale alpha as a separate greyscale pass, or with an RGBA-capable model/tool (chaiNNer can split and merge channels), and then clamp it to 0–128.
- **Runtime upscaler (already present).** CMake options `ENABLE_UPSCALING` (ncnn, FetchContent tag `20240410`) and `ENABLE_UPSCALING_ONNX` (ONNX Runtime 1.20.1, **win-x64 zip only**) (`port/Windows/Renderer/CMakeLists.txt`). `VulkanRenderer.cpp` loads `../../upscaling/models/4x-UltraSharpV2_fp32_op17.onnx`. `Renderer::Native::UpscaleTexture` runs asynchronously and applies the result via `GSSimpleTexture::Resize`. It's triggered per texture from the debug menu (`port/DebugMenu/src/DebugTexture.cpp`). For macOS/Linux the ONNX fetch would need per-platform packages; ncnn (Vulkan, via MoltenVK on macOS) is already cross-platform.
- **Sizes the shader sees.** `PS2::EmulateTextureSampler(width, height, …)` sets `WH = (1<<TW, 1<<TH, width, height)`: the PS2 logical size plus the real image size, as in PCSX2's HW renderer (`VulkanPS2.cpp`). `ps2.hlsl` uses `WH.zw` for texel snapping and `MinMax` for region clamp/repeat. So a larger replacement is mechanically supported. Region-repeat (`WMS/WMT == 3`) textures should still get a visual check at scale. Note that `bilinear` is hard-coded to `false` there. Upscaled textures will look noticeably better once TEX1 filtering is honoured or a linear option is added. That's a renderer item, not an asset item.
- **Identity available for a replacement key.** Each `SimpleTexture` has a name `<g2d file name> (m: <material> l: <layer>)` and `Details{layerIndex, materialIndex, …, hash}`, where `hash` is the bitmap's `ed_hash_code` (`Texture.cpp`, `ProcessTexture`). The G2D name is the original build path (for example `D:\PROJECTS\B-WITCH\RESOURCE\Build\...\BOUCHON_Scene01_for_ilot_11_06.g2d`, commented in `G2D::G2D`).

## 2. How replacement assets would be loaded

The sequel decision [#28](https://github.com/mgiuditta/Kya/issues/28) chose a per-entry loose-file override: `mods/<bank path>/<entry path>` replaces a bank entry's bytes at edBank choke points, plus a `mods/` check in `edCFiler_CDVD::open` for whole files.

| Asset | Via #28 override? | Recommendation |
|---|---|---|
| HD textures | **No.** A replaced `.g2d` must still be a PS2 G2D (GS upload packets, PSM formats, TW/TH ≤ 2^10, and the game's memory budgets from #23). Authoring that is a G2D *writer* problem and doesn't allow HD sizes anyway. | Add a **renderer-side replacement lookup** in `SimpleTexture::CreateRenderer`: if `textures/replacements/<key>.png` exists, upload it instead of `pUploadBuffer`. Keep the original decoded buffer for "Revert". Key = content hash of the decoded RGBA8 (or of bitmap + palette bytes), which survives path/entry renames and dedupes shared textures, with the G2D name + m/l index as a human-readable sidecar. Add a **dump** mode that writes `textures/dumps/<key>.png` on first load. This mirrors PCSX2 (§3), which modders already know. |
| Music instrument samples | Yes, in principle. Samples live in bank bodies that `edMusicData` parses. | Replacing sample bytes means re-encoding to PS-ADPCM and a matching bank layout. Better: a port-side hook where `edMusicData` decodes a sample (`MusicSample{pcm, rate, loop}`) and looks up a loose WAV by bank + sample index, so it can be higher rate and uncompressed. Loop points must be rescaled by the rate ratio. |
| Streamed VAG (voice/cinematics) | Yes, whole-file. | Already partly there: `GetStreamPathCandidates` probes `assets/<path>` and `bin/WIN/<path>` (`port/Audio/edSoundStreamService.cpp`). The stream service decodes VAG to PCM and plays it through XAudio2 at the header rate, so accepting a WAV/FLAC at a higher rate is a small port-side change. Watch out: `SOUND_GetStreamInfo` reports position in *ADPCM bytes* (16 bytes per 28 samples), which the game may use for lip-sync/cine timing. Replacements must keep the **same duration** and the position conversion must use the original rate. |

So the #28 override is the right mechanism for data (text, scenes, whole files, audio). HD textures need a small parallel mechanism in the renderer. Both are port-only and PS2 behaviour stays untouched.

## 3. PCSX2 texture-replacement practice (the community reference)

Primary source: `pcsx2/GS/Renderers/HW/GSTextureReplacements.cpp` and `GSTextureReplacementLoaders.cpp` on PCSX2 master (GPL-3.0).

- Directories per game: `textures/<serial>/dumps` and `textures/<serial>/replacements` (`TEXTURE_DUMP_SUBDIRECTORY_NAME`, `TEXTURE_REPLACEMENT_SUBDIRECTORY_NAME`).
- File name = `<TEX0 hash>[-<CLUT hash>][-r<W>x<H>]-<packed TEX0 bits>[-mip<N>].png`. The CLUT hash is present only for palettised PSMs (`HasPalette()`), the region size only for sub-rect textures (`TEXTURE_FILENAME_*_FORMAT_STRING`, `CreateTextureName`).
- Loaders: **PNG and DDS** (DDS allows pre-built BC mips) (`GSTextureReplacementLoaders.cpp`). Mip levels can be supplied as `-mipN` files. Otherwise they're generated, but not for compressed formats.
- Replacements load asynchronously or are precached. Replacements "without CLUT hash" force palette textures to be expanded instead of using GPU palette lookup ("paltex").
- Takeaway for Kya: we don't need PCSX2's hashing of GS memory, because we know texture identity at the G2D level. PCSX2 packs are keyed on GS state and wouldn't transfer to the port unchanged. The workflow (dump → upscale → review → drop into `replacements/`) and the "one file per palette variant" rule do transfer.

## 4. Texture upscaling models

Licences are taken from each model's release page or OpenModelDB entry (`OpenModelDB/open-model-database`, `data/models/*.json`, fetched 2026-09-30). "Free fan project OK?" assumes a free, non-commercial, disc-required project that ships **upscaled outputs or the tooling**, not the model.

| Model | Arch / scale | Licence | Free fan project OK? | Notes |
|---|---|---|---|---|
| **4x-PBRify_UpscalerV4** (Kim2091) | DAT2, 4× | **CC0** ([release](https://github.com/Kim2091/Kim2091-Models/releases/tag/4x-PBRify_UpscalerV4)) | Yes, no conditions | "meant to take existing game textures from older 2000s era games". Best licence/purpose fit. Slow (DAT2). Ships `.pth`/`.safetensors`, so it needs ONNX export (spandrel/chaiNNer) to run in the port. |
| 4x-PBRify-UpscalerSPANV4 / -RPLKSRd-V3 / -UpscalerSIR-M-V2 / -UpscalerDAT2-V1 | SPAN / RealPLKSR / SwinIR / DAT, 4× | CC0 (OpenModelDB) | Yes | Faster siblings. SPAN is suited to runtime/ncnn. SIR-M-V2 is tagged dedither. |
| 1x-GameSmooth_SuperUltraCompact (Kim2091) | Compact, 1× | CC0 ([release](https://github.com/Kim2091/Kim2091-Models/releases/tag/1x-GameSmooth_SuperUltraCompact)) | Yes | Pre-pass cleanup. Ships ONNX and ncnn files. |
| **RealESRGAN_x4plus / x2plus / x4plus_anime_6B / animevideo-v3** | ESRGAN / Compact | **BSD-3-Clause** (xinntao/Real-ESRGAN; OpenModelDB) | Yes (keep the notice) | General photo/anime models, tends to over-smooth painted textures. The ncnn build (xinntao/Real-ESRGAN-ncnn-vulkan, MIT) is what the port's ncnn path expects. |
| 4x-UltraSharpV2 (Kim2091) | DAT2, 4× | **CC BY-NC-SA 4.0** ([release](https://github.com/Kim2091/Kim2091-Models/releases/tag/4x-UltraSharpV2)) | Yes, but attribution + non-commercial + share-alike. **Not** if the commercial path (#36) is ever taken. | What the port currently loads. Downloaded from Ko-fi, not GitHub. Good general quality. |
| 4x-UltraSharp (v1) | ESRGAN | CC BY-NC-SA 4.0 | As above | Community default in 2021–23. |
| Nomos family (Nomos8k, NomosUni, Nomos2; Phhofm) | SPAN/ESRGAN/DAT/HAT/ATD… | **CC BY 4.0** (OpenModelDB) | Yes, attribution only | Photo-oriented, useful for realistic material (stone, wood). |
| 4x-NMKD-Siax-CX, 4x-NMKD-Superscale, 1x-DitherDeleterV3-Smooth, 4x-GameAI-2-0, 4x-RealisticRescaler | ESRGAN | **WTFPL** (OpenModelDB) | Yes | DitherDeleter is relevant for dithered PS2 gradients. GameAI targets game screenshots and textures. |
| 4x-HDCube / Normal-RG0 / Wood-BC1 / Skyrim-Misc | ESRGAN | CC0 (OpenModelDB) | Yes | Niche game-texture models. |
| Many others tagged `game-textures` (Morrowind, Fallout, Skyrim-Armory, FatePlus, Faithful…) | ESRGAN | CC BY-NC-SA / CC BY-NC | Non-commercial only | Avoid, so the project stays open to a future commercial question. |
| 4x-FSDedither, 4x-Link | ESRGAN | GPL-3.0-only | Yes, but GPL terms on the weights | Avoid bundling. |

Licence spread across all 671 OpenModelDB entries: 227 CC BY-NC-SA 4.0, 123 CC BY 4.0, 73 WTFPL, 53 CC BY-NC 4.0, 51 Apache-2.0, 44 CC0, 27 GPL-3.0, 22 MIT, 14 CC BY-SA, 9 BSD-3, 27 unlicensed. **Rule for this project: prefer CC0 / BSD / MIT / Apache / WTFPL / CC BY. Accept NC-SA only for local, non-shipped experiments.**

Whether a model licence binds its *outputs* is legally unsettled. Model licences cover the weights. The bigger constraint on outputs is that upscaled textures are **derivatives of Atari/Eden's copyrighted textures**, whatever model is used. That fits #36's "requires the player's own disc" stance best if the port **generates or dumps locally**, or if packs are distributed as fan mods the way PCSX2 packs are (grey area, and not something the licence of any tool changes).

Practical suitability for Kya's art (painted, low-res, often 4/8-bit palettised, fantasy organic surfaces):
- Palette quantisation and dithering are the main artefacts. Run a 1× cleanup pass (DitherDeleter / GameSmooth / PBRify SIR-M dedither) before a 4× pass, or use a model trained on old game textures (PBRify).
- Tiling textures must stay tileable. Upscale with wrap padding (tile the image 3×3, upscale, crop the centre). chaiNNer and most CLI tools don't do this automatically.
- UI, fonts and 2D sprites with hard edges: use a pixel-art or `nearest` path, not GAN models.
- Faithfulness (#36 constraint): 2× or 4× then downscale to 2× often looks more faithful than raw 4× GAN output.

## 5. Tools and pipeline

| Tool | Licence (repo) | Role | OK? |
|---|---|---|---|
| **chaiNNer** (chaiNNer-org/chaiNNer) | GPL-3.0 | Node GUI for batch pipelines: split alpha, tile/wrap, chain 1×→4×, save PNG. Best for building and reviewing the pipeline. | Yes (used as a tool, not shipped) |
| **spandrel** (chaiNNer-org/spandrel) | MIT | Python library that loads almost every SR architecture (.pth/.safetensors). Basis for a scripted batch runner and for ONNX export. | Yes |
| Upscayl (upscayl/upscayl) | AGPL-3.0 | Simple desktop GUI (Real-ESRGAN-ncnn based). Good for quick trials, weak for alpha and tiling control. | Yes (tool) |
| Real-ESRGAN (xinntao/Real-ESRGAN) | BSD-3-Clause | Reference PyTorch inference and training code. | Yes |
| Real-ESRGAN-ncnn-vulkan (xinntao) | MIT (LICENSE file) | Portable CLI, Vulkan on Win/macOS/Linux. | Yes |
| ncnn (Tencent/ncnn) | BSD-3-Clause (plus third-party notices) | Already an optional port dependency. | Yes |
| ONNX Runtime (microsoft/onnxruntime) | MIT | Already an optional port dependency (win-x64 only today). | Yes |
| traiNNer-redux (the-database) / neosr (neosr-project) | Apache-2.0 | Fine-tuning a Kya-specific model later, if generic models aren't faithful enough. | Yes |
| BasicSR (XPixelGroup) | Apache-2.0 | Underlying training toolbox. | Yes |
| waifu2x-ncnn-vulkan (nihui) | MIT | Alternative for flat/cartoon textures. | Yes |

**Suggested batch pipeline with manual review** (offline, not in the game loop):
1. **Dump.** Port "texture dump" mode writes each decoded RGBA8 texture once as `dumps/<key>.png`, plus a JSON sidecar (G2D path, material/layer, palette id, PSM, TW/TH, clamp mode, alpha stats: all-0x80 / binary / smooth).
2. **Classify** from the sidecar: opaque, cut-out alpha, smooth alpha, tiling (clamp mode repeat), UI/font (by G2D path).
3. **Process per class** with a spandrel script (or chaiNNer chain): optional 1× dedither → 4× model → optional downscale to 2×. Alpha goes as a separate greyscale pass (or nearest for cut-out). Tiling uses wrap padding. Clamp alpha to 0–128.
4. **Review.** Generate a contact sheet or HTML side-by-side (original nearest-scaled vs. candidate per model), with accept / reject / hand-fix per texture. Also review in game: drop accepted files into `replacements/` and use the debug texture viewer (it already has Upscale/Revert, so add "reload replacement").
5. **Ship** only the accepted set, with a manifest of model name + licence per file for attribution.

## 6. Audio

**What the formats are** (from the code):
- Music: `edMusicData` "parses Sony PS2 sequence and instrument-bank chunks … and decodes ADPCM samples". `edMusicSynth` "renders stereo PCM at 48 kHz using sample-clock sequencing, envelopes, instrument loops, pitch bend…". That's 90 songs and 13 banks (`port/Audio/MusicPlaybackImplementationPlan.md`, `edMusicControls.h: MusicSampleRate = 48000`). Each `MusicSample` has its own `rate` and loop points (`edMusicData.h`).
- Streams: VAG (`VAGp` header, big-endian sample rate at 0x10) and raw PS-ADPCM, 16-byte blocks for 28 samples, decoded to 16-bit PCM (`edSoundStreamService.cpp`: `DecodeVag`, `DecodePsxAdpcm`). Sound effects are PS-ADPCM samples with a per-sample rate (`edSoundSampleService.cpp`).

**Can it be cleaned or upsampled?**
- Decoding to PCM is lossless relative to the disc. The port already does it, and **vgmstream** (licence: ISC-style permissive notice in `COPYING`) decodes VAG and PS-ADPCM for offline batch export (its `doc/FORMATS.md` lists "Sony VAG header" and "Sony PSX ADPCM a.k.a VAG"). vgmstream doesn't list Sony sequence/bank formats, so music samples must be exported through the port's own `edMusicData` parser (a small extractor tool next to `music_render.cpp`).
- PS-ADPCM at low rates loses high frequencies and adds quantisation noise. Tools:
  - **AudioSR** (haoheliu/versatile_audio_super_resolution): code **MIT**, weights `haoheliu/audiosr_basic` **Apache-2.0** (Hugging Face model card). "any → 48 kHz" band extension for music and speech. Generative, so it can invent content. Needs A/B review and is slow.
  - **Resemble Enhance** (resemble-ai/resemble-enhance): code **MIT**, weights **MIT** (HF card). Speech denoise plus enhancement, for voice lines only.
  - **DeepFilterNet** (Rikorose/DeepFilterNet): **MIT or Apache-2.0**. Real-time speech noise suppression, conservative.
  - **Demucs** (facebookresearch/demucs): **MIT**, but the repo is **archived**. Source separation, only useful if a stream mixes voice and music.
  - Apollo (JusperLee/Apollo): code **CC BY-SA 4.0**, weight licence not stated. Targets MP3-lossy music, not a great fit, and the licence is unclear, so skip it.
- **Recommendation.** For music, upsample and clean **individual instrument samples** (short, reviewable, loop-sensitive: keep the loop region intact and rescale loop points), then let the synth play them. That keeps interactivity and is faithful. For voice/cine streams, use DeepFilterNet or Resemble Enhance, then AudioSR only if review shows a clear win, keeping exact duration. Low confidence on the size of the win: without disc data I couldn't measure the actual sample rates. First checklist item below.

**Higher-quality masters.** Wikipedia's infobox credits composers **Manuel Lauvernier and Thomas Colin** (developer Eden Games, publisher Atari). No official soundtrack release showed up in searches, and VGMdb/MobyGames refused automated access (403), so this is **unverified, not proven absent**. Masters, if they exist, would be with the composers or the rights holder of the Atari-era catalogue. The user could ask the composers directly. A fan project can't count on it.

## 7. Open checks (need disc data / a Windows run)

1. Histogram of texture PSM (4/8/16/32-bit), sizes, and how many bitmaps are used with more than one palette. That gives the replacement count and effort.
2. Actual sample rates of music bank samples, SFX and VAG streams. If they're mostly 22–48 kHz, audio upsampling gains little and cleanup is the real work.
3. Check whether cinematic lip-sync or subtitles read `SOUND_GetStreamInfo` positions, which fixes the replacement-duration rule.
4. Test UltraSharpV2 vs. PBRify V4 vs. RealESRGAN x4plus on ~30 representative textures (terrain, character skin, foliage with alpha, UI) with the review sheet from §5.
5. Region-repeat textures at 4× in game.

## Sources

- Port code: `port/KyaTexture/src/Texture.cpp`, `Texture.h`; `port/Windows/Renderer/Vulkan/src/Texture/TextureCache.{h,cpp}`, `TextureUpscale.{h,cpp}`, `VulkanPS2.cpp`, `VulkanRenderer.cpp`; `port/Windows/Renderer/Shaders/src/ps2/ps2.hlsl`; `port/Windows/Renderer/CMakeLists.txt`; `port/DebugMenu/src/DebugTexture.cpp`; `port/Audio/edSoundStreamService.cpp`, `edSoundSampleService.cpp`, `edMusicData.h`, `edMusicControls.h`, `MusicPlaybackImplementationPlan.md`.
- Sequel override decision: https://github.com/mgiuditta/Kya/issues/28 (resolution comment).
- PCSX2: https://github.com/PCSX2/pcsx2/blob/master/pcsx2/GS/Renderers/HW/GSTextureReplacements.cpp, https://github.com/PCSX2/pcsx2/blob/master/pcsx2/GS/Renderers/HW/GSTextureReplacementLoaders.cpp (GPL-3.0).
- Model licences: https://github.com/Kim2091/Kim2091-Models/releases (per-release "License:" lines), https://github.com/OpenModelDB/open-model-database (`data/models/*.json`), https://github.com/xinntao/Real-ESRGAN (BSD-3-Clause).
- Tool licences (GitHub licence metadata / LICENSE files, 2026-09-30): chaiNNer-org/chaiNNer, chaiNNer-org/spandrel, upscayl/upscayl, xinntao/Real-ESRGAN-ncnn-vulkan, Tencent/ncnn (LICENSE.txt), microsoft/onnxruntime, the-database/traiNNer-redux, neosr-project/neosr, XPixelGroup/BasicSR, nihui/waifu2x-ncnn-vulkan.
- Audio: https://github.com/vgmstream/vgmstream (`COPYING`, `doc/FORMATS.md`), https://github.com/haoheliu/versatile_audio_super_resolution, https://huggingface.co/haoheliu/audiosr_basic, https://github.com/resemble-ai/resemble-enhance, https://huggingface.co/ResembleAI/resemble-enhance, https://github.com/Rikorose/DeepFilterNet (README licence section), https://github.com/facebookresearch/demucs, https://github.com/JusperLee/Apollo.
- Credits: https://en.wikipedia.org/wiki/Kya:_Dark_Lineage (infobox).
