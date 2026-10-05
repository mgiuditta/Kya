#!/usr/bin/env python3
"""Write a mods/ override of LEVEL_8 cinematic.bin that keeps CIN_28BIS_SC_40 (Brazul) from ever starting.

Research only. Usage: block_cin28.py <bin/MAC or game root>
Zeroes the cinematic's bank-size field (CCinematic::field_0x4c). CCinematic::Create then sets
CINEMATIC_RUNTIME_FLAG_CONDITION_BLOCKED, which no reset clears, so every non-forced trigger is refused.
"""
import os
import struct
import sys

root = sys.argv[1]
bank = open(os.path.join(root, "CDEURO/LEVEL/LEVEL_8/LEVEL.BNK"), "rb").read()

# ponytail: entry located by its CINE chunk magic + the record's file name, not by parsing the bank's name tree.
name = b"CIN_28BIS_SC_40_CIN_28BIS_SC_40_SceneMontage.cin\0"
rec = bank.find(name)
start = bank.rfind(bytes.fromhex("efbeadde") + b"\xa8\x0b\x00\x00ENIC", 0, rec)
assert rec > 0 and start > 0, "LEVEL_8 cinematic table not found"
entry = bytearray(bank[start:start + 2984])

# After the file name string (4-byte aligned): 6 x s32, then field_0x4c (u32).
off = rec - start + len(name)
off = (off + 3) & ~3
off += 6 * 4
assert struct.unpack_from("<I", entry, off)[0] == 0x128800, "unexpected field_0x4c"
struct.pack_into("<I", entry, off, 0)

out = os.path.join(root, "mods/PROJECTS/B-WITCH/RESOURCE/BUILD/LEVEL_8/cinematic.bin")
os.makedirs(os.path.dirname(out), exist_ok=True)
open(out, "wb").write(entry)
print(f"wrote {out} (field_0x4c at entry offset {off:#x})")
