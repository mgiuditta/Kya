# Which audio backend replaces XAudio2 on macOS

Research for mgiuditta/Kya#15 (map #8, "Kya on macOS"). Checked 2026-09-29 against the repo at `origin/docs/research` (`028d4d4`) and these primary sources:

- **miniaudio v0.11.25** (2026-03-04): [`miniaudio.h`](https://github.com/mackron/miniaudio/blob/0.11.25/miniaudio.h), [LICENSE](https://github.com/mackron/miniaudio/blob/master/LICENSE), [manual](https://miniaud.io/docs/manual/index.html), [releases](https://github.com/mackron/miniaudio/releases)
- **SDL 3.4.16** (2026-09-02): [CategoryAudio](https://wiki.libsdl.org/SDL3/CategoryAudio), [SDL_SetAudioStreamFrequencyRatio](https://wiki.libsdl.org/SDL3/SDL_SetAudioStreamFrequencyRatio), [LICENSE.txt](https://github.com/libsdl-org/SDL/blob/main/LICENSE.txt), [releases](https://github.com/libsdl-org/SDL/releases)
- **OpenAL Soft**: [COPYING](https://github.com/kcat/openal-soft/blob/master/COPYING). **Apple OpenAL**: `OpenAL.framework/Headers/al.h` in the current Xcode macOS SDK
- **Apple AVFAudio / AudioToolbox**: [AVAudioEngine](https://developer.apple.com/documentation/avfaudio/avaudioengine), [AVAudioUnitVarispeed](https://developer.apple.com/documentation/avfaudio/avaudiounitvarispeed), [AVAudioPlayerNode.scheduleBuffer](https://developer.apple.com/documentation/avfaudio/avaudioplayernode/schedulebuffer(_:at:options:completionhandler:)), [AVAudioPlayerNodeBufferOptions.loops](https://developer.apple.com/documentation/avfaudio/avaudioplayernodebufferoptions/loops), [AudioQueueNewOutput](https://developer.apple.com/documentation/audiotoolbox/audioqueuenewoutput(_:_:_:_:_:_:_:))
- **GPL compatibility**: [FSF license list](https://www.gnu.org/licenses/license-list.html) (#Unlicense, #ZLib, #LGPLv2.1)

## Answer

**Use miniaudio (the high-level `ma_engine`/`ma_sound` API) on macOS. Keep XAudio2 on Windows.** This confirms the tentative default from #9/#14.

- Every XAudio2 feature Kya uses has a direct miniaudio equivalent: per-voice PCM buffers with a loop region, frequency ratio, volume, left/right gain, a played-frames cursor, end-of-buffer detection, and a queue for streaming music. SDL3 lacks loop regions, panning and a cursor. AVAudioEngine needs Objective-C++ and a varispeed node per voice. Apple's OpenAL is deprecated, and OpenAL Soft is LGPL and a heavier dependency.
- License: miniaudio is Unlicense or MIT-0, so it can be vendored as a single header with no GPL-3.0 friction.
- Kya already decodes PS2 ADPCM (VAG/MIB/SPU samples) and synthesizes music itself, so the backend only has to play s16/f32 PCM. Decoding stays shared and platform-neutral.
- Effort is about 300 lines of new `port/` code plus a small, behavior-neutral seam in `edSoundStreamService.cpp`. Windows keeps its XAudio2 code and CMake link line unchanged, and `src/` needs no new ifdefs.

## The audio surface today

Everything backend-specific lives in `port/Audio/`. `src/` calls the `Audio::` services under `PLATFORM_WIN`, which #17 defines as "PC port", so macOS builds define it too and `src/` needs no change. Callers: `src/EdenLib/edSound/sources/edSoundPlay.cpp:313-340`, `src/EdenLib/edSound/sources/ps2/multistream/ee/sound.cpp:26-27, 164-166, 216-218`, `src/EdenLib/edSound/sources/edSoundInstance.cpp:378`, `src/EdenLib/edMusic/...`, `src/b-witch/CinematicManager.cpp`.

What each service needs from a device:

| Service | Data it plays | Controls | Threading |
|---|---|---|---|
| `edSoundStreamService` (VAG/MIB streams, `Audio::StartStream` ...) | Whole file decoded up front to interleaved s16 PCM, 1-2 channels, at the file's rate (`edSoundStreamService.cpp:404-425`) | start/stop/flush, volume 0..1, cursor in played frames converted to VAG bytes (`:245-263`), finished = no buffers queued | game thread |
| `edSoundSampleService` (SFX voices) | Mono s16 PCM decoded from SPU ADPCM, optional loop region `[loopBegin, loopBegin+loopLength)` in frames (`edSoundSampleService.cpp:133-161`) | independent left/right gain in [-1, 1], pitch ratio up to 4.0, play/pause, finished, frames played (`:56-78`) | game thread only; "no Eden objects cross into XAudio2 callbacks" (`edSoundSampleService.h:43-44`) |
| `edMusicService` (sequenced music) | Stereo f32 at 48 kHz (`edMusicControls.h:7`), pushed in 10 ms blocks, at most 3 queued (`edMusicService.cpp:109-126`) | play/pause, queued-buffer count | a `std::jthread` worker services it every 2 ms (`:233-239`) |

ADPCM decoding (`DecodePsxAdpcm`, `edSoundStreamService.cpp:140-180`; `DecodeVag` `:292`; `DecodeMib` `:316`; `DecodeRawAdpcm` `:282`) and the music synth (`edMusicSynth.cpp:127`, which also does per-note pitch/resampling at `:166`) are plain C++. No backend has to understand PS2 formats.

Two services already have a backend seam, with an abstract class plus a factory used by tests: `SampleVoice` / `SetSampleVoiceFactory` (`edSoundSampleService.h:45-54, 65`) and `MusicOutput` / `SetMusicOutputFactory` (`edMusicService.h:11-19, 30`). `edSoundStreamService` doesn't have one; XAudio2 calls are inlined in 13 `#ifdef _WIN32` blocks.

## XAudio2 API surface actually used

| API | Where | Purpose |
|---|---|---|
| `CoInitializeEx` / `CoUninitialize` | `edSoundStreamService.cpp:69, 277` | COM for XAudio2 |
| `XAudio2Create(&engine, 0, XAUDIO2_DEFAULT_PROCESSOR)` | `edSoundStreamService.cpp:77` | device/engine |
| `IXAudio2::CreateMasteringVoice` | `edSoundStreamService.cpp:84` | default output |
| `IXAudio2MasteringVoice::GetVoiceDetails` (`InputChannels`) | `edSoundSampleService.cpp:91-92` | size the pan matrix |
| `IXAudio2MasteringVoice::DestroyVoice`, `IXAudio2::Release` | `edSoundStreamService.cpp:272-273` | shutdown (`ShutdownAudioDevice`, called from `edSysTransferService.cpp:119-121`) |
| `CreateSourceVoice(&v, &fmt)` PCM s16, 1-2 ch, file rate | `edSoundStreamService.cpp:114-123` | stream voice |
| `CreateSourceVoice(&v, &fmt, 0, 4.0f)` PCM s16 mono | `edSoundSampleService.cpp:93-100` | SFX voice, max freq ratio 4 |
| `CreateSourceVoice(&v, &fmt)` `WAVE_FORMAT_IEEE_FLOAT`, stereo, 48 kHz | `edMusicService.cpp:32-37` | music voice |
| `SubmitSourceBuffer` (whole buffer) | `edSoundStreamService.cpp:461-464` | stream |
| `SubmitSourceBuffer` with `XAUDIO2_END_OF_STREAM`, `LoopBegin`, `LoopLength`, `LoopCount = XAUDIO2_LOOP_INFINITE` | `edSoundSampleService.cpp:101-108` | looping SFX |
| `SubmitSourceBuffer` of 10 ms blocks (caller keeps them alive in a deque) | `edMusicService.cpp:46-53` | music queue |
| `Start` / `Stop` | `edSoundStreamService.cpp:471, 437, 517`; `edSoundSampleService.cpp:66`; `edMusicService.cpp:55` | transport |
| `FlushSourceBuffers` | `edSoundStreamService.cpp:102, 438` | rewind stream |
| `SetVolume` | `edSoundStreamService.cpp:130, 496` | stream volume |
| `SetOutputMatrix(master, 1, channels, matrix)` | `edSoundSampleService.cpp:58-63` | left/right gain |
| `SetFrequencyRatio` (clamped to `XAUDIO2_MIN_FREQ_RATIO`..4) | `edSoundSampleService.cpp:64` | pitch |
| `GetState` → `SamplesPlayed`, `BuffersQueued` (with `XAUDIO2_VOICE_NOSAMPLESPLAYED` where only the count matters) | `edSoundStreamService.cpp:249-253, 458-460`; `edSoundSampleService.cpp:69-77`; `edMusicService.cpp:41-44` | cursor, finished, queue depth |
| `DestroyVoice` | `edSoundStreamService.cpp:103`; `edSoundSampleService.cpp:54`; `edMusicService.cpp:27` | voice teardown |

No voice callbacks (`IXAudio2VoiceCallback`), submix voices, effects/XAPO, or X3DAudio are used. Everything is polled from the game thread or the music worker. Build: `port/Audio/CMakeLists.txt:22-24` links `XAudio2 ole32` under `if(WIN32)`. The device-level tests are `DISABLED_` and XAudio2-specific (`port/Test/src/audio_sample_tests.cpp:200-208`, `audio_music_tests.cpp:311-313`). The deterministic tests use the factories and don't depend on a backend.

## Candidate comparison

| | miniaudio 0.11.25 | CoreAudio / AVAudioEngine | SDL3 audio (3.4) | OpenAL (Apple) / OpenAL Soft |
|---|---|---|---|---|
| **License vs GPL-3.0** | Unlicense **or** MIT-0 (LICENSE "Alternative 1/2"); FSF lists the Unlicense as GPL-compatible | System framework, no redistribution | zlib, GPL-compatible (FSF #ZLib) | Apple's: system. OpenAL Soft: **LGPL v2** (COPYING); compatible, but it's a separate library to ship/link |
| **Dependency cost** | One header (~96k lines), vendored under `port/ext/`; one TU with `MINIAUDIO_IMPLEMENTATION` | None, but needs `.mm` Objective-C++ files (AVFAudio is an ObjC/Swift API) | A new platform library. Kya uses GLFW (`port/CMakeLists.txt:6-8`), not SDL, so this would be a second one | Apple: none, but **deprecated**. `al.h`: `API_DEPRECATED("OpenAL is deprecated in favor of AVAudioEngine", macos(10.4, 10.15))`. Soft: CMake subproject |
| **Voice per sound** | `ma_sound_init_from_data_source` over an `ma_audio_buffer_ref` pointing at Kya's PCM (no copy) | `AVAudioPlayerNode` + `scheduleBuffer`, one node per voice attached to the mixer | `SDL_AudioStream` bound to a device; "it will pull it from all bound streams and mix them" | source + buffer |
| **Loop region `[begin, end)`** | `ma_data_source_set_loop_point_in_pcm_frames` + `ma_sound_set_looping` (header L5890, L11462) | Only whole-buffer loop (`.loops`, "the buffer loops indefinitely"), so you need an intro buffer plus a separate loop buffer | None; the app re-queues data itself | Whole-buffer loop only (`AL_LOOPING`); Soft adds `AL_SOFT_loop_points` |
| **Pitch / freq ratio** | `ma_sound_set_pitch` (> 0; header L11414); built-in resampler | `AVAudioUnitVarispeed` node per voice ("allows control of the playback rate") | `SDL_SetAudioStreamFrequencyRatio`, 0.01..100 (since 3.2.0) | `AL_PITCH` |
| **Volume / L-R gain** | `ma_sound_set_volume` + `ma_sound_set_pan` (balance mode is default, header L5078; mono is copied to both channels, L2812) | mixer `volume` / `pan` per node | `SDL_SetAudioStreamGain`; **no pan**, so you'd pre-mix stereo yourself | `AL_GAIN`; pan only through 3D position |
| **Played-frames cursor** | `ma_sound_get_cursor_in_pcm_frames` (L11468) | `playerTime(forNodeTime:)` | None (only `SDL_GetAudioStreamQueued`) | `AL_SAMPLE_OFFSET` |
| **Finished** | `ma_sound_at_end` (L11464) or end callback (audio thread) | completion handler (audio thread) | queued bytes == 0 | `AL_SOURCE_STATE` |
| **Push queue for music** | `ma_pcm_rb`, lock-free SPSC ring buffer ("14. Ring Buffers"), usable as a data source; `ma_pcm_rb_available_read` gives queue depth | `scheduleBuffer` chain | `SDL_PutAudioStreamData` / `GetAudioStreamQueued` (a natural fit) | buffer queueing |
| **Formats needed (s16, f32)** | both (`ma_format_s16`, `ma_format_f32`) | both, via `AVAudioFormat` | both | s16 core; f32 via extension |
| **macOS specifics** | Core Audio backend. For notarization, build with `MA_NO_RUNTIME_LINKING` and link `CoreFoundation`, `CoreAudio`, `AudioToolbox` (manual §2.2) | native | native | n/a |
| **Fit with current service shape** | **Direct 1:1** for all three services | Good, but voice graph wiring in ObjC++, two-buffer loop workaround, one varispeed per voice | Good for streams/music; SFX needs a hand-written loop, pan and cursor layer (basically a mini-mixer) | Apple: ruled out by deprecation. Soft: fits, but heavier and LGPL |

### Why not the others

- **AVAudioEngine**: zero dependencies, but it's the only option that needs Objective-C++ in `port/`, and the loop region needs splitting each looping sample into two scheduled buffers, which makes `GetSamplePosition` (`edSoundSampleService.cpp:236-247`) harder. It also adds one varispeed node per SFX voice. That's more code for no functional gain. Raw AudioQueue/AUGraph would mean writing our own mixer.
- **SDL3**: clean license and a good streaming model, but it adds a second windowing/platform library next to GLFW just for audio. Its missing loop, pan and cursor features are exactly what the SFX service needs.
- **OpenAL**: Apple's framework is deprecated since macOS 10.15 (SDK header). OpenAL Soft would work, but it's LGPL, a larger build, and its 3D-positional model is a poor match for Kya's precomputed left/right gains.

## Plan (for the implementation ticket, not done here)

1. **Vendor miniaudio** 0.11.25 as `port/ext/miniaudio/miniaudio.h` (or a submodule, matching the other `port/ext` deps). Compile one TU with `MINIAUDIO_IMPLEMENTATION`, `MA_NO_RUNTIME_LINKING`, `MA_NO_DECODING`, `MA_NO_ENCODING`, `MA_NO_GENERATION`, `MA_NO_RESOURCE_MANAGER` (all documented build options, manual/header L632-683), since Kya decodes everything itself.
2. **Device**: add `port/Audio/edSoundDeviceMiniaudio.cpp` (macOS only). It owns one `ma_engine` and exposes `Audio::ShutdownAudioDevice()` like the Windows version. In `edSoundDevice.h`, keep the `IXAudio2*` getters under `_WIN32` and move `ShutdownAudioDevice` out of the guard. Also drop the `_WIN32` guard at `edSysTransferService.cpp:119-121` so shutdown runs on every PC build.
3. **Samples**: add `MiniaudioSampleVoice : SampleVoice`. Use `ma_audio_buffer_ref_init(ma_format_s16, 1, pcm, frames)` and set its `sampleRate` field (the struct has no init param for it, header L5900-5911). Then `ma_sound_init_from_data_source(..., MA_SOUND_FLAG_NO_SPATIALIZATION)`, set the loop points and looping, and map `SetControls` to volume = max(|L|,|R|), balance pan from the L/R ratio, and pitch = clamp(p, ε, 4). `IsFinished` maps to `ma_sound_at_end`, and `GetFramesPlayed` to the cursor. The cursor is already folded into the loop, which the existing fold at `edSoundSampleService.cpp:242-244` leaves unchanged. Return it from `CreateVoice` under `#elif defined(__APPLE__)` (`edSoundSampleService.cpp:85-112`), or better through a per-OS default factory.
4. **Music**: add `MiniaudioMusicOutput : MusicOutput` with an `ma_pcm_rb` (f32, 2 ch, 48 kHz, ≥ 3 × 480 frames) as the sound's data source. `Submit` writes into the ring buffer, `Queued` returns `ceil(available_read / 480)`, and `Play` calls start/stop. SPSC matches the one worker producer and the audio-thread consumer.
5. **Streams**: extract a small `StreamVoice` interface (create from PCM + channels + rate, start, stop, flush, set volume, frames played, finished) out of the 13 `_WIN32` blocks in `edSoundStreamService.cpp`. Move today's XAudio2 code into `XAudioStreamVoice` without changing it, then add `MiniaudioStreamVoice`. This is the only Windows-touching refactor. It doesn't change behavior and can be checked by the existing tests plus a manual stream playback check on Windows.
6. **CMake** (`port/Audio/CMakeLists.txt`): keep `if(WIN32) ... XAudio2 ole32` as it is and add `elseif(APPLE)` for the miniaudio sources plus `-framework CoreFoundation CoreAudio AudioToolbox`. Per #17, the macOS-specific file can also live under `port/macOS/Audio/` and be selected by the per-OS CMake file.
7. **Tests**: add `DISABLED_Miniaudio...` device smoke tests mirroring the XAudio2 ones. The factory-based deterministic tests run unchanged on macOS.

Rough size: device ~40 lines, sample voice ~60, music output ~70, stream voice ~60, stream seam ~80 lines moved, CMake ~10.

### Risks and open details

- **Negative gains.** `SetControls` clamps L/R to [-1, 1] (`edSoundSampleService.cpp:59-60`), and XAudio2's output matrix keeps the sign, so the PS2 SPU's phase inversion (surround trick) survives. miniaudio's volume+balance can't express opposite-sign L/R gains. If the game produces them, add a tiny custom `ma_node` (a 2×1 gain matrix) instead of pan. Check the logs from `edSoundPlay.cpp:333-336` (`left=`/`right=`) to see whether negative values ever occur.
- **Pitch clamp.** XAudio2 caps the ratio at 4.0 because of `CreateSourceVoice(..., 4.0f)`. miniaudio has no cap, so keep the same clamp in the adapter for parity.
- **Callbacks.** miniaudio end callbacks run on the audio thread (manual §5). Keep the current polling model (`PollFinishedSamples`) and don't use them, which preserves the "no Eden objects cross into callbacks" rule.
- **Optional later cleanup:** miniaudio also runs on Windows (WASAPI), so one backend could eventually serve both. That's the upstream maintainer's call and out of scope here. Windows stays on XAudio2.
- Cutscene audio (`CBWCinSourceAudio`, `CinematicManager.cpp:6019-6060`) plays through `edSoundStream_*`, which is the same stream service,, so it needs no separate backend.
