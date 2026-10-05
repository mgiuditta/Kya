# Forgotten Island: checkpoints, sectors and cinematics

Input for round 2 of "Map Act I beats onto Forgotten Island" (issue 31). This was captured on the Mac on 2026-10-05 from a `macos/arm64-port` build (86dfc69) with every guard live, except for two research-only bypasses in `ActorShoot.cpp` (see "How this was captured"). Screenshots are in `sectors/`. Raw dumps: `cp_and_cine_L8.txt`, `cp_and_cine_L9.txt`, `text_and_audio_L8.tsv`, `text_and_audio_L9.tsv`.

"Nearby" means within 60 units of the checkpoint waypoint, taken from `actors_L8.tsv`, which holds the spawn positions. Blazer, ShooterDrago and Electrolla only appear as counts.

## Sector shapes (0x8)

- **Sector 1** (y ≈ 1945): a grey ash canyon with high rock walls. You can see sky at the end. There are fire crystals but **no lava pools in view**. Kya's start is here, with the Companion and a hut marked with red glyphs.
- **Sector 2** (y ≈ 800–860): a vertical lava tunnel. Kya falls through it, past crystals.
- **Sector 3** (y ≈ 308): the main volcanic plateau, made of ochre ground paths and islands set in a lava sea. It holds the Wolfen camp (banners, palisades), the Nativ cage, the Rune cave, a grey cave behind an arch, Air Post cannons (`AIRCANON`/DCA) and teleporters. Almost every view shows lava.
- Sectors 4 and 6–9 have no checkpoints (see issue 46).

## Checkpoints, 0x8

| CP | Sector | Description | Notable actors nearby | Lava? |
|---|---|---|---|---|
| 0 | 1 | Level start. A grey ash canyon floor with a fallen log and a hut with red glyphs. Narrow but open to the sky. | **Companion**, Teleporter_L1 | **No** |
| 1 | 1 | Further up the same canyon, a wide grey slope between rock walls, with two fire crystals. | none | **No** (lava only implied in the wall veins) |
| 2 | 2 | Falling through the crystal lava tunnel. Kya is in the air. | none | Yes |
| 3 | 3 | The funnel exit, where Kya drops onto the plateau over a lava pit (the LavaFall cinematic zone). | none | Yes |
| 4 | 3 | The plateau landing: ochre ground, a hut with glyphs, a palisade, a dead tree. Semi-open. | Teleporter, Wolfen SCRAWNY_01, AirCanon_L11, ShooterDrago | Lava out of view |
| 5 | 3 | A wide ochre clearing with a lava sea along the edges. | BonusFountain, HelperSign_01, 2 Stiller, 3 AirCanon | Yes (edges) |
| 6 | 3 | A raised ochre ridge path between dark rock walls. | Commander_L3, Araignos, Micken (trampoline), 2 Wolfen Grunt_01, 8 Blazer, 5 Electrolla | Lava below |
| 7 | 3 | An ochre ledge with fire crystals and a lattice fence. | 3 Araignos, Teleporter_L5, 4 Electrolla | Yes |
| 8 | 3 | The Wolfen camp: a banner with the Wolfen crest, spiked palisades, a lava moat. | **Woof**, 2 Wolfen Grunt_02, 11 Blazer, 2 ShooterDrago | Yes |
| 9 | 3 | A camp gate by a dark cave mouth with a blue portal glow, a Wolfen banner and a ballista. | **HelperSign**, Teleporter_L3, 9 Blazer | Yes (right) |
| 10 | 3 | Above a large violet slab platform in a lava basin (the platform puzzle). | HelperSign, Teleporter_L3 | Yes |
| 11 | 3 | A wooden pier or scaffold over lava, with crates. | 3 AirCanon | Yes |
| 12 | 3 | An **enclosed dark cave room** with a dark-matter swirl, a target door and a crystal. This is the ElectroRune cinematic zone. | **Rune_01**, Commander_01, KRONOS_02, Wolfen Grunt_02 / Scrawny_02_Rocket, 5 Electrolla | **No** (closed cave) |
| 13 | 3 | A long ochre causeway along a rope line, with a lava sea and the horizon on the right. Open. | 3 AirCanon | Yes |
| 14 | 3 | A palisade ridge over a lava gorge, with a hanging cauldron. | 2 Commander, 3 Wolfen (incl. Rocket), 11 Blazer | Yes |
| 15 | 3 | A narrow ochre path near the Nativ cage. The camera is blocked by a fence post. | **4 Nativ** (GRUNTNATIV_01/02, NATIVSCRAWNY_01/02, at 270,318,180), Commander_02, Wolfen Scrawny_02, AirCanon_L1 | Yes |
| 16 | 3 | An ochre path in a dark gorge with hanging cargo baskets on a cable. | none | Lava wall on the right |
| 17 | 3 | A dark plateau edge looking at a lava island with a camp. | BonusFountain, HelperSign_01, 2 Stiller, Teleporter | Yes |
| 18 | 3 | A grey and ochre ledge over lava, with a fence and Wolfen structures behind. | **Woof**, 4 Blazer | Yes |
| 19 | 3 | A narrow ochre bridge path across lava. | Woof, 15 Blazer | Yes |
| 20 | 3 | A hilltop with a large dark dome and a blue portal glow (a teleporter or the machine). | Commander_L3, 3 Araignos, Micken, 2 Wolfen, Teleporter_L5 | Yes |
| 21 | 3 | Violet stepping slabs over open lava. | 2 Commander, KRONOS_02, 3 Wolfen | Yes |
| 22 | 3 | A **grey cave chamber** with a round stone arch looking out on lava, eyes in the dark and a fire crystal. | none | No in the chamber, lava through the arch |

