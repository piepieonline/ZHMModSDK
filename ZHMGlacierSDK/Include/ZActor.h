#pragma once

#include "Enums.h"
#include "ZEntity.h"
#include "ZPrimitives.h"
#include "ZHM5BaseCharacter.h"
#include "ZResource.h"

class ZCharacterTemplateAspect;
class ZCostumeFeatureCollection;
class ZAccessoryItemPool;
class ZItemRepositoryKeyEntity;
class ZOutfitProfessionEntity;
class ZAIVisionConfigurationEntity;
class ZHTNDomainEntity;
class ZCompiledBehaviorTree;
class ZSpatialEntity;
class ZKnowledge;

class ICharacterCollision :
	public IComponentInterface
{
public:
	virtual ~ICharacterCollision() {}
	virtual void ICharacterCollision_unk00() = 0;
};

class IActor
{
public:
	virtual void IActor_unk00() = 0;
	virtual void RequestDisable() = 0;
	virtual void IActor_unk02() = 0;
	virtual void IActor_unk03() = 0;
	virtual void IActor_unk04() = 0;
	virtual void IActor_unk05() = 0;
	virtual void IActor_unk06() = 0;
	virtual void IActor_unk07() = 0;
	virtual void IActor_unk08() = 0;
	virtual void IActor_unk09() = 0;
	virtual bool IsDead() = 0;
	virtual bool IsAlive() = 0;
	virtual bool IsPacified() = 0;
	virtual void IActor_unk13() = 0;
	virtual void IActor_unk14() = 0;
	virtual void IActor_unk15() = 0;
	virtual void IActor_unk16() = 0;
	virtual void IActor_unk17() = 0;
	virtual void IActor_unk18() = 0;
	virtual void IActor_unk19() = 0;
	virtual ZKnowledge* Knowledge() = 0;
	virtual void IActor_unk21() = 0;
	virtual void IActor_unk22() = 0;
	virtual void IActor_unk23() = 0;
	virtual void IActor_unk24() = 0;
	virtual void IActor_unk25() = 0;
	virtual void IActor_unk26() = 0;
	virtual void IActor_unk27() = 0;
	virtual void IActor_unk28() = 0;
	virtual void IActor_unk29() = 0;
	virtual void IActor_unk30() = 0;
	virtual void IActor_unk31() = 0;
	virtual void IActor_unk32() = 0;
	virtual void IActor_unk33() = 0;
	virtual void IActor_unk34() = 0;
	virtual void IActor_unk35() = 0;
	virtual void IActor_unk36() = 0;
	virtual void IActor_unk37() = 0;
	virtual void IActor_unk38() = 0;
	virtual void IActor_unk39() = 0;
	virtual void IActor_unk40() = 0;
	virtual void IActor_unk41() = 0;
	virtual void IActor_unk42() = 0;
	virtual void IActor_unk43() = 0;
	virtual void IActor_unk44() = 0;
	virtual void IActor_unk45() = 0;
	virtual void IActor_unk46() = 0;
	virtual void IActor_unk47() = 0;
	virtual void IActor_unk48() = 0;
	virtual void IActor_unk49() = 0;
	virtual void IActor_unk50() = 0;
	virtual void IActor_unk51() = 0;
	virtual void IActor_unk52() = 0;
	virtual void IActor_unk53() = 0;
	virtual void IActor_unk54() = 0;
};

class IActorProxy :
	public IComponentInterface
{
public:
	virtual ~IActorProxy() {}
	virtual void IActorProxy_unk00() = 0;
};

class ISequenceTarget
{
public:
	virtual void ISequenceTarget_unk00() = 0;
	virtual void ISequenceTarget_unk01() = 0;
	virtual void ISequenceTarget_unk02() = 0;
};

class ISequenceAudioPlayer :
	public IComponentInterface
{
public:
	virtual ~ISequenceAudioPlayer() {}
	virtual void ISequenceAudioPlayer_unk00() = 0;
	virtual void ISequenceAudioPlayer_unk01() = 0;
	virtual void ISequenceAudioPlayer_unk02() = 0;
	virtual void ISequenceAudioPlayer_unk03() = 0;
};

