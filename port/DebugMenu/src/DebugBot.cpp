// KYA_BOT=1: drive the pad so Kya walks to the nearest living Wolfen and fights it, logging
// fight events to stderr. Used to play fights unattended (crash and render regressions).
#include "DebugMenu.h"
#include "input_functions.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "Actor.h"
#include "ActorManager.h"
#include "ActorAutonomous.h"
#include "ActorFighter.h"
#include "ActorHero.h"
#include "CameraViewManager.h"
#include "LevelScheduler.h"
#include "Frontend.h"
#include "SectorManager.h"

namespace Debug::Bot {
	static std::atomic<float> gRoutes[ROUTE_END];

	static void Clear() {
		for (auto& route : gRoutes) {
			route = 0.0f;
		}
	}

	// World direction (x, z) to left stick halves, same mapping as the free camera move in CPlayerInput.
	static void SetStickToward(float dx, float dz) {
		const edF32MATRIX4& camera = CCameraManager::_gThis->transformationMatrix;
		float rightX = camera.rowX.x, rightZ = camera.rowX.z;
		float forwardX = camera.rowZ.x, forwardZ = camera.rowZ.z;
		const float length = sqrtf(dx * dx + dz * dz);
		if (length < 0.001f) {
			return;
		}
		dx /= length;
		dz /= length;

		const float stickX = -(dx * rightX + dz * rightZ);
		const float stickY = dx * forwardX + dz * forwardZ;
		gRoutes[ROUTE_L_ANALOG_RIGHT] = stickX > 0.0f ? stickX : 0.0f;
		gRoutes[ROUTE_L_ANALOG_LEFT] = stickX < 0.0f ? -stickX : 0.0f;
		gRoutes[ROUTE_L_ANALOG_UP] = stickY > 0.0f ? stickY : 0.0f;
		gRoutes[ROUTE_L_ANALOG_DOWN] = stickY < 0.0f ? -stickY : 0.0f;
	}

	static float Life(CActorAutonomous* pActor) {
		CLifeInterface* pLife = pActor->GetLifeInterface();
		return pLife ? pLife->GetValue() : -1.0f;
	}

