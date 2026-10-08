#include "ActorShoot.h"
#include "MemoryStream.h"
#include "MathOps.h"
#include "TimeController.h"
#include "ActorHero.h"
#include "EventManager.h"

CActorShoot::~CActorShoot()
{
	//(this->base).base.base.pVTable = (CActorFighterVTable*)&_vt;
	//lVar1 = (**(code**)(*(int*)&this->field_0x360 + 0x14))();
	//if (lVar1 != 0) {
	//	StaticMeshComponent::Term((StaticMeshComponent*)&this->field_0x360, CScene::_scene_handleA);
	//}
	this->addOnGenerator.Term();
	//if (this != (CActorShoot*)0xfffff810) {
	//	*(CBehaviourVtable**)&this->field_0x7f0 = &CBehaviourShootFireWave::_vt;
	//	CConicalWaveShoot::~CConicalWaveShoot((long)(int)&this->conicalWaveShoot, 0xffffffffffffffff);
	//	if ((this != (CActorShoot*)0xfffff810) && (*(CBehaviourVtable**)&this->field_0x7f0 = &CBehaviourShoot::_vt, this != (CActorShoot*)0xfffff810)) {
	//		*(CBehaviourVtable**)&this->field_0x7f0 = &CBehaviour::_vt;
	//	}
	//}
	//if (((this != (CActorShoot*)0xfffffad0) && (*(CBehaviourVtable**)&this->field_0x530 = &CBehaviourShootFire::_vt, this != (CActorShoot*)0xfffffad0)) &&
	//	(*(CBehaviourVtable**)&this->field_0x530 = &CBehaviourShoot::_vt, this != (CActorShoot*)0xfffffad0)) {
	//	*(CBehaviourVtable**)&this->field_0x530 = &CBehaviour::_vt;
	//}
	//StaticMeshComponent::~StaticMeshComponent((StaticMeshComponent*)&this->field_0x360, -1);
	//if (lVar2 != 0) {
	//	(this->base).base.base.pVTable = (CActorFighterVTable*)&CActorAutonomous::_vt;
	//	CLifeInterface::~CLifeInterface((long)(int)&(this->base).lifeInterface, 0xffffffffffffffff);
	//	CActorMovable::~CActorMovable(lVar2, 0);
	//}
}

void CActorShoot::Create(ByteCode* pByteCode)
{
	CActorAutonomous::Create(pByteCode);

	this->field_0x350 = pByteCode->GetU32();
	this->field_0x354 = pByteCode->GetU32();
	this->staticMeshComponent.textureIndex = pByteCode->GetS32();
	this->staticMeshComponent.meshIndex = pByteCode->GetS32();
	this->staticMeshComponent.Reset();
	this->field_0x3c0 = pByteCode->GetF32();
	this->field_0x3c4 = pByteCode->GetF32();
	this->field_0x3c8 = pByteCode->GetF32();
	this->field_0x3cc = pByteCode->GetF32();
	this->field_0x3d0 = pByteCode->GetU32();
	this->field_0x3d4 = pByteCode->GetF32();
	this->field_0x3d8 = pByteCode->GetF32();
	this->field_0x3dc = pByteCode->GetF32();
	this->field_0x3e0 = pByteCode->GetF32();
	this->field_0x3e8 = pByteCode->GetU32();
	this->field_0x3e4 = pByteCode->GetU32();
	this->field_0x3ec = pByteCode->GetU32();
	this->field_0x3f0 = pByteCode->GetU32();

	this->addOnGenerator.Create(this, pByteCode);

	return;
}

void CActorShoot::Init()
{
	CActorAutonomous::Init();
	ClearLocalData();

	memset(&this->altHierarchySetup, 0, sizeof(ed_3d_hierarchy_setup));
	this->altBoundingSphere = this->subObjA->boundingSphere;
	this->altHierarchySetup.pBoundingSphere = &this->altBoundingSphere;
	this->altClipping = *this->hierarchySetup.clipping_0x0;
	this->altHierarchySetup.clipping_0x0 = &this->altClipping;

	this->addOnGenerator.Init(0);
	return;
}

void CActorShoot::ComputeLighting()
{
	CScene::ptable.g_LightManager_004516b0->ComputeLighting(this->lightingFloat_0xe0, this, this->lightingFlags, &this->lightingConfig);

	return;
}

