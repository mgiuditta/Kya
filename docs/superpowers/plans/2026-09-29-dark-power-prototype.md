# Dark Power Prototype Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Debug-menu-toggled "dark power" for Kya: boosted melee damage, a corruption counter fed by landed hits, and hero darkening proportional to corruption.

**Architecture:** Pure logic lives in a small `DarkPower` module in `src/port/`, built only on `PLATFORM_WIN` and unit-tested with GoogleTest. The decompiled fighter code calls it through two `#ifdef PLATFORM_WIN` hooks in `CActorFighter::_SV_HIT_FightCollisionProcessHit`, gated to the hero by `typeID`. The debug menu (`DebugHero.cpp`) owns the UI and applies darkening each frame through the existing `lightingFloat_0xe0` light multiplier.

**Tech Stack:** C++20, CMake, GoogleTest (vendored in `port/Test/ext`), Dear ImGui. Windows only: VS2022 Clang tools + Vulkan SDK.

**Spec:** `docs/superpowers/specs/2026-09-29-dark-power-prototype-design.md`

**Build/test commands (run from repo root on Windows):**
- Configure: `cmake --preset x64-debug`
- Build: `cmake --build out/build/x64-debug`
- Tests: `ctest --test-dir out/build/x64-debug --output-on-failure`, or `bin/WIN/KyaPortTest.exe --gtest_filter=DarkPower.*`

---

## File Structure

| File | Action | Responsibility |
|---|---|---|
| `src/port/dark_power.h` | Create | `DarkPower::State` and the pure functions |
| `src/port/dark_power.cpp` | Create | Implementation and the single global state |
| `CMakeLists.txt` (~line 746) | Modify | Add both files to the Windows `main_src` list |
| `port/Test/src/dark_power_tests.cpp` | Create | Unit tests for the pure logic |
| `port/Test/CMakeLists.txt` (line 11) | Modify | Register the test file |
| `src/b-witch/ActorFighter.cpp` (~5638, ~5659) | Modify | Damage hook and hit-landed hook |
| `port/DebugMenu/src/DebugHero.cpp` | Modify | UI section and per-frame darkening |

---

### Task 1: DarkPower module with unit tests

**Files:**
- Create: `src/port/dark_power.h`
- Create: `src/port/dark_power.cpp`
- Create: `port/Test/src/dark_power_tests.cpp`
- Modify: `CMakeLists.txt` (Windows `main_src` list, next to `debug_draw`)
- Modify: `port/Test/CMakeLists.txt:11`

- [ ] **Step 1: Write the failing tests**

Create `port/Test/src/dark_power_tests.cpp`:

```cpp
#ifdef PLATFORM_WIN

#include <gtest/gtest.h>

#include "../../../src/port/dark_power.h"

TEST(DarkPower, DisabledLeavesDamageUnchanged)
{
	DarkPower::State state;
	state.bEnabled = false;
	state.damageMultiplier = 3.0f;

	EXPECT_FLOAT_EQ(DarkPower::ApplyDamage(state, 10.0f), 10.0f);
}

TEST(DarkPower, EnabledMultipliesDamage)
{
	DarkPower::State state;
	state.bEnabled = true;
	state.damageMultiplier = 2.5f;

	EXPECT_FLOAT_EQ(DarkPower::ApplyDamage(state, 4.0f), 10.0f);
}

TEST(DarkPower, HitLandedAddsCorruptionOnlyWhenEnabled)
{
	DarkPower::State state;
	state.corruptionPerHit = 5.0f;

	state.bEnabled = false;
	DarkPower::OnHitLanded(state);
	EXPECT_FLOAT_EQ(state.corruption, 0.0f);

	state.bEnabled = true;
	DarkPower::OnHitLanded(state);
	DarkPower::OnHitLanded(state);
	EXPECT_FLOAT_EQ(state.corruption, 10.0f);
}

TEST(DarkPower, CorruptionClampsAtMax)
{
	DarkPower::State state;
	state.bEnabled = true;
	state.corruptionPerHit = 60.0f;

	DarkPower::OnHitLanded(state);
	DarkPower::OnHitLanded(state);

	EXPECT_FLOAT_EQ(state.corruption, DarkPower::kMaxCorruption);
}

TEST(DarkPower, LightScaleFollowsCorruption)
{
	DarkPower::State state;
	state.bEnabled = true;

	state.corruption = 0.0f;
	EXPECT_FLOAT_EQ(DarkPower::GetLightScale(state), 1.0f);

	state.corruption = DarkPower::kMaxCorruption;
	EXPECT_FLOAT_EQ(DarkPower::GetLightScale(state), DarkPower::kMinLightScale);

	state.corruption = DarkPower::kMaxCorruption * 0.5f;
	EXPECT_FLOAT_EQ(DarkPower::GetLightScale(state), 1.0f - 0.5f * (1.0f - DarkPower::kMinLightScale));
}

TEST(DarkPower, DisabledHasNoDarkening)
{
	DarkPower::State state;
	state.bEnabled = false;
	state.corruption = DarkPower::kMaxCorruption;

	EXPECT_FLOAT_EQ(DarkPower::GetLightScale(state), 1.0f);
}

TEST(DarkPower, ResetCorruptionClears)
{
	DarkPower::State state;
	state.corruption = 42.0f;

	DarkPower::ResetCorruption(state);

	EXPECT_FLOAT_EQ(state.corruption, 0.0f);
}

#endif
```

