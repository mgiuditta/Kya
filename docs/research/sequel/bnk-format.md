# BNK container format (research for issue #23)

Question: what is the `.bnk` container format, as reconstructed from the decompiled loaders? Could a writer produce a BNK that the unmodified engine accepts?

**Status: every fact below is from code and unverified.** This machine has no game data, so nothing here has been checked against a real archive. The one exception is marked "repo note". It comes from `port/Audio/MusicPlaybackImplementationPlan.md`, written by an earlier session that did have assets. Even that is second-hand and was not re-checked here.

## Sources

- Loader code lives in the **`src/EdenLib/edBank` submodule** (`https://github.com/Icey1717/edBank`, pinned at `d2b70d3b41a70733df1854ac20aa51f435e749a7`). The submodule is not checked out in this repo, so it was read from a tarball of that exact commit. Citations `edBankFile.cpp:N` and the like refer to `src/EdenLib/edBank/sources/<file>` at that commit.
- Headers in the parent repo: `src/EdenLib/Include/edBank/edBankFile.h`, `edBankBuffer.h`, `edBankFiler.h`, `edBankStackFile.h`.
- Callers: `src/b-witch/LevelScheduler.cpp`, `FrontEndBank.cpp`, `BootData.cpp`, `SectorManager.cpp`, `kya.cpp`, `LocalizationManager.cpp`.
- `src/b-witch/BankManager.cpp`, `src/b-witch/PathBank.cpp` and `src/edC/edC*Bank*` are empty stubs and play no part in the format.

## Big picture

- A `.bnk` is one flat archive. It holds a directory header, a per-entry offset/size table, an optional per-entry type table, a compressed **name tree**, name-to-index tables, and the payloads.
- The engine usually reads the **whole file** into a pre-sized "bank buffer" (`edCBankBuffer::file_access`, `edBankBuffer.cpp:112-246`).
- If the archive is packed, the engine decompresses it in place (`edCBankFileHeader::unpack`, `edBankFile.cpp:93-242`).
- Then it walks every entry. For each entry it calls the callback registered for that entry's `(type, stype)` pair, passing `(payloadPtr, size)` (`apply_callback`, `edBankFile.cpp:473-535`).
- Some banks, such as `Messages.bnk`, are instead **mounted** as a virtual drive `<BNK>0:`. Only their header is kept in memory. Single entries are then opened by name and streamed from disk (`CFilerBank::mount_unit` / `open`, `edBankFiler.cpp:161-345`; `kya.cpp:1397-1398,1518`; `LocalizationManager.cpp:30-31`).

## Layout

All integers are little-endian 32-bit unless noted. The engine sees the file as an 8-byte tag followed by `edCBankFileHeader` (`struct edCFiler_Bnk_static_header`, `edBankFile.h:77-80`; `FILE_DATA_TAG_SIZE 0x8`, `edBankBuffer.h:17`).

### Offset conventions (important for a writer)

| Field kind | Relative to | Evidence |
|---|---|---|
| Table offsets (entries, types, name tree, index tables) | **header start = file + 8** | `get_entry` computes `header + off - 8 + 8` (`edBankFile.cpp:460-471`); `get_entry_typepair` uses `header + off` (`:392`); `get_index` (`:347-359`); `get_entry_filename` (`:420-427`) |
| Payload `offset` inside an entry | **file start (absolute)** | `get_entry_data` returns `header + offset - 8` (`edBankFile.cpp:456`); `CFilerBank::open` seeks the raw disk file to `entry->offset` (`edBankFiler.cpp:235-236`) |

Repo note: an earlier session observed the same split on real files ("Table offsets are relative to that header; payload offsets are absolute", `port/Audio/MusicPlaybackImplementationPlan.md:75-76`). Its tool reads count, table and types at file offsets 40, 48 and 52 (`port/Audio/music_render.cpp:34-44`), which matches the struct below.

### Header (file offsets)

