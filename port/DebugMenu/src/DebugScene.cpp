#include "DebugMenu.h"
#include "DebugScene.h"

#include "imgui.h"

#include "LargeObject.h"
#include "DebugSetting.h"
#include "ed3D.h"
#include "Rendering/DisplayList.h"
#include "LevelScheduler.h"
#include "ActorCheckpointManager.h"
#include "ActorHero.h"
#include "ActorHero_Private.h"
#include "ActorManager.h"
#include "SectorManager.h"
#include "WayPoint.h"
#include <cstdio>
#include "CinematicManager.h"
#include "CameraViewManager.h"
#include "TranslatedTextData.h"
#include "Audio.h"
#include "DebugMenuWorld.h"

namespace Debug {
	namespace Scene {
		Debug::Setting<int> gAutoLoadLevelId = { "Auto Load Level ID", -1 };

		void Startup()
		{
			gDebugLevelLoadOverride = gAutoLoadLevelId;

			// Reset, if we load successfully we will set it back.
			gAutoLoadLevelId = -1;
		}

		static void ShowMenu(bool* bOpen) {
			ImGui::Begin("Scene", bOpen, ImGuiWindowFlags_AlwaysAutoResize);

			ImGui::PushID("Scene");

			if (ImGui::Button("Term Scene")) {
				GameFlags = GameFlags | GAME_REQUEST_TERM;
			}

			gAutoLoadLevelId.DrawImguiControl();

			ImGui::SameLine();

			if (ImGui::Button("Reset")) {
				gAutoLoadLevelId = -1;
			}

			if (ImGui::CollapsingHeader("Teleport", ImGuiTreeNodeFlags_DefaultOpen)) {
				static Debug::Setting<int> gTeleportLevelId = { "Teleport Level ID", 0 };
				static Debug::Setting<int> gTeleportElevatorId = { "Teleport Elevator ID", 0 };
				static Debug::Setting<int> gTeleportCutsceneId = { "Teleport Cutscene ID", 0 };

				gTeleportLevelId.DrawImguiControl();
				gTeleportElevatorId.DrawImguiControl();
				gTeleportCutsceneId.DrawImguiControl();

				if (ImGui::Button("Level Teleport")) {
					EnqueueLevelManageTask([=]() {
						CScene::ptable.g_LevelScheduleManager_00451660->Level_Teleport(nullptr, gTeleportLevelId, gTeleportElevatorId, gTeleportCutsceneId, -1);
						});
				}
			}

			if (ImGui::CollapsingHeader("Scene List")) {
				static int selectedScene = -1;

				for (int i = 0; i < ged3DConfig.sceneCount; i++) {
					char buttonText[256];
					std::sprintf(buttonText, "gScene3D[%d]", i + 1);
					if (ImGui::Selectable(buttonText)) {
						selectedScene = i;
					}
				}

				static edNODE* pSelectedNode = nullptr;
				static ed_g3d_manager* pMeshInfo = nullptr;

				if (selectedScene != -1 && ImGui::CollapsingHeader("Scene Details", ImGuiTreeNodeFlags_DefaultOpen)) {
					ed_3D_Scene* pSelectedScene = &gScene3D[selectedScene];

					static bool bFilterAnim = false;
					ImGui::Checkbox("Filter Anim", &bFilterAnim);

					ImGui::Text("Shadow: %d", pSelectedScene->bShadowScene);
					ImGui::Text("Flags: 0x%x", pSelectedScene->flags);

					if (ImGui::CollapsingHeader("Hierarchy")) {
						edNODE* pCurNode;
						edLIST* pList = pSelectedScene->pHierListA;
						if (((pSelectedScene->flags & SCENE_FLAG_IN_USE) != 0) && ((pSelectedScene->flags & 4) == 0)) {
							for (pCurNode = pList->pPrev; (edLIST*)pCurNode != pList; pCurNode = pCurNode->pPrev) {

								ed_3d_hierarchy* pHierarchy = (ed_3d_hierarchy*)pCurNode->pData;

								if (pHierarchy && (pHierarchy->pAnimMatrix || !bFilterAnim)) {
									char nodeText[256];
									std::sprintf(nodeText, "Node: %p", pCurNode);
									if (ImGui::Selectable(nodeText)) {
										pSelectedNode = pCurNode;
									}
								}
							}
						}
						else {
							ImGui::Text("Flags disabled nodes");
						}
					}

					if (ImGui::CollapsingHeader("Mesh Nodes")) {
						edNODE* pClusterNode;

						if ((pSelectedScene->bShadowScene != 1) && ((pSelectedScene->flags & 8) == 0)) {
							for (pClusterNode = (edNODE*)(pSelectedScene->meshClusterList).pPrev;
								(edLIST*)pClusterNode != &pSelectedScene->meshClusterList; pClusterNode = pClusterNode->pPrev) {
								edCluster* pCluster = (edCluster*)pClusterNode->pData;

								char nodeText[256];
								std::sprintf(nodeText, "Cluster: %p (%p)", pClusterNode, pCluster->pMeshInfo->CSTA);
								if (ImGui::Selectable(nodeText)) {
									ed_g3d_manager* pMesh;
									if (((pCluster->flags & 2) == 0) && (pMesh = pCluster->pMeshInfo, pMesh->CSTA != (ed_Chunck*)0x0)) {
										pMeshInfo = pMesh;
									}
								}
							}
						}
					}
				}
			}

			ImGui::PopID();
			ImGui::End();
		}