void CActorShoot::Reset()
{
	CActorAutonomous::Reset();
	ClearLocalData();

	return;
}

struct S_SAVE_CLASS_SHOOT
{
	uint field_0x0;
};

void CActorShoot::SaveContext(void* pData, uint mode, uint maxSize)
{
	S_SAVE_CLASS_SHOOT* pSaveData = reinterpret_cast<S_SAVE_CLASS_SHOOT*>(pData);

	if (mode == 1) {
		pSaveData->field_0x0 = 0.0f < GetLifeInterface()->GetValue();
	}

	return;
}

void CActorShoot::LoadContext(void* pData, uint mode, uint maxSize)
{
	S_SAVE_CLASS_SHOOT* pSaveData = reinterpret_cast<S_SAVE_CLASS_SHOOT*>(pData);

	if ((mode == 1) && (pSaveData->field_0x0 == 0)) {
		LifeAnnihilate();

		this->flags = this->flags & 0xffffff7f;
		this->flags = this->flags | 0x20;

		EvaluateDisplayState();

		this->flags = this->flags & 0xfffffffd;
		this->flags = this->flags | 1;

		SetBehaviour(SHOOT_BEHAVIOUR_FIRE_WAVE, 0xf, -1);
	}

	return;
}

CBehaviour* CActorShoot::BuildBehaviour(int behaviourType)
{
	CBehaviour* pBehaviour;

	if (behaviourType == SHOOT_BEHAVIOUR_FIRE_WAVE) {
		IMPLEMENTATION_GUARD();
		pBehaviour = &this->behaviourShootFireWave;
	}
	else {
		if (behaviourType == SHOOT_BEHAVIOUR_FIRE) {
			pBehaviour = &this->behaviourShootFire;
		}
		else {
			pBehaviour = CActorAutonomous::BuildBehaviour(behaviourType);
		}
	}

	return pBehaviour;
}

StateConfig CActorShoot::_gStateCfg_SHT[10] = {
	{ 0x00, 0x104 },
	{ 0x0C, 0x144 },
	{ 0x0D, 0x944 },
	{ 0x06, 0x9C4 },
	{ 0x0E, 0x9C4 },
	{ 0x0F, 0x9C4 },
	{ 0x10, 0x9C4 },
	{ 0x06, 0x104 },
	{ 0x12, 0x104 },
	{ 0x14, 0x101 },
};

StateConfig* CActorShoot::GetStateCfg(int state)
{
	StateConfig* pStateConfig;

	if (state < 5) {
		pStateConfig = CActor::GetStateCfg(state);
	}
	else {
		assert((state - 6) < 10);
		pStateConfig = _gStateCfg_SHT + state + -6;
	}

	return pStateConfig;
}

void CActorShoot::ChangeManageState(int state)
{
	bool bVar1;

	CActor::ChangeManageState(state);

	if ((state == 0) && (bVar1 = this->staticMeshComponent.HasMesh(), bVar1 != false)) {
		this->staticMeshComponent.Term(CScene::_scene_handleA);
	}

	return;
}