class ICrowdAIActor :
	public IComponentInterface
{
public:
	virtual ~ICrowdAIActor() {}
	virtual void ICrowdAIActor_unk00() = 0;
	virtual void ICrowdAIActor_unk01() = 0;
	virtual void ICrowdAIActor_unk02() = 0;
};

// Size = 0x1410
class ZActor :
	public ZHM5BaseCharacter,
	public ICharacterCollision,
	public IActor,
	public IActorProxy,
	public ISequenceTarget,
	public ISequenceAudioPlayer,
	public ICrowdAIActor
{
public:
	PAD(0x100); // 0x300
	bool m_bStartEnabled; // 0x400
	TEntityRef<ZCharacterTemplateAspect> m_rCharacter; // 0x408
	bool m_bBlockDisguisePickup; // 0x418
	ZRepositoryID m_OutfitRepositoryID; // 0x420
	int32 m_nOutfitCharset; // 0x430
	int32 m_nOutfitVariation; // 0x434
	TEntityRef<ZCostumeFeatureCollection> m_pCostumeFeatures; // 0x438
	TEntityRef<ZAccessoryItemPool> m_pAccessoryItemPool; // 0x448
	TArray<TEntityRef<ZItemRepositoryKeyEntity>> m_InventoryItemKeys; // 0x458
	TArray<TEntityRef<ZOutfitProfessionEntity>> m_aEnforcedOutfits; // 0x470
	ZString m_sActorName; // 0x488
	EActorGroup m_eActorGroup; // 0x498
	TResourcePtr<ZCompiledBehaviorTree> m_pCompiledBehaviorTree; // 0x49C
	EActorVoiceVariation m_eRequiredVoiceVariation; // 0x4A4
	TResourcePtr<ZSpatialEntity> m_pBodybagResource; // 0x4A8
	bool m_bWeaponUnholstered; // 0x4B0
	int32 m_nWeaponIndex; // 0x4B4
	int32 m_nGrenadeIndex; // 0x4B8
	bool m_bIsGrenadeDroppable; // 0x4BC
	bool m_bEnableOutfitModifiers; // 0x4BD
	TEntityRef<ZAIVisionConfigurationEntity> m_AgentVisionConfiguration; // 0x4C0
	TEntityRef<ZHTNDomainEntity> m_DomainConfig; // 0x4D0
	PAD(0xC70); // 0x4E0
	bool m_bUnk00 : 1; // 0x1150
	bool m_bUnk01 : 1;
	bool m_bUnk02 : 1;
	bool m_bUnk03 : 1;
	bool m_bUnk04 : 1;
	bool m_bUnk05 : 1;
	bool m_bIsBeingDragged : 1;
	bool m_bIsBeingDumped : 1;
	bool m_bUnk08 : 1; // 0x1151
	bool m_bUnk09 : 1;
	bool m_bUnk10 : 1;
	bool m_bUnk11 : 1;
	bool m_bUnk12 : 1;
	bool m_bUnk13 : 1;
	bool m_bUnk14 : 1;
	bool m_bUnk15 : 1;
	bool m_bUnk16 : 1; // 0x1152
	bool m_bUnk17 : 1;
	bool m_bUnk18 : 1;
	bool m_bUnk19 : 1;
	bool m_bUnk20 : 1;
	bool m_bUnk21 : 1;
	bool m_bUnk22 : 1;
	bool m_bUnk23 : 1;
	bool m_bUnk24 : 1; // 0x1153
	bool m_bUnk25 : 1;
	bool m_bUnk26 : 1;
	bool m_bUnk27 : 1;
	bool m_bUnk28 : 1;
	bool m_bUnk29 : 1;
	bool m_bUnk30 : 1;
	bool m_bUnk31 : 1;
	bool m_bUnk32 : 1; // 0x1154
	bool m_bUnk33 : 1;
	bool m_bBodyHidden : 1;
	bool m_bUnk35 : 1;
	bool m_bUnk36 : 1;
	bool m_bUnk37 : 1;
	bool m_bUnk38 : 1;
	bool m_bUnk39 : 1;
	PAD(0x2B8);	
};

class ZActorManager :
	public IComponentInterface
{	
public:
	virtual ~ZActorManager() {}

public:
	PAD(0x1F60);
	TEntityRef<ZActor> m_aActiveActors[1000]; // 0x1F68
};