| File off | Hdr off | Field (`edBankFile.h:51-74`) | Meaning / use |
|---|---|---|---|
| 0x00 | n/a | 8-byte tag | Skipped (`FILE_DATA_TAG_SIZE`); no engine read found. Contents unknown. |
| 0x08 | 0x00 | `char header[4]` | Magic. Repo note: `KNAB`, which is `"BANK"` read as an LE u32 `0x42414E4B` (`MusicPlaybackImplementationPlan.md:75`, `music_render.cpp:34`). **The engine never checks it.** |
| 0x0C | 0x04 | `flags_0x4` | Bit 0 means packed: the engine calls `unpack()` after load (`edBankStackFile.cpp:31-33`, `edBankFile.cpp:113`). Other bits unknown. |
| 0x10 | 0x08 | unknown (4 bytes) | Not read. |
| 0x14 | 0x0C | `FileTypeData typeData` {u16 `stype`, u16 `type`} | Bank-wide type. **If both are 0 the bank is heterogeneous** and the per-entry type table is used. Otherwise every entry gets this type (`get_entry_typepair` / `is_heterogeneous`, `edBankFile.cpp:377-410`). |
| 0x18 | 0x10 | `sizePacked` | Overwritten after load with the real file size (`edBankStackFile.cpp:47-51`). Subtracted from the buffer's used space on `close()` (`edBankBuffer.cpp:494`). |
| 0x1C | 0x14 | `sizeUnpacked` | No reader found. |
| 0x20 | 0x18 | unknown | Not read. |
| 0x24 | 0x1C | `field_0x1c` | **Payload alignment** used when laying out unpacked entries: `(size + a-1) & ~(a-1)` (`edBankFile.cpp:162,171,223,232`). Only read when packed. Must be a power of two. |
| 0x28 | 0x20 | `fileCount` | Entry count (`edBankBuffer.cpp:414`). |
| 0x2C | 0x24 | unknown | Not read. |
| 0x30 | 0x28 | `fileHeaderDataOffset` | Offset of the entry table. |
| 0x34 | 0x2C | `fileTypeDataOffset` | Offset of the per-entry type table (heterogeneous banks only). |
| 0x38 | 0x30 | `field_0x30` | Offset of the **name tree**. |
| 0x3C | 0x34 | `field_0x34` | Offset of the name-order to entry-index table ("mode 0"). |
| 0x40 | 0x38 | `field_0x38` | Offset of a second index table ("mode 1"). No caller found that uses mode 1. |
| 0x44 | 0x3C | (past the recovered struct) | Read as the **length of the header block** to load. Used by `edBankFilerReadHeader` (`edBankFile.cpp:49-71`) and by the header-only bank mode (`edBankBuffer.cpp:96-99`). Hypothesis: this is really `entry[0].offset`, the start of the first payload and so the end of the header block. That would mean the entry table usually begins at header offset 0x3C. Unverified. |

`edBankFilerReadHeader` rejects files smaller than **0x40 bytes** (`edBankFile.cpp:37`). It reads the first 0x800 bytes, then rounds the rest of the header block up to 0x800 (`:41-71`).

### Entry table: `FileHeaderFileData[fileCount (+1)]`, 16 bytes each (`edBankFile.h:26-36`)

| Off | Field | Notes |
|---|---|---|
| 0x0 | `offset` | Absolute file offset of the payload. |
| 0x4 | `size` | Payload size. This is the **unpacked** size when the bank is packed (`edBankFile.cpp:175`). Passed as the callback's `size` argument (`:527`). A mounted-bank read must request exactly this many bytes (`edBankFiler.cpp:134`). |
| 0x8 | unknown | Not read. |
| 0xC | `crc` | Returned by `get_info` (`edBankBuffer.cpp:358`). **No consumer found**, so it is never verified. |

When packed, `unpack` reads `entry[fileCount]` as a **sentinel**: its offset marks the end of the packed data (`edBankFile.cpp:136`, loop `<= fileCount` at `:239`). A packed writer must therefore emit `fileCount + 1` entries.

Note that `unpack` addresses entries from `file start` rather than `header start` (`piVar10 = &this[-1].field_0x34`, which is `this - 8`). Its `+8` and `+0xC` are therefore the struct's `offset` and `size`, not unknown fields.

### Type table (heterogeneous banks)

`fileCount` × 4 bytes: {u16 `stype`, u16 `type`}, so reading it as an LE u32 gives `(type << 16) | stype` (`edBankFile.h:15-18`, `edBankFile.cpp:392`). The repo-note tool matches `0x30005/6/7` this way (`music_render.cpp:42-43`).

### Names: compressed path tree, no hashes

There is **no hashing**. Names are case-insensitive paths stored as a prefix tree.

