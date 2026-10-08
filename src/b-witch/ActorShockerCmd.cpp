#include "ActorShockerCmd.h"
#include "MemoryStream.h"
#include "LargeObject.h"

void CActorShockerCmd::Create(ByteCode* pByteCode)
{
	uint uVar1;
	int iVar2;
	int iVar6;
	CSequenceParam* piVar3;

	CActor::Create(pByteCode);
	uVar1 = pByteCode->GetU32();
	this->bSequentialOrder = uVar1;
	iVar2 = pByteCode->GetS32();
	this->sequenceParamsCount = iVar2;
	uVar1 = this->sequenceParamsCount;
	if (uVar1 != 0) {
		this->aSequenceParams = new CSequenceParam[uVar1];
		iVar2 = 0;
		if (0 < this->sequenceParamsCount) {
			do {
				piVar3 = &this->aSequenceParams[iVar2];
				iVar6 = pByteCode->GetS32();
				piVar3->pShocker.index = iVar6;
				iVar6 = pByteCode->GetS32();
				piVar3->count_0x4 = iVar6;
				uVar1 = piVar3->count_0x4;
				if (uVar1 != 0) {
					piVar3->aFloats = new float[uVar1];
					iVar6 = 0;
					if (0 < piVar3->count_0x4) {
						do {
							piVar3->aFloats[iVar6] = pByteCode->GetF32();
							iVar6 = iVar6 + 1;
						} while (iVar6 < piVar3->count_0x4);
					}
				}
				iVar2 = iVar2 + 1;
			} while (iVar2 < this->sequenceParamsCount);
		}
	}
	return;
}

void CActorShockerCmd::Init()
{
	int i;

	for (i = 0; i < this->sequenceParamsCount; i = i + 1) {
		this->aSequenceParams[i].pShocker.Init();
		this->aSequenceParams[i].bFired = 0;
	}

	CActor::Init();

	ResetSequence();

	return;
}

void CActorShockerCmd::Reset()
{
	CActor::Reset();

	ResetSequence();

	return;
}

// Inlined into Init and Reset on PS2: registers this command with every shocker (command 0).
void CActorShockerCmd::ResetSequence()
{
	int i;
	int command;

	this->curSequenceIndex = -1;
	this->nbFiredShockers = 0;
	this->nbWaitingShockers = 0;

	for (i = 0; i < this->sequenceParamsCount; i = i + 1) {
		this->aSequenceParams[i].bFired = 0;
	}

	for (i = 0; i < this->sequenceParamsCount; i = i + 1) {
		command = 0;
		DoMessage(this->aSequenceParams[i].pShocker.Get(), (ACTOR_MESSAGE)0x69, &command);
	}

	return;
}

CBehaviour* CActorShockerCmd::BuildBehaviour(int behaviourType)
{
	CBehaviour* pNewBehaviour;

	if (behaviourType == SHOCKER_CMD_BEHAVIOUR_STAND) {
		pNewBehaviour = &this->behaviourStand;
	}
	else {
		pNewBehaviour = CActor::BuildBehaviour(behaviourType);
	}

	return pNewBehaviour;
}

StateConfig CActorShockerCmd::_gStateCfg_SHC[2] =
{
	StateConfig(0, 0),
	StateConfig(0, 0)
};

StateConfig* CActorShockerCmd::GetStateCfg(int state)
{
	StateConfig* pStateConfig;

	if (state < SHOCKER_CMD_STATE_STAND) {
		pStateConfig = CActor::GetStateCfg(state);
	}
	else {
		assert((state - SHOCKER_CMD_STATE_STAND) < 2);
		pStateConfig = _gStateCfg_SHC + state - SHOCKER_CMD_STATE_STAND;
	}

	return pStateConfig;
}