int CActorShoot::InterpretMessage(CActor* pSender, int msg, void* pMsgParam)
{
	CLifeInterface* pCVar2;
	int iVar3;
	edF32VECTOR4* v0;
	float fVar4;
	edF32VECTOR4 eStack32;
	edF32VECTOR4 eStack16;

	if (msg == 3) {
		pCVar2 = GetLifeInterface();
		fVar4 = pCVar2->GetValue();
		if (0.0f < fVar4) {
			if ((pSender->typeID != JAMGUT) && (pSender->typeID != ACTOR_HERO_PRIVATE)) {
				SetState(0xf, -1);

				return 1;
			}

			if ((this->field_0x3f0 & 4) == 0) {
				SetState(0xf, -1);

				return 1;
			}
		}
	}
	else {
		if (msg != 2) {
			iVar3 = CActor::InterpretMessage(pSender, msg, pMsgParam);
			return iVar3;
		}

		_msg_hit_param* pHitParam = (_msg_hit_param*)pMsgParam;

		if ((this->field_0x43c != 0) && (pHitParam->projectileType == 4)) {
			this->dynamicExt.normalizedTranslation.x = 0.0f;
			this->dynamicExt.normalizedTranslation.y = 0.0f;
			this->dynamicExt.normalizedTranslation.z = 0.0f;
			this->dynamicExt.normalizedTranslation.w = 0.0f;
			this->dynamicExt.field_0x6c = 0.0f;

			eStack16.y = 1.0f;
			eStack16.x = 0.0f;
			eStack16.z = 0.0f;
			eStack16.w = 0.0f;

			edF32Vector4ScaleHard(300.0f, &eStack16, &eStack16);
			edF32Vector4ScaleHard(0.02f / GetTimer()->cutsceneDeltaTime, &eStack32, &eStack16);
			v0 = this->dynamicExt.aImpulseVelocities;
			edF32Vector4AddHard(v0, v0, &eStack32);
			fVar4 = edF32Vector4GetDistHard(this->dynamicExt.aImpulseVelocities);
			this->dynamicExt.aImpulseVelocityMagnitudes[0] = fVar4;

			LifeDecrease(pHitParam->damage);

			pCVar2 = GetLifeInterface();
			fVar4 = pCVar2->GetValue();
			if (fVar4 <= 0.0f) {
				SetState(0xf, -1);
			}
			else {
				if (this->field_0x438 == (CActorHero*)0x0) {
					SetState(8, 0x13);
					this->field_0x438 = CActorHero::_gThis;
				}
				else {
					if (this->currentAnimType == 0x13) {
						RestartCurAnim();
					}
					else {
						PlayAnim(0x13);
					}
				}
			}

			return 1;
		}
	}

	return 0;
}

void CActorShoot::ClearLocalData()
{
	float fVar1;
	float fVar2;
	bool bVar3;
	int iVar4;
	edF32MATRIX4* peVar5;
	edF32MATRIX4* peVar6;
	float fVar7;
	CAnimation* pAnim;

	bVar3 = this->staticMeshComponent.HasMesh();
	if (bVar3 != false) {
		this->staticMeshComponent.Term((ed_3D_Scene*)0x0);
	}
	this->staticMeshComponent.Reset();

	this->field_0x438 = (CActorHero*)0x0;
	this->field_0x43c = true;

	this->field_0x3f8 = this->field_0x3cc;
	this->field_0x3fc = this->field_0x3d4;
	this->field_0x3f4 = 0.0f;
	this->field_0x400 = 0;
	this->field_0x440 = 0;

	this->lightAmbient = gF32Vector4Zero;
	this->lightDirection = gF32Matrix4Unit;
	this->lightColor = gF32Matrix4Unit;
	
	(this->lightingConfig).pLightAmbient = &this->lightAmbient;
	(this->lightingConfig).pLightDirections = &this->lightDirection;
	(this->lightingConfig).pLightColorMatrix = &this->lightColor;

	this->field_0x43f = 0;
	this->field_0x43e = false;

	pAnim = this->pAnimationController;
	iVar4 = GetIdMacroAnim(0x11);
	if (iVar4 < 0) {
		fVar7 = 0.0f;
	}
	else {
		fVar7 = pAnim->GetAnimLength(iVar4, 0);
	}

	if (this->field_0x3cc < fVar7 + 0.15f) {
		this->field_0x3cc = fVar7 + 0.15f;
	}

	return;
}

