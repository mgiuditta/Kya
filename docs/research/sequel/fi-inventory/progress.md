# Forgotten Island inventory: progress (stopped 2026-09-30)

Fact-finding for "Map Act I beats onto Forgotten Island" (issue 31). Stopped by the user partway through. This folder holds everything the agent had found.

## What is done

- **Actor inventory, 0x8 (LEVEL_8)**: 812 actors in 38 classes, with name, sector and position for each one. See `L8_classes.txt` (summary) and `actors_L8.tsv` (raw rows).
- **Actor inventory, 0x9 (LEVEL_9)**: 102 actors in 17 classes. See `L9_classes.txt` and `actors_L9.tsv`.
- **Cinematic list**: 15 cinematics, with file name, banks, actor counts and trigger zones. See `cine_and_levelinfo.txt`. The dump header says `level 9`, while bank `2DDAF421` is in `LEVEL_8/CINE`, so which level it belongs to still has to be confirmed (it could be 0x9, or 0x8 read through a different index).
- **0x8 loads**, but only with a hack: every `IMPLEMENTATION_GUARD*` macro is disabled (`src/b-witch/Types.h`). This is not a fix. See "Code hacks".
- **Screenshots**: `L8_sector1_cp0.png`, `L8_s3_cp4.png` and `L9_elev0.png`. In `failed_attempts_all_show_sector1/`, the file names say sector 2 or 3, but the pictures all show sector 1.

## Key facts for the Act I mapping

- **0x8 has**: `CActorNativ` ×4 (GRUNTNATIV_01/02, NATIVSCRAWNY_01/02, all sector -1 = level-wide), `CActorCompanion` ×1, `CActorWolfen` ×8 (Grunt and Scrawny variants plus `KRONOS_02`), `CActorElectrolla` ×9, `CActorBlazer` ×28, `CActorAraignos` ×3, `CActorCommander` ×5, `CActorHelperSign` ×2, `CActorTeleporter` ×4, `CActorRune` ×1, `CActorWoof` ×1, `CActorBonusFountain` ×1, and many lava platforms and props.
- Actors sit in only a few sectors: -1 (level-wide), 1, 2 and 3. Most are in sector 3.
- **0x9 is the Frank fight**: `FRANK_PROJ*`, `LVL09_MOBPLAT_BEFORE/AFTER_EXORCISM`, `PTF_SCRAWNIEFACE_BEFORE/AFTER_EXORCISM`, `MOTHER_FENCE` and `KRONOS_BUNCH`. The cinematics include `FRANK_ATTACK_prepare_launch1..3`, `CIN_29/30/31`, `BEGIN_LEVEL9`, `FRANCKSTAND`, `CIN_KYA_DEATH` and the Airlift to Nativ City. This is the natural arena for the boss beat.
- No `ActorBrazul` and no `ActorShadows`/`WolfenGhost` class showed up in either level.

## Not done

- Walking through sectors 2–9 of 0x8 and describing each one: the teleport attempts kept going back to sector 1. The last success was "Sector 3 loaded" (`L8_s3_cp4.png`).
- The subtitle key count per cinematic and the `.MIB` stream each one uses.
- Point 4 (cloning a class from another level). Per issue 32, a clone copies an actor in the same level, so a class missing from 0x8/0x9 can't be cloned in.

## Code hacks (uncommitted-quality, research branch only)

- `src/b-witch/Types.h`: all `IMPLEMENTATION_GUARD*` turned into no-ops, which is what lets 0x8 load.
- `src/b-witch/ActorElectrolla.h`, `ActorBlazer.h`: the guard log is commented out.
- `src/b-witch/ActorManager.cpp`: writes every actor and class to the scratch file `actors.tsv`.
- `src/b-witch/LevelScheduler.cpp`: reads `forcesector.txt` to force the start sector.
- `port/DebugMenu/src/DebugScene.cpp`: a command-file hook (`cmd.txt` → `cmd_out.txt`) with the commands `dump`, `cine`, `linfo`, `lvl`, `elev`, `sector`, `cp`, `go`, `cpsec` and `start`.
- The paths point at a session scratchpad under `/private/tmp`. Change them before rerunning.

## Scripts

`run.sh` / `dbg.sh` (run, or run under lldb, with `Auto Load Level ID`), `shot.sh` + `winid.swift` (capture the game window), `classmap.py` + `summ.py` (class id → name, per-class summary), `build.sh`.

## Next step when resuming

Get sector teleport working (sector switch plus hero position and camera reset), then screenshot and describe each sector of 0x8.