Register it in `port/Test/CMakeLists.txt`, changing line 11 from:

```cmake
target_sources(KyaPortTest PRIVATE "src/draw_trace_tests.cpp")
```

to:

```cmake
target_sources(KyaPortTest PRIVATE "src/draw_trace_tests.cpp" "src/dark_power_tests.cpp")
```

- [ ] **Step 2: Build to verify it fails**

Run: `cmake --build out/build/x64-debug`
Expected: compile error in `dark_power_tests.cpp` saying `../../../src/port/dark_power.h` is not found.

- [ ] **Step 3: Write the minimal implementation**

Create `src/port/dark_power.h`:

```cpp
#pragma once

namespace DarkPower
{
#ifdef PLATFORM_WIN
	constexpr float kMaxCorruption = 100.0f;
	constexpr float kMinLightScale = 0.35f;

	struct State
	{
		bool bEnabled = false;
		float damageMultiplier = 2.0f;
		float corruptionPerHit = 5.0f;
		float corruption = 0.0f;
	};

	State& Get();

	float ApplyDamage(const State& state, float damage);
	void OnHitLanded(State& state);
	float GetLightScale(const State& state);
	void ResetCorruption(State& state);
#endif
}
```

Create `src/port/dark_power.cpp`:

```cpp
#include "dark_power.h"

#include <algorithm>

namespace DarkPower
{
#ifdef PLATFORM_WIN
	State& Get()
	{
		static State gState;
		return gState;
	}

	float ApplyDamage(const State& state, float damage)
	{
		if (!state.bEnabled) {
			return damage;
		}

		return damage * state.damageMultiplier;
	}

	void OnHitLanded(State& state)
	{
		if (!state.bEnabled) {
			return;
		}

		state.corruption = std::min(state.corruption + state.corruptionPerHit, kMaxCorruption);
	}

	float GetLightScale(const State& state)
	{
		if (!state.bEnabled) {
			return 1.0f;
		}

		const float t = state.corruption / kMaxCorruption;
		return 1.0f - t * (1.0f - kMinLightScale);
	}

	void ResetCorruption(State& state)
	{
		state.corruption = 0.0f;
	}
#endif
}
```

Add both files to the root `CMakeLists.txt` Windows source list, right after the `debug_draw` entries (currently lines 746-747):

```cmake
		"${src_root}/port/debug_draw.cpp"
		"${src_root}/port/debug_draw.h"
		"${src_root}/port/dark_power.cpp"
		"${src_root}/port/dark_power.h"
```

- [ ] **Step 4: Build and run the tests to verify they pass**

Run: `cmake --preset x64-debug && cmake --build out/build/x64-debug && bin/WIN/KyaPortTest.exe --gtest_filter=DarkPower.*`
Expected: `[  PASSED  ] 7 tests.`

- [ ] **Step 5: Commit**

```bash
git add src/port/dark_power.h src/port/dark_power.cpp port/Test/src/dark_power_tests.cpp port/Test/CMakeLists.txt CMakeLists.txt
git commit -m "Add DarkPower prototype logic with unit tests."
```

---

### Task 2: Hook damage and landed hits in the fighter code

**Files:**
- Modify: `src/b-witch/ActorFighter.cpp` (includes at top; `_SV_HIT_FightCollisionProcessHit` around lines 5638 and 5659)

This is layout-sensitive decompiled code. Add only `#ifdef PLATFORM_WIN` blocks and change nothing else.

- [ ] **Step 1: Add the include**

After the existing include block (the last include is `#include "ed3D/ed3DG2D.h"`, line 15), add:

```cpp
#ifdef PLATFORM_WIN
#include "port/dark_power.h"
#endif
```

- [ ] **Step 2: Add the damage hook**

In `CActorFighter::_SV_HIT_FightCollisionProcessHit`, find:

```cpp
		hitMsg.damage = hitMsg.damage * this->hitMultiplier;
```

and add directly after it:

```cpp
#ifdef PLATFORM_WIN
		if (this->typeID == ACTOR_HERO_PRIVATE) {
			hitMsg.damage = DarkPower::ApplyDamage(DarkPower::Get(), hitMsg.damage);
		}
#endif
```

- [ ] **Step 3: Add the hit-landed hook**

In the same function, find:

```cpp
			uVar3 = pFighter->FUN_0031b4d0(pFighter->actorState);
			if (uVar3 == 0) {
				this->fightFlags = this->fightFlags | FIGHT_FLAG_ATTACK_CONNECTED;
			}
```

and change the `if` body to:

