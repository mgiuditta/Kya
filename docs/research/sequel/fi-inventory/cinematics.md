# Reused Act I cinematics: what they show, and blocking Brazul's

Research for issue 48 ("What the reused Act I cinematics show and whether Brazul's can be neutralised"), 2026-10-05. Everything was checked on the Mac (`research/fi-inventory`, PAL data), playing each cinematic with the `play N` command hook. The frames are in `cinematics/`.

**Caveat on forced playback:** `play N` starts a cinematic outside its normal flow. CIN_29 and CIN_28 played with their own cameras and actors. CIN_30, CIN_30BIS and the end of CIN_31BIS fell back to the gameplay camera with no cinematic actors (subtitles only), so for those the content comes from the subtitle table (`text_and_audio_L9.tsv`).

## 1. 0x9 cinematics

| Cinematic | What is on screen | Frank | Brazul |
|---|---|---|---|
| **CIN_29_SC_41** | Frank is **human** (green T-shirt, grey jeans), tied up in a net cage (`cin29_frank_human.png`, `cin29_frank_caged.png`). Kya runs to free him ("Frank! I'm gonna get you out of here!"), and he warns her off ("You better go now!! GO! NOW!"). He then **transforms on screen into a big brown Wolfen** (`cin29_frank_turns_wolfen.png`), knocks Kya down (`cin29_frank_wolfen_vs_kya.png`), and the boss fight starts. Lines: "KYA! How did you…", "I… never thought I'd… see you again!", "Haaaaa!", "Frank!", "NO!", "Oh no…" | Human, then Wolfen | No |
| **CIN_30_SC_42** | After the exorcism, Frank is **human** again. Banter: "You owe me big-time.", "Oof! My head is killing me.", "Kya, where ARE we? What has been going on?", "Ha! Where to begin? I'll tell you, but first we need to get out of here.", "OK, boss!", "OK! We're outta here!" | Human | No |
| **CIN_30BIS_SC_42** | Shares the `cin_30` table. It holds the escape lines ("Maybe there's a way there…", "Jump in!", "Jump in here. It's surprising the…"). | Human (from the text) | No |
| **CIN_31BIS_SC_44** | **Brazul** (armoured, with his crow) on his flying ship: "She came looking for him, didn't she?", "I knew it… She will end up bringing me what I need." (`cin31bis_brazul_ship.png`, `cin31bis_brazul_line.png`). Its table is the shared Nativ City `cin_31`, of which this cinematic plays only the Brazul lines. | No | **Yes, the whole visible part** |

**How recognisable Frank-Wolfen is:** very. The model is unique: a bulky dark-brown, shaggy Wolfen that still wears **Frank's grey jeans and belt** (`cin29_frank_wolfen_vs_kya.png`). The generic Wolfen of 0x8 (the henchmen in `cin28_brazul_henchmen.png`) are slim and lilac-white, with red armour belts. On top of that, CIN_29 shows the transformation from human Frank on screen, so the stand-in only works if CIN_29 is **not** used and the boss is met already transformed.

## 2. CIN_28BIS_SC_40 (0x8) and how to block it

**What it shows** (`cin28_*.png`): Brazul in his lab with two lilac Wolfen henchmen loading a crate, and human Frank lying on a table. The lines are "Load this up! We're leaving immediately…", "Thanks for everything! Now I will be able to control humans too…", "Get rid of him." and "Stay here and inform me when it is all over…". Brazul is in nearly every shot.

**How it is started:**
- In the cinematic table of `LEVEL_8\cinematic.bin` (bank type 0x13/1, 2984 bytes, uncompressed), CIN_28 (index 7) has **no trigger zone A** and **no `AUTO_TRY_START`** (`flags_0x4 = 0x2006d1`). Its zone B (54.2, 308.8, −30.2) only preloads the bank.
- So it is started from outside, by `NotifyCinematic(7, msg 0xe/0xf)` from some actor's or event's `S_STREAM_NTF_TARGET_*` (`CinematicManager.cpp`, `NotifyCinematic`, which calls `TryTriggerCutscene(pActor, 0)`).
- Walking Kya near the zone with a `pos` hook didn't fire it (that area is a lava drop), so **the actual sender is still unknown**.

**Cheapest replace-only block, verified:** set the cinematic's `field_0x4c` (bank A buffer size) to 0 in `cinematic.bin`. That is 4 bytes at **entry offset 0x970**: `00 88 12 00` becomes `00 00 00 00`. The entry size doesn't change.
- Why it works:
  - `CCinematic::Create` sets `CINEMATIC_RUNTIME_FLAG_CONDITION_BLOCKED` when `field_0x4c == 0` (`CinematicManager.cpp`, just after `switchListB.Create`).
  - No reset clears that bit: it isn't in `CINEMATIC_RUNTIME_RESET_FLAGS`, and `Level_ClearAll` re-tests only when `field_0x4c != 0`.
  - `TryTriggerCutscene` refuses every start that has a blocker unless it is forced (`param_3 != 0`), and no caller in the codebase forces one.
- Test on the Mac:
  - **Control:** `ntf 7 14` played the cinematic (frames above), and the runtime flags became `0x1403`.
  - **With the override:** `mods/PROJECTS/B-WITCH/RESOURCE/BUILD/LEVEL_8/cinematic.bin`, written by `block_cin28.py`. The log shows "Mod override: … (2984 bytes)", the flags are `0x8` straight after load, and both `ntf 7 14` and `ntf 7 15` reach `TryTriggerCutscene` but it never starts. Other cinematics are untouched (LAVAFALL keeps flags `0x0`).
- What it doesn't cover:
  - Side effects that the cinematic's own `switchListA/B` or end state would have run (for example a door, or where Kya is placed afterwards) never happen. The playtest has to check that the CP 4 area flow still works without them.
  - The sender of the trigger is still unknown, but the block doesn't depend on it.

**Alternatives that weren't needed:**
- Moving zone B: it only controls preloading.
- Adding a scenario condition: it grows the record.
- Editing the unknown sender actor.

## 3. Subtitle keys

The cinematic's text table is loaded by `textData.select_language(bankObj, fileName, AUTO)` from the cine bank (`CinematicManager.cpp`, in the per-file install loop). Where each keyframe references a key wasn't found in this pass. It remains open (see `sectors.md`).

## Impact on the Act I mapping (for issue 31's table)

- **Beat 4 (sibling tension) can't use CIN_29.** It shows human Frank caged and then turning into the Wolfen, which contradicts both Frank-at-Kya's-side and Frank-Wolfen standing in for the chief.
- **Beat 6 (closing) can't use CIN_31BIS.** Only Brazul is visible in it.
- **CIN_30/30BIS are usable.** They are human-Frank-and-Kya dialogue after a fight, with 13 lines (shared table). Rewritten, they can carry both the tension and the "ancient blood" closing.
- **CIN_28 can be switched off** with a same-size poke through the existing override.
- **Caveat for the boss beat:** CIN_29 is what leads into the Frank fight in 0x9. Dropping it, whether with the same `field_0x4c` poke or by rewriting it, may leave the fight without its start. That needs a playtest with the same block applied to 0x9 index 2.
