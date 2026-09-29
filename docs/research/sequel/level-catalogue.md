# Catalogue of original levels and island-like sets

Research for ticket [#27](https://github.com/mgiuditta/Kya/issues/27) (map [#22](https://github.com/mgiuditta/Kya/issues/22)).
Context: Act I of the sequel happens on a deserted island that is an outpost of the ancient jailer people ("la Stirpe"), built from existing sets and assets ([plot spec](../../superpowers/specs/2026-09-29-kya-sequel-plot-design.md), lines 37, 76, 90).

There is no game data on this machine. Everything about ids and file layout comes from the decompiled loaders. Everything about what a level looks like comes from web sources. **Which level id is which named location is not in the code** and has to be checked on Windows (checklist at the end).

## Answer

- The engine has **16 level slots**. Ids `0x0`–`0xD` are the 14 playable levels, `0xE` is the title level, `0xF` is the pre-intro, and `0x10` means "no level". Names, sector counts and bank sizes are all data, read from `CDEURO/LEVEL/Info/levels.bnk`. The code contains no name strings.
- The player-facing world has **9 locations**: The Roots, Nativ City (the hub), Flying Forest, Hunters Domain (with the Wind Tower), The Quarry (with the Amber Quarry), The Air Post, Forgotten Island, Wolfen City (with the Wolfen prison) and The Fortress.
- **Best island candidate: Forgotten Island.** It is literally an island, desolate and volcanic, and Brazul's laboratory (alien technology) is there. It is also a dead end: the only way in is the Air Post cannon, and its only exit leads back to the Air Post. **Runner-up for a jungle set: The Roots.** It has a single exit, to Nativ City, and it is already the opening/tutorial level.
- **No source describes a beach or a stone-temple set in the original game.** Coastline, sea and temple ruins would have to be improvised from existing props.

## 1. What the code says

### Level table

| Fact | Source |
|---|---|
| Level root is `CDEURO/Level/`; it can be overridden by `[Router] SetPath` and extended with `[Router] AddLevel` in the INI | `src/b-witch/LevelScheduler.cpp:292`, `:2255-2258` |
| The level table is loaded from `Info/levels.bnk`. Each entry is a V9 header with `levelId`, an 8-char `levelName`, bank sizes, a title message hash, `sectorStartIndex`, `nbSectors`, `nbTeleporters` and language files | `LevelScheduler.cpp:601`, `:926`, `:940-942`; header struct `LevelScheduler.h:210-236` |
| Only ids `0 <= id < 0x10` are accepted; the scheduler holds `S_LEVEL_INFO aLevelInfo[16]` | `LevelScheduler.cpp:942`, `LevelScheduler.h:547` |
| `0xE` = title level, `0xF` = pre-intro, `0x10` = no level. Saves on `0x10` are invalid | `port/DebugMenu/src/DebugSaveLoad.cpp:461`, `src/b-witch/SaveManagement.h:24` |
| The displayed level name is a message hash (`titleMsgHash`) resolved through the Messages bank. Pause menu, map and save screens use it | `LevelScheduler.cpp:608`, `Pause.cpp:816`, `MapManager.cpp:1701`, `SaveManagement.cpp:1055` |
| Folder names follow `LEVEL_x` (comments show `level_1`, `LEVEL_x`, `PREINTRO`). A disabled debug block hot-swaps slot `0xF` with a 12-sector `LEVEL_T` test level | `LocalizationManager.cpp:55`, `CinematicManager.cpp:1913`, `SectorManager.cpp:752`, `LevelScheduler.cpp:945-1000` |

### Per-level file layout (all under `CDEURO/LEVEL/<levelName>/`)

- `Level.bnk` and `LevelIOP.bnk`: level-wide resources and audio (`LevelScheduler.cpp:2761-2762`).
- `SECT<n>.bnk`: one bank per sector. The sector manager closes the previous sector bank before it loads the next, so **only one sector bank is resident at a time** (`SectorManager.cpp:151`, `:752-780`).
- `Cine\`, `Stream\`, `STREAMCH\` and `map.bnk`: cutscenes, streams, audio streams and map (`CinematicManager.cpp:1432`, `:6202`, `Audio.cpp:3161`, `MapManager.cpp:2494`).

### Sectors and sub-zones

- `S_LEVEL_INFO` stores `maxSectorId` (= `nbSectors`), `sectorStartIndex` (the entry sector), up to **30** sector entries (`aSectorSubObj[30]`) and up to **12** sub-sector records (`aSubSectorInfo[12]`). Each sub-sector record holds a teleporter hash plus exorcised/max-Wolfen counters (`LevelScheduler.h:238-276`). Those records map onto the in-game shell-elevator "buttons": each elevator zone shows its Wolfen count ([wiki: Shell Elevator](https://kyadarklineage.fandom.com/wiki/Shell_Elevator)).
- Loops over a level's sectors use `maxSectorId + 1` (`ActorBonusServices.cpp:524`, `:617`).
- Transitions between levels go through teleporters (shell elevators) with per-level destination lists. **Level `0` is special-cased as the hub**: teleporter destination logic and the elevator switch both branch on `currentLevelID == 0` (`ActorTeleporter.cpp:714`, `:877-879`; `ActorSwitch.cpp:472`). This matches Nativ City's "giant tree with a set of elevators to each and every area" ([wiki: Nativ City](https://kyadarklineage.fandom.com/wiki/Nativ_City)).

### Id hints (hypotheses, unverified)

- `0x0` = **Nativ City** (hub special-casing above; upstream commit `22f6b6b` "Prep for mini nativ city level").
- `0x1` = probably **The Roots**: the intro FMV plays when `nextLevelID == 1` (`kya.cpp:1809`), and The Roots is where the story starts.
- `0xD`: the hero resets `SCN_ABILITY_BOOMY_TYPE` to 0 on a specific message only in this level (`ActorHero_Private.cpp:3157-3160`). This is consistent with Wolfen City, where Brazul takes Kya's weapons ([wiki: Wolfen City](https://kyadarklineage.fandom.com/wiki/Wolfen_City)). It is a guess.
- 14 playable slots but only 9 named locations: some locations probably span more than one level id (candidates are Wind Tower, Amber Quarry, the Wolfen prison and Flying Forest's areas). Unverified.

## 2. The world as the player sees it

Source for this table: fandom wiki [Destinations](https://kyadarklineage.fandom.com/wiki/Destinations) and the per-location pages linked in each row, fetched through the MediaWiki API on 2026-09-29. Location names are cross-checked with [Wikipedia](https://en.wikipedia.org/wiki/Kya:_Dark_Lineage) (hub "Nativ City", the "Mining Area", Brazul's laboratory and fortress, and the desert-island ending). The wiki is fan-written, so treat these descriptions as secondary.

| Location | Look | Connects to | Shell elevators | Story role |
|---|---|---|---|---|
| [The Roots](https://kyadarklineage.fandom.com/wiki/The_Roots) | **Jungle**: trees, strange flowers, flatland plus thick forest, wind tunnels, freefall | Nativ City only | 4 | Opening chase; later a rune side-quest |
| [Nativ City](https://kyadarklineage.fandom.com/wiki/Nativ_City) | Lush grassland village, shops, zoo, dojo, elevator tree | Every area (hub) | hub tree | Home base |
| [Flying Forest](https://kyadarklineage.fandom.com/wiki/Flying_Forest) | Forest in the sky, **floating islands with waterfalls**, an orange swamp, two Wolfen bases | Nativ City, Hunters Domain, Air Post | 4 | Blue Egg (Stuff), Receptacle, Lava Worm fight |
| [Hunters Domain](https://kyadarklineage.fandom.com/wiki/Hunters_Domain) (+ [Wind Tower](https://kyadarklineage.fandom.com/wiki/Wind_Tower)) | Desert mountains, thorns, floating rocks | Flying Forest, Quarry | 6 | Rescue Atea, the Hunter |
| [The Quarry](https://kyadarklineage.fandom.com/wiki/The_Quarry) (+ Amber Quarry) | Deep mining pits, poisonous gas, industrial | Hunters Domain, Wolfen City | ? | Aton's betrayal; first air cannon |
| [The Air Post](https://kyadarklineage.fandom.com/wiki/The_Air_Post) | "Old" windy, rocky mountain post with an air cannon | Flying Forest, Forgotten Island | ? | Second cannon to the island. The wiki says it has the fewest scenes of any location |
| [Forgotten Island](https://kyadarklineage.fandom.com/wiki/Forgotten_Island) | **Volcanic island** "lost in time", mountains, lava lakes, black/white palette with red-orange lava, dead flora | Air Post only (Fortress entrance blocked) | ? | Brazul's lab (the Nativ→Wolfen machine), Frank exorcised |
| [Wolfen City](https://kyadarklineage.fandom.com/wiki/Wolfen_City) | Mountain village on top, underground camp/prison below | Quarry, Fortress | 3 | Kya captured and disarmed |
| [The Fortress](https://kyadarklineage.fandom.com/wiki/The_Fortress) | Floating fortress in storm clouds, sewers, lava canyon, rock tower | Wolfen City | 1 | Final arena, Aton, Brazul. One-way after the gate |
| [Mysterious Desert World](https://kyadarklineage.fandom.com/wiki/Mysterious_Desert_World) | Orange desert under heavy clouds | none | none | Ending cliffhanger only. Wikipedia calls it a "desert island". Not known to be a playable level; it may exist only as a cutscene |

## 3. Candidates for the Stirpe island outpost

The requested moods were beaches, ruins, jungle and stone temples. Ranked:

1. **Forgotten Island: strongest fit.**
   - *Look*: desolate volcanic island, dead vegetation, lava. Not a tropical beach, but it reads well as a forgotten prison outpost.
   - *Lore fit*: Brazul (Alan) put his lab here. The sequel can say he chose it because it was a Stirpe outpost, which ties the alien-looking lab props to the jailers.
   - *Self-containment*: very high. Its one link is the Air Post cannon, and the wiki notes the Fortress entrance is blocked, so the level already works as an island with no walk-out exits.
   - *Unknowns*: sector count, how many elevator zones, and whether lava hazards dominate the whole set.
2. **The Roots: best jungle.**
   - *Look*: dense jungle, wind tunnels, freefall.
   - *Self-containment*: high. One exit (Nativ City), 4 shell-elevator zones. It is the game's first level, so it already carries tutorial-friendly pacing, which fits "base mechanics as tutorial" (plot spec line 37).
   - *Risk*: players will recognise it as Brazelia's opening area, and it has no sea or island silhouette.
3. **The Air Post: the "outpost" piece.** An old, rocky, windy post with a cannon, and small (fewest scenes). Its only links are Flying Forest and Forgotten Island. Paired with Forgotten Island, it can serve as the Stirpe outpost structure, with the cannon as the in-world "portal" jump.
4. **Flying Forest: floating islands and waterfalls.** This is the best "island" silhouette (sky islands), and it has a swamp and Wolfen bases. It is less self-contained (three exits, Wolfen bases) and it is central to the first game's story.

Not suitable: Nativ City (living town and hub), Hunters Domain (desert), Quarry (industrial), Wolfen City (Wolfen settlement and prison), Fortress (destroyed in the story, one-way). Mysterious Desert World would be the most canonical choice, since the sequel starts exactly there, but nothing shows it is a loadable level.

**Gaps:** no source mentions a beach, a coastline or a stone temple anywhere in the original. Ruin-like structures exist only as Wolfen bases, the "old" Air Post and the Fortress. A beach or temple would need a new layout from existing props, which the plot spec already assumes (line 76: "layout nuovo costruito con asset e set esistenti").

## 4. Windows verification checklist

1. List `CDEURO/LEVEL/` folders and count the `SECT*.bnk` files in each one.
2. Dump `aLevelInfo[0..15]` after `Levels_LoadInfoBank`: `levelName`, resolved `titleMsgHash` text, `maxSectorId`, `sectorStartIndex` and teleporter count. The debug scenario panel already formats "Level N (levelName)" (`port/DebugMenu/src/DebugScenario.cpp:114-119`). Record the names with `Debug::WorldNames::SetLevelName` (`port/DebugMenu/src/DebugWorldNames.h`).
3. Confirm the id ↔ location mapping, especially `0x1` = Roots, `0xD`, and which ids are Forgotten Island and the Air Post.
4. Load Forgotten Island and The Roots, walk every sector, and note sector count, elevator zones, and whether a lava-free region big enough for an Act I layout exists.
5. Check whether the ending's desert world is an in-engine level or a pre-rendered cutscene: look at the last cinematic bank of the Nativ City level after the Fortress.

## Sources

- Decompiled code in this repo (paths and lines above), branch `docs/sequel-plot` at `09412e2`.
- [Kya Dark Lineage wiki: Destinations](https://kyadarklineage.fandom.com/wiki/Destinations) and linked location pages (fan wiki).
- [Wikipedia: Kya: Dark Lineage](https://en.wikipedia.org/wiki/Kya:_Dark_Lineage).
- Upstream history: [Icey1717/Kya](https://github.com/Icey1717/Kya) commit `22f6b6b` ("Prep for mini nativ city level").
