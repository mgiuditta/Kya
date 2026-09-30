# Silencing original voice under rewritten cinematic subtitles

Research for [#33](https://github.com/mgiuditta/Kya/issues/33) (map [#22](https://github.com/mgiuditta/Kya/issues/22)). Builds on the override decision in [#28](https://github.com/mgiuditta/Kya/issues/28), Tier 1 text rewrites in [#30](https://github.com/mgiuditta/Kya/issues/30), and the text-format notes in [#25](https://github.com/mgiuditta/Kya/issues/25).

There is no game data on the machine this was written on. **Everything below comes from reading decompiled code and is unverified until someone checks it on Windows.** Line numbers refer to commit `2dfa7f0`. edBank lines refer to submodule `d2b70d3`.

## TL;DR

- **Yes, reused cinematics can be voiceless, using replacement only.** A cinematic can carry voice in up to two audio paths, and lip sync is a third resource. Each can be neutralized by replacing a file or entry with a same-shape "empty" version:
  1. **Streamed audio track** (`aAudioTrackIds`): a loose `<level>\Stream\<NAME>.MIB` file on disc. Replace it with an **all-zero file of the same byte length**. This needs one port-side addition: a `mods/` candidate in `GetStreamPathCandidates`. The Windows stream loader opens the file with `std::ifstream`, so the `edCFiler_CDVD::open` hook from #28 does not see it.
  2. **Cine-bank sound samples** (type 3/1 VAG entries played by actor sound tracks): replace each entry with the same VAG header and a zeroed ADPCM body. This is a pure bank-entry override.
  3. **Lip tracks** (`COT_LipTrack` entries in the cine bank): replace each one with a 16-byte empty track (`0, 0, nbTracks=0, 0.0f`). This is a pure bank-entry override. Every lip channel then reads 0.
- **"Dropping" a resource is not an option.** Overrides can only replace, and a missing lip track would crash (null dereference in `SetLipsynch`). An empty or missing stream file freezes the cinematic clock for 5 s.
- **Pointing `aAudioTrackIds` at nothing** (patching the ids to `-1` in the level's cinematic entry) also works. It is not the cheapest route, though: it needs an in-place patch of a variable-length bytecode entry, so a partial `CCinematic::Create` parser is required just to find the offsets. Keep it as a fallback in case the port hook is unwanted.
- **Music and SFX: expect them to go too.** Both audio paths play whole files with no stems, so silencing a stream or a sample removes everything mixed into it. Level music (`edMusic`) is a separate system. Whether it plays under cinematics is unknown. The mitigation is still replace-only: the replacement stream can be a voice-free mix built from the game's own music instead of silence.
- **Decision:** reuse is **not** limited to dialogue-free scenes. A reused dialogue cinematic ships with silent (or music-only) stream/sample overrides plus neutral lip tracks. Whether a scene is worth reusing then depends on how much it relies on its original mix, not on whether it has dialogue.

## 1. How a cinematic finds its audio and lip sync

### 1.1 Cinematic definitions (level bank)

- Each level bank has a cinematic definitions entry of type `0x13/1`, installed by `BnkInstallCinematic` → `CCinematicManager::Level_AddAll` (src/b-witch/LevelScheduler.cpp:2541-2550, table at :2743; src/b-witch/CinematicManager.cpp:272-316). It loads through `apply_callback`, hook 1 in #28.
- `CCinematic::Create` reads, for each cinematic (src/b-witch/CinematicManager.cpp:817-834):
  - `defaultAudioTrackId`, a `u32`, present only if the scene version is ≥ 2.16. Otherwise it is `-1`.
  - a count, then `count × (u32 languageIndex, u32 trackId)` pairs into `aAudioTrackIds[5]`, which is pre-filled with `0xFF`.
  - If the default is `-1`, it falls back to `aAudioTrackIds[0]`.
- `Install` sets `CINEMATIC_RUNTIME_FLAG_HAS_AUDIO_TRACK` only if `defaultAudioTrackId != -1` (:1501-1503).

### 1.2 Streamed audio track

- At play time the track id is chosen by `aAudioTrackIds[CMessageFile::get_default_language()]`, or the default if that entry is `-1`. The result goes to `CBWCinSourceAudio::SetAudioTrack` (:1090-1097, and again at :2160-2165 and :3699-3705).
- The id is an index into `CAudioManager::pGlobalSoundFileData`, a table of `{sampleRate, nbChannels, interleaveBlockSize, interleaveBlockCount, name[64]}` (src/EdenLib/Include/edSound/edSoundPlay.h, `GlobalSound_FileData`). The table is read from the level's sound config entry (`BnkInstallSoundCfg`, type `0x03/4`, LevelScheduler.cpp:2418-2425 → Audio.cpp:1129, :2127-2173).
- `SetAudioTrack` builds the path `<levelPath><levelName>\Stream\<NAME>`, with the name uppercased and its last character replaced by `B` (`.MIH` → `.MIB`) (CinematicManager.cpp:6194-6203). It then streams the file (:6237-6255).
- On Windows the stream is read by `Audio::LoadStream` with `std::ifstream` over `GetStreamPathCandidates(path)` (port/Audio/edSoundStreamService.cpp:225-243, :373-412). Files without a `VAGp` header are decoded as raw interleaved PS-ADPCM (`DecodeMib`, :316). **This bypasses edFile and edBank, so none of the #28 hooks see it.**
- **The stream is the cinematic's clock.** While `HAS_AUDIO_TRACK` is set, `IncrementCutsceneDelta` takes time from the stream's playback position (`Func_0x1c`), not from frame delta (:2661-2680, :6080-6160).
- Localization: a separate id per language. On Windows `get_default_language` is effectively English (index 0), per #30.

### 1.3 Cine-bank sound samples

- The cine bank is loaded by `CCinematic::LoadInternal`. When `CINEMATIC_FLAG_LOCALIZED_CINE_PATH` is set and a file named `<bank>.<LANG>` (`US/FR/GER/SP/IT`, :1391-1393) exists, that per-language bank is loaded instead of the base one (:1419-1443).
- `_gCinSoundCallback` → `InstallSounds` collects every type `3/1` entry through `get_info` and uploads it with `edSoundSampleLoad` (:1396-1403, :2750-2820). `get_info` reads the entry size through `get_entry` and the data through `get_entry_data` (edBank `edBankBuffer.cpp:326-385`). Both are #28 hook points.
- `.cin` actor sound tracks (`0x6e756fb7`) point at these samples by resource index and call `CBWCinActor::SetSound` (CinScene.cpp:892-917; CinematicManager.cpp:4552-4625). `InstallResource` resolves `COT_Sound` names against the loaded samples. For localized cinematics the name is rewritten into a `\<LANG>\` folder (:1599-1631). One recovered example name is `...\wav\master_mono.vag`, which suggests at least some cinematics keep a whole **master mix** as a sample.

### 1.4 Lip sync

- `COT_LipTrack` resources are cine-bank entries. `InstallResource` fetches them through `get_info` and hands them to `CLipTrackManager::InstallFromMem` (:1695-1701; src/b-witch/LipSync.cpp:59-85).
- The format is parsed by `CKFrameTrackReader::Create` (LipSync.cpp:113-195): `s32, s32, s32 nbTracks, f32`, then `nbTracks` typed sub-tracks. Only type 0 (`CTrackMultiChannel`) is decompiled. Types 1-7 are `IMPLEMENTATION_GUARD` stubs (src/b-witch/Lipsync.h:50-95).
- Actor track `0xdbd3d7c5` calls `SetLipsynch(time, reader)` (CinScene.cpp:920-940). `SetLipsynch` dereferences the reader unconditionally (CinematicManager.cpp:4788-4803).
- `CActor::AnimEvaluateLipsync` copies `reader->GetValue(i)` into face-animation channels. `GetValue` returns `0.0f` for any channel no sub-track writes (LipSync.cpp:223-233). The face layer weight is `1 - value[0xb] * 0.01` (Actor.cpp:3123-3165).

## 2. Candidates and what happens at the edges

| Candidate | Mechanism | Replace-only? | Edge behaviour (from code) |
|---|---|---|---|
| **A. Silent stream file** | Same-length all-zero `.MIB` at `mods/<level>/Stream/<NAME>.MIB` | Yes (loose file), but needs a `mods/` candidate added to `GetStreamPathCandidates` | Zero PS-ADPCM blocks decode to silence. Same length keeps the stream clock identical. **Empty or unopenable file:** `LoadStream` fails (`fileSize <= 0`, :396), `SetAudioTrack` returns with `soundInstanceId == 0`, and `Func_0x1c` retries for 5 s while returning 0. The cinematic **freezes at t=0 for 5 s**, then falls back to the wall clock (:6089-6104). |
| **B. `aAudioTrackIds` → -1** | Overwrite `defaultAudioTrackId` and every pair's `trackId` with `0xFFFFFFFF` in place, in the level's `0x13/1` entry | Yes (same-size entry override) | `HAS_AUDIO_TRACK` stays clear. `SetAudioTrack` is never called and the timeline runs on frame delta (:2661-2667). There is also a Windows guard for `id < 0` (:6190-6194). **Cost:** the offsets sit behind variable-length data (actor configs, conditions, switch lists, :721-852), so a parser for the whole entry is needed to find them. Changing the pair *count* would shift the bytecode, so don't. |
| **C. Silent samples** | Override each type `3/1` VAG entry in the cine bank (or the `.US` variant) with the same header and zeroed body | Yes (bank entry, hooks 2+3) | The sample still loads and plays silently. The override must report its own size through `get_entry`, because `InstallSounds` sizes sound RAM from it (:2779-2800). Dropping a sample isn't possible (replace-only). A sample that fails to resolve would only stop the actor's sound (`SetSound` with a null sample, :4567), which is safe, but it can't be produced by override anyway. |
| **D. Neutral lip track** | Override each `COT_LipTrack` entry with the 16 bytes `00000000 00000000 00000000 00000000` | Yes (bank entry) | `nbTracks = 0` → no sub-tracks. `GetValue` returns 0 for every channel and the face layer weight is 1.0. This also avoids the undecompiled track types 1-7. **Not dropping:** a missing entry leaves a null reader, and `SetLipsynch` then crashes (:4796). What "all channels 0" looks like (closed or neutral mouth?) is unverified. |

## 3. Recommendation

For each reused dialogue cinematic, in the language(s) being shipped (US first on Windows):

1. **Stream (if `defaultAudioTrackId != -1`):** candidate A. Emit a zero-filled file with the original `.MIB`'s exact size, or a music-only mix with the same size, format and length built from the game's own music. Port change: prepend `mods/` + host path to `GetStreamPathCandidates` in `port/Audio/edSoundStreamService.cpp`, and log hits like the other override hooks do. This is a few lines of port code and leaves decompiled code untouched.
2. **Samples (if the cine bank has type 3/1 entries holding voice or a master mix):** candidate C. Same size, zeroed ADPCM body.
3. **Lip tracks:** candidate D for every `COT_LipTrack` entry in the cine bank.
4. Keep B as a fallback only, if the stream hook is rejected or if a stream turns out to be shared with something that must keep playing.

Constraints to carry forward:
- Subtitle timing stays tied to the original timeline. A same-length silent stream preserves it exactly, and B preserves it approximately (frame delta instead of audio clock).
- Everything mixed into a silenced stream or sample is lost. If a scene's music or SFX lives there, the replacement has to supply them (music-only mix), or the scene has to be accepted without them.
- Localized cine banks (`<bank>.US`, ...) are separate files. Override the variant that actually loads.

## 4. Open questions surfaced

- Does voice actually live in the stream, in cine-bank samples, or in both? Does the stream carry music and SFX too? This is decided per cinematic and can only be checked with data.
- Does level music (`edMusic`) keep playing under a cinematic, or is it paused? If it plays, a silent stream still leaves a music bed.
- What does an all-zero lip-sync pose look like (closed or neutral mouth, or odd blend)? If it looks wrong, a one-key `CTrackMultiChannel` "rest" viseme is needed, which means knowing the output-type → face-channel mapping.
- Is any `.MIB` stream shared between several cinematics? Silencing it would affect all of them.
- Should the `mods/` override also cover `Audio::LoadStream` (and any other `ifstream`-based port loaders) as part of the #28 implementation, rather than as a separate hook?

## 5. Windows verification checklist

1. Log `defaultAudioTrackId`, `aAudioTrackIds[]` and the resolved `.MIB` path for every cinematic in the Act I candidate levels (`CCinematic::Create`, `SetAudioTrack`).
2. For one dialogue cinematic, list the cine-bank entries by type (3/1 samples, lip tracks) with `DebugFindFilePath`, and record whether the `.US` localized bank is the one that loads.
3. Listen to the original `.MIB` (decode offline) and the samples, and note which contain voice, music and SFX.
4. Add the `mods/` candidate to `GetStreamPathCandidates`, drop a zero-filled same-size `.MIB`, and play the cinematic. Confirm silence, unchanged length, and subtitles in sync.
5. Repeat step 4 with a **0-byte** file to confirm the predicted 5 s freeze (this documents the failure mode).
6. Override one lip-track entry with the 16-byte empty track, then check that there is no crash and look at the face pose.
7. Override one type 3/1 sample with a same-size zeroed VAG, and confirm it plays silently and that `InstallSounds` sizing still works.
8. Optional fallback: patch one cinematic's track ids to `0xFFFFFFFF` in the `0x13/1` entry and confirm the timeline runs on frame delta without audio.