- **Path normalisation** (`TreeInfo_OptimizeFilePath`, `edBankFile.cpp:750-796`): the extension becomes a pseudo-directory placed before the basename. So `dir\arrow_pressed.g2d` becomes `dir\.g2d\arrow_pressed`. `TreeInfo_UnOptimizeFilePath` reverses this (`:711-748`). The separator is `\`.
- **Tree encoding** (`TreeInfo_Recurse`, `:627-709`; `TreeInfo_RecurseWhileCountingIndexesUsingReference`, `:537-623`). The tree is a sequence of top-level nodes ended by a `0x00` byte (`:312-325`, `:430`).
  - **Leaf** (first byte `c >= 0`): the name is `c` bytes long. If `c == 0x7F`, the length is `0x7F + next byte`. The name bytes follow.
  - **Directory** (first byte `c < 0`): the name is `-c` bytes long. If `c == -128`, a next-byte extension applies; the two decompiled copies disagree on the sign handling here, so avoid names of 127 characters or more. Then comes a **7-bit little-endian varint** child count, then the name bytes, then the children.
- **Lookup** (`get_entryindex_from_filename`, `:286-334`) walks the tree depth-first and counts leaves.
  - The match is a **suffix compare** from the end of the query. It is case-insensitive via `& 0xDF` (`:607-616`).
  - The leaf ordinal is mapped to the real entry index through the **index table** at `field_0x34`.
  - That table's element width is 1 byte if `fileCount < 0x100`, 2 bytes if `< 0x10000`, and 4 bytes otherwise (`get_index`, `:336-373`).

Name lookups are used by frontend resources (`FrontEndBank.cpp:147`), by mounted `<BNK>0:` files (`edBankFiler.cpp:210`), and by `get_info`'s path output. The per-entry callback path (`apply_callback`) does **not** need names at all.

### Compression: LZ77 variant, in place, back to front

- The whole-archive flag is `flags_0x4 & 1`. Each entry is judged separately: if `packedSpan = entry[i+1].offset - entry[i].offset` is smaller than `size`, the entry is LZ-compressed; otherwise it is copied as stored (`edBankFile.cpp:136,179-195`).
- Entries are unpacked from last to first into a new layout. The first payload stays at `entry[0].offset`, and each next one starts at the previous start plus `align(size, field_0x1c)`. The entry offsets are then rewritten to that layout (`:143-174`, `:198-239`).
- 0x100 bytes past each destination are saved and restored as scratch space (`:180-188`). The bank buffer therefore needs room for the **unpacked** size plus slack. Callers do add `+0x1000` (`LevelScheduler.cpp:2789,2835`).
- **Decoder** (`edDataLZ77Unpack`, `:244-284`):
  - Flag bytes are read LSB first, one flag bit per token.
  - Bit 1 means copy one literal byte.
  - Bit 0 means a 2-byte back-reference `b0 b1`: `length = (b0 & 0xF) + 3` (3 to 18), `distance = ((b1 << 8) | b0) >> 4` (12 bits), copying from `dst - (distance + 1)`.
  - The loop stops when the remaining output count reaches exactly 0. The encoder must hit the size exactly, because the counter is never clamped.
- An **uncompressed** archive (bit 0 clear) skips `unpack` entirely.

### Alignment

- The bank buffer is allocated 0x800-aligned (`edBankBuffer.cpp:60`). Each loaded file is placed at the next free byte, with no rounding: `flagB += fileSize` (`:219-223`). Only the first bank in a buffer is guaranteed 0x800-aligned.
- Payload alignment inside the file is up to the writer. Unpacked payloads use `field_0x1c`.
- No explicit alignment check on payloads was found. The payload parsers (G2D/G3D and others) may still need 16-byte or larger alignment for PS2 DMA-style data. **Unknown: copy the alignment used by the original files.**

### Load-time flags (`edCBankInstall.fileFlagA`, set by the caller, not stored in the file)

| Bit | Meaning | Evidence |
|---|---|---|
| 1 | Header-only "bank of bank" mode | `edBankBuffer.cpp:47-100`. The sub-entry read path is still `IMPLEMENTATION_GUARD` on Windows (`:152-174`). |
| 4 | No-wait (asynchronous) read | `edBankBuffer.cpp:138,235`. Cleared when `_edBankAllowNoWait == 0` (`:43-45`). |
| 8 | Skip callbacks at load; the caller runs `install()` later | `edBankStackFile.cpp:35`; `Level.bnk` uses `0xC`, then `Level_Install` (`LevelScheduler.cpp:2840,2852-2859`). |

## File types (from callback tables)

`TableBankCallback` (`LevelScheduler.cpp:2721-2746`) is used for `Level.bnk`, `LevelIOP.bnk` and sector banks:

| type | stype | Handler | Content |
|---|---|---|---|
| 0x02 | 1 / 2 | `BnkInstallAnim` / `BnkInstallAnimMacro` | Animations |
| 0x03 | 4 / 1 / 5 / 6 / 7 | SoundCfg / Sample / Song / Bank / BankHeader | Audio. Repo note: 5 = song, 6 = sample body, 7 = bank header (`MusicPlaybackImplementationPlan.md:76-77`) |
| 0x04 | 1 | `BnkInstallG3D` (`BANK_CALLBACK_MESH`) | Meshes |
| 0x05 | 1 | `BnkInstallG2D` (`BANK_CALLBACK_TEXTURE`) | Textures |
| 0x06 | 1 / 2 | Scene / SceneCfg | Scene data |
| 0x07 | 1 / 2 | Col / DynCol | Collision |
| 0x08 | 1 | Events | Events |
| 0x09 | 1 / 2 | FxCfg / ParticleManager | Effects |
| 0x0A | 1 | `BnkInstallTrack` | Event tracks |
| 0x0B | 1 | Cameras | Cameras |
| 0x0C | 1 | (null handler) | Present but ignored |
| 0x0F | 1 | Astar | Pathfinding |
| 0x10 | 1 | Lights | Lights |
| 0x11 | 1 | Liptracks | Lip-sync |
| 0x13 | 1 | Cinematic | Cinematics |

- `TableFrontendBankCallback` (`FrontEndBank.cpp:53-58`) accepts only G3D and G2D. `Frontend.bnk` is loaded into a buffer of 0x64000 bytes (`:72`).
- Banks whose callback table is empty and which are read by index or name include `Info/levels.bnk` (`LevelScheduler.cpp:600-601,921-939`, buffer 0x10000) and `maps.bnk`, cinematics and pause banks (`Pause.cpp:3025`, `MapManager.cpp:2427`, `CinematicManager.cpp:1292`).
- Entries whose type has no matching callback are silently ignored (`edBankFile.cpp:505-515`).

## Can a writer produce a BNK the unmodified engine accepts?

**Probably yes, at the container level. From code, unverified.**

- There is no magic check and no CRC or checksum validation (`crc` is never consumed).
- The only structural gate is `size >= 0x40` (`edBankFile.cpp:37`).
- Compression is optional: clear bit 0 and write stored payloads, and no LZ77 encoder is needed.

The hard constraints on a writer:

1. **Offsets.** Table offsets are relative to file+8; payload offsets are absolute. `fileCount` must be correct.
2. **Types.** Either set a non-zero bank-wide `typeData`, or leave it zero and write a 4-byte-per-entry type table with `(stype, type)` pairs that match the caller's callback table.
3. **Names.** If anything looks entries up by name (frontend, mounted `<BNK>` banks, cinematics or maps by path), emit a valid name tree plus a mode-0 index table of the right width. Mirror the original path strings, since lookup is a suffix match.
4. **Header-length word at file+0x44.** Required for mounted banks and header-only mode. It must cover the header, the tables and the tree.
5. **Size budgets.** The file (and the unpacked size, if packed) must fit the caller's buffer.
   - Per-level banks: `bankSizeLevel/Sect/IOP + 0x1000`, sizes that come from `Info/levels.bnk` (`LevelScheduler.cpp:612-614,2789,2835`).
   - Fixed buffers: frontend 0x64000, boot 0x32000 (`BootData.cpp:87`), levels info 0x10000.
   - The check is `edBankBuffer.cpp:178-182`. A bigger custom level bank also means editing `levels.bnk`.
6. **Payloads.** These are the real work. Every payload (G2D, G3D, scene, collision and so on) must be in its own format, which is out of scope here. Copying existing payloads byte for byte and repacking is the low-risk first step.
7. **Unknown fields** (the tag at 0x00, 0x10, 0x20, 0x2C, entry +0x8, `sizeUnpacked`, the other flag bits). Copy them from an original file. The code shows no reader, but PS2-only paths or the original executable may still use them.

## Checklist for Windows, once data is available

1. Dump bytes 0x00-0x47 of `Frontend.bnk`, `Level.bnk`, `LevelIOP.bnk`, `Messages.bnk` and `Info/levels.bnk`. Check for `KNAB` at 0x08, the `fileCount` at 0x28, the table offsets, and what the 8-byte tag at 0x00 holds.
2. Check whether the u32 at 0x44 equals `entry[0].offset` and whether `fileHeaderDataOffset == 0x3C`, which would confirm the header-length hypothesis.
3. Record `flags_0x4` for each shipped bank. Are any packed (bit 0)? What is `field_0x1c`, the alignment?
4. Record whether entry `+0xC` looks like a real CRC32 (compare with `edFileCRC32`) and what entry `+0x8` holds.
5. Record which banks are homogeneous (`typeData != 0`) and list the `(type, stype)` histogram for each bank.
6. Decode the name tree of one bank and confirm the `.ext`-as-directory layout, the varint child count, and the index-table width.
7. Record the payload offset alignment for each type (G2D/G3D especially).
8. Round-trip test: rewrite one bank (such as `Frontend.bnk`) **uncompressed**, with identical payloads and freshly built tables, then boot `KyaPort` and confirm the frontend renders. Repeat with a level bank, updating `levels.bnk` sizes if needed.
9. If any shipped bank is packed, decompress it with a port of `edDataLZ77Unpack` and diff against an in-engine memory dump taken after `unpack()`.