void CActorShoot::BehaviourShootFire_Manage(CBehaviourShootFire* pBehaviour)
{
	bool bVar1;
	edF32VECTOR4* peVar2;
	int iVar3;
	float fVar4;
	edF32VECTOR4 eStack32;
	edF32VECTOR4 eStack16;

	UpdateVulnerability();
	pBehaviour->fireshot.ManageShots();
	UpdateFireTimers();

	if (((GetStateFlags(this->actorState) & 0x800) != 0) && (this->field_0x438 != (CActorHero*)0x0)) {
		SetLookingAtOn(0.0f);
		SetLookingAtRotationHeight(3.141593f, &this->field_0x438->currentLocation);
	}

	switch (this->actorState) {
	case 6:
		ManageDyn(4.0f, 0x100a023b, (CActorsTable*)0x0);

		iVar3 = CheckDetection();
		if (iVar3 == 1) {
			bVar1 = this->staticMeshComponent.textureIndex != -1;
			if (bVar1) {
				bVar1 = this->staticMeshComponent.meshIndex != -1;
			}

			if (bVar1) {
				SetState(7, -1);
			}
			else {
				SetState(8, -1);
			}
		}
		break;
	case 7:
		if (this->field_0x438 != (CActorHero*)0x0) {
			SV_UpdateOrientationToPosition2D(0.7853982f, &this->field_0x438->currentLocation);
		}

		ManageDyn(4.0f, 0x1002023b, (CActorsTable*)0x0);

		eStack16 = gF32Vector4UnitY;
		peVar2 = &this->currentLocation;
		fVar4 = this->timeInAir;
		bVar1 = this->staticMeshComponent.HasMesh();
		if ((bVar1 != false) && (peVar2 != (edF32VECTOR4*)0x0)) {
			if (0.4f < fVar4) {
				fVar4 = 0.4f;
			}
			else {
				if (fVar4 < 0.0f) {
					fVar4 = 0.0f;
				}
			}

			edFIntervalLERP(fVar4, 0.0f, 0.4f, 0.0f, 0.32f);
			edF32Vector4ScaleHard(-(0.32f - fVar4), &eStack16, &eStack16);
			edF32Vector4AddHard(&eStack32, peVar2, &eStack16);

			if (this->staticMeshComponent.pMeshTransformData != (ed_3d_hierarchy_node*)0x0) {
				this->staticMeshComponent.pMeshTransformData->base.transformA.rowT = eStack32;
			}
		}

		if (0.4f < this->timeInAir) {
			SetState(8, -1);
		}
		break;
	case 8:
		if (this->field_0x438 != (CActorHero*)0x0) {
			SV_UpdateOrientationToPosition2D(0.7853982f, &this->field_0x438->currentLocation);
		}

		ManageDyn(4.0f, 0x1002023b, (CActorsTable*)0x0);

		iVar3 = CheckDetection();
		if (iVar3 == 2) {
			if ((this->field_0x3f0 & 1) == 0) {
				if (this->currentAnimType == 0x13) {
					SetState(6, 0x13);
				}
				else {
					SetState(6, -1);
				}
			}
			else {
				SetState(0xe, -1);
			}
		}
		else {
			if ((this->pAnimationController->IsCurrentLayerAnimEndReached(0)) && (this->currentAnimType == 0xd)) {
				SetState(9, -1);
			}
		}
		break;
	case 9:
		StateShootApproach(pBehaviour);
		break;
	case 0xc:
		StateShootFire(pBehaviour);
		break;
	case 0xd:
		StateShootComeBack(pBehaviour);
		break;
	case 0xe:
		if (this->pAnimationController->IsCurrentLayerAnimEndReached(0)) {
			SetState(6, -1);
		}
		break;
	case 0xf:
		if (this->pAnimationController->IsCurrentLayerAnimEndReached(0)) {
			this->flags = this->flags & 0xffffff7f;
			this->flags = this->flags | 0x20;
			EvaluateDisplayState();
			this->flags = this->flags & 0xfffffffd;
			this->flags = this->flags | 1;
		}
		break;
	}

	if ((this->currentAnimType == 0x13) && (this->pAnimationController->IsCurrentLayerAnimEndReached(0))) {
		SetState(this->actorState, -1);
	}

	return;
}

void CActorShoot::StateShootComeBack(CBehaviourShoot* pBehaviour)
{
	int iVar1;
	CActorMovParamsOut movParamsOut;
	CActorMovParamsIn movParamsIn;

	movParamsOut.flags = 0;
	movParamsIn.flags = 0;
	movParamsIn.pRotation = (edF32VECTOR4*)0x0;
	movParamsIn.flags = movParamsIn.flags | 0x50;
	movParamsIn.rotSpeed = this->field_0x3c4;
	movParamsIn.flags = movParamsIn.flags | 2;
	movParamsIn.speed = this->field_0x3c0;
	movParamsIn.acceleration = 10.0f;
	movParamsIn.flags = movParamsIn.flags | 0x400;
	SV_AUT_MoveTo(&movParamsOut, &movParamsIn, &this->baseLocation);

	ManageDyn(4.0f, 0x1002023b, (CActorsTable*)0x0);

	if (movParamsOut.moveVelocity < 1.0f) {
		this->dynamic.speed = 0.0f;

		if ((this->field_0x3f0 & 1) == 0) {
			if (this->currentAnimType == 0x13) {
				SetState(6, 0x13);
			}
			else {
				SetState(6, -1);
			}
		}
		else {
			SetState(0xe, -1);
		}
	}

	iVar1 = CheckDetection();
	if (iVar1 == 1) {
		SetState(9, -1);
	}

	return;
}

