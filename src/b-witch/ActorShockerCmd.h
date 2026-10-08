#ifndef ACTOR_SHOCKER_CMD_H
#define ACTOR_SHOCKER_CMD_H

#include "Types.h"
#include "Actor.h"

#define SHOCKER_CMD_BEHAVIOUR_STAND 2
#define SHOCKER_CMD_STATE_STAND 5
#define SHOCKER_CMD_STATE_FIRE_SEQUENCE 6

class CActorShockerCmd;

class CSequenceParam {
public:
	S_STREAM_REF<CActor> pShocker;
	int count_0x4;
	float* aFloats;
	byte bFired;
};

class CBehaviourShockerCmd : public CBehaviour
{
public:
	virtual void Create(ByteCode* pByteCode);
	virtual void Begin(CActor* pOwner, int newState, int newAnimationType);
	virtual int InterpretMessage(CActor* pSender, int msg, void* pMsgParam);
	virtual int InterpretEvent(edCEventMessage* pEventMessage, undefined8 param_3, int param_4, uint* param_5);

	CActorShockerCmd* pOwner;
};

class CBehaviourShockerCmdStand : public CBehaviourShockerCmd
{
public:
	virtual void Create(ByteCode* pByteCode);
	virtual void Manage();
	virtual void Begin(CActor* pOwner, int newState, int newAnimationType);
	virtual void InitState(int newState);
	virtual void TermState(int oldState, int newState);
	virtual int InterpretMessage(CActor* pSender, int msg, void* pMsgParam);
};

class CActorShockerCmd : public CActor {
public:
	static StateConfig _gStateCfg_SHC[2];

	virtual void Create(ByteCode* pByteCode);
	virtual void Init();
	virtual void Reset();
	virtual CBehaviour* BuildBehaviour(int behaviourType);
	virtual StateConfig* GetStateCfg(int state);
	virtual int InterpretMessage(CActor* pSender, int msg, void* pMsgParam);
	virtual int InterpretEvent(edCEventMessage* pEventMessage, undefined8 param_3, int param_4, uint* param_5);

	void StateShockerCmdFireSequence();
	void ResetSequence();

	CBehaviourShockerCmdStand behaviourStand;

	uint bSequentialOrder;
	int sequenceParamsCount;
	CSequenceParam* aSequenceParams;
	int nbWaitingShockers;
	int nbFiredShockers;
	int nbShockersToFire;
	int curSequenceIndex;
};

#endif //ACTOR_SHOCKER_CMD_H