### Lava-free areas

- **Sector 1 (CP 0–1), the grey ash canyon**, is the only open, lava-free area in 0x8, and it is where the Companion starts. It is narrow (a canyon, not a clearing), but it is the best fit for a lava-free open area.
- **CP 12 (the Rune cave) and CP 22 (the grey arch cave)** are lava-free interiors, but enclosed.
- Everything else in sector 3 is ochre ground bordered by lava.

## Checkpoints, 0x9 (red cave, 1 sector)

| CP | Sector | Description | Notable actors | Lava? |
|---|---|---|---|---|
| 0 | 1 | Frank's exorcism arena: a round grey stone floor with a violet exorcism beam, skull-shaped red rock walls and lava falls. | Frank platforms (`*_BEFORE/AFTER_EXORCISM`), KRONOS_BUNCH | Lava falls on the walls |
| 1 | 1 | A teal ledge by a large dark rock, with an "UNLOCK" prompt and a HUD counter at 20/50. | — | No in view |

## Cinematics

"Text" is the number of entries in the cinematic's own text table (`cin_XX_<lang>.bin`). The engine loads it as the cinematic's subtitle source, and lines ending in `6bXX` are alternate variants of the same line. "Stream" is the `.MIB` the cinematic plays, for the GB language. The sector of each trigger zone comes from the centre of the zone's bounding sphere.

### 0x8

| Cinematic | Stream (.MIB) | Text | Trigger zone → sector |
|---|---|---|---|
| ELECTRORUNE_FORGOTTEN_ISLAND (Rune) | 96A2F391 | 0 | (380, 312, −103), sector 3, Rune cave (CP 12) |
| LAVAFALL | B6F55B19 | 0 | (−1, 325, 20), sector 3, funnel exit (CP 3) |
| CIN_28BIS_SC_40BIS StandObus | none | 0 | (−1066, 1947, 1674), sector 1, level start (CP 0) |
| CIN_28BIS_SC_40BIS (cin_28bis) | 112C02F6 | 0 | no zone (started by script) |
| **CIN_28BIS_SC_40** (Brazul and his henchmen) | EF6D9B22 | **9** (`cin_28`) | (54, 309, −30), sector 3, near CP 4 |
| forg_island_TO_machine | none | 0 | (391, 289, −69), sector 3, east of CP 12 |
| Airlift_To_Nativ / Airlift_To_Airlift | none | 0 | level-wide sphere (r ≈ 1400) |
| CIN_KYA_DEATH | none | 0 | no zone |

Level text `lvl08_<lang>.bin` has 14 entries: 7 HelperSign/Companion hints (lava rhythm, collapsing platform, pools, "Direction: Laboratory / Hell's Road", the door) plus 7 empty keys.

### 0x9

| Cinematic | Stream (.MIB) | Text | Trigger zone → sector |
|---|---|---|---|
| BEGIN_LEVEL9 | F30646E0 | 0 | no zone (level start) |
| **CIN_29_SC_41** (Kya finds Frank) | 7200BD90 | **10** (`cin_29`) | (392, 244, −50), sector 1 |
| **CIN_30_SC_42** (after the exorcism) | AEB82DFA | **13** (`cin_30`) | (341, 246, −36), sector 1 |
| **CIN_30BIS_SC_42** | 27CCEA63 | 13 (shares `cin_30`) | (337, 245, 20), sector 1 (CP 1) |
| **CIN_31BIS_SC_44** | none (voice in per-language cine bank `584B38E.{FR,GER,IT,SP,US}`) | **25** (`cin_31`, Nativ City table) | (337, 245, 20), sector 1 |
| FRANK_ATTACK_prepare_launch1/2/3 (+ `_short`) | none | 0 | (342, 243, −38), sector 1 |
| FRANCKSTAND | none | 0 | (337, 245, 20), sector 1 |
| Airlift_To_Nativ / Airlift_To_Airlift | none | 0 | (337, 245, 20) |
| CIN_KYA_DEATH | none | 0 | no zone |

No two cinematics share a `.MIB`. Four `.MIB` files in `LEVEL_8/STREAM` (C818805E, 96A386A7, 77E0FD95, 31314C28) aren't used by any cinematic, so they are probably music or ambience.

**Open:** no `.sce` scene of these cinematics contains a subtitle track (type `0xd9cee9bc`), and the `cin_28` keys don't appear in the scene bytes. So it is still unknown how the subtitle keys are referenced (maybe from the top-level `.cin` or the subtitle source itself). The text counts above are the table sizes, not the number of keyframes.

## How this was captured

- Branch `research/fi-inventory`, merged with `macos/arm64-port`. The old guard-disabling hack is gone.
- The command hook in `port/DebugMenu/src/DebugScene.cpp` (`cmd.txt`) has three commands. `go N` uses the same reset as the Sectors tab Go button. `play N` loads and starts a cinematic. `cine` dumps the audio track, the stream name and the trigger zone spheres.
- `src/EdenLib/edCinematic/Sources/Cinematic.cpp` logs the scene, audio and subtitle sources of each cinematic. `src/b-witch/TranslatedTextData.cpp` dumps every text table it prepares. Both are research only.
- **Guard hit**: `CBehaviourShootFire::Manage` is still an `IMPLEMENTATION_GUARD`, and it asserts as soon as Kya is in sector 3 near CP 4 (ShooterDrago). With it bypassed, `CActorShoot::ComputeLighting` then segfaulted (null ambient), so that is skipped too. Both bypasses are in `ActorShoot.cpp` on this branch only. The real fix belongs on `macos/arm64-port`: 0x8 sector 3 isn't fully playable with guards live.