void CActorShoot::StateShootFire(CBehaviourShootFire* pBehaviour)
{
	int turnAnim;
	int iVar1;
	bool bVar2;
	edF32VECTOR4 toHero;
	edF32VECTOR4 bonePosition;
	edF32VECTOR4 direction;
	edF32VECTOR4 target;

	turnAnim = GetTurnAnim(this->field_0x3c4, this->field_0x438);

	bVar2 = false;
	if (((this->field_0x43f != 0) && (this->field_0x438 != (CActorHero*)0x0)) && (this->currentAnimType != 0x11)) {
		bVar2 = SV_UpdateOrientationToPosition2D(this->field_0x3c4, &this->field_0x438->currentLocation);
	}

	ManageDyn(4.0f, 0x1002023b, (CActorsTable*)0x0);

	if (bVar2 == true) {
		this->field_0x43f = 0;
	}

	iVar1 = CheckDetection();
	if (iVar1 == 2) {
		SetState(0xd, -1);
	}
	else {
		edF32Vector4SubHard(&toHero, &this->field_0x438->currentLocation, &this->currentLocation);
		toHero.y = 0.0f;
		if (this->field_0x3dc < edF32Vector4GetDistHard(&toHero)) {
			SetState(9, -1);
		}
		else {
			if ((this->field_0x43f == 0) && (this->field_0x43d != 0)) {
				SV_GetBoneWorldPosition(pBehaviour->field_0x2b0, &bonePosition);
				edF32Vector4SubHard(&direction, &this->field_0x438->currentLocation, &bonePosition);
				direction.y = 0.0f;
				edF32Vector4NormalizeHard(&direction, &direction);
				edF32Vector4ScaleHard(pBehaviour->field_0x2b4, &direction, &direction);
				edF32Vector4AddHard(&target, &bonePosition, &direction);
				pBehaviour->fireshot.FireNewShotStraight(&bonePosition, &target, this);

				this->field_0x3f4 = 0.0f;
				this->field_0x3f8 = 0.0f;
				this->field_0x400 = this->field_0x400 + 1;
			}

			if (this->currentAnimType == 0x11) {
				if (this->pAnimationController->IsCurrentLayerAnimEndReached(0)) {
					if (this->field_0x43f != 0) {
						PlayAnim(turnAnim);
					}
					else {
						if (this->field_0x43e == false) {
							PlayAnim(0x10);
						}
						else {
							RestartCurAnim();
							this->field_0x43e = false;
						}
					}
				}
			}
			else {
				if (this->field_0x43f != 0) {
					PlayAnim(turnAnim);
				}
				else {
					if (this->field_0x43e == false) {
						PlayAnim(0x10);
					}
					else {
						PlayAnim(0x11);
						this->field_0x43e = false;
					}
				}
			}
		}
	}

	return;
}

void CActorShoot::StateShootApproach(CBehaviourShoot* pBehaviour)
{
	int iVar1;
	CActorMovParamsOut movParamsOut;
	CActorMovParamsIn movParamsIn;

	movParamsOut.flags = 0;
	movParamsIn.flags = 0;
	movParamsIn.pRotation = (edF32VECTOR4*)0x0;
	movParamsIn.flags = movParamsIn.flags | 0x50;
	movParamsIn.rotSpeed = this->field_0x3c4;
	movParamsIn.flags = movParamsIn.flags | 2;
	movParamsIn.speed = this->field_0x3c0;
	movParamsIn.acceleration = 10.0f;
	movParamsIn.flags = movParamsIn.flags | 0x400;
	SV_AUT_MoveTo(&movParamsOut, &movParamsIn, &this->field_0x438->currentLocation);

	ManageDyn(4.0f, 0x1002023b, (CActorsTable*)0x0);

	if (movParamsOut.moveVelocity <= this->field_0x3d8) {
		this->dynamic.speed = 0.0f;
		SetState(0xc, -1);
	}
	else {
		iVar1 = CheckDetection();
		if (iVar1 == 2) {
			SetState(0xd, -1);
		}
	}

	return;
}