```cpp
			if (uVar3 == 0) {
				this->fightFlags = this->fightFlags | FIGHT_FLAG_ATTACK_CONNECTED;
#ifdef PLATFORM_WIN
				if (this->typeID == ACTOR_HERO_PRIVATE) {
					DarkPower::OnHitLanded(DarkPower::Get());
				}
#endif
			}
```

Leave the non-fighter `else` branch unchanged. Hits on crates and other props should not add corruption.

- [ ] **Step 4: Build and run all tests**

Run: `cmake --build out/build/x64-debug && ctest --test-dir out/build/x64-debug --output-on-failure`
Expected: the build succeeds and every test passes, same as before plus the 7 `DarkPower` tests.

- [ ] **Step 5: Commit**

```bash
git add src/b-witch/ActorFighter.cpp
git commit -m "Hook DarkPower into hero melee damage and landed hits."
```

---

### Task 3: Debug menu UI and darkening

**Files:**
- Modify: `port/DebugMenu/src/DebugHero.cpp`

- [ ] **Step 1: Add the include**

After `#include "Native/NativeDebugShapes.h"` (line 20), add:

```cpp
#include "port/dark_power.h"
```

If the DebugMenu target cannot resolve `port/dark_power.h`, use `#include "../../../src/port/dark_power.h"` instead. That relative-path style is already used by `port/Test/src/tests.cpp` for `MathOps.h`.

- [ ] **Step 2: Add the UI and update functions**

Directly above `void Debug::Hero::ShowMenu(bool* bOpen)`, add:

```cpp
static void ShowDarkPowerMenu()
{
	DarkPower::State& darkPower = DarkPower::Get();

	ImGui::SeparatorText("Dark Power");
	ImGui::Checkbox("Enabled##DarkPower", &darkPower.bEnabled);
	ImGui::SliderFloat("Damage Multiplier##DarkPower", &darkPower.damageMultiplier, 1.0f, 5.0f);
	ImGui::SliderFloat("Corruption Per Hit##DarkPower", &darkPower.corruptionPerHit, 0.0f, 25.0f);
	ImGui::ProgressBar(darkPower.corruption / DarkPower::kMaxCorruption, ImVec2(-1.0f, 0.0f));

	if (ImGui::Button("Reset Corruption##DarkPower")) {
		DarkPower::ResetCorruption(darkPower);
	}
}

// ponytail: rewrites the hero light multiplier every frame from its init value; fine while
// only Init/Reset write lightingFloat_0xe0. Switch to a ComputeLighting override if that changes.
static void UpdateDarkPowerLighting()
{
	CActorHeroPrivate* pActorHero = reinterpret_cast<CActorHeroPrivate*>(CActorHeroPrivate::_gThis);

	if ((pActorHero == nullptr) || (pActorHero->subObjA == nullptr)) {
		return;
	}

	pActorHero->lightingFloat_0xe0 = pActorHero->subObjA->lightingFloat_0x4c * DarkPower::GetLightScale(DarkPower::Get());
}
```

- [ ] **Step 3: Call the UI from the Hero window**

In `Debug::Hero::ShowMenu`, find:

```cpp
		ShowCheckpointMenu();
```

and add directly before it:

```cpp
		ShowDarkPowerMenu();
```

- [ ] **Step 4: Register the per-frame update**

At the end of the file, change:

```cpp
namespace Debug {
    MenuRegisterer sDebugHeroMenuReg("Hero", Debug::Hero::ShowMenu, true);
}
```

to:

```cpp
namespace Debug {
    MenuRegisterer sDebugHeroMenuReg("Hero", Debug::Hero::ShowMenu, true);
    UpdateRegisterer sDarkPowerUpdateReg(UpdateDarkPowerLighting);
}
```

- [ ] **Step 5: Build and run all tests**

Run: `cmake --build out/build/x64-debug && ctest --test-dir out/build/x64-debug --output-on-failure`
Expected: the build succeeds and all tests pass.

- [ ] **Step 6: Manual check in game**

Run the game (`Kya_Win`), load any level with Wolfen, and open the Hero window.
1. With Dark Power off, hit a Wolfen. The corruption bar stays at 0 and Kya's lighting is unchanged.
2. Turn Dark Power on and hit a Wolfen. It goes down in roughly half the usual hits, the bar rises by 5 per landed hit, and Kya gets darker.
3. Hit a Wolfen while it blocks. The bar does not rise.
4. Click "Reset Corruption". The bar goes to 0 and Kya's normal lighting returns.
5. Turn Dark Power off at max corruption. Kya's normal lighting returns and the bar value is kept.

If step 2 shows no darkening, check `LightManager.cpp:368-369`. Lighting early-outs when there are no active lights or when the actor's lighting flags are 0. Record the outcome and do not work around it in this prototype.

- [ ] **Step 7: Commit**

```bash
git add port/DebugMenu/src/DebugHero.cpp
git commit -m "Add Dark Power controls and darkening to the Hero debug menu."
```
