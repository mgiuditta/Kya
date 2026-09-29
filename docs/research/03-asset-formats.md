# 03 - Asset formats (as read by the code)

## TL;DR

1. Almost every game asset lives inside a **BNK bank**: `KNAB` magic at file offset 8, a 0x3C-byte `edCBankFileHeader`, a 16-byte-per-entry table (absolute payload offsets), a per-entry `(type, stype)` pair that picks the install callback, a compressed name tree for lookups by path, and optional LZ77 packing.
2. **G2D** (textures, `.G2D` magic) and **G3D** (meshes, `.G3D` magic) share a 16-byte `GXD_FileHeader` followed by nested 16-byte `ed_Chunck` FourCC chunks. Stored offsets are turned into pointers at load time by the `REAA` chunk (`REAL`/`RSTR`/`ROBJ`/`RGEO`). Chunks are linked by 8-byte name hashes (`HASH`/`MBNK` tables).
3. **Collision** (`type 7`) has no FourCC. It is a raw `StaticCollisionEntry` whose file-relative offsets are patched into an OBB tree of triangles, quads, boxes and spheres. Dynamic collision entries carry a `.COLCOLI` tag.
4. **Levels:** `CDEURO/Level/Info/levels.bnk` holds one record per level (version 9 only): name, bank sizes, teleporters/elevators, sectors with scenario conditions, and objectives. Each level folder then has `LevelIOP.bnk` (audio), `Level.bnk` (scene, shared meshes/textures, anims, collision, cameras, events and so on), `SECT<n>.bnk` (one per sector) and `Cine/<hash>.bnk`.
5. **KyaBank** (list/extract `.BNK`, G2D→PNG, G3D→glTF) is an **external** repo (https://github.com/Icey1717/KyaBank), linked from `README.md:26`. It is not a submodule and not in `tools/`. It reuses the same `edBank`/`KyaMesh`/`KyaTexture` code. `src/EdenLib/edBank` and `edFile`, `port/KyaMesh` and `port/KyaTexture` are **uninitialized submodules** in this checkout.

---

## 0. Sources and conventions

- In-repo code is cited as `path:line`.
- The `src/EdenLib/edBank`, `src/EdenLib/edFile`, `port/KyaMesh` and `port/KyaTexture` submodules are **not checked out** (`git submodule status` shows `-` for all four; the directories are empty). Their headers exist in `src/EdenLib/Include/edBank/*` and `src/EdenLib/Include/edFile/*`. The `.cpp` sources were read by cloning the pinned commits from GitHub into a scratch directory. They are cited as **[ext]** with a GitHub URL at the pinned commit:
  - edBank `d2b70d3`: `https://github.com/Icey1717/edBank/blob/d2b70d3b41a70733df1854ac20aa51f435e749a7/…`
  - KyaTexture `263737b`, KyaMesh `aa0bcce`, KyaBank HEAD `4ce2f8b`.
- **[web]** marks claims taken from community websites, not from code.
- All data is little-endian 32-bit, since the target is PS2 (EE). Pointer-sized fields in file structs are declared `strd_ptr(T)`, which is an `int` on disk (`src/port/pointer_conv.h:21`). On PC they are converted with `STORE_POINTER` / `LOAD_POINTER` (`src/port/pointer_conv.h:37-40`).
- There is **no file called `INFO.BIN` in the code**. The code loads `Info/levels.bnk` (`src/b-witch/LevelScheduler.cpp:601`, `:926`). A community site says the per-level records are `/CDEURO/LEVEL/INFO/<level name>.BIN` files **[web]** (https://kyadlfiles.github.io/technical/disc_files). These are most likely the entries packed inside `levels.bnk` (see §5).

---

## 1. BNK bank archives (`edBank`)

### 1.1 Where things live

| Piece | Location |
|---|---|
| Header structs | `src/EdenLib/Include/edBank/edBankFile.h:15-82` |
| Buffer/entry classes | `src/EdenLib/Include/edBank/edBankBuffer.h:19-98` |
| Async load queue | `src/EdenLib/Include/edBank/edBankStackFile.h:9-29` |
| `<BNK>` virtual filer | `src/EdenLib/Include/edBank/edBankFiler.h:11-22`, `:24-39`, `:536-537` |
| Parsing code | [ext] `edBank/sources/edBankFile.cpp`, `edBankBuffer.cpp`, `edBankStackFile.cpp`, `edBankFiler.cpp` |
| Build list | `CMakeLists.txt:457-471` |

### 1.2 On-disk layout

The file begins with an 8-byte tag (`FILE_DATA_TAG_SIZE 0x8`, `edBankBuffer.h:17`). `edCBankFileHeader` starts at file+8 (`edCFiler_Bnk_static_header { char header[8]; edCBankFileHeader fileHeader; }`, `edBankFile.h:77-80`). The loaders set `pFileHeader = pFileBuffer + FILE_DATA_TAG_SIZE` ([ext] `edBankBuffer.cpp#L98`, `#L195`).

Neither edBank nor the game checks the magic. The in-repo music inspector requires `data[8:12] == b"KNAB"` (`python/inspect_music.py:17`), and the audio notes say "Archives use `KNAB` at byte 8" (`port/Audio/MusicPlaybackImplementationPlan.md:75`). What the first 8 bytes of the file contain is **unknown**.

`edCBankFileHeader` (size 0x3C, `edBankFile.h:38-82`). "H+x" is the offset from the header; "F+x" is the file offset.

| H+ | F+ | Field | Meaning (from use) |
|---|---|---|---|
| 0x00 | 0x08 | `char header[4]` | `KNAB` (per inspector) |
| 0x04 | 0x0C | `flags_0x4` | bit0 = LZ77-packed ([ext] `edBankFile.cpp#L113`; `edBankStackFile.cpp#L31`) |
| 0x08 | 0x10 | `field_0x8..b` | unknown |
| 0x0C | 0x14 | `FileTypeData typeData {u16 stype; u16 type;}` | if non-zero, the bank is **homogeneous** and every entry has this type. If both are zero, per-entry pairs are used ([ext] `edBankFile.cpp#L388-L397`, `is_heterogeneous` `#L402`) |
| 0x10 | 0x18 | `sizePacked` | overwritten with the on-disk file size after load ([ext] `edBankStackFile.cpp#L47-L51`). Subtracted from the buffer fill level on close ([ext] `edBankBuffer.cpp#L494`) |
| 0x14 | 0x1C | `sizeUnpacked` | not read in the decompiled code |
| 0x18 | 0x20 | `field_0x18..1b` | unknown |
| 0x1C | 0x24 | `field_0x1c` | payload **alignment** used when unpacking (`~(a-1) & (size + a-1)`, [ext] `edBankFile.cpp#L162-L171`) |
| 0x20 | 0x28 | `fileCount` | number of entries |
| 0x24 | 0x2C | `field_0x24..27` | unknown |
| 0x28 | 0x30 | `fileHeaderDataOffset` | entry table offset, relative to H (file offset = 8 + value) |
| 0x2C | 0x34 | `fileTypeDataOffset` | per-entry type-pair table, relative to H |
| 0x30 | 0x38 | `field_0x30` | offset (relative to H) of the **filename tree** |
| 0x34 | 0x3C | `field_0x34` | offset of the tree-order → entry-index map (mode 0) |
| 0x38 | 0x40 | `field_0x38` | offset of a second index map (mode 1, never used by the game code seen) |
| (0x3C) | 0x44 | *(unnamed)* | size of the header + TOC region. `edBankFilerReadHeader` reads `*(u32*)(buf+0x44)` and loads that many bytes as the "header only" copy ([ext] `edBankFile.cpp#L49-L71`) |

**Entry table** (`FileHeaderFileData`, 0x10 bytes, `edBankFile.h:26-36`): `{ int offset; int size; u8 field_0x8[4]; int crc; }`. The code proves that `offset` is **absolute from the start of the file**: data = `header + offset - 8` ([ext] `edBankFile.cpp#L444-L457`; KyaBank uses the same maths, [ext] `KyaBank/src/main.cpp#L120`). `crc` is copied out by `get_info` but never verified ([ext] `edBankBuffer.cpp#L357-L358`). In packed banks the table has `fileCount+1` entries, because unpack uses `offset[i+1]-offset[i]` as the packed length and the final loop runs `<= fileCount` ([ext] `edBankFile.cpp#L136`, `#L239`).

**Type-pair table:** `fileCount × {u16 stype; u16 type}` at H + `fileTypeDataOffset` ([ext] `edBankFile.cpp#L392`). The inspector reads the same two values as `<HH` = (subtype, category) (`python/inspect_music.py:27`).

**Index map** (`get_index`): an array of u8, u16 or u32. The width depends on `fileCount` (<0x100, <0x10000, otherwise) ([ext] `edBankFile.cpp#L336-L373`).

**Filename tree** (`TreeInfo_*`, [ext] `edBankFile.cpp#L537-L709`): a recursive, prefix-compressed directory tree.
- A signed length byte. Negative means a directory node: `-len`, with `0x80` meaning an extended length (next byte added). It is followed by a 7-bit varint child count, the name bytes, then the children.
- A non-negative value means a leaf: the length (`0x7F` = extended), then the name bytes.
- Leaves are numbered in DFS order. That number goes through the index map to get the entry index.
- Before searching, names are rewritten "extension first" (`arrow_pressed.g2d` → `g2d\arrow_pressed`, `TreeInfo_OptimizeFilePath`, `#L750-L796`). The comparison is case-insensitive (`& 0xDF`, `#L616`). The path is rebuilt backwards and un-rotated by `TreeInfo_UnOptimizeFilePath` (`#L711`).
- Stored paths are full build paths such as `D:\PROJECTS\B-WITCH\RESOURCE\BUILD\LEVEL_2\install.bin` ([ext] KyaBank `README.md#L86-L90`, **[web/ext]**).

**LZ77** (`edDataLZ77Unpack`, [ext] `edBankFile.cpp#L244-L284`): an 8-bit flag byte, LSB first. A set bit is a literal byte. A clear bit is a 2-byte back-reference: `len = (b0 & 0xF) + 3`, `dist = ((b1<<8 | b0) >> 4) + 1`. `unpack()` decompresses entries in place, back to front, and then rewrites the entry offsets (`#L93-L242`).

### 1.3 Runtime lookup and loading

- `edCBankBuffer::initialize(size, nEntries, install)` allocates one 0x800-aligned block. It holds the entry array followed by `size` bytes of file space. If `install.fileFlagA & 1`, it first reads only the header via `edBankFilerReadHeader` (a "bank of bank") ([ext] `edBankBuffer.cpp#L19-L103`).
- `get_free_entry()` → `load(install)` → `file_access()`. This opens the file (open flag 9 if async bit 4 is set, else 1), checks it fits, reads the **whole file** into the buffer, and pushes an `edCBankStackElement` onto `edBankStack` (12-slot ring) ([ext] `edBankBuffer.cpp#L112-L246`, `edBankStackFile.cpp#L66-L85`).
- When the read completes, `edBankStackFileCallBack` (hooked to `ED_HANDLER_FILE_READ`) unpacks the data if needed. Then, **unless install flag bit 8 is set**, it calls `fileFunc(false)`, `apply_callback(cb, 0)` and `fileFunc(true)` ([ext] `edBankStackFile.cpp#L13-L63`). With bit 8 set, installation is deferred until `edCBankBufferEntry::install()` ([ext] `edBankBuffer.cpp#L421-L458`).
- `apply_callback` walks every entry. It finds the first `edCBankCallback {type, stype, pFunction[6]}` (`edBankFile.h:20-24`) that matches the entry's pair, and calls `pFunction[mode](data, size)`. Mode 0 is install and mode 5 is unload (`close()`, [ext] `edBankBuffer.cpp#L496`). The table is terminated by `type == 0xFFFFFFFF` ([ext] `edBankFile.cpp#L473-L535`).
- By name: `get_index(name)` → `get_entryindex_from_filename` (tree search) → `get_info(i, edBANK_ENTRY_INFO*)` returns `{type, stype, size, crc, fileBufferStart}` (`edBankBuffer.h:29-36`; [ext] `edBankBuffer.cpp#L326-L402`). Example: menu icons and the font come from `CDEURO/menu/MenuData.bnk` (`src/b-witch/BootData.cpp:21`, `:87-128`).
- `<BNK>` filer: `edFileMount("<BNK>0:", "CDEURO/menu/Messages.bnk")` (`src/b-witch/kya.cpp:1310-1325`, `:1397-1398`, `:1518`). It keeps the bank header resident (`BnkUnitTable[4]`), so `"<BNK>0:UserInterface_%s.bin"` opens a single entry as a stream (seek to `entry.offset`) ([ext] `edBankFiler.cpp#L161-L267`, `#L291-L345`; used at `src/b-witch/LocalizationManager.cpp:30-31`).

Install flag bits (`edCBankInstall::fileFlagA`, `edBankBuffer.h:19-27`):
- bit0: header-only / bank-of-bank.
- bit2 (4): async no-wait. Cleared if `_edBankAllowNoWait == 0` ([ext] `edBankBuffer.cpp#L43-L45`).
- bit3 (8): defer callbacks.

Game usage:
- `LevelIOP.bnk`: flag 4 (`LevelScheduler.cpp:2795`).
- `Level.bnk`: flag 0xC, installed later by `Level_Install` (`:2840`, `:2852-2860`).
- Sectors: 0xC or 4 (`SectorManager.cpp:782-787`).

### 1.4 Type/stype registry (what each bank entry is)

From `TableBankCallback[24]` (`src/b-witch/LevelScheduler.cpp:2721-2746`) plus sector-bank dispatch (`src/b-witch/SectorManager.cpp:580-621`) and the music tool (`python/inspect_music.py:31`):

| type | stype | Content | Handler |
|---|---|---|---|
| 2 | 1 | animation (`ANM!`/`DATA`) | `BnkInstallAnim` → `edAnmAnim::LoadFromMem` (`LevelScheduler.cpp:2552`) |
| 2 | 2 | anim macro table | `BnkInstallAnimMacro` (`:2694`) |
| 3 | 1 | sound sample (VAG) | `BnkInstallSample` (`:2566`) |
| 3 | 4 | sound config (ByteCode) | `BnkInstallSoundCfg` → `CAudioManager::Level_AddAll` (`:2418`, `Audio.cpp:1071`) |
| 3 | 5 | music song (`IECSsreV…`) | `BnkInstallSong` (`:2650`) |
| 3 | 6 | music bank body (ADPCM) | `BnkInstallBank` (`:2597`) |
| 3 | 7 | music bank header (`IECSdaeH…`) | `BnkInstallBankHeader` (`:2615`) |
| 4 (`BANK_CALLBACK_MESH`) | 1 | G3D mesh | `BnkInstallG3D` (`:2441`). In sectors: sector mesh (`SectorManager.cpp:614-621`) |
| 4 | 2 | background G3D (sector bank) | `SectorManager.cpp:593-601` |
| 5 (`BANK_CALLBACK_TEXTURE`) | 1 | G2D texture | `BnkInstallG2D` (`:2427`). In sectors: sector texture (`SectorManager.cpp:608-612`) |
| 5 | 3 | background G2D (sector bank) | `SectorManager.cpp:587-591` |
| 6 | 1 | scene (ByteCode) | `BnkInstallScene` (`:2363`) |
| 6 | 2 | scene config (ByteCode) | `BnkInstallSceneCfg` (`:2386`) |
| 7 | 1 | static collision | `BnkInstallCol` (`:2465`). In sectors: `InstallColFile` (`SectorManager.cpp:603-606`) |
| 7 | 2 | dynamic collision library | `BnkInstallDynCol` (`:2671`) |
| 8 | 1 | events | `BnkInstallEvents` → `edEventAddChunk` (`:2479`, `EventManager.cpp:1129`) |
| 9 | 1 | FX config | `BnkInstallFxCfg` (`:2530`) |
| 9 | 2 | particle manager | `BnkInstallParticleManager` (no-op, `:2689`) |
| 0xA | 1 | track | `BnkInstallTrack` (`:2736`) |
| 0xB | 1 | cameras | `BnkInstallCameras` (`:2502`) |
| 0xC | 1 | **unknown** (registered with NULL handler) | `:2738` |
| 0xF | 1 | A* pathfinding | `BnkInstallAstar` (`:2704`) |
| 0x10 | 1 | lights | `BnkInstallLights` (`:2517`) |
| 0x11 | 1 | liptracks | `BnkInstallLiptracks` (stub, `:2715`) |
| 0x13 | 1 | cinematic list | `BnkInstallCinematic` (`:2541`) |
| 0x17 | 1 | sector animated hierarchy (`MeshData_ANHR`) | `SectorManager.cpp:581-585`, `ed3D.h:307-316` |

The frontend bank registers only types 4 and 5 (`src/b-witch/FrontEndBank.cpp:53-58`). `levels.bnk`, map, cine and pause banks use empty callback tables and read entries by index or name (`LevelScheduler.cpp:600`, `MapManager.cpp:2427`, `CinematicManager.cpp:1292`, `Pause.cpp:3025`).

### 1.5 KyaBank extractor

- It is **not** in this repo, its submodules or `tools/`: `rg -i kyabank` only matches `README.md:26`.
- `README.md:26` links https://github.com/Icey1717/KyaBank, and `README.md:27-28` link edBank and edFile.
- **[ext]** KyaBank (HEAD `4ce2f8b`) has submodules edBank, edFile, KyaTexture, KyaMesh, TextureUpload, tinygltf, lodepng and argparse. Commands:
  - `list` and `extract [-o] [--flatten]`: allocates a 0x3200000 bank buffer and loops `get_index`/`get_entry_filename`/`get_entry` ([ext] `KyaBank/src/main.cpp#L39-L158`).
  - `texconvert` (G2D→PNG) and `meshconvert` (G3D→glTF/GLB) ([ext] `KyaBank/README.md#L25-L65`).
- `port/Audio/music_render.cpp` (`KyaMusicRender LEVELIOP.BNK …`, `port/Audio/music_render.cpp:25`) and `python/inspect_music.py` are in-repo, read-only BNK readers for audio entries only. They reject compressed banks (`python/inspect_music.py:20-21`).

**Unknown:** the first 8 file bytes, H+0x08, 0x18, 0x24, the mode-1 index map, and whether `crc` is CRC32 (edFile has `edFileCRC32.h`, but no caller in bank code was seen).

---

## 2. G2D / G3D shared container

**`GXD_FileHeader`** (0x10, `src/b-witch/ed3D.h:119-128`): `{u16 field_0x0; u16 field_0x2; u32 flags; int field_0x8; u32 hash;}`.
- `hash` is `.G2D` (`0x4432472e`, `HASH_CODE_T2D`, `ed3D.h:31`) or `.G3D` (`0x4433472e`, `ed3D.cpp:13330`).
- Flag bit0 means "prepared". The unprepared G2D path is `IMPLEMENTATION_GUARD` (`ed3D.cpp:12662-12675`), and G3D chooses its code path on it (`ed3D.cpp:13371`, `:13408`).
- A synthetic header uses `{1, 0, flags=5, 0x10, .G2D}` (`src/b-witch/edDlist.cpp:583-587`).

**`ed_Chunck`** (0x10, `ed3D.h:100-117`): `{u32 fourcc; i16 field_0x4; i16 field_0x6; i32 size; i32 nextChunckOffset;}`.
- `size` covers the header plus all children. Siblings are walked with `edChunckGetFirst/Next` (`ed3D.cpp:300-319`).
- `nextChunckOffset` is really the "own header+payload before children" size: 0x10 for pure containers, equal to `size` for leaves (`edDlist.cpp:589-637`). `edHashcodeGet` uses it to count hash entries (`ed3D.cpp:12870-12883`).
- `field_0x4 == 0x7B6` is used as a runtime "relocated" marker on `REAA` (`ed3D.cpp:12530-12537`).

**`ed_hash_code`** (0x10, `ed3D.h:91-98`): `{Hash_8 hash; strd_ptr pData; pad[4];}`.
- `Hash_8` is 8 bytes (`src/b-witch/Types.h:75-81`). It is computed by `ed3DComputeHashCode`: uppercase a–z, then add byte `i` into slot `i & 7` (`ed3D.cpp:15679-15713`). For example, `0x43494d414e5944` = `"DYNAMIC"` (`FileManager3D.cpp:122`).
- `HASH` (`0x48534148`) and `MBNK` (`0x4b4e424d`) chunks are sorted arrays of these, searched by binary search (`ed3D.cpp:12834-12883`).

**Pointer relocation** (`ed3DPreparePointer`, `ed3D.cpp:12494-12619`):
- An optional `INFO` chunk holds ASCII text (logged at `:12518-12521`).
- `REAA` holds sub-chunks `REAL`, `RSTR`, `ROBJ` and `RGEO`, each an array of u32 offsets of fields to patch. `REAL`: file→file. `RSTR`: field in the second buffer (B) → file. `ROBJ`: field in the file → B. `RGEO`: B→B (`:12555-12610`).
- Relocation targets are relative to `fileStart = body - 0x10`, which is the `GXD_FileHeader` (`:12507`).

### 2.1 G2D (textures)

- Owner: `ed3DInstallG2D` (`ed3D.cpp:12680-12731`) → `ed3DPrepareG2DManageStruct` (`:12429-12485`) → `ed3DPreparePointer` → `ed3DPrepareG2D` (`:12649`). Result struct: `ed_g2d_manager` (`ed3D.h:150-176`).
- Top-level chunks: `*2D*` (`0x2a44322a`) container, `ANMA`, `PALL`, `T2DA` (`ed3D.cpp:12425-12427`, `:12442-12480`). Inside `*2D*`: `MATA` → `HASH`, then `MAT.` chunks.
- Nesting, as reconstructed by `ed3DG2DDuplicateMaterial` (`src/EdenLib/ed3D/sources/ed3DG2D.cpp:279-375`) and `edDlistFrameBufMaterialInit` (`src/b-witch/edDlist.cpp:579-646`):
  - `*2D*` › `MATA` › `HASH` (material hash → `MAT.`) › `MAT.` + `ed_g2d_material`
  - `LAYA` › `LAY.` + `ed_g2d_layer`
  - `TEXA` (`0x41584554`) › `TEX.` + `ed_g2d_texture` + N palette `ed_hash_code` (+ optional anim ST vec4)
  - `T2DA` › `T2D.` + `ed_g2d_bitmap`
  - `PALL` › `PA32` palette chunks (`ed3DG2D.cpp:357`)
- Structs (`ed3D.h`):
  - `ed_g2d_material` (packed, `:418-428`): `u8 nbLayers; u8 _; u16 flags; ptr pDMA_Material; ptr pCommandBufferTexture; int commandBufferTextureSize; int aLayers[4]` (pointers to `LAY.` chunks).
  - `ed_g2d_layer` (`:436-464`): `u32 flags_0x0, flags_0x4; … byte field_0x1b @0x1B; i16 bHasTexture @0x1C; u16 paletteId @0x1E; ptr pTex @0x20` (→ `TEX.` chunk).
  - `ed_g2d_texture` (`:475-482`): `ed_hash_code hashCode` (pData → hash entry → `T2D.` bitmap); `int bHasPalette @0x10`; `ptr pAnimSpeedNormalExtruder @0x14`; `float animSTMaxDist @0x18`; `ptr pAnimChunck @0x1C`; followed by `bHasPalette` palette hash codes.
  - `ed_g2d_bitmap` (`:467-473`): `u16 width, height, psm, maxMipLevel; ptr pPSX2`. `pPSX2` points to a pre-built GS upload DMA/GIF packet list; the pixel data is referenced from inside that packet.
- Lookup helpers: `ed3DG2DGetMaterialFromIndex`, `ed3DG2DGetG2DMaterialFromIndex`, `ed3DG2DGetBitmapFromMaterial` (`src/EdenLib/ed3D/sources/ed3DG2D.cpp:6-139`), `ed3DG2DGetG2DNbMaterials` (`src/EdenLib/ed3D/sources/ed3DG3D.cpp:250-264`, count = `(size-0x10)>>4`).
- PC decoding: [ext] `KyaTexture/src/Texture.cpp`. It walks `pMATA_HASH` → materials → layers → texture → bitmap/palette (`#L227-L376`). It then parses the upload packet for `BITBLTBUF`/`TRXPOS`/`TRXREG` and image pointers (`#L137-L193`), and the material render commands for `TEX0`/`CLAMP`/`TEST`/`ALPHA` (`#L24-L126`). Pixels go through `TextureUpload` (PCSX2 GS local memory) to RGBA8 ([ext] `KyaTexture/src/Texture.h#L8-L16`).
- **Unknown:** the purpose of `ANMA` in G2D (anim), the `PALL`/`T2DA` sibling order on disk, most `ed_g2d_layer` bytes 0x08-0x1A, and material `flags`. Read `ed3DPrepareMaterial` (`ed3D.cpp:12020`) and `MakeTextureDataFromPacket` (`src/b-witch/edDlist.h:109`) next.

### 2.2 G3D (meshes)

- Owner: `ed3DInstallG3D` (`ed3D.cpp:13332-13420+`) → `ed3DPrepareG3DManageStruct`/`ProcessG3DChunck` (`:12734-12832`) → `ed3DPreparePointer` → `ed3DPrepareG3D` (`:13300-13328`). Result struct: `ed_g3d_manager` (`ed3D.h:130-148`).
- Top-level FourCCs (`ed3D.cpp:101-112`): `OBJA`, `LIA.`, `CAMA`, `SPRA`, `HALL`, `CSTA`, `GEOM`, `MBNA`, `INFA`, `CDZA`, `ANMA`.
- **HALL** (object hierarchy): `HALL` › `HASH` (one `ed_hash_code` per node → `HIER` chunk, `0x52454948`) (`ed3D.cpp:13266-13298`). Each `HIER` payload is `ed_g3d_hierarchy` (0xB0 + LOD array, `ed3D.h:343-366`):
  - `transformA @0x00`, `transformB @0x40`, `Hash_8 hash @0x80`
  - `u16 bRenderShadow @0x8A`, `ptr pShadowAnimMatrix @0x8C`, `ptr pLinkTransformData @0x90` (parent), `ptr pTextureInfo @0x98`
  - `u16 lodCount @0x9C`, `u16 flags_0x9e @0x9E` (bit 2 = prepared)
  - `ptr pHierarchySetup @0xA0`, `ptr pAnimMatrix @0xA8`
  - `ed3DLod aLods[]` @0xB0, each `{ptr pObj; i16 renderType; i16 sizeBias}` (0x8, `ed3D.h:276-283`)
  - `pObj` → `OBJ.` chunk (`0x2e4a424f`) with `ed_g3d_object` (`ed3D.h:688-703`: `stripCount @0x4`, `boundingSphere @0x8`, `p3DData @0x1C` → strip or sprite list).
  - The `BONI` (`0x494e4f42`) sub-chunk after a HIER is an unimplemented path (`ed3D.cpp:13222-13233`).
- **Strips:** `ed_3d_strip` (0x40, `ed3D.h:593-614`):
  - `flags; i16 materialIndex; i16 cachedIncPacket; int vifListOffset; ptr pNext`
  - `vec4 boundingSphere @0x10`
  - `ptr pSTBuf @0x20, pColorBuf @0x24, pVertexBuf @0x28, pNormalBuf @0x2C`
  - `i16 shadowCast/Receive @0x30`, `DMA_Matrix @0x34`, `u8 primListIndex @0x39`, `i16 meshCount @0x3A`, `ptr pBoundSpherePkt @0x3C`
  - Sprites use `ed_3d_sprite` (`ed3D.h:617-636`).
- **CSTA** (static clusters for level geometry): `CSTA` › `CDQA` (quadtree) › `CDQU` chunks, or `CDOA` (octree) › `CDOC` chunks (`ed3D.cpp:13120-13191`, `ed3D.h:17-20`). Each is followed by `ed_g3d_cluster` (0x40, `ed3D.h:201-223`):
  - `u16 aClusterStripCounts[13]`, `u16 clusterHierCount @0x1A`, `flags @0x1C`, `spriteCount @0x1E`
  - `ClusterDetails {pXYZW,pWH,pRGBA,pNORMAL,pMBNK} @0x20`
  - `p3DStrip @0x38`, `p3DSprite @0x3C`
- **MBNA › MBNK:** the material bank. It is an array of `ed_hash_code` whose hashes are resolved against the G2D `MATA/HASH` by `ed3DPrepareMaterialBank` / `ed3DLinkG2DToG3D` (`ed3D.cpp:12912-12953`, `:13193-13201`). This is how a G3D binds to a separately loaded G2D. In level banks, the pairing of mesh index and texture index comes from the scene stream (`FileManager3D.cpp:100-146`).
- **GEOM:** geometry block. When `(header.flags & 1) == 0`, it is copied out and relocated separately (buffer B, `ed3D.cpp:13371-13394`, an `IMPLEMENTATION_GUARD` path).
- **ANHR** (type 0x17 bank entry, not a chunk inside G3D): `MeshData_ANHR {u32 hash; u32 _; u32 nb3dHierarchies; u32 fileDataEntryCount}` followed by per-hierarchy pointers to `S_HIERANM_HIER` (`ed3D.h:307-341`, `SectorManager.h:28`, used at `SectorManager.cpp:698-711`).
- PC mesh traversal: [ext] `KyaMesh/src/Mesh.h#L23-L97` (`G3D::ProcessHALL`, `ProcessCSTA`, `ProcessCluster`, `Hierarchy→Lod→Object→Strip`).
- **Unknown:** the payloads of `LIA.` (lights), `CAMA` (cameras), `SPRA`, `INFA`, `CDZA` and G3D `ANMA`, which are stored in the manager but never parsed in the decompiled code. Also the exact VIF/vertex encoding inside strips (read `ed3DPrepareCluster` `ed3D.cpp:12998` and the renderer's strip conversion in KyaMesh `Strip::PreProcessVertices`).

---

## 3. Collision (`type 7`)

### 3.1 Static collision (stype 1)

- Entry points: `BnkInstallCol` (`LevelScheduler.cpp:2465-2477`) and `CCollisionManager::InstallColFile` (`CollisionManager.cpp:426-434`) → `edColLoadStatic` (`src/EdenLib/edCollision/sources/edCollisions.cpp:95-249`). (`edLoadCollisions.cpp` is an empty stub, `src/EdenLib/edCollision/sources/edLoadCollisions.cpp:1-4`.)
- Layout: `StaticCollisionEntry` (`src/EdenLib/Include/edCollision/edCollisions.h:360-376`). A 0x10 header, whose `field_0x8` is the size and matches `*(int*)(file+8)` (`edCollisions.cpp:113-116`), followed by `edColG3D_OBB_TREE` (`edCollisions.h:317-358`):

| off | field |
|---|---|
| 0x01 | `bLoaded` (set to 1 after relocation) |
| 0x08 | `nbTriangles` |
| 0x10 | `nbQuads` |
| 0x20 | `field_0x20` = number of OBB tree nodes |
| 0x28 | `field_0x28` → vertex array (vec4, 0x10 stride) |
| 0x2C | `aTriangles` (`edF32TRIANGLE4` 0x10: 3 vertex indices + flags, `edCollisions.h:22-38`) |
| 0x30 | `field_0x30` (type-5 prims, 0x18 stride) |
| 0x34 | `aQuads` |
| 0x38 | `aBoxes` (`edColPRIM_BOX` 0x90) |
| 0x3C | `aSpheres` (`edColPRIM_SPHERE` 0x90) |
| 0x44 | `pObbTree` (`edObbTREE` array, 0x70 each) |

- All pointers are **file-relative** offsets. Vertex indices in triangles and quads are multiplied by 0x10 (`edCollisions.cpp:122-187`).
- `edObbTREE` (`src/EdenLib/Include/edCollision/OBBTree.h:11-36`): `edObbBOX bbox` (4x4 matrix + width/height/depth), `u8 type @0x51`, `u8 count_0x52`, `ptr field_0x54[7]`. `type` is one of `COL_TYPE_TREE 1`, `TRIANGLE 4`, `5`, `QUAD 8`, `BOX 0xA`, `SPHERE 0xB` (`edCollisions.h:10-18`). For tree nodes, the children are node indices. For leaves, `field_0x54[0]` is the primitive index (`edCollisions.cpp:189-230`).

### 3.2 Dynamic collision (stype 2)

- `BnkInstallDynCol` registers the blob (`LevelScheduler.cpp:2671-2687`). `CCollisionManager::InstanciateDynCol` relocates each instance (`CollisionManager.cpp:302-424`).
- `DynColEntry` (`CollisionManager.h:136-146`): `int[3]; char szTag[8] == ".COLCOLI" @0x0C` (asserted, `CollisionManager.cpp:323-328`), followed by `edColG3D_OBB_TREE_DYN` @0x20 (0x3C, `edCollisions.h:290-315`).
- Offsets here are relative to the entry, not the file. Node types `0xD` (BOX_DYN) and `0xE` (PRIM_OBJ) index 0x150-byte `edColPRIM_OBJECT` (`edCollisions.h:239-288`).
- `edObbTREE_DYN` is 0xC0 (`OBBTree.h:38-45`).

**Unknown:** static header bytes 0x0-0x7 and 0xC-0xF (possibly a `.COL` tag like the dynamic one; not checked in code), `field_0x30`/type 5 primitive semantics, and `edColINFO` material/flag meaning. Read `edColConvertToTriangle4Fast` (guarded) and `CCollisionManager::Level_Create/Level_AddAll` (`CollisionManager.cpp:111`, `:256`).

---

## 4. Other formats found in loaders

### ByteCode streams (scene, config, cameras, lights, events, cinematics, A*, FX)

- `ByteCode::Init` treats bytes 0-3 as an ID and bytes 4-7 as the total size, and starts reading at +8 (`src/b-witch/MemoryStream.cpp:118-129`).
- `GetChunk()` skips 8 bytes, returning the first u32 (`:20-28`). Strings are NUL-terminated and 4-byte aligned (`:30-43`).
- Scene (6/1) is consumed in a fixed order (`LevelScheduler.cpp:2370-2381`): `C3DFileManager` → `CScene::Level_Setup` → WayPoint → Path → Collision → Actors → Sectors.
  - `C3DFileManager::Level_AddAll` reads `hasLevelMesh, meshIdx, texIdx, _` and `bgMeshIdx (-1 = none), bgTexIdx`. The indices refer to the order of type 4/5 entries in `Level.bnk` (`FileManager3D.cpp:91-149`).
  - Actors: `count`, then per actor `chunk(8) + actorIndex + classId + class-specific Create()` (`ActorManager.cpp:131-173`).
  - Sectors: `count`, then 0x24-byte records (`+4` sectorId, `+0xC` fog def, `+0x1C` clip float), each followed by conditions (`SectorManager.cpp:952-988`, `:1133-1140`).
- Scene config (6/2) order: 3 scene values + 2 skipped u32 → `C3DFileManager::Level_Create` (mesh count + skipped per-mesh ints, texture count, …) → Collision → Anim → Sector → `ActorManager::Level_LoadClassesInfo` → FX (`LevelScheduler.cpp:2386-2416`, `FileManager3D.cpp:349-384`).

### Animations (2/1)

- `edAnmAnim::LoadFromMem` walks `{u32 fourcc; u32 size; …}` chunks (`AnimHeaderPacked`, `AnmManager.h:13-17`). It fails if `ANM!` has version ≠ 1.1f, and returns a pointer to the payload of `DATA` as `edANM_HDR` (`AnmManager.cpp:226-244`).
- `edANM_HDR` is `{int count; union{f32,int} ×2; float data[]}` (`Animation.h:85-93`).
- The anim macro (2/2) is `u32 count` followed by data (`LevelScheduler.cpp:2694-2702`).

### Sound

- Samples (3/1) are standard **VAG** files: `SoundFileData { VAGp header; adpcmBlock[] }` (`src/EdenLib/Include/edSound/edSoundPlay.h:49-52`). The loader subtracts the 0x30 header from the length (`LevelScheduler.cpp:2575`).
- Music:
  - Songs are Sony PS2 sequences with chunks `IECSsreV`/`IECSuqeS`/`IECSidiM` and a non-standard MIDI event stream.
  - Banks are `IECSdaeH`/`gorP`/`tesS`/`lpmS`/`igaV`. Bodies (3/6) and headers (3/7) are paired by load order (`port/Audio/MusicPlaybackImplementationPlan.md:80-89`, parser `python/inspect_music.py:35-99`, `port/Audio/edMusicData.*`).
- Streamed audio uses `.VAG` host files (`port/Audio/edSoundStreamService.cpp:237-239`).

### Text / localisation

- `CMessageFile` data (`TRC_%s.bin`, `UserInterface_%s.bin`, per-level `*_xx.bin`; languages `GB FR GE SP IT`) (`LocalizationManager.cpp:30-43`, `TranslatedTextData.cpp:100-106`).
- Layout: `u32 entryCount; u32 preparedFlag;` followed by `entryCount × {u64 key; u32/ptr offset (8-byte slot)}`. Offsets are file-relative and patched in place (`TranslatedTextData.cpp:44-66`). Lookup is a linear search on the 64-bit key (`:266-315`).
- Fonts (`medium.fon`) are installed raw via `edTextInstallFont` (`BootData.cpp:117-128`, struct `src/Rendering/edCTextFont.h:37`).

### Cinematics

- The per-cutscene bank is `<levelPath><level>\Cine\<name>.bnk` (`CinematicManager.cpp:1517-1550`).
- The `.cin` file is a chunk list: `DATA` (edCinematicTag + sources of 0xC each), `RES!` (resource collection: `int count; {u32 flags; int pData; int size}[]`), and `CIN!` (version must be 0x81) (`src/EdenLib/edCinematic/Sources/Cinematic.cpp:8-60`, `CinResCollection.h:8-38`).
- Resource types: Sound 0, MeshModel 1, Animation 2, Scene 3, MeshTexture 4, LipTrack 6, Particle 8, Text 9 (`CinResCollection.h:22-31`). They are loaded via `CCinematic::InstallResource` (`CinematicManager.cpp:1554+`).

### Save games

- The in-memory `CChunk {u32 magic(0x16660666 for root); u32 hash; u32 size; int offset}` tree (`LevelScheduler.h:114-124`, `LevelScheduler.cpp:1122-1143`) uses FourCCs `BSAV BSHD BSCN BGNF BLEV BLHD BLCL BLAC BLCI BLMP BOBJ` (`LevelScheduler.h:42-52`).
- **[web]** On disk, `.dat` saves get a `NEDE` header with 2 checksums and sizes, and `settings.dat` has an `STGS` block (https://kyadlfiles.github.io/technical). The code for this is in `SaveManagement.cpp` and was not verified here.

### Misc

- `BWITCH.INI` (`kya.cpp:953`, `:1431-1435`): `[Router] SetPath` / `AddLevel` (`LevelScheduler.cpp:292-296`, `:2249-2258`), and `Episode_*` keys (`:1192-1215`).
- Movies are `CDEURO/movies/<name>.pss` (MPEG-PS) (`kya.cpp:1709-1710`).
- Other frontend banks: `CDEURO/Frontend/Frontend.bnk` (`FrontEndBank.cpp:5`, `:76`), `maps.bnk` (`Pause.cpp:3093`), and `kyatitle.g2d` loaded directly (`Pause.cpp:484-485`).
- `port/Archive` is a shader pack format only, unrelated to game assets (`port/Archive/archive.h:6-15`).

---

## 5. Level data: levels.bnk, sectors, elevators

### 5.1 Listing

- `CLevelScheduler::Game_Init` sets `levelPath = "CDEURO/Level/"`, which can be overridden by INI `[Router] SetPath`. It reads `AddLevel` as the start level name, calls `Levels_LoadInfoBank()`, then matches `AddLevel` against the loaded `levelName`s (`LevelScheduler.cpp:2247-2292`).
- There are up to 16 levels (`aLevelInfo[0x10]`, loop bounds `:2261-2273`). ID 0x10 means "none" (`:2188`, `:3425`). ID 0xE is started by default (`:2296-2309`).
- **[web]** ID → folder: 0 NATIV, 1-13 LEVEL_1..LEVEL_13, 0xE PREINTRO, 0xF CREDITS (https://kyadlfiles.github.io/technical#starting-level-addlevel).
- `Levels_LoadInfoBank` (`LevelScheduler.cpp:899-1073`) loads `levelPath + "Info/levels.bnk"` into a 64 KiB bank with no callbacks, and iterates the entries. For each entry, `u32 version` is at +0 and **only `case 9` is handled**. The body at +4 is:

```
S_LVLNFO_LEVEL_HEADER_V7   (0x38)            LevelScheduler.h:210-236
S_LVLNFO_TELEPORTERS_V7_V9 [nbTeleporters]   (0x1c each) :197-208
S_LVLNFO_SECTOR_V7_V9      [nbSectors]       (0xc + variable conditions) :190-195
S_LVLNFO_LANGUAGE_V7_V9    [nbLanguageFileNames] (0x1c each) :278-289
char levelPath[] (NUL-terminated, only if field_0x30 >= 1, max 20 chars copied)
```

**Header** (`S_LVLNFO_LEVEL_HEADER_V7`):

| off | field | notes |
|---|---|---|
| 0x00 | `levelId` | 0..15 |
| 0x04 | `levelName[8]` | folder name, e.g. `LEVEL_1` |
| 0x0C | `bankSizeLevel` | buffer size for `Level.bnk` (+0x1000, `:2835`) |
| 0x10 | `bankSizeSect` | |
| 0x14 | `bankSizeIOP` | buffer size for `LevelIOP.bnk` (+0x1000, `:2789`) |
| 0x18 | `hashA`, 0x1C `hashB` | → `titleMsgHash` = text key of the level title (`:608`) |
| 0x20 | `sectorStartIndex` | default start sector |
| 0x24 | `nbSectors` | |
| 0x28 | `field_0x28` | total wolfen count (→ `field_0x20`, distributed at `:892-893`) |
| 0x2C | `nbTeleporters` | |
| 0x30 | `field_0x30` | count of trailing path strings (clamped to 1) |
| 0x34 | `nbLanguageFileNames` | actually objective entries (see below) |

### 5.2 Sectors and links between sectors

- Each `S_LVLNFO_SECTOR_V7_V9 {u32 sectorId; u32 sectBankSize; int nbConditions}` is followed by `nbConditions × { int targetSectorId; ScenaricCondition }` (variable size, `LevelScheduler.cpp:667-800`).
- For `0 < target < 30`, the loader sets bit `target` in `aSectorSubObj[sectorId].flags`. Conditions that have simple sub-conditions are copied into `S_COMPANION_INFO {conditionIdentifier; ScenaricCondition}` (`LevelScheduler.h:184-188`).
- At level start, `CSectorManager::LevelLoading_Begin` clears the bits whose condition fails (`SectorManager.cpp:167-182`). It then computes, per sector, the set of sectors that must be **co-resident** and allocates that many `CSector` slots, ordered by bank size (`:185-328`). `SetupCompanionSectors(flags)` assigns slots (`:451-502`).
- So the "links" are *which additional sector banks stay loaded with sector N*. This matches the community description of area and partial sectors **[web]** (https://kyadlfiles.github.io/technical/disc_files).
- The sector bank is `levelPath + levelName + "/SECT" + n + ".bnk"` (`SectorManager.cpp:151`, `CSector::Load` `:739-799`). Its contents are dispatched by type/stype in `CSector::InstallCallback` (`:528-729`): sector G3D and G2D, background G3D and G2D, collision, and ANHR.
- Per-sector fog and clip come from the scene stream (`SectorManager.cpp:1117-1149`).

### 5.3 Elevators / teleporters

- `S_LVLNFO_TELEPORTERS_V7_V9` (packed 0x1C) is read into `S_SUBSECTOR_INFO aSubSectorInfo[12]` (`LevelScheduler.h:238-252`) by `LevelsInfo_ReadTeleporters_V7_V9` (`LevelScheduler.cpp:846-896`):

| off | → S_SUBSECTOR_INFO | meaning (from use) |
|---|---|---|
| 0x00 | `teleporterActorHashCode` | actor looked up by `GetActorByHashcode` (`:3461-3463`, `:3753-3754`) |
| 0x04 | `field_0xc` | sector to spawn in when arriving by this elevator (`Level_FillRunInfo`, `:339-343`) |
| 0x08 | index | elevator ID (slot), `maxElevatorId = max+1` |
| 0x0C | `field_0x10` | bit0 → `flags \|= 1` (initially open/unlocked?) |
| 0x10 | `nbMaxExorcisedWolfen` | |
| 0x14 | `field_0x0` (u64) | unknown (probably a name/message hash) |

- `Level_Teleport` switches sector within the same level, or calls `Level_FillRunInfo` for a cross-level jump (`LevelScheduler.cpp:3438-3487`). `ManageLoadElevator` updates the `CActorTeleporter` state after a load (`:3747-3765`).

### 5.4 Objective records

- `S_LVLNFO_LANGUAGE_V7_V9` (0x1C) is `{u32 id; u32 keyA; u32 keyB; u32 field_0xc; float x,y,z}`, loaded into `ObjectiveEntry` with `messageKey` and position (`LevelScheduler.cpp:802-844`). The "LanguageFileNames" name is misleading.

### 5.5 Load sequence per level

1. `LevelIOP.bnk` is loaded async with the full callback table (`LevelScheduler.cpp:2765-2801`).
2. Once sound and music are loaded, the IOP bank is freed (`:2822-2829`).
3. `Level.bnk` is loaded async with deferred install (`:2832-2844`).
4. `Level_Install` runs the callbacks (`:2852-2860`).
5. Sector banks are loaded on demand by `CSectorManager`.

**Unknown:** versions other than 9 (the V7 names suggest v7 and v8 existed), `S_LEVEL_INFO::field_0x30`/`levelPath` semantics, and teleporter `field_0x14`. Read `CLevelScheduler::Level_UpdateCurLiveLevelInfo` (`:3511`) and `CActorTeleporter` next.

---

## 6. Suggested next reads

- **BNK packing/crc:** check the edBank `analyse()` stub ([ext] `edBankFile.cpp#L87-L90`) against the binary, and check `edFileCRC32` callers.
- **G3D vertex/VIF data:** `ed3DPrepareCluster` (`ed3D.cpp:12998-13118`) and KyaMesh `Strip::PreProcessVertices`.
- **G2D GS packets:** `ed3DPrepareMaterial` (`ed3D.cpp:12020`) and `MakeTextureDataFromPacket` (`edDlist.h:109`).
- **Collision:** `CCollisionManager::Level_Create` (`CollisionManager.cpp:111`) for how dyn-col banks are sized.
- **Scene ByteCode:** each manager's `Level_AddAll`/`Level_Create` (list in §4) and `CActor::Create` overrides per actor class.