	static void Update() {
		static const bool bEnabled = getenv("KYA_BOT") != nullptr;
		if (!bEnabled) {
			return;
		}

		static bool bHooked = false;
		if (!bHooked) {
			bHooked = true;
			Input::gInputFunctions.botAnalog = [](uint32_t routeId) { return routeId < ROUTE_END ? gRoutes[routeId].load() : -1.0f; };
		}

		using Clock = std::chrono::steady_clock;
		static const Clock::time_point start = Clock::now();
		const double t = std::chrono::duration<double>(Clock::now() - start).count();

		Clear();

		CActorHero* pHero = CActorHero::_gThis;
		CActorManager* pActorManager = CScene::ptable.g_ActorManager_004516a4;
		if (pHero == nullptr || pActorManager == nullptr || CCameraManager::_gThis == nullptr || CLevelScheduler::gThis == nullptr) {
			return;
		}

		static CActorFighter* pLastAdversary = nullptr;
		static float lastHeroLife = -1.0f;
		static double nextStatus = 0.0;

		const float heroLife = Life(pHero);
		if (lastHeroLife > 0.0f && heroLife <= 0.0f) {
			fprintf(stderr, "[bot] %.1f hero died\n", t);
		}
		lastHeroLife = heroLife;

		// Hero draw state: log every change of behaviour/state/visibility so a grab that leaves Kya hidden shows up.
		static int lastBehaviour = -2, lastState = -2;
		static uint lastVisibleFlags = 0xffffffff;
		const uint visibleFlags = pHero->flags & 0x4100;
		if (pHero->curBehaviourId != lastBehaviour || pHero->actorState != lastState || visibleFlags != lastVisibleFlags) {
			fprintf(stderr, "[bot] %.1f hero behaviour %d state %d flags 0x%x (vis 0x%x) loc %.1f %.1f %.1f sphere %.1f %.1f %.1f r %.1f\n", t, pHero->curBehaviourId, pHero->actorState,
				pHero->flags, visibleFlags, pHero->currentLocation.x, pHero->currentLocation.y, pHero->currentLocation.z,
				pHero->sphereCentre.x, pHero->sphereCentre.y, pHero->sphereCentre.z, pHero->sphereCentre.w);
			lastBehaviour = pHero->curBehaviourId;
			lastState = pHero->actorState;
			lastVisibleFlags = visibleFlags;
		}

		CActorFighter* pAdversary = pHero->pAdversary;
		if (pAdversary != pLastAdversary) {
			fprintf(stderr, "[bot] %.1f adversary %s\n", t, pAdversary ? pAdversary->name : "none");
			pLastAdversary = pAdversary;
		}

		if (t >= nextStatus) {
			nextStatus = t + 2.0;
			fprintf(stderr, "[bot] %.1f level 0x%x hero life %.1f behaviour %d adversary %s life %.1f\n", t, CLevelScheduler::gThis->currentLevelID,
				heroLife, pHero->curBehaviourId, pAdversary ? pAdversary->name : "-", pAdversary ? Life(pAdversary) : -1.0f);
			fflush(stderr);
		}

		if (pAdversary != nullptr) {
			// Fighter attacks are R2 / L2 (CActorFighter::UpdateFightCommandInternal). Rhythm: R2 x3 then L2, 6 frames down / 6 up.
			const int beat = static_cast<int>(t * 5.0);
			const bool bDown = (static_cast<int>(t * 10.0) & 1) == 0;
			const uint32_t route = (beat % 4 == 3) ? ROUTE_L2 : ROUTE_R2;
			gRoutes[route] = bDown ? 1.0f : 0.0f;
			SetStickToward(pAdversary->currentLocation.x - pHero->currentLocation.x, pAdversary->currentLocation.z - pHero->currentLocation.z);
			return;
		}

		CActor* pTarget = nullptr;
		float nearest = 1e9f;
		int nbWolfens = 0, nbAlive = 0, nbSector = 0;
		CSectorManager* pSectorManager = CScene::ptable.g_SectorManager_00451670;
		const int heroSector = pSectorManager ? pSectorManager->baseSector.currentSectorID : -1;
		for (int i = 0; i < pActorManager->nbActors; ++i) {
			CActor* pActor = pActorManager->aActors[i];
			if (pActor == nullptr || pActor->typeID != WOLFEN) {
				continue;
			}
			nbWolfens++;
			if (!static_cast<CActorFighter*>(pActor)->IsAlive()) {
				continue;
			}
			nbAlive++;
			if (pActor->sectorId != -1 && pActor->sectorId != heroSector) {
				continue;
			}
			nbSector++;
			const float dx = pActor->currentLocation.x - pHero->currentLocation.x;
			const float dy = pActor->currentLocation.y - pHero->currentLocation.y;
			const float dz = pActor->currentLocation.z - pHero->currentLocation.z;
			const float distance = sqrtf(dx * dx + dy * dy + dz * dz);
			if (distance < nearest) {
				nearest = distance;
				pTarget = pActor;
			}
		}

		static double nextTarget = 0.0;
		if (t >= nextTarget) {
			nextTarget = t + 2.0;
			fprintf(stderr, "[bot] %.1f wolfens %d alive %d in sector %d (hero sector %d)\n", t, nbWolfens, nbAlive, nbSector, heroSector);
			fprintf(stderr, "[bot] %.1f hero at %.1f %.1f %.1f target %s dist %.1f stick R%.2f L%.2f U%.2f D%.2f\n", t, pHero->currentLocation.x, pHero->currentLocation.y, pHero->currentLocation.z,
				pTarget ? pTarget->name : "-", pTarget ? nearest : -1.0f, gRoutes[ROUTE_L_ANALOG_RIGHT].load(), gRoutes[ROUTE_L_ANALOG_LEFT].load(), gRoutes[ROUTE_L_ANALOG_UP].load(), gRoutes[ROUTE_L_ANALOG_DOWN].load());
		}

		// Too far to walk: put Kya 3 units from the Wolfen, at most every 8 s.
		static double nextTeleport = 0.0;
		if (pTarget != nullptr && nearest > 8.0f && t >= nextTeleport) {
			nextTeleport = t + 8.0;
			edF32VECTOR4 position = pTarget->currentLocation;
			const float dx = pHero->currentLocation.x - position.x;
			const float dz = pHero->currentLocation.z - position.z;
			const float length = sqrtf(dx * dx + dz * dz);
			if (length > 0.001f) {
				position.x += dx / length * 3.0f;
				position.z += dz / length * 3.0f;
			}
			position.y += 0.5f;
			fprintf(stderr, "[bot] %.1f teleport to %s (was %.1f away)\n", t, pTarget->name, nearest);
			EnqueueLevelManageTask([position]() mutable {
				if (CActorHero::_gThis != nullptr) {
					CActorHero::_gThis->UpdatePosition(&position, true);
				}
			});
			return;
		}

		if (pTarget != nullptr && nearest > 2.0f) {
			SetStickToward(pTarget->currentLocation.x - pHero->currentLocation.x, pTarget->currentLocation.z - pHero->currentLocation.z);
		}
	}

	UpdateRegisterer sBotUpdateReg(Update);
}
