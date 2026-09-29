# Which data defines actor placement in a level

Research ticket: [#24](https://github.com/mgiuditta/Kya/issues/24), part of map [#22](https://github.com/mgiuditta/Kya/issues/22).

**Status: every fact below is from code, unverified.** This machine has no game data. All claims come from reading the decompiled loaders. Line numbers refer to branch `docs/sequel-plot` at `09412e2`.

## Answer

Actor instances are in one bank entry per level: the **Scene** file (bank type `0x06`, subtype `0x1`) inside `<level>/Level.bnk`. Its companion **SceneCfg** (type `0x06`, subtype `0x2`) holds the per-class instance counts that size the actor pools.

- **Moving** an existing actor only needs a fixed-size edit to its record in Scene.
- **Adding** an actor also needs a SceneCfg class-count bump, a new unique `hashCode`, and an appended (dense) `actorIndex`. Other files link to actors by that index.

## 1. Where the data lives

- Level bank path: `levelPath + levelName + "/" + "Level.bnk"` (`src/b-witch/LevelScheduler.cpp:2761`, `:2836-2838`). It is loaded with `TableBankCallback`.
- `TableBankCallback` maps `(type, stype)` to install handlers (`src/b-witch/LevelScheduler.cpp:2721-2746`). The entries that matter here:
  - `0x06/0x1` `BnkInstallScene`: waypoints, paths, collision, **actors**, sectors.
  - `0x06/0x2` `BnkInstallSceneCfg`: scene version float, manager configs, **actor class counts**.
  - `0x08/0x1` `BnkInstallEvents`: event zones (`ed_zone_3d`), which actors refer to by index.
  - `0x13/0x1` cinematics, `0x0B/0x1` cameras, `0x10/0x1` lights. These can also hold actor, zone or waypoint indices.
- Each bank entry carries a `FileTypeData {ushort stype; ushort type;}` pair (`src/EdenLib/Include/edBank/edBankFile.h:15-24`). The code does not reveal the entry file names inside the bank. Dump them on Windows (see the checklist).
- Sector banks (`SECTn.bnk`, `src/b-witch/SectorManager.cpp:751-790`) use a separate install path (`CSector::InstallCallback`, `:528`). That path appears to handle geometry only, not actors. Sector membership is a field in the actor record (see below).

## 2. Scene file layout (`BnkInstallScene`, `src/b-witch/LevelScheduler.cpp:2363-2384`)

The file is read as one sequential `ByteCode` stream in this order:

1. chunk header (`ByteCode::GetChunk` reads a 4-byte tag and skips a 4-byte size, `src/b-witch/MemoryStream.cpp:20-28`)
2. `C3DFileManager::Level_AddAll`
3. `CScene::Level_Setup`
4. `CWayPointManager::Level_AddAll`
5. `CPathManager::Level_AddAll`
6. `CCollisionManager::Level_AddAll`
7. **`CActorManager::Level_AddAll`** (actors)
8. `CSectorManager::Level_AddAll`

All sections share one stream. If the actor section changes size, every section after it (the sector list) moves too. The file has no offset directory. Each reader starts where the previous one stopped.

### 2.1 Actor section (`CActorManager::Level_AddAll`, `src/b-witch/ActorManager.cpp:131-172`)

```
s32 nbActors
repeat nbActors:
  chunk header (8 bytes; the size field is ignored by the reader)
  s32 actorIndex        // slot in aActors[]; written with no bounds check (:170)
  u32 actorClass        // ACTOR_CLASS, 0..0x56
  <per-class record>    // pActor->Create(stream), virtual per class
```

- The loader does not allocate the instance object here. It takes it from a pre-allocated per-class pool: `aClassInfo[class].aActors + allocatedCount * size`, then `allocatedCount++` (`:156-159`). Nothing checks this against `totalCount`.
- `aActors` is `new CActor*[nbActors * 3]` (all, active and sector lists) (`:145-147`).
- After creation, the loader sizes the animation, anim-layer and shadow pools from the actors themselves (`:175-262`). They are derived at runtime, not baked.

### 2.2 Base actor record (`CActor::Create`, `src/b-witch/Actor.cpp:692-850`)

In stream order:

| Order | Data | Code |
|---|---|---|
| 1 | `char name[]`, NUL-terminated, then padded (`GetString` skips 3 more bytes, `MemoryStream.cpp:30-40`), copied into a 64-byte name | `Actor.cpp:712-716` |
| 2 | align 4, then `CinNamedObject30` (0x30 bytes): `int meshIndex; int textureIndex; int collisionDataIndex; vec3 position; vec3 rotationEuler; vec3 scale` | `Actor.cpp:722-725`, struct `Actor.h:310-317` |
| 3 | `MacroAnimTable`: `int nbEntries` + `nbEntries * 8` bytes | `Actor.cpp:727-736` |
| 4 | Behaviour list: align 4, `int count`, `count * {int id; int size}`, then `count` behaviour bodies of `size` bytes each. The first pass skips it (`:738-753`). `LoadBehaviours` later rewinds and parses it (`:825-828`, `Actor.cpp:2470-2520`) | |
| 5 | `int nbMeshTex` + `nbMeshTex * (string mesh, string texture)`, hashed with `ed3DComputeHashCode`. They go into a 16-entry stack buffer (`:710`), so more than 16 overflows it | `Actor.cpp:754-767` |
| 6 | `KyaUpdateObjA` (0x50 bytes, packed): `defaultBehaviourId`, `boundingSphere`, visibility/culling distances, `field_0x24` = **sectorId** (0 means "all sectors", i.e. -1), `hashCode` (**persistent actor id**), `lodBiases[2]`, `actorFieldS` (save flag etc.), `animLayerCount`, `field_0x40` (has shadow), `flags_0x48`, `lightingFloat_0x4c` | `Actor.cpp:769-779`, struct `Actor.h:284-307` |
| 7 | `ScenaricCondition` (spawn/enable condition; if false, flag `0x2000000`) | `Actor.cpp:819-824` |
| 8 | **Class-specific parameters**, read by the subclass `Create` after `CActor::Create` returns | e.g. `ActorTeleporter.cpp:33-68` |

Placement is `CinNamedObject30.position/rotationEuler/scale` (`SetupDefaultPosition`, `Actor.cpp:2208-2236`; rotation and scale at `:832-845`). `pCinData` and `subObjA` point straight into the loaded file buffer. Some fields are patched in place (for example scale snapping at `:2217-2225`).

### 2.3 Links (teleporters, event generators, etc.)

Links are `S_STREAM_REF<T>`: a 32-bit **index** that the owning class resolves to a pointer in its `Init()` (`src/b-witch/Types.h:832-845`). `-1` means null for all three types:

- `S_STREAM_REF<CActor>`: `aActors[index]`, a link by **actorIndex** (`src/b-witch/CinematicManager.cpp:864-877`).
- `S_STREAM_REF<ed_zone_3d>`: zone `index` in the **Events** file chunk (`CinematicManager.cpp:904-915`).
- `S_STREAM_REF<CWayPoint>`: index into the Scene's waypoint section (`src/b-witch/WayPoint.cpp:39-52`).

Examples:

- **Teleporter** (`src/b-witch/ActorTeleporter.cpp:14-68`): `subsectorMaterialId`, `u64 hash`, `u32`, `S_DESTINATION_LIST` (`nbEntries * {levelId, elevatorId, field_0x8}`, `ActorTeleporter.h:42-54`), three camera-event streams, zone/actor refs, material ids, a cinematic id, a waypoint list and a condition array.
- **Event generator** (`src/b-witch/ActorEventGenerator.cpp:358-386`): `u32 isGlobal` then `s32 zoneRef` (an Events zone index). Behaviour parameters (`CBehaviourEventGen::Create`, `:711-714`) sit inside the behaviour body.

## 3. Can an actor be moved or added by editing only that data?

### Moving an existing actor: yes (from code, unverified)

Overwrite `position` (and optionally `rotationEuler`/`scale`) in its `CinNamedObject30`. The record size stays the same, so no other data needs to change. Sector bounding boxes are rebuilt at load from actor positions (`PrecomputeSectorsBoundindBoxes`, `ActorManager.cpp:789-870`). The actor cluster is also built at runtime (`:34`).

Caveats:

- `sectorId` (`KyaUpdateObjA.field_0x24`) decides which sector list the actor joins (`ActorManager.cpp:461-523`). If you move an actor into another sector's space and leave this field unchanged, it activates and deactivates at the wrong time.
- `boundingSphere` is copied from the file. The code does not show whether it is local or world space. If it is world space, it must move with the actor.
- Class parameters may hold absolute positions or zone/waypoint refs that do not move with the actor (patrol paths, trigger zones in the Events file).
- A save file restores actor state by matching `hashCode` (`Level_LoadContext`, `ActorManager.cpp:635-733`, match at `:669`). For classes that save their location, the saved state may override the new position.

### Adding an actor: needs Scene and SceneCfg edits, plus index care

Counts, pools and ids that are baked outside the actor record:

1. **Class pool size in SceneCfg.** `Level_LoadClassesInfo` (`ActorManager.cpp:973-991`, called from `BnkInstallSceneCfg`, `LevelScheduler.cpp:2410`) reads `s32 nbClasses; repeat {s32 classId; s32 totalCount}` and allocates `totalCount` instances per class. `Level_AddAll` takes instances from that pool with no bounds check. An extra actor without a `totalCount` bump overruns the pool.
2. **Collision pool.** `CCollisionManager::Level_Create` sums `aClassInfo[c].totalCount` for classes with property flag `0x800`, plus the bank collision counts (`CollisionManager.cpp:165-188`). An actor with `collisionDataIndex != -1` calls `NewCCollision()` (`Actor.cpp:2618-2622`). Bumping the SceneCfg count keeps this pool consistent.
3. **Dense actorIndex.** Links are indices into `aActors`. Append the new actor with `actorIndex = old nbActors`, then increment `nbActors`. Every existing reference stays valid. Inserting in the middle would mean renumbering refs in Scene, Events, cinematics, cameras and similar files. `actorIndex` must also be `< nbActors`, because the array is `nbActors*3` long and unused slots stay uninitialised.
4. **Unique `hashCode`.** `GetActorByHashcode` (`ActorManager.cpp:993-1005`) and save/load matching (`:623`, `:669`) use it. A duplicate breaks lookups and saves.
5. **Asset indices.** `meshIndex`, `textureIndex`, `collisionDataIndex` and material ids index assets already installed from the bank (G3D/G2D/Col install order). A new actor can only reuse assets the level already loads, unless those are added too.
6. **Fixed limits.** 0x57 actor classes (`Types.h:827`), 0x80 linked actors (`ActorManager.cpp:18`), 30 sectors (`SectorManager.cpp:969`), and 16 mesh/texture pairs per actor (`Actor.cpp:710`).
7. **Stream exactness.** The reader ignores the per-actor chunk size, and the sector section comes right after the actor section. The new record must therefore be byte-exact for its class `Create`. The loader cannot skip over bad bytes.

The search found no other global actor or instance count beyond the SceneCfg class table and the Scene `nbActors` header. The save format sizes itself (`BLCL`/`BLAC` chunks, `ActorManager.cpp:567-633`).

## Checklist to run on Windows (with game data)

- [ ] Dump the `Level.bnk` entry table for one level (for example PREINTRO): file names and `(type, stype)` pairs. Confirm there is exactly one `0x06/0x1` and one `0x06/0x2` entry.
- [ ] With actor logging at Info, load that level. Check that the `CActor::Create <name>` / `id: type:` lines (`ActorManager.cpp:136,166`, `Actor.cpp:714`) match the SceneCfg class counts from `Level_LoadClassesInfo`.
- [ ] Add a temporary assert in `Level_AddAll` that `allocatedCount < totalCount` and `actorIndex < nbActors`. Load every level to confirm the invariants hold for shipped data.
- [ ] Log each actor's file offset of `CinNamedObject30.position`. Hex-edit one position in the unpacked Scene entry and confirm the actor moves in game. This also shows whether the bank is LZ77-packed (`edCBankFileHeader::unpack`) and needs repacking.
- [ ] Compare `KyaUpdateObjA.boundingSphere` with `position` for a few actors to tell whether it is local or world space.
- [ ] Check that `hashCode` is unique within every level.
- [ ] Clone a simple existing actor (for example a bonus or box): append it with a new index and hash, bump `nbActors` and the SceneCfg `totalCount`, move it, and confirm it spawns, collides and survives save/load.