void CActorShoot::UpdateVulnerability()
{
	switch (this->actorState) {
	case 6:
		this->field_0x43c = 1;
		break;
	case 7:
	case 8:
		this->field_0x43c = (this->field_0x3ec & 1) == 0;
		break;
	case 9:
		this->field_0x43c = (this->field_0x3ec & 4) == 0;
		break;
	case 0xc:
		this->field_0x43c = (this->field_0x3ec & 8) == 0;
		break;
	case 0xd:
		this->field_0x43c = (this->field_0x3ec & 0x20) == 0;
		break;
	case 0xe:
		this->field_0x43c = 0;
		break;
	default:
		this->field_0x43c = 1;
	}

	return;
}

int CActorShoot::GetTurnAnim(float rotSpeed, CActor* pTarget)
{
	int turnAnim;
	float fVar1;
	float angle;
	edF32VECTOR4 toTarget;
	edF32VECTOR4 flatToTarget;
	edF32VECTOR4 flatRotation;

	if (pTarget == (CActor*)0x0) {
		return -1;
	}

	turnAnim = -1;

	edF32Vector4SubHard(&toTarget, &pTarget->currentLocation, &this->currentLocation);
	edF32Vector4NormalizeHard(&toTarget, &toTarget);

	flatRotation.x = this->rotationQuat.x;
	flatRotation.y = 0.0f;
	flatRotation.z = this->rotationQuat.z;
	flatRotation.w = 0.0f;
	edF32Vector4NormalizeHard(&flatRotation, &flatRotation);

	flatToTarget.y = 0.0f;
	flatToTarget.w = 0.0f;
	flatToTarget.x = toTarget.x;
	flatToTarget.z = toTarget.z;
	edF32Vector4NormalizeHard(&flatToTarget, &flatToTarget);

	GetTimer();

	fVar1 = edF32Vector4DotProductHard(&flatRotation, &flatToTarget);
	if (1.0f < fVar1) {
		fVar1 = 1.0f;
	}
	else {
		if (fVar1 < -1.0f) {
			fVar1 = -1.0f;
		}
	}

	angle = acosf(fVar1);

	if (flatRotation.x * flatToTarget.z - flatToTarget.x * flatRotation.z < 0.0f) {
		if ((0.7853982f < angle) || (this->field_0x43f != 0)) {
			this->field_0x43f = 1;
			turnAnim = 0xf;
		}
	}
	else {
		if ((0.7853982f < angle) || (this->field_0x43f != 0)) {
			this->field_0x43f = 1;
			turnAnim = 0xe;
		}
	}

	return turnAnim;
}

void CActorShoot::UpdateFireTimers()
{
	bool bCanRestart;
	bool bCanFire;

	bCanRestart = false;
	bCanFire = false;

	this->field_0x3fc = this->field_0x3fc + GetTimer()->cutsceneDeltaTime;
	this->field_0x3f8 = this->field_0x3f8 + GetTimer()->cutsceneDeltaTime;

	if (this->field_0x3d4 <= this->field_0x3fc) {
		this->field_0x3fc = this->field_0x3d4;

		if (this->field_0x3cc <= this->field_0x3f8) {
			if ((this->actorState == 0xc) && (this->currentAnimType == 0x11)) {
				this->field_0x3f4 = this->field_0x3f4 + GetTimer()->cutsceneDeltaTime;
			}

			if (this->field_0x3c8 < this->field_0x3f4) {
				bCanFire = true;
			}

			bCanRestart = true;
		}

		if (this->field_0x400 == this->field_0x3d0) {
			this->field_0x400 = 0;
			this->field_0x3fc = 0.0f;
		}
	}

	this->field_0x43e = bCanRestart;
	this->field_0x43d = bCanFire;

	return;
}

