# Game text and dialogue: storage and lookup

Research for [#25](https://github.com/mgiuditta/Kya/issues/25) (map [#22](https://github.com/mgiuditta/Kya/issues/22)).

There is no game data on the machine this was written on. **Every statement below comes from reading the decompiled code and has not been checked against real files.** Treat each one as "from code, unverified". Line numbers refer to branch `docs/sequel-plot` at `09412e2`.

## TL;DR

- All translatable text is stored in flat binary **message files** (`*_GB.bin`, `*_FR.bin`, ...). Each one is a table of `(u64 key, string offset)` pairs followed by 8-bit strings. `CMessageFile` loads a file and `gMessageManager` searches every loaded file for a key.
- A key is an opaque 64-bit value. Keys are baked into actor bytecode, `levels.bnk`, cinematic tracks and C++ constants. The one key that can be decoded (`GAME_NAME`) is the symbol name's ASCII XOR-folded into 8 bytes.
- There are 5 languages: GB, FR, GE, SP, IT. The language is chosen by file name suffix. Changing it at runtime reloads every loaded message file.
- Cinematics show dialogue through subtitle keyframes, each carrying a key, a duration and alignment flags. The text lives in a per-cinematic message file inside the cinematic's bank. In gameplay, actors (companion alerts, signs, shops) keep a key in their bytecode.
- Voice audio and lip sync are **not linked to text keys**. They are separate tracks on the cinematic timeline. Voice audio is chosen per language (an audio track id per language, or a localized folder or bank). Lip-sync tracks are resources in the cinematic bank.
- To add a line you need: a new key, an entry in every language's `.bin` (same bank), and a reference to the key from data, either a cinematic subtitle key or actor bytecode. We have no tool that writes any of these today.

## 1. Message file format

`CMessageFile` (src/b-witch/TranslatedTextData.h:6-132) holds `languageID`, `entryCount`, a 32-byte `szFilePath` template, `pDataA` (the entry table), `pFileData`, the bank it came from, and prev/next links into `gMessageManager`.

`CMessageFile::prepare_buffer` (src/b-witch/TranslatedTextData.cpp:44-66) implies this on-disk layout:

| Offset | Type | Meaning |
|---|---|---|
| 0x0 | `int32` | entry count |
| 0x4 | `int32` | "relocated" flag. It is 0 on disk and the code sets it to 1 after fixing up the offsets in place |
| 0x8 | `entry[count]` | 16 bytes each: `u64 key`, then `u64` whose low 32 bits are a **byte offset from file start** to the string. After relocation the pointer is written in place |
| ... | `char[]` | NUL-terminated strings |

Notes:

- Lookup is a linear scan with no sorting or hashing (`CMessageFile::get_message`, TranslatedTextData.cpp:266-315). A key of `0` returns `""`. A miss returns `""` in mode 0 and `NULL` in mode 1.
- Relocation writes a pointer into the 8-byte slot. The Windows port keeps it working because the slot is 8 bytes wide (TranslatedTextData.cpp:60).
- **Pointer keys:** `CMessageFile::pointer2hash` (TranslatedTextData.cpp:317-326) tags a raw `char*` with high bits `0xffffffff` (PS2) or `0xffff` (x64). `get_message` then returns that pointer unchanged (TranslatedTextData.cpp:281-290). Code uses this to pass runtime strings wherever a key is expected, for example save slot names (src/b-witch/SaveManagement.cpp:1074).
- Strings are single-byte characters. The font maps a code to a glyph (see §6). There is no UTF-8 or UTF-16 handling anywhere in the path.

## 2. Which files exist and where they load from

`CLanguageManager` (src/b-witch/LocalizationManager.h:8-57) owns three `CMessageFile`s:

| Member | Source | Loaded in |
|---|---|---|
| `trcText` | `<bnk>0:TRC_%s.bin` | `Game_Init`, LocalizationManager.cpp:30 |
| `userInterfaceText` | `<BNK>0:UserInterface_%s.bin` | `Game_Init`, LocalizationManager.cpp:31 |
| `levelText` | `<levelPath>_xx.bin` inside the current **level bank** | `Level_Init`, LocalizationManager.cpp:46-60; unloaded in `Level_Term` :63-67 |

- `<BNK>0:` is a mounted bank, `CDEURO/menu/Messages.bnk` (src/b-witch/kya.cpp:1397-1398, mounted at :1518 via `edFileMount` :1310-1325). Files from this path go through `edFileOpen` (TranslatedTextData.cpp:192-262).
- Level text loads only when the `levels.bnk` header field `field_0x30` is non-zero. The base name `levelPath` comes from `levels.bnk` (src/b-witch/LevelScheduler.cpp:616, :1033-1048; struct at LevelScheduler.h:254-266). The bank overload (TranslatedTextData.cpp:122-190) looks the name up in the already-loaded level bank buffer. It rewrites the last `_xx` into `_%s` (:154-158) and then formats in the language suffix (:160).
- **Cinematic text:** every `CCinematic` has its own `CMessageFile textData` (src/b-witch/CinematicManager.h:829). It is filled when the cinematic installs a `COT_Text` resource from its cine bank (CinematicManager.cpp:1694-1696). The request comes from subtitle source type 3 (src/EdenLib/edCinematic/Sources/Cinematic.cpp:108-120 → `CBWCinSourceSubtitle::Create`, CinematicManager.cpp:6273-6281). It is dropped on teardown (CinematicManager.cpp:3384-3387). The file name comes from the cinematic's resource collection and must end in `_XX.bin` for the suffix rewrite to work.
- `gMessageManager` is a linked list of every loaded `CMessageFile`, in load order. `MessageManager::get_message` (TranslatedTextData.cpp:328-342) returns the **first** match across all of them: TRC, then UI, then level, then any live cinematic. Most call sites use this global lookup rather than a specific file.

## 3. Key and id scheme

- A key is a `u64`. The file stores it as-is. Data stores it as two `u32` halves joined with `ByteCode::BuildU64(lo, hi)` (src/b-witch/MemoryStream.cpp:5-8) or read with `GetU64`.
- Keys are hard-coded as C++ constants for UI and system text, for example `0x2414d455f4e414d` (LocalizationManager.cpp:32) and the button-name table `_commands[]` (:142-153). Pause, MapManager, MenuMessageBox, ActorNativ and ActorNativShop contain dozens more (`rg -n "get_message\(0x" src/b-witch`).
- **Key derivation hypothesis:** `0x02414D455F4E414D` is exactly `"GAME_NAME"` XOR-folded into 8 bytes, big-endian: byte *i* is the XOR of chars *i*, *i+8*, and so on. The engine then registers that string as the `%[GAMENAME]s` token (LocalizationManager.cpp:23,33). If this holds for every key, the original toolchain derived keys from symbolic names such as `GAME_NAME`, and a new line only needs a unique name. Only one key has been checked, so this is unverified. Collisions are possible and nothing detects them.
- Keys that data supplies:
  - Level info title: `S_LEVEL_INFO::titleMsgHash` (LevelScheduler.cpp:608).
  - Per-level map/objective markers: `messageKey`, gated by a scenario variable (LevelScheduler.cpp:802-845; used in MapManager.cpp:313, :1694). The loader is misnamed `ReadLanguageFileNames`.
  - Actor bytecode: `CActorHelperSign` reads a `u64` at create time (src/b-witch/ActorHelperSign.cpp:20-22) and shows it on interaction (:835). Companion alerts read `levelStringKey` (src/b-witch/ActorCompanion.cpp:890-897) and resolve it against **level text** only (:1334, :2148).
  - Cinematic subtitle keyframes: `keyA`/`keyB` (§5).

## 4. Languages

- `enum LANGUAGE { GB, FR, GE, SP, IT, AUTO }` (src/b-witch/Types.h:696-704). Text file suffixes are `"GB","FR","GE","SP","IT"` (TranslatedTextData.cpp:100-106).
- Cinematic audio uses **different** suffixes: `"US","FR","GER","SP","IT"` (src/b-witch/CinematicManager.cpp:1391-1393).
- The default language comes from the PS2 system language in PAL mode, otherwise GB (src/b-witch/kya.cpp:1328-1395). On Windows `GetSystemLanguage` always returns English (:1327, :1363).
- The in-game setting change goes through `CSettings` → `CMessageFile::set_default_language` → `CLanguageManager::ApplyLanguage` (src/b-witch/Settings.cpp:60-66). That runs `gMessageManager.reload()` (TranslatedTextData.cpp:344-358), which reselects every loaded file. For bank-backed files it looks up the other language's file **in the same bank**. So level and cinematic banks must contain all five language variants (inferred).
- The intro FMVs pick a video file by a language letter plus an `s` suffix for subtitles, so their subtitles are burned into the video (kya.cpp:1815-1822).

## 5. How dialogue is triggered

### Cinematics (almost all spoken dialogue)

1. A `.cin` contains sources. Source type 3 is a subtitle source (Cinematic.cpp:108-120). Its `Create` installs the per-cinematic text file (§2).
2. Each frame, an animated property of `propType == 3` holds keyframes of `SUBTITLE_PARAMStag { int sourceIndex; u32 keyA; u32 keyB; float time; u32 flags; }` (src/EdenLib/edCinematic/Sources/Cinematic.h:46-60). They are read at Cinematic.cpp:384-406.
3. `CBWCinSourceSubtitle::SetSubtitle` (CinematicManager.cpp:6296-6345) does `key = BuildU64(keyB, keyA)` and then `gMessageManager.get_message(key)`. It stores `pSubtitleText` and sets the alignment from `flags & 3`. `flags & 4` anchors the text to the source's 3D position. `time` is the display duration.
4. There is also a per-actor subtitle track (track type `0xd9cee9bc`, src/EdenLib/edCinematic/Sources/CinScene.cpp:867-889 → `CBWCinActor::SetSubtitle`, CinematicManager.cpp:4665-4689). It anchors to the actor's location. This path still has an `IMPLEMENTATION_GUARD` (CinScene.cpp:885), so no shipped cinematic has exercised it in the port yet.
5. `CCinematicManager::DrawBandsAndSubtitle` (CinematicManager.cpp:5688 onward) draws the text with `edCTextFormat`. It is drawn only when `bUseSubtitles` is set (:5782; the setting is Settings.cpp:68-70).
6. `SetMessage` tracks (CinScene.cpp:844-864 → CinematicManager.cpp:4692+) are **not text**. They are named engine events such as `AUTO_BLINK_ON` and rumble commands (CinematicManager.cpp:74-110).

Gameplay code starts cinematics from actors and triggers through the cinematic manager. Text is never passed in; it always comes from the `.cin`.

### Gameplay (non-cinematic)

- Companion alerts use level text and reveal it character by character (ActorCompanion.cpp:1322-1345, :2140-2160).
- Helper signs and shop signs use `get_message(key)` from bytecode (ActorHelperSign.cpp:835; ActorNativShop.cpp:634, :714).
- Menus, pause, map, credits and message boxes use hard-coded or table keys (Pause.cpp, MapManager.cpp, ActorCredits.cpp:254-285, MenuMessageBox.cpp:161-189).
- `CHelpManager` (src/b-witch/Help.cpp) mostly loads equipment icons. It sets `userInterfaceText` as the help menu's message source (Help.cpp:60), and `DrawHelpMenu` is an empty stub (:160-163). It is not a dialogue system.

## 6. Inline markup and font

- `edCTextFormat` parses printf-like `%` codes (src/Rendering/edCTextFormat.cpp:1095-1131, :1160-1222). `%%` is a literal percent sign. `%[NAME]x` looks up the 8-char token `NAME` (packed like `TextAdd`, edTextResources.cpp:59-110) in `edTextResources` (:1236-1261). The suffix picks the kind: `s` = text, `b` = bitmap, `k`/`K` = style callback. An unknown token renders as `(???)`.
- The tokens are registered in `BootData.cpp`. Bitmaps (button icons) are `MAGIC`, `JUMP`, `ACTION`, `MONEY`, `UDLR`, and others (src/b-witch/BootData.cpp:138-160). Callbacks are `RED`, `GREEN`, `BLUE`, `YELLOW`, `BLACK`, `WHITE`, `BLINK`, `NOBLINK`, `ALPHA*` (:162-174). The one text token is `GAMENAME` (LocalizationManager.cpp:33). New lines can use the same markup.
- The font is `medium.fon` from `CDEURO/menu/MenuData.bnk` (BootData.cpp:20-24, :116-128). Glyph lookup goes through code-point segments (src/Rendering/edCTextFont.cpp:4-34). A 256-entry override table remaps a few bytes: 156→`œ` (0x153), ñ, ç, Ç, Ü (BootData.cpp:130-136). The encoding therefore looks like Latin-1/CP1252. A new line must stay within glyphs that `medium.fon` contains.

## 7. Voice audio and lip sync

- **They are not tied to text keys.** Nothing in the audio or lip-sync paths reads a message key, and nothing in the text path references audio. The only link is timing: the subtitle keyframe, the sound keyframe and the lip-sync keyframe sit on the same cinematic timeline.
- **Voice audio** is localized per cinematic in two ways:
  - The cinematic script bytecode carries `aAudioTrackIds[5]`, one id per language, plus a default (CinematicManager.cpp:817-834, used at :1090-1097 through `BWCinSourceAudio_Obj.SetAudioTrack`).
  - With `CINEMATIC_FLAG_LOCALIZED_CINE_PATH` (CinematicManager.h:53) on PAL, the sound file path is rewritten into a `\US\`, `\FR\`... subfolder (CinematicManager.cpp:1600-1620). The whole cine bank can also be swapped for a `name.<LANG>` variant if one exists (:1419-1442).
- **Lip sync:** `COT_LipTrack` resources in the cine bank are installed into `CLipTrackManager` (CinematicManager.cpp:1698-1701; src/b-witch/Lipsync.h:107-125). Actor track `0xdbd3d7c5` keyframes `{ resourceIndex, timeOffset }` call `SetLipsynch` (CinScene.cpp:919-940). `CActor::AnimEvaluateLipsync` then drives face animation (src/b-witch/Actor.cpp:3123+). Lip tracks are per-cinematic data. They are per-language only if the localized cine-bank variant is used.

## 8. What adding a new line would take

Two cases, both inferred from the code:

**A. New subtitle in a new or modified cinematic**
1. Pick a key, ideally a symbolic name folded by the §3 scheme once that is confirmed.
2. Add `(key, string)` to the cinematic's `*_XX.bin` for all 5 languages and pack them into the cine bank. Any other loaded file would also work, because lookup is global.
3. Add a subtitle keyframe (`keyA`/`keyB`, `time`, `flags`) to the `.cin` subtitle property. This requires a `.cin` writer or patcher, which we do not have.
4. Optional: add a voice sound source, per-language track ids, and a lip-sync track. Every one of these is a separate asset. Subtitles work without them.

**B. New line shown by an actor (sign, companion alert)**
1. Add the entry to the level's `<levelPath>_XX.bin` for each language, in the level bank.
2. Write the key into that actor's bytecode field (HelperSign `field_0x160`; Companion `levelStringKey`) in the level's actor data.

**Prerequisites neither case has yet:**
- a `.bin` message file reader/writer. The format is trivial, see §1.
- a bank (`.bnk`) repacker.
- confirmation of the key derivation.
- either a `.cin` or actor-bytecode editor.

A shortcut for prototyping only: register an extra `CMessageFile` at runtime with `select_language(path, AUTO)` from a loose file, or append entries to `levelText`. Because lookup is global, any key in that file becomes resolvable immediately, so an existing subtitle or sign key can be overridden by loading the new file earlier in the list. This is untested.

## 9. Checklist to run on Windows (with game data)

- [ ] Dump `UserInterface_GB.bin` from `CDEURO/menu/Messages.bnk`. Confirm the header (`count`, flag 0) and the 16-byte `(u64 key, u64 offset)` entries, and check whether entries are sorted.
- [ ] Confirm that `0x02414D455F4E414D` resolves to the game name. Try the XOR-fold hypothesis on a few other keys against guessed names, such as those beside `_commands[]` in LocalizationManager.cpp:142-153.
- [ ] List a level bank and a cine bank. Check that `_GB/_FR/_GE/_SP/_IT.bin` all exist, and find the exact name of a cinematic's text file.
- [ ] Log `select_language` and the load order of `gMessageManager` (`MY_LOG` already exists, TranslatedTextData.cpp:133, :172, :184) during a level with a cinematic.
- [ ] Break in `CBWCinSourceSubtitle::SetSubtitle` during a dialogue cinematic. Record `keyA/keyB/time/flags` and the resolved string.
- [ ] Check whether any cinematic reaches the actor subtitle path (`CinScene.cpp:885` guard).
- [ ] Check whether any cinematic has `CINEMATIC_FLAG_LOCALIZED_CINE_PATH`, and whether `*.US`/`*.FR` cine bank variants or `\US\` WAV folders exist.
- [ ] Check whether lip-sync resources differ between languages.
- [ ] Switch the language in settings mid-level and confirm that level and cinematic text reload without a crash.
- [ ] Render a test string containing `%[JUMP]b`, `%[RED]k`, `%%` and accented characters to confirm the markup and glyph coverage of `medium.fon`.