int CActorShockerCmd::InterpretMessage(CActor* pSender, int msg, void* pMsgParam)
{
	CActor* pShocker;
	int command;
	int nbReady;
	int i;

	if (msg != 0x69) {
		return CActor::InterpretMessage(pSender, msg, pMsgParam);
	}

	command = *reinterpret_cast<int*>(pMsgParam);
	if (command == 3) {
		this->nbWaitingShockers = this->nbWaitingShockers + -1;
		return 1;
	}

	if (command != 2) {
		return 0;
	}

	this->nbWaitingShockers = this->nbWaitingShockers + 1;

	nbReady = 0;
	for (i = 0; i < this->sequenceParamsCount; i = i + 1) {
		pShocker = this->aSequenceParams[i].pShocker.Get();
		if (((pShocker->GetStateFlags(pShocker->actorState) & 1) == 0) && ((pShocker->flags & 4) != 0)) {
			nbReady = nbReady + 1;
		}
	}

	if ((this->nbWaitingShockers == nbReady) && (this->nbFiredShockers == 0)) {
		if (this->bSequentialOrder == 0) {
			this->curSequenceIndex = (int)(this->aSequenceParams->count_0x4 * CScene::Rand()) / 0x8000;
		}
		else {
			this->curSequenceIndex = (this->curSequenceIndex + 1) % this->aSequenceParams->count_0x4;
		}

		SetState(SHOCKER_CMD_STATE_FIRE_SEQUENCE, -1);
	}

	return 1;
}

int CActorShockerCmd::InterpretEvent(edCEventMessage* pEventMessage, undefined8 param_3, int param_4, uint* param_5)
{
	return CActor::InterpretEvent(pEventMessage, param_3, param_4, param_5);
}

void CActorShockerCmd::StateShockerCmdFireSequence()
{
	CSequenceParam* pParam;
	int command;
	int i;

	for (i = 0; i < this->sequenceParamsCount; i = i + 1) {
		pParam = this->aSequenceParams + i;
		if ((pParam->bFired == 0) && (pParam->aFloats[this->curSequenceIndex] < this->timeInAir)) {
			command = 1;
			if (DoMessage(pParam->pShocker.Get(), (ACTOR_MESSAGE)0x69, &command) != 0) {
				this->nbFiredShockers = this->nbFiredShockers + 1;
				this->aSequenceParams[i].bFired = 1;
			}
		}
	}

	if (this->nbFiredShockers == this->nbShockersToFire) {
		this->nbFiredShockers = 0;

		for (i = 0; i < this->sequenceParamsCount; i = i + 1) {
			this->aSequenceParams[i].bFired = 0;
		}

		SetState(SHOCKER_CMD_STATE_STAND, -1);
	}

	return;
}

void CBehaviourShockerCmd::Create(ByteCode* pByteCode)
{
	return;
}

void CBehaviourShockerCmd::Begin(CActor* pOwner, int newState, int newAnimationType)
{
	this->pOwner = static_cast<CActorShockerCmd*>(pOwner);

	return;
}

int CBehaviourShockerCmd::InterpretMessage(CActor* pSender, int msg, void* pMsgParam)
{
	return 0;
}

int CBehaviourShockerCmd::InterpretEvent(edCEventMessage* pEventMessage, undefined8 param_3, int param_4, uint* param_5)
{
	return 0;
}

void CBehaviourShockerCmdStand::Create(ByteCode* pByteCode)
{
	return;
}

void CBehaviourShockerCmdStand::Manage()
{
	int state;

	state = this->pOwner->actorState;
	if (state == SHOCKER_CMD_STATE_FIRE_SEQUENCE) {
		this->pOwner->StateShockerCmdFireSequence();
	}

	return;
}

void CBehaviourShockerCmdStand::Begin(CActor* pOwner, int newState, int newAnimationType)
{
	this->pOwner = static_cast<CActorShockerCmd*>(pOwner);

	if (newState == -1) {
		this->pOwner->SetState(SHOCKER_CMD_STATE_STAND, -1);
	}
	else {
		this->pOwner->SetState(newState, newAnimationType);
	}

	return;
}

void CBehaviourShockerCmdStand::InitState(int newState)
{
	CActorShockerCmd* pShockerCmd;
	CActor* pShocker;
	int nbReady;
	int i;

	pShockerCmd = this->pOwner;
	if (newState == SHOCKER_CMD_STATE_FIRE_SEQUENCE) {
		pShockerCmd->nbFiredShockers = 0;

		nbReady = 0;
		for (i = 0; i < pShockerCmd->sequenceParamsCount; i = i + 1) {
			pShocker = pShockerCmd->aSequenceParams[i].pShocker.Get();
			if (((pShocker->GetStateFlags(pShocker->actorState) & 1) == 0) && ((pShocker->flags & 4) != 0)) {
				nbReady = nbReady + 1;
			}
		}

		pShockerCmd->nbShockersToFire = nbReady;
	}

	return;
}

void CBehaviourShockerCmdStand::TermState(int oldState, int newState)
{
	return;
}

int CBehaviourShockerCmdStand::InterpretMessage(CActor* pSender, int msg, void* pMsgParam)
{
	return 0;
}
