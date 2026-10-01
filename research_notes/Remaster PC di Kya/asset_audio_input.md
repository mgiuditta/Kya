# Asset, Audio, Video, Input and UX Upgrades for a PS2-Era PC Remaster (Kya: Dark Lineage)

Research date: 2026-10-01. Scope: everything except the renderer. Repo-local observations about the Kya port are cited to file paths in this repository; everything else is cited to web URLs. Items with no source are kept in Inferences or Gaps.

## AI texture upscaling workflows and texture replacement pack systems

### Takeaway
The current open toolchain is chaiNNer plus models from OpenModelDB, with game-texture-specific models such as PBRify (DAT2, CC0). The proven replacement design is PCSX2's: dump each texture under a hash of its GS texture data (TEX0) plus its CLUT/palette, and load PNG/DDS (BC1/2/3/7) replacements with the same name. Kya's port decodes assets natively, so it can copy that hashing scheme, or key replacements by asset name, which is simpler and more stable.

### Cited Findings
- OpenModelDB is a community database of AI upscaling models with a search for specific use cases, including game textures. It distributes models as .pth (PyTorch), .onnx and NCNN .bin/.param. Each model has its own license, separate from the site's GPL-3.0 — [OpenModelDB FAQ](https://openmodeldb.info/docs/faq)
- OpenModelDB recommends chaiNNer as the main tool: a free, open-source, node-based GUI with "the most support for models listed on OpenModelDB", with PyTorch (CUDA), ONNX (CUDA) and NCNN backends. Other maintained tools: Upscayl (simple one-button UI), VapourKit (video, Windows), AnimeJaNaiConverterGui (TensorRT/DirectML video) and ComfyUI. chaiNNer is maintained by the same people as OpenModelDB — [OpenModelDB FAQ](https://openmodeldb.info/docs/faq)
- "Most of the models on OpenModelDB have been trained with a specialized dataset ... and therefore work best on specific kinds of content." Pick each model for the kind of content, not as a general model — [OpenModelDB FAQ](https://openmodeldb.info/docs/faq)
- 4x-PBRify_UpscalerV4: DAT2 architecture, 4x, aimed at "older video game textures, particularly from 2000s-era titles". It covers compression removal and restoration, was trained on a custom texture dataset (finished 2025-05-19), and is licensed CC0-1.0, so commercial use is allowed. It is slower than V3 but produces more detail — [OpenModelDB: 4x-PBRify_UpscalerV4](https://openmodeldb.info/models/4x-PBRify-UpscalerV4)
- Tiled inference (tile size 512 by default; 0 turns tiling off) keeps VRAM use bounded. "Best quality is achieved if you upscale the alpha channel separately" — [dgenerate upscaler docs (chaiNNer-compatible model runner)](https://dgenerate.readthedocs.io/en/latest/_modules/dgenerate/imageprocessors/upscaler.html)
- Fan projects that upscale with Real-ESRGAN report that tiled/repeating textures show their repetition more clearly after upscaling — [ModDB: Project Nomads upscaled (Real-ESRGAN 4x)](https://www.moddb.com/addons/project-nomads-upscaled)
- **PCSX2 texture replacement (primary source code, master branch):**
  - It writes to two subfolders, `dumps` and `replacements`, under the per-game texture folder — [GSTextureReplacements.cpp](https://raw.githubusercontent.com/PCSX2/pcsx2/master/pcsx2/GS/Renderers/HW/GSTextureReplacements.cpp)
  - File names come from a 64-bit TEX0 hash, a 64-bit CLUT hash (only for paletted PSMs; otherwise 0 and left out) and a 32-bit packed field (PSM, TW, TH, etc.). Format strings: `%016llx-%08x` (non-paletted), `%llx-%llx-%08x` (paletted), and region variants `-r{W}x{H}-` for partial-texture regions. Mip levels add the suffix `-mip{N}.png` — [GSTextureReplacements.cpp](https://raw.githubusercontent.com/PCSX2/pcsx2/master/pcsx2/GS/Renderers/HW/GSTextureReplacements.cpp)
  - Each palette variant of a CLUT texture is a separate replacement, because the CLUT hash is part of the name. PCSX2 also keeps a lookup of names *without* the CLUT hash so it can "know when we need to disable paltex" (GPU palette conversion) — [GSTextureReplacements.cpp](https://raw.githubusercontent.com/PCSX2/pcsx2/master/pcsx2/GS/Renderers/HW/GSTextureReplacements.cpp)
  - The loaders accept PNG and DDS. DDS can be BC1 (DXT1), BC2 (DXT2/3), BC3 (DXT4/5), BC7 (DX10 header, DXGI 98) or uncompressed A8R8G8B8/X8R8G8B8/etc. The loader computes per-texture alpha min/max (`GetBCAlphaMinMax`) so alpha-test/blend decisions still work with replaced textures. Formats without alpha are loaded with alpha "set to full intensity" — [GSTextureReplacementLoaders.cpp](https://raw.githubusercontent.com/PCSX2/pcsx2/master/pcsx2/GS/Renderers/HW/GSTextureReplacementLoaders.cpp); [GSTextureReplacements.cpp](https://raw.githubusercontent.com/PCSX2/pcsx2/master/pcsx2/GS/Renderers/HW/GSTextureReplacements.cpp)
  - Replacements can be loaded asynchronously (`LoadTextureReplacementsAsync`) and kept in a cache — [GSTextureReplacements.cpp](https://raw.githubusercontent.com/PCSX2/pcsx2/master/pcsx2/GS/Renderers/HW/GSTextureReplacements.cpp)
  - User setup: turn on "Load Textures" and "Async Texture Loading" in Graphics settings. Textures go in per-title-ID folders under `PCSX2/textures/` — [PCGamesN on PCSX2 custom textures](https://www.pcgamesn.com/emulation/pcsx2-custom-textures); [ModDB PCSX2-Qt texture replacement tutorial](https://www.moddb.com/tutorials/pcsx2-qt-texture-replacement-tutorial)
- DuckStation (PS1) takes a similar approach. It hashes VRAM writes with XXH3-128 and dumps them as `vram-write-{hash}.png` — [DuckStation texture_replacements.cpp mirror](https://repo.retrodeck.net/Xargon/Duckstation/src/branch/main/src/core/texture_replacements.cpp)
- The Dolphin custom-texture guide (dolphin-emu.org/docs/guides/custom-textures/) returned HTTP 403 to my fetch. See Gaps.

### Inferences
- Kya's textures are in-engine assets that the port decodes itself (port/KyaTexture submodule, Vulkan texture cache), not GS memory snapshots. A native replacement system can therefore key on a stable asset identifier, such as the source file/archive path plus the texture index, or a hash of the decoded indexed pixels plus a hash of the CLUT. Following PCSX2, keep the palette hash in the key so that palette-swapped variants (e.g. enemy recolors) can be replaced one by one.
- PS2 alpha usually runs 0–128 (0x80 = opaque). When dumping, rescale to 0–255 in the PNG and convert back on load. Otherwise artists and AI models see washed-out alpha. (This is standard PS2 knowledge, not confirmed in the sources fetched here.)
- Suggested CLUT/palettized pipeline: expand to RGBA, upscale RGB with a texture model (PBRify, or an artist-curated model from OpenModelDB), and upscale alpha separately (e.g. single-channel or nearest/bicubic for cutouts, then threshold). Ship RGBA (BC7), or optionally re-quantize to a palette when runtime palette animation is needed.
- Seams: before inference, pad tileable textures by wrapping (tile 3x3, or mirror-pad), upscale, then crop the center. This avoids edge artifacts at UV wrap boundaries. Treat it as a standard practice to confirm; I found no primary source for it.
- Normal/PBR maps: the PBRify project is known for normal/roughness generation models, but I could not confirm that from the fetched page. A Kya remaster probably does not need full PBR anyway, because the original look is unlit, vertex-colored and stylized.
- QA gate (lesson from GTA DE below): every texture containing text, signage or logos needs human review or a redraw. Never ship an AI-only pass on text.

### Gaps
- I could not fetch Dolphin's official custom texture/HD pack docs (403). Its naming scheme `tex1_WxH_hash[_tlut]_fmt`, wildcards and arbitrary-mipmap detection come from memory and are not cited here.
- I found no primary source on PBRify's normal/height model variants, or on chaiNNer's built-in seamless/tile-wrap nodes.
- No Real-ESRGAN versus DAT/SPAN quality benchmarks specific to PS2 textures.

## Model upgrades and professional remaster case studies / lessons

### Takeaway
Remasters that worked kept the original art direction and gameplay metrics exactly (Spyro Reignited measured jump distances; BG&E 20th Anniversary updated models and textures but kept the style). The failures came from AI-only asset passes without human QA (GTA Trilogy DE) and from minimal-effort ports (MGS Master Collection at 720p, no AA).

### Cited Findings
- **Spyro Reignited Trilogy** (Toys for Bob) was rebuilt from scratch but "faithful to the structure and design". The studio built a tool, SpyroScope, to extract data from the original games. It measured jump distances, step lengths and point-to-point map distances so that muscle memory carries over. It reused original level models, collision, animated objects, character models, music, text and sounds as reference or base — [PlayStation Blog / search summary](https://blog.playstation.com/?p=199742); [Pocket-lint review](https://www.pocket-lint.com/nl-nl/games/reviews/activision/144874-spyro-reignited-trilogy-eerste-recensie-de-meest-liefdevol-gemaakte-remaster)
- **Jak and Daxter on PS4** used Sony's PS2 emulation layer: up-rendered to 1080p, trophies added. Sony itself warns that it "may play differently" from the original — [PlayStation Blog](https://blog.playstation.com/2017/12/01/jak-and-daxter-ps2-classics-available-for-download-on-ps4-december-6/comment-page-2); [TheSixthAxis](https://www.thesixthaxis.com/2017/04/03/the-jak-and-daxter-games-are-getting-the-ps2-classics-treatment-this-year/)
- **Beyond Good & Evil 20th Anniversary Edition** (2024): higher-resolution textures, updated 3D models for many objects and most main characters, and added ambient occlusion. It "retains the original's distinct art style" — [TechRadar review](https://www.techradar.com/gaming/beyond-good-and-evil-20th-anniversary-edition-review); [Business Today review](https://businesstoday.in/technology/news/story/beyond-good-evil-20th-anniversary-edition-review-a-timeless-classic-returns-435508-2024-07-03); [DSOGaming comparison](https://www.dsogaming.com/videotrailer-news/beyond-good-evil-original-vs-remaster-graphics-comparison)
- **GTA: The Trilogy – Definitive Edition** (Nov 2021, Grove Street Games): launched with game-breaking bugs, typos, odd character models and poor performance — [PC Gamer](https://www.pcgamer.com/uk/gta-trilogy-definitive-edition-is-a-mess/). Development focused on upscaling graphics with AI, and that left errors in signage and textures, e.g. "enchilaoas" instead of "enchiladas" — [Inverse](https://www.inverse.com/gaming/gta-trilogy-definitive-edition-patch-fixed-finally-surprise); [Multiplayer.it on AI texture errors](https://multiplayer.it/notizie/gta-the-trilogy-definitive-edition-errori-texture-allargate-dallia.html). CEO Thomas Williamson later said he agreed with much of the criticism — [Gfinity](https://www.gfinityesports.com/article/the-gta-trilogy-was-a-trainwreck-but-was-the-hate-warranted)
- **Metal Gear Solid Master Collection Vol. 1** (2023): MGS2/3 run at native 1280x720 on every platform, PC included, despite pre-release hints of 1080p. The Switch and PC versions of MGS2 lack anti-aliasing. Digital Foundry: "little ambition to update these games for modern hardware", "could, and should, have been so much more" — [Pure Xbox on DF](https://purexbox.com/news/2023/11/metal-gear-solid-master-collection-vol-1-is-anything-but-masterful-says-digital-foundry); [Nintendo Life DF video](https://nintendolife.com/news/2023/11/video-digital-foundrys-technical-analysis-of-metal-gear-solid-master-collection-vol-1). PC modders added 1080p/4K soon after launch — [VGC](https://videogameschronicle.com/news/metal-gear-solid-master-collection-on-pc-has-already-been-modded-for-1080p-and-4k-support)

### Inferences
- For Kya the "faithful plus" tier (BG&E 20th, Jak-style) is realistic. The port already runs original game logic, so collision and movement metrics stay exact by construction. Model upgrades should be optional overlays, such as higher-poly character meshes with the same skeleton and UV layout, so they can be switched back to the originals.
- Keep a toggle between original and upgraded assets. That protects the work from "style drift" criticism and makes A/B QA easy.
- LOD: the original PS2 meshes can serve as the low LOD and upgraded meshes as LOD0. Retopology must keep the skeleton and skin weights compatible with the original animation data.

### Gaps
- I did not find specific postmortems or GDC talks on Kingdom Hearts HD (texture redraws), the Sly Collection, Psychonauts or the Prince of Persia HD trilogy within the tool budget.
- No primary source on retopology/LOD workflows specific to remasters.

## FMV/cutscene upscaling and audio (PS2 ADPCM/VAG, resampling, remix, surround)

### Takeaway
PS2 FMVs are PSS containers: MPEG-2 video interleaved with PS2 ADPCM or PCM audio. FFmpeg only partly handles them, so demux first (the Kya port already ships a PSS decoder in `port/ext/pss`), then deinterlace if needed and upscale offline. Topaz went subscription-only in October 2025; Video2X and SeedVR2 are the open alternatives. For audio, vgmstream is the reference PS2 ADPCM decoder. The Kya port already decodes raw ADPCM and plays it through XAudio2.

### Cited Findings
- PSS = Sony PS2 game video: MPEG-2 video plus linear PCM or proprietary ADPCM audio, in a custom container (not plain MPEG) — [docs.fileformat.com PSS](https://docs.fileformat.com/game/pss)
- FFmpeg ticket #4478: PS2 PSS is "not fully supported"; video is barely extractable and audio is not detected (PS2 ADPCM in PSS) — [FFmpeg trac #4478](https://trac.ffmpeg.org/ticket/4478)
- Topaz Labs ended perpetual licenses in October 2025. Pricing is now Personal $299/yr or $59/mo, and Pro $699/yr — [Motionbox, Topaz alternatives 2026](https://motionbox.io/blog/topaz-video-ai-alternatives-2026)
- Open-source options: Video2X (free, needs model configuration, strong for anime/2D) and SeedVR2 (diffusion-transformer). The market splits into CNN-based tools (Topaz Proteus/Iris), diffusion-transformer tools (SeedVR2) and browser one-click tools — [Motionbox](https://motionbox.io/blog/topaz-video-ai-alternatives-2026); [UniFab Video2X vs Topaz](https://unifab.ai/resource/video2x-vs-topaz)
- OpenModelDB lists VapourKit (real-time preview/comparison) and AnimeJaNaiConverterGui (TensorRT/DirectML) as maintained video upscaling front-ends for OpenModelDB models — [OpenModelDB FAQ](https://openmodeldb.info/docs/faq)
- vgmstream decodes PS2/PSX ADPCM (VAG and many container extensions such as .ads/.ss2), respects original loop points, and ships a CLI (`-o out.wav`) and player plugins (foobar2000 foo_input_vgmstream) — [fileinfo .VAG](https://fileinfo.com/extension/vag); [foobar2000 vgmstream component](https://www.foobar2000.org/components/view/foo_input_vgmstream/release/r1879); [libvgmstream on Aminet](https://www.aminet.net/package/dev/lib/libvgmstream)
- **Repo state (Kya port):** `port/Audio/edMusicData.cpp` validates ADPCM ranges in 16-byte blocks and calls `DecodeRawAdpcm`. `port/Audio/edSoundDevice.h` and `edSoundSampleService.h` use an XAudio2 engine/mastering voice. `port/ext/pss/` contains a PSS demuxer/decoder (`pss.c`, `decode.c`) used for cinematics (`src/b-witch/CinematicManager.cpp`) — local files in this repository.

### Inferences
- FMV pipeline for Kya: (1) demux PSS with the existing `port/ext/pss` code, or a custom tool, into an MPEG-2 elementary stream plus ADPCM, and decode the audio with the port's ADPCM decoder or vgmstream. (2) Check field order. Many PS2 FMVs are progressive at 512x448 or 640x448 with non-square pixels, so correct the aspect ratio to 4:3 before upscaling, and deinterlace with QTGMC (VapourSynth/AviSynth) only if the content is interlaced. (3) Upscale 2–4x with a model chosen for compressed CG (an OpenModelDB DAT/SPAN video model, or SeedVR2/Topaz). (4) Re-encode to AV1/H.264/HEVC and play through a modern decoder, e.g. the Media Foundation or FFmpeg/libav path, keeping the original PSS as a fallback. Note: the 512x448 resolution figure is from memory, not a fetched source.
- Since Topaz is now subscription-only, a commercial remaster pays per seat per year. For a fan or open project, chaiNNer/VapourSynth with OpenModelDB models is the license-clean route, as long as each model's own license is checked.
- Audio quality: PS2 SPU2 voices run at 48 kHz output but samples are often stored at lower rates (11–32 kHz). Better resampling, such as windowed-sinc instead of the SPU's Gaussian interpolation, gives cleaner highs. Some listeners consider the original interpolation part of the sound, so offer an "original" option. (Hardware details are from domain knowledge, not fetched.)
- Surround: XAudio2 supports multichannel mastering voices and X3DAudio. Positional SFX could be upmixed to 5.1/7.1 and music left stereo. Remixing music needs the original stems or MIDI sources. Kya's music is sequenced (`edMusicSynth.cpp` suggests a synth/sequencer path), so higher-quality instrument samples could be swapped in at the sequencer level. That is a rare advantage over streamed-audio games.

### Gaps
- No primary-source documentation of QTGMC settings for PS2 FMVs, and no measured comparison of Video2X/SeedVR2/Topaz on PS2-era CG.
- No source on PS2 SPU2 interpolation versus high-quality resampling, or on remaster postmortems about audio remixing.

## Input: SDL3 gamepad, Steam Input, rebinding, mouse/keyboard camera, prompts, haptics

### Takeaway
Use the SDL3 Gamepad API as the native layer: hotplug, button labels and types for per-controller prompts, rumble and trigger rumble, gyro and touchpad. Make it cooperate with Steam Input (Steam can suppress SDL input when it handles the device). Hotplug support is required for Steam Deck and Xbox certification.

### Cited Findings
- SDL3 gamepad API surface: `SDL_GetGamepadButtonLabel` / `SDL_GetGamepadButtonLabelForButton` (face-button labels for prompts); `SDL_GetGamepadType`, `SDL_GetGamepadTypeForID`, `SDL_GetRealGamepadType`; `SDL_RumbleGamepad`, `SDL_RumbleGamepadTriggers`, `SDL_SendGamepadEffect`; `SDL_GamepadHasSensor` / `SDL_GetGamepadSensorData` (gyro/accel); `SDL_SetGamepadLED`; `SDL_GetNumGamepadTouchpads` / `SDL_GetGamepadTouchpadFinger`; `SDL_AddGamepadMapping`, `SDL_AddGamepadMappingsFromFile`, `SDL_ReloadGamepadMappings` — [SDL3 CategoryGamepad](https://wiki.libsdl.org/SDL3/CategoryGamepad)
- SDL docs: "your application should always support gamepad hotplugging", and they note this is required for Xbox and Steam Deck certification — [SDL3 CategoryGamepad](https://wiki.libsdl.org/SDL3/CategoryGamepad)
- Steam Input guidance: define action sets (at minimum "Menu" and "In-game", plus layers for variants). Use `GetGlyphForActionOrigin` for official per-controller glyphs, and poll `GetDigitalActionOrigins` / `GetAnalogActionOrigins` every frame because they are cheap and users can rebind mid-session. `ShowBindingPanel` opens the overlay rebinding UI. Games with native gamepad support can instead opt controller types into Steam Input from the partner site. Use SDL ≥ 2.0.8 so Steam can suppress SDL input when Steam Input is active and avoid double input. Allow gamepad and mouse input at the same time — [Steamworks: Steam Input getting started](https://partner.steamgames.com/doc/features/steam_controller/getting_started_for_devs)
- Steam Deck Verified input requirements: support Deck physical controls; the default controller config must reach all content; "on-screen glyphs must match the inputs being used" (no KB/M glyphs while using a controller); text entry must use the Steamworks on-screen keyboard API or a built-in controller-driven entry method — [Steamworks: Steam Deck compatibility review](https://partner.steamgames.com/doc/steamdeck/compat)

### Inferences
- Kya maps original PS2 pad reads (libpad/edDev in the decompiled code) to a single virtual DualShock 2 state. That mapping is the right place to merge SDL3 gamepad and keyboard/mouse into one virtual pad, and to apply rebinding at the action level rather than the raw-button level.
- Glyphs: use `SDL_GetGamepadType` / `SDL_GetGamepadButtonLabel` to pick PS/Xbox/Nintendo icon sets, and swap prompts to keyboard glyphs on the last-used device. Hot-swapping on last input is effectively required by the "glyphs must match" Deck rule.
- Mouse camera: the original pad camera expects a rate (stick deflection). Mouse needs a delta-to-angle path that bypasses stick dead-zone and acceleration curves, with separate sensitivity and invert options. Gyro aiming via `SDL_GetGamepadSensorData` is a cheap extra.
- Haptics: map PS2 DualShock motor values to `SDL_RumbleGamepad` (low/high frequency) one-to-one. Trigger rumble and DualSense effects (`SDL_SendGamepadEffect`) are optional enhancements.
- Ship an in-game rebinding menu even with Steam Input available, since non-Steam builds (GOG, itch, fan builds) need one.

### Gaps
- No source fetched for SDL3 `SDL_GamepadButtonLabel` enum values or for SDL3 Steam Input coexistence beyond the SDL2-era Steamworks note.

## UI/HUD: resolution independence, fonts, accessibility, localization, settings menus, Steam Deck

### Takeaway
The hard numbers to design against: Steam Deck smallest glyph ≥ 9 px tall at 1280x800 (12 px recommended), 30 fps default at 800p, 16:10 support, full controller navigability. For subtitles, follow XAG 104: speaker ID, ≤ 40 characters per line, ≤ 2 lines, resizable to 200 %, adjustable background opacity 0–100 %, on by default or settable before the first sound.

### Cited Findings
- Steam Deck Verified: resolution 1280x800 (preferred) or 1280x720. The smallest on-screen character "should never fall below 9 pixels in height at 1280x800", with 12 px recommended. Ship a default config that reaches 30 fps at 800p. Launchers must be fully controller-navigable and are discouraged. No unsupported-hardware warnings — [Steamworks: Steam Deck compatibility](https://partner.steamgames.com/doc/steamdeck/compat)
- Steam Input docs recommend UI readable from a distance (minimum 24 px fonts at 1920x1080), fullscreen by default in Big Picture, and detecting the screen resolution at first launch — [Steamworks Steam Input](https://partner.steamgames.com/doc/features/steam_controller/getting_started_for_devs)
- XAG 104 (subtitles/captions): subtitles for all speech, FMVs included. Identify the speaker when the speaker changes; color may be used only together with text. Subtitles on by default, or settable before the intro plays. Captions for important non-speech sounds, with directional arrows. Separate toggles for dialogue, barks and ambient sounds. Text meets the XAG 101 minimum size and scales to at least 200 %. At most ~40 characters per line and 2 lines on screen (3 in exceptional cases). Manual line breaks, mixed case, at least one sans-serif option, a solid background with configurable color and opacity 0–100 %, a live preview, and full FMV transcripts on a website — [Microsoft Learn XAG 104](https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/104)
- Game Accessibility Guidelines (gameaccessibilityguidelines.com) group features by tier. Basic includes subtitles with a solid background; intermediate includes a colorblind mode — [abratabia summary of GAG](https://www.abratabia.com/game-accessibility/accessibility-guidelines.php); [GAG: subtitle presentation](http://gameaccessibilityguidelines.com/if-any-subtitles-captions-are-used-present-them-in-a-clear-easy-to-read-way/)
- Xbox requires subtitles to be resizable up to 200 % for the "Subtitle options" feature tag — [Xbox Wire accessibility updates](https://news.xbox.com/en-us/2021/10/01/xbox-announces-accessibility-updates/)

### Inferences
- Kya's HUD and menus were authored for a 640x448/512-ish 4:3 frame. Render the 2D layer in a virtual 4:3 canvas with anchors (left/right/top/bottom), so HUD elements can move to the edges in 16:9/16:10/ultrawide while menus stay centered and pillarboxed. Check the result against the 9 px at 800p minimum.
- Bitmap fonts from the PS2 will fail the legibility rules once scaled. Redraw them as vector/SDF fonts (MSDF atlas) that keep the original letterforms, and add a sans-serif fallback for subtitles to meet XAG.
- Localization: Kya shipped in several European languages. Extract text tables to UTF-8 external files (also a modding hook), and plan for glyph coverage for any new languages.
- The settings menu should be controller-navigable and in-game, not a launcher (Deck rule). Include resolution, scaling, frame cap, original/upgraded asset toggles, subtitles (size/background/speaker), audio (master/music/SFX/voice, resampler mode, stereo/surround) and input rebinding.
- Difficulty/assist options (e.g. damage reduction, checkpoints) fall under GAG intermediate. Colorblind options matter mainly if gameplay relies on color-coded collectibles or enemies.

### Gaps
- I did not fetch the XAG 101 text-size numbers (minimum px at 1080p), Steam Deck "Playable" versus "Verified" nuances, or any localization postmortem for a remaster.

## Modding support best practices (loose-file overrides, mod loaders)

### Takeaway
The proven pattern is a virtual file system that checks a mod/override folder before the packed game archives, with folder structure that mirrors the game's. Reloaded-II and CriFs/Unreal hooks do exactly this from outside the game. A native port can build it in at the file-open layer.

### Cited Findings
- Reloaded-II is a mod loader that makes it easy for mods to replace game files. Loose files are placed in a mod folder that mirrors the original game's folder structure. It has engine-specific plugins, e.g. CRI file system redirection (CriFsHook) and Unreal UTOC/PAK loading that also removes signature checks — [CriFsHook.ReloadedII](https://github.com/Sewer56/CriFsHook.ReloadedII); [UnrealEssentials.Interfaces](https://www.nuget.org/packages/UnrealEssentials.Interfaces/1.2.0)
- A VFS gives platform-independent file access, lower file access costs, modding support and hotloading for fast iteration — [Wildfire Games (0 A.D.) VFS wiki](https://trac.wildfiregames.com/wiki/Virtual_File_System)
- Redirectors intercept file-system calls and redirect a request to the mod directory when a mod file exists, so mods can be swapped without moving game files — [Saints Row mod file redirector thread](https://www.saintsrowmods.com/forum/threads/saints-row-the-third-mod-file-redirector.5325/post-47768)
- Emulator texture packs use the same idea at texture level: per-game folders with `replacements` keyed by hash, loaded asynchronously, optionally cached — [PCSX2 GSTextureReplacements.cpp](https://raw.githubusercontent.com/PCSX2/pcsx2/master/pcsx2/GS/Renderers/HW/GSTextureReplacements.cpp)

### Inferences
- In Kya, add an override lookup in the port's archive/file layer (`port/Archive`, the edFile shim): `mods/<modname>/<original path>` is checked first, with load order from a simple manifest (toml/json). Add asset-level overrides as well, for textures by stable ID, sounds and text tables. The upscaled texture pack, HD FMVs and remastered audio then become "first-party mods" on the same system, which keeps the original assets as fallback and lets the community extend them.
- Ship a dump mode (textures to PNG with stable names, audio to WAV, text to UTF-8) next to the loader. PCSX2's dump/replacement pair is what made its texture-pack community possible.
- Hot reload in the debug menu (ImGui, `port/DebugMenu`) is cheap once a VFS exists, and speeds up artist iteration.

### Gaps
- No published GDC talk or postmortem on mod support specifically in remasters was found within the tool budget.
