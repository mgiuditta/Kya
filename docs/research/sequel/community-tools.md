# What community tools and docs already know about Kya formats

Research for [mgiuditta/Kya#26](https://github.com/mgiuditta/Kya/issues/26), part of map [#22](https://github.com/mgiuditta/Kya/issues/22). Surveyed 2026-09-29.

## Answer

The public record is **read-only**. KyaBank lists and extracts `.BNK` archives and converts G2D textures to PNG and G3D meshes to glTF. Nobody has published a BNK writer or repacker, a level or actor placement editor, or a text/localisation tool for Kya. There are no public issues, branches or forks that start one. The only written format documentation is what the decompiled reader code implies, plus one fan wiki (The Kya: Dark Lineage Files). That wiki covers the *layout* of levels and sectors on disc, save files and `BWITCH.ini`. It says outright that the level `INFO/<level>.BIN` format is "yet to be completely reversed and documented". The closest existing BNK **writer** is for a different Eden game, Test Drive Unlimited (TDUF / TDUMT). That BNK format looks related but has not been shown to be the same.

## Sources surveyed

| Source | What it is | License | Maturity / activity |
|---|---|---|---|
| [Icey1717/KyaBank](https://github.com/Icey1717/KyaBank) | CLI: `list`, `extract`, `texconvert` (G2D to PNG), `meshconvert` (G3D to glTF/GLB) | GPL-3.0 in `LICENSE.txt`. The README says "MIT License", which conflicts with the file. Treat it as GPL-3.0. | Releases v0.1.0 (2024-04), v0.2.0 (2024-12), v0.3.0 and v0.3.1 (2025-07-21). Last code commit 2025-07-21; after that, only a glm bump (2026-03). No issues. README says "Actively maintained". |
| [Icey1717/edBank](https://github.com/Icey1717/edBank) | Decompiled `edBank` library (bank reader), used as a submodule of Kya and KyaBank | No license file | Source only (`sources/*.cpp`). Headers live in Kya under `src/EdenLib/Include/edBank/` |
| [Icey1717/edFile](https://github.com/Icey1717/edFile) | Decompiled `edFile` (file I/O, CRC32, path handling) | No license file | Source only |
| [Icey1717/KyaMesh](https://github.com/Icey1717/KyaMesh), [KyaTexture](https://github.com/Icey1717/KyaTexture), [TextureUpload](https://github.com/Icey1717/TextureUpload) | Mesh/texture decoding shared by the port and KyaBank | GPL-3.0 / none / GPL-3.0 | Library code, no docs |
| [Icey1717/Kya](https://github.com/Icey1717/Kya) (upstream) | Decomp and Windows port | GPL-3.0 | No wiki pages (the `/wiki` URL redirects to the repo), Discussions turned off. One open issue (#4, a broken TextureUpload submodule). PRs #1-#3 came from KyaDLFiles (build fixes). #5 is ours. The README points to KyaBank, edBank, edFile and kyadlfiles.github.io. |
| Forks of upstream | [KyaDLFiles/Kya](https://github.com/KyaDLFiles/Kya), [Davide-1998/Kya](https://github.com/Davide-1998/Kya) | GPL-3.0 | Davide-1998 is 1 commit ahead (submodule fix). KyaDLFiles holds its merged build fixes. No tooling work. The [KyaDLFiles/KyaBank](https://github.com/KyaDLFiles/KyaBank) fork is 0 commits ahead of upstream. |
| [The Kya: Dark Lineage Files](https://kyadlfiles.github.io/) ([source](https://github.com/KyaDLFiles/kyadlfiles.github.io)) | Fan wiki: technical info, disc files, cheats, saves, pictures | Content CC BY-NC-SA 4.0 | Last updated 2025-08. Mentions three Discord servers (general, speedrun, decomp/research) that are invite-only through the author and were not surveyed here |
| KyaDLFiles save tools: [kya_dl_gen_save_checksum](https://github.com/KyaDLFiles/kya_dl_gen_save_checksum), [python port](https://github.com/KyaDLFiles/kya_dl_gen_save_checksum_python), [bruteforce_checksum](https://github.com/KyaDLFiles/kya_dl_bruteforce_checksum) | Fix save-file checksums so edited saves load | GPL-3.0 | Small, finished tools (2023-2024) |
| [KyaDLFiles/Kya_DL_cheat_tables](https://github.com/KyaDLFiles/Kya_DL_cheat_tables), [Kya_DL_cheats](https://github.com/KyaDLFiles/Kya_DL_cheats) | Cheat Engine tables and PCSX2 pnach/Cheat Device codes (RAM hacking) | none | 2025 |
| This repo's `docs/` | Renderer, particles, draw inspector, sound handles, sequel specs | GPL-3.0 (repo) | No format docs. The sequel spec already records "no BNK writer, no level editor" ([plot design, section on the playable chapter](../../superpowers/specs/2026-09-29-kya-sequel-plot-design.md)) |
| TCRF ([tcrf.net/Kya:_Dark_Lineage](https://tcrf.net/Kya:_Dark_Lineage)) | Unused content and debug menu page, linked from the KyaDL Files wiki | CC BY 3.0 | Could not be read with automated tools (it blocks bots). KyaDL Files cites it only for a dormant debug menu |

General web searches for "Kya Dark Lineage BNK", and for Kya on Xentax, ResHax and ZenHax, found no other Kya tooling or format threads. GitHub code search for `edCBankFileHeader` finds matches only in Icey1717's repos.

## By format

### BNK (bank archives)

- **Tools:** KyaBank can read and extract only. Its `main.cpp` registers exactly four subcommands: `list`, `extract`, `texconvert`, `meshconvert` ([src/main.cpp](https://github.com/Icey1717/KyaBank/blob/main/src/main.cpp)). There is no pack, repack or write command.
- **Documentation:** there is no prose spec anywhere. Everything known is in the decompiled reader, `src/EdenLib/Include/edBank/edBankFile.h` in this repo:
  - a `0x3C`-byte `edCBankFileHeader` (4-byte tag, flags, a `type/stype` pair, `sizePacked`, `sizeUnpacked`, `fileCount`, offsets to per-file data and type tables). It sits after an 8-byte prefix (`edCFiler_Bnk_static_header`).
  - 16-byte per-file records `FileHeaderFileData { offset, size, ?, crc }` and 4-byte `FileTypeData { stype, type }` pairs.
  - a path tree with "optimised" file paths (`TreeInfo_OptimizeFilePath` / `UnOptimizeFilePath`), optional LZ77 packing (`edDataLZ77Unpack`), and 0x800-byte alignment in `edBankBuffer.cpp` ([edBank sources](https://github.com/Icey1717/edBank/tree/main/sources)). The CRC helper lives in [edFile `edFileCRC32.cpp`](https://github.com/Icey1717/edFile/tree/main/sources).
  - Field names such as `field_0x8`, `field_0x1c` and `field_0x30`-`0x38` are still unknown, so a writer would have to copy them from real files.
- **What is in the banks:** KyaBank's README shows an example listing. Level banks hold original build paths like `D:\PROJECTS\B-WITCH\RESOURCE\BUILD\LEVEL_2\install.bin` plus G2D/G3D assets. KyaDL Files documents that each level is split into `SECTx.bnk` sector banks under `/CDEURO/LEVEL/<LEVEL>/`, and which sector numbers are missing per level ([disc files page](https://kyadlfiles.github.io/technical/disc_files/)).
- **Related Eden format (hypothesis, not verified):** Test Drive Unlimited, a later Eden Games title, also uses `.bnk`. [djey47/tduf-next](https://github.com/djey47/tduf-next) (MIT, last push 2023-05) publishes a spec in [doc/RE/bnk/README.md](https://github.com/djey47/tduf-next/blob/master/doc/RE/bnk/README.md) and a Kaitai [tdu-bnk.ksy](https://github.com/djey47/tduf-next/blob/master/doc/RE/bnk/tdu-bnk.ksy). It has sections for header, sizes, type mapping, tree and order; per-file size entries are 16 bytes in TDU1; each section carries a CRC32 checksum; padding goes to a block size. The spec notes that the TDUMT2 tool includes a **BNK writer**. [djey47/tduf](https://github.com/djey47/tduf) (custom license, last push 2021-04) wraps a "genuine BNK gateway" to unpack and repack TDU banks. The shape is similar to Kya's reader: per-file offset/size/CRC records, type pairs, a path tree. That makes TDU's spec and writer a useful reference, but nobody has checked whether Kya's 2003 banks share the layout. TDU also ships on PC, not PS2, and is at least 3 years newer.

### Level / actor data

- **Documented:** only disc layout and behaviour. The [KyaDL Files disc files page](https://kyadlfiles.github.io/technical/disc_files/) gives the level folder list (NATIV, LEVEL_1-13, PREINTRO, CREDITS), sector counts, area vs partial sectors, and always-loaded sections. It also says that `/CDEURO/LEVEL/INFO/<level>.BIN` controls which sectors load together, and that its "file structure [is] yet to be completely reversed and documented". The [technical page](https://kyadlfiles.github.io/technical/) covers `BWITCH.ini` (`AddLevel` start level, `SetPath`) and RAM addresses for Kya's position.
- **Not documented anywhere public:** the actor/object placement format inside the sector banks. The only description is the decompiled loader code in this repo (`src/b-witch/LevelScheduler.cpp`, `Actor.cpp`, `LargeObject.cpp`).
- **Editors:** none. This repo's debug menu (`port/DebugMenu/`) inspects actors, the camera, collision and meshes at runtime, but it does not write anything back to level files. Its ImGuizmo dependency is used only in the mesh viewer.

### Text / localisation

- **Documented:** nothing in community sources. In this repo, `src/b-witch/LocalizationManager.cpp` loads per-language `.bin` string tables from banks (`<BNK>0:UserInterface_%s.bin`, `<bnk>0:TRC_%s.bin`, level text with a `_xx.bin` suffix), and `src/EdenLib/edText/` parses them.
- **Tools:** none for extracting, editing or rebuilding text.

### Saves (for completeness)

The best-documented format. KyaDL Files gives the header layout (`NEDE` magic, header checksum, two data-block checksums and sizes) and the `settings.dat` fields. It also publishes GPL-3.0 checksum-fixer tools, so edited saves already work.

## Is a writer / repacker / level editor in progress?

No evidence of one anywhere:

- KyaBank has no open issues or branches. Its last feature release (v0.3.x, July 2025) added directory-input handling, not writing.
- Upstream Kya has no wiki, has Discussions disabled, and has no issues or PRs about modding tools.
- The forks are build fixes only.
- Web and GitHub searches find no third-party Kya tooling.
- The Discord decomp/research server could hold unpublished work, but it cannot be reached without an invite from the KyaDL Files author.

## Implications for the sequel modding pipeline

1. BNK writing has to be built by us. The decompiled reader (`edBankFile.h`, `edBankBuffer.cpp`, `edBankFile.cpp`) is the spec. KyaBank's extractor gives round-trip test data. The TDU BNK spec and TDUMT writer are worth reading as prior art for how Eden laid out banks.
2. KyaBank is GPL-3.0, like this repo, so we can fork it and add a `pack` command without license friction. Keep the README's "MIT" line in mind and don't rely on it.
3. Level `INFO/*.BIN`, actor placement and text tables have no external documentation. The decomp code is the only source, so a follow-up ticket should reverse each of them from the loader code.