int CActorShoot::CheckDetection()
{
	CEventManager* pEventManager;
	CActorHero* pHero;
	ed_zone_3d* pDetectZone;
	ed_zone_3d* pTriggerZone;
	int detectResult;
	int triggerResult;
	edF32VECTOR4 toHero;

	pEventManager = CScene::ptable.g_EventManager_006f5080;

	pDetectZone = (ed_zone_3d*)0x0;
	if (this->field_0x3e4 != -1) {
		pDetectZone = edEventGetChunkZone(pEventManager->activeChunkId, this->field_0x3e4);
	}

	pTriggerZone = (ed_zone_3d*)0x0;
	if (((this->field_0x3f0 & 2) == 0) && (this->field_0x3e8 != -1)) {
		pTriggerZone = edEventGetChunkZone(pEventManager->activeChunkId, this->field_0x3e8);
	}

	pHero = CActorHero::_gThis;

	if ((this->field_0x3f0 & 2) == 0) {
		triggerResult = edEventComputeZoneAgainstVertex(pEventManager->activeChunkId, pTriggerZone, &pHero->currentLocation, 0);
	}
	else {
		edF32Vector4SubHard(&toHero, &pHero->currentLocation, &this->currentLocation);
		if (edF32Vector4GetDistHard(&toHero) < this->field_0x3e0) {
			triggerResult = 1;
		}
		else {
			triggerResult = 2;
		}
	}

	detectResult = edEventComputeZoneAgainstVertex(pEventManager->activeChunkId, pDetectZone, &pHero->currentLocation, 0);

	if ((detectResult != 1) && (triggerResult != 1)) {
		this->field_0x438 = (CActorHero*)0x0;
		return 2;
	}

	if (triggerResult == 1) {
		this->field_0x438 = pHero;
		return 1;
	}

	return -1;
}

void CBehaviourShoot::Create(ByteCode* pByteCode)
{
	return;
}

void CBehaviourShoot::Begin(CActor * pOwner, int newState, int newAnimationType)
{
	this->pOwner = static_cast<CActorShoot*>(pOwner);

	return;
}

int CBehaviourShoot::InterpretMessage(CActor * pSender, int msg, void* pMsgParam)
{
	return 0;
}

int CBehaviourShoot::InterpretEvent(edCEventMessage* pEventMessage, undefined8 param_3, int param_4, uint* param_5)
{
	return 0;
}

void CBehaviourShootFire::Create(ByteCode* pByteCode)
{
	this->field_0x2b0 = pByteCode->GetU32();
	this->field_0x8 = pByteCode->GetF32();
	this->field_0x2b4 = pByteCode->GetF32();

	this->fireshot.Create(pByteCode);

	return;
}

void CBehaviourShootFire::Init(CActor * pOwner)
{
	this->fireshot.Init();

	return;
}

void CBehaviourShootFire::Manage()
{
	this->pOwner->BehaviourShootFire_Manage(this);

	return;
}

void CBehaviourShootFire::Draw()
{
	return;
}

void CBehaviourShootFire::Begin(CActor* pOwner, int newState, int newAnimationType)
{
	CActorShoot* pShoot;

	CBehaviourShoot::Begin(pOwner, newState, newAnimationType);

	this->fireshot.Reset();

	if (newState == -1) {
		pShoot = this->pOwner;
		pShoot->SetState(6, -1);
	}
	else {
		pShoot = this->pOwner;
		pShoot->SetState(newState, newAnimationType);
	}

	return;
}