		static int gHoldFrames = 0; static int gHoldSector = -1; static float gHold[3];
		static void TmpCmdHook()
		{
			if (gHoldFrames > 0 && CActorHero::_gThis) {
				gHoldFrames--;
				edF32VECTOR4 pos = CActorHero::_gThis->currentLocation;
				pos.x = gHold[0]; pos.y = gHold[1]; pos.z = gHold[2];
				reinterpret_cast<CActorHeroPrivate*>(CActorHero::_gThis)->UpdatePosition(&pos, false);
				if (CScene::ptable.g_SectorManager_00451670->baseSector.currentSectorID != gHoldSector) CScene::ptable.g_SectorManager_00451670->SwitchToSector(gHoldSector, false);
				if (gHoldFrames == 60 || gHoldFrames == 0) {
					CCameraManager* pCamMan = reinterpret_cast<CCameraManager*>(CScene::GetManager(MO_Camera));
					CActorHeroPrivate* h = reinterpret_cast<CActorHeroPrivate*>(CActorHero::_gThis);
					pCamMan->SetMainCamera(h->pMainCamera);
					h->pMainCamera->SetTarget(h);
					pCamMan->AlertCamera(2, (void*)1);
				}
			}
			const char* kCmd = "/private/tmp/claude-501/-Users-matteo-dev-kya/619f66d4-43a6-4a8e-bd4d-0b15727c29ea/scratchpad/cmd.txt";
			const char* kOut = "/private/tmp/claude-501/-Users-matteo-dev-kya/619f66d4-43a6-4a8e-bd4d-0b15727c29ea/scratchpad/cmd_out.txt";
			FILE* f = fopen(kCmd, "r");
			if (!f) return;
			char line[256] = {};
			fgets(line, sizeof(line), f);
			fclose(f);
			remove(kCmd);
			FILE* o = fopen(kOut, "a");
			auto* pAM = CScene::ptable.g_ActorManager_004516a4;
			auto* pSM = CScene::ptable.g_SectorManager_00451670;
			int n = 0; float x, y, z;
			if (strncmp(line, "dump", 4) == 0) {
				if (o && pSM) fprintf(o, "curSector %d level %d\n", pSM->baseSector.currentSectorID, CScene::ptable.g_LevelScheduleManager_00451660->currentLevelID);
				if (o && CActorHero::_gThis) fprintf(o, "hero %.1f %.1f %.1f\n", CActorHero::_gThis->currentLocation.x, CActorHero::_gThis->currentLocation.y, CActorHero::_gThis->currentLocation.z);
				for (int i = 0; pAM && i < pAM->nbActors; i++) {
					CActor* a = pAM->aActors[i];
					if (a && a->typeID == CHECKPOINT_MANAGER) {
						auto* m = static_cast<CActorCheckpointManager*>(a);
						for (int c = 0; c < m->checkpointCount; c++) {
							CWayPoint* w = m->aCheckpoints[c].pWayPointA.Get();
							if (o) fprintf(o, "cp %d sector %d flags 0x%x wp %.1f %.1f %.1f\n", c, m->aCheckpoints[c].sectorId, m->aCheckpoints[c].flags, w ? w->location.x : 0.f, w ? w->location.y : 0.f, w ? w->location.z : 0.f);
						}
					}
				}
			}
			else if (strncmp(line, "cine", 4) == 0 && o) {
				auto* cm = g_CinematicManager_0048efc;
				fprintf(o, "level %d numCine %d\n", CScene::ptable.g_LevelScheduleManager_00451660->currentLevelID, cm->numCutscenes_0x8);
				for (int i = 0; i < cm->numCutscenes_0x8; i++) {
					CCinematic* c = cm->ppCinematicObjB_A[i];
					if (!c) continue;
					CActor* trig = c->triggerActorRef.Get();
					CActor* rb = c->actorRefB.Get();
					fprintf(o, "CINE %d file=%s bankA=%s bankB=%s nbCinActors=%d nbLevelActors=%d flags4=0x%x uid=0x%x end=%d/%d/%d trig=%s refB=%s zones=%d/%d/%d cfg=%d:",
						i, c->fileName, c->pBankName_0x48, c->pBankName_0x50, c->nbTotalCinematicActors, c->nbTotalLevelActors, c->flags_0x4, c->uniqueIdentifier,
						c->endLevelId, c->endElevatorId, c->endCutsceneId, trig ? trig->name : "-", rb ? rb->name : "-", c->zoneRefA.index, c->zoneRefB.index, c->zoneRefC.index, c->cineActorConfigCount);
					for (int k = 0; k < c->cineActorConfigCount; k++) { CActor* a = c->aCineActorConfig[k].pActor.Get(); fprintf(o, " %s", a ? a->name : "?"); }
					{ ed_zone_3d* zb = c->zoneRefB.Get(); ed_zone_3d* za = c->zoneRefA.Get();
						fprintf(o, " | audio def=%d gb=%d", (int)c->defaultAudioTrackId, (int)c->aAudioTrackIds[CMessageFile::get_default_language()]);
						int at = (int)c->aAudioTrackIds[CMessageFile::get_default_language()]; if (at == -1) at = (int)c->defaultAudioTrackId;
						auto* pAud = CScene::ptable.g_AudioManager_00451698;
						if (at >= 0 && at < pAud->field_0x30) fprintf(o, " stream=%s", pAud->GetStreamFileNameFromIndex_00184a40(at));
						if (za) fprintf(o, " zoneA=%.1f,%.1f,%.1f r%.1f", za->boundSphere.x, za->boundSphere.y, za->boundSphere.z, za->boundSphere.w);
						if (zb) fprintf(o, " zoneB=%.1f,%.1f,%.1f r%.1f", zb->boundSphere.x, zb->boundSphere.y, zb->boundSphere.z, zb->boundSphere.w);
						fprintf(o, " text=%d", c->textData.entryCount); }
					fprintf(o, "\n");
				}
			}
			else if (strncmp(line, "linfo", 5) == 0 && o) {
				auto* ls = CScene::ptable.g_LevelScheduleManager_00451660;
				for (int L = 0; L < 16; L++) {
					S_LEVEL_INFO& li = ls->aLevelInfo[L];
					fprintf(o, "LEVEL %d maxSector %d maxElev %d sectors:", L, li.maxSectorId, li.maxElevatorId);
					for (int k = 0; k <= li.maxSectorId && k < 30; k++) fprintf(o, " %d:%d", k, li.aSectorSubObj[k].bankSize);
					fprintf(o, " | elev:");
					for (int k = 0; k < li.maxElevatorId && k < 12; k++) {
						CActor* t = (L == ls->currentLevelID) ? pAM->GetActorByHashcode(li.aSubSectorInfo[k].teleporterActorHashCode) : nullptr;
						fprintf(o, " %d:0x%x(%s,sec%d,wolf%d)", k, li.aSubSectorInfo[k].teleporterActorHashCode, t ? t->name : "-", t ? t->sectorId : -9, li.aSubSectorInfo[k].nbMaxExorcisedWolfen);
					}
					fprintf(o, "\n");
				}
			}
			else if (sscanf(line, "lvl %d %f", &n, &x) == 2) {
				int e = (int)x;
				EnqueueLevelManageTask([=]() { CScene::ptable.g_LevelScheduleManager_00451660->Level_Teleport(nullptr, n, e, -1, -1); });
				if (o) fprintf(o, "lvl %d %d\n", n, e);
			}
			else if (sscanf(line, "cpsec %d", &n) == 1) {
				CActorHeroPrivate* h = reinterpret_cast<CActorHeroPrivate*>(CActorHero::_gThis);
				h->lastCheckPointSector = n; h->field_0xea0 = n;
				h->ProcessDeath();
				if (o) fprintf(o, "cpsec %d\n", n);
			}
			else if (sscanf(line, "start %d %f", &n, &x) == 2) {
				auto* ls = CScene::ptable.g_LevelScheduleManager_00451660;
				if (o) fprintf(o, "start L%d was %d -> %d\n", n, ls->aLevelInfo[n].sectorStartIndex, (int)x);
				ls->aLevelInfo[n].sectorStartIndex = (int)x;
			}
			else if (sscanf(line, "go %d", &n) == 1) {
				for (int i = 0; pAM && i < pAM->nbActors; i++) {
					CActor* a = pAM->aActors[i];
					if (a && a->typeID == CHECKPOINT_MANAGER) {
						auto* m = static_cast<CActorCheckpointManager*>(a);
						EnqueueLevelManageTask([m, n]() { m->ActivateCheckpoint(n); CScene::_pinstance->Level_CheckpointReset(); });
						if (o) fprintf(o, "go cp %d sector %d\n", n, m->aCheckpoints[n].sectorId);
						break;
					}
				}
			}
			else if (sscanf(line, "pos %f %f %f", &x, &y, &z) == 3) {
				EnqueueLevelManageTask([=]() { edF32VECTOR4 v = { x, y, z, 1.0f }; CActorHero::_gThis->UpdatePosition(&v, true); });
				if (o) fprintf(o, "pos %.1f %.1f %.1f\n", x, y, z);
			}
			else if (sscanf(line, "ntf %d %f", &n, &x) == 2) {
				int m = (int)x;
				EnqueueLevelManageTask([=]() { g_CinematicManager_0048efc->NotifyCinematic(n, CActorHero::_gThis, m, 0); });
				if (o) fprintf(o, "ntf %d 0x%x\n", n, m);
			}
			else if (sscanf(line, "block %d", &n) == 1) {
				// Simulates field_0x4c == 0 in the cinematic table entry (CCinematic::Create sets CONDITION_BLOCKED).
				CCinematic* c = g_CinematicManager_0048efc->ppCinematicObjB_A[n];
				c->flags_0x8 |= CINEMATIC_RUNTIME_FLAG_CONDITION_BLOCKED;
				if (o) fprintf(o, "block %d %s flags8=0x%x f4c=%u\n", n, c->fileName, c->flags_0x8, c->field_0x4c);
			}
			else if (sscanf(line, "state %d", &n) == 1) {
				CCinematic* c = g_CinematicManager_0048efc->ppCinematicObjB_A[n];
				if (o) fprintf(o, "state %d %s state=%d flags8=0x%x f4c=%u\n", n, c->fileName, (int)c->state, c->flags_0x8, c->field_0x4c);
			}
			else if (sscanf(line, "play %d", &n) == 1) {
				auto* cm = g_CinematicManager_0048efc;
				CCinematic* c = cm->ppCinematicObjB_A[n];
				EnqueueLevelManageTask([c]() { c->flags_0x8 &= ~CINEMATIC_RUNTIME_FLAG_ONE_SHOT_LOCKED; c->Load(1); c->Start(); });
				if (o) fprintf(o, "play %d %s\n", n, c->fileName);
			}
			else if (sscanf(line, "elev %d", &n) == 1) {
				CScene::ptable.g_LevelScheduleManager_00451660->Level_Teleport(nullptr, CScene::ptable.g_LevelScheduleManager_00451660->currentLevelID, n, -1, -1);
				if (o) fprintf(o, "elev %d\n", n);
			}
			else if (sscanf(line, "cp %d", &n) == 1) {
				for (int i = 0; pAM && i < pAM->nbActors; i++) {
					CActor* a = pAM->aActors[i];
					if (a && a->typeID == CHECKPOINT_MANAGER) {
						auto* m = static_cast<CActorCheckpointManager*>(a);
						m->ActivateCheckpoint(n);
						CActorHeroPrivate* h = reinterpret_cast<CActorHeroPrivate*>(CActorHero::_gThis);
						if (o) fprintf(o, "hero lastCPsec %d ea0 %d -> %d\n", h->lastCheckPointSector, h->field_0xea0, m->aCheckpoints[n].sectorId);
						h->lastCheckPointSector = m->aCheckpoints[n].sectorId;
						h->ProcessDeath();
						if (o) fprintf(o, "cp %d activated\n", n);
						break;
					}
				}
			}
			else if (sscanf(line, "sector %d", &n) == 1) {
				if (pSM) pSM->SwitchToSector(n, false);
				if (o) fprintf(o, "switch sector %d\n", n);
			}
			else if (sscanf(line, "tp %f %f %f", &x, &y, &z) == 3) {
				edF32VECTOR4 pos = CActorHero::_gThis->currentLocation;
				pos.x = x; pos.y = y; pos.z = z;
				reinterpret_cast<CActorHeroPrivate*>(CActorHero::_gThis)->UpdatePosition(&pos, false);
				if (o) fprintf(o, "tp %.1f %.1f %.1f\n", x, y, z);
			}
			if (o) fclose(o);
		}

		void Update()
		{
			TmpCmdHook();
			static bool bSuccesfullyLoaded = false;

			if (!bSuccesfullyLoaded) {
				if (CScene::ptable.g_LevelScheduleManager_00451660->currentLevelID == gDebugLevelLoadOverride) {
					bSuccesfullyLoaded = true;
					gAutoLoadLevelId = gDebugLevelLoadOverride;
				}
			}
		}
	} // namespace Scene
} // namespace Debug

namespace Debug {
	MenuRegisterer sDebugSceneMenuReg("Scene", Debug::Scene::ShowMenu, true);
	StartupRegisterer sDebugSceneStartupReg(Debug::Scene::Startup);
	UpdateRegisterer sDebugSceneUpdateReg(Debug::Scene::Update);
}