void CBehaviourShootFire::InitState(int newState)
{
	int iVar1;
	ed_3d_hierarchy_node* peVar2;
	CCollision* pCVar3;
	bool bVar4;
	StateConfig* pSVar5;
	edF32VECTOR4* peVar6;
	uint uVar7;
	edF32VECTOR4 eStack48;
	edF32VECTOR4 eStack32;
	edF32VECTOR4 eStack16;
	CActorShoot* pShoot;

	pShoot = this->pOwner;

	if ((pShoot->GetStateFlags(newState) & 0x800) == 0) {
		iVar1 = pShoot->prevActorState;

		if ((pShoot->GetStateFlags(iVar1) & 0x800) != 0) {
			pShoot->pAnimationController->UnRegisterBone(pShoot->field_0x350);
			pShoot->pAnimationController->UnRegisterBone(pShoot->field_0x354);
			pShoot->SetLookingAtOff();
		}
	}

	if ((pShoot->GetStateFlags(newState) & 0x800) != 0) {
		iVar1 = pShoot->prevActorState;

		if ((pShoot->GetStateFlags(iVar1) & 0x800) != 0) {
			pShoot->pAnimationController->RegisterBone(pShoot->field_0x350);
			pShoot->pAnimationController->RegisterBone(pShoot->field_0x354);
			pShoot->SetLookingAtBones(pShoot->field_0x354, pShoot->field_0x350);
			pShoot->SetLookingAtBounds(-0.08726646f, 0.08726646f, -0.7853982f, 0.7853982f);
		}
	}

	if (newState != 8) {
		if (newState == 0xf) {
			pCVar3 = pShoot->pCollisionData;
			peVar6 = pShoot->GetBottomPosition();
			edF32Vector4AddHard(&eStack16, &pCVar3->highestVertex, peVar6);
			edF32Vector4ScaleHard(0.5f, &eStack16, &eStack16);
			pShoot->addOnGenerator.Generate(&eStack16);
		}
		else {
			if (newState == 0xc) {
				pShoot->pAnimationController->RegisterBone(this->field_0x2b0);

				pShoot->dynamicExt.normalizedTranslation.x = 0.0f;
				pShoot->dynamicExt.normalizedTranslation.y = 0.0f;
				pShoot->dynamicExt.normalizedTranslation.z = 0.0f;
				pShoot->dynamicExt.normalizedTranslation.w = 0.0f;
				pShoot->dynamicExt.field_0x6c = 0.0f;
				pShoot->dynamic.speed = 0.0f;
			}
			else {
				if (newState == 7) {
					if ((pShoot->field_0x3f0 & 1) != 0) {
						pShoot->flags = pShoot->flags & 0xffffff7f;
						pShoot->flags = pShoot->flags | 0x20;
						pShoot->EvaluateDisplayState();
					}

					bVar4 = (pShoot->staticMeshComponent).textureIndex != -1;
					if (bVar4) {
						bVar4 = (pShoot->staticMeshComponent).meshIndex != -1;
					}

					if (bVar4) {
						pShoot->staticMeshComponent.Init(CScene::_scene_handleA, (ed_g3d_manager*)0x0, &pShoot->altHierarchySetup, (char*)0x0);
						eStack48 = gF32Vector4UnitY;
						peVar6 = &pShoot->currentLocation;
						bVar4 = pShoot->staticMeshComponent.HasMesh();
						if ((bVar4 != false) && (peVar6 != (edF32VECTOR4*)0x0)) {
							edFIntervalLERP(0.0f, 0.0f, 0.4f, 0.0f, 0.32f);
							edF32Vector4ScaleHard(-0.32f, &eStack48, &eStack48);
							edF32Vector4AddHard(&eStack32, peVar6, &eStack48);
							peVar2 = (pShoot->staticMeshComponent).pMeshTransformData;
							if (peVar2 != (ed_3d_hierarchy_node*)0x0) {
								pShoot->staticMeshComponent.pMeshTransformData->base.transformA.rowT = eStack32;
							}
						}
					}
				}
				else {
					if ((newState == 6) && ((pShoot->field_0x3f0 & 1) != 0)) {
						pShoot->flags = pShoot->flags & 0xffffff7f;
						pShoot->flags = pShoot->flags | 0x20;
						pShoot->EvaluateDisplayState();
					}
				}
			}
		}
	}

	return;
}

void CBehaviourShootFire::TermState(int oldState, int newState)
{
	bool bVar1;
	CActorShoot* pShoot;

	pShoot = this->pOwner;
	if (oldState == 0xe) {
		bVar1 = pShoot->staticMeshComponent.HasMesh();
		if (bVar1 != false) {
			pShoot->staticMeshComponent.Term(CScene::_scene_handleA);
		}
	}
	else {
		if (oldState == 0xc) {
			pShoot->pAnimationController->UnRegisterBone(this->field_0x2b0);
			pShoot->SetLookingAtOff();
		}
		else {
			if (oldState == 7) {
				if ((pShoot->field_0x3f0 & 1) != 0) {
					pShoot->flags = pShoot->flags & 0xffffff5f;
					pShoot->EvaluateDisplayState();
				}
			}
			else {
				if ((oldState == 6) && ((pShoot->field_0x3f0 & 1) != 0)) {
					pShoot->flags = pShoot->flags & 0xffffff5f;
					pShoot->EvaluateDisplayState();
				}
			}
		}
	}

	return;
}

int CBehaviourShootFire::InterpretMessage(CActor * pSender, int msg, void* pMsgParam)
{
	return 0;
}

void CBehaviourShootFire::Reset()
{
	this->fireshot.Reset();

	return;
}
