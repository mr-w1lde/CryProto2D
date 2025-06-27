// Copyright 2017-2019 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <CryEntitySystem/IEntityComponent.h>
#include <DefaultComponents/Physics/CharacterControllerComponent.h>

#include <ICryMannequin.h>
#include <CrySchematyc/Utils/EnumFlags.h>

#include <DefaultComponents/Cameras/CameraComponent.h>
#include <DefaultComponents/Input/InputComponent.h>
#include <DefaultComponents/Audio/ListenerComponent.h>
#include <CrySchematyc/CoreAPI.h>

#include "Schematyc/FFlipbookAnim.h"
#include "Schematyc/SpriteFlipbookComponent.h"

////////////////////////////////////////////////////////
// Represents a player participating in gameplay
////////////////////////////////////////////////////////
class CPlayerComponent final : public IEntityComponent
{
	enum class EInputFlagType
	{
		Hold = 0,
		Toggle
	};

	enum class EInputFlag : uint8
	{
		MoveLeft = 1 << 0,
		MoveRight = 1 << 1,
		MoveForward = 1 << 2,
		MoveBack = 1 << 3,
		Jump = 1 << 4
	};

	enum class EPlayerState
	{
		Idle,
		Moving,
		Jumping,
		Falling,
		Attacking
	};

public:
	CPlayerComponent();
	virtual ~CPlayerComponent() = default;

	// IEntityComponent
	virtual void Initialize() override;

	virtual Cry::Entity::EventFlags GetEventMask() const override;
	virtual void ProcessEvent(const SEntityEvent& event) override;
	
	// Reflect type to set a unique identifier for this component
	static void ReflectType(Schematyc::CTypeDesc<CPlayerComponent>& desc)
	{
		desc.SetGUID("{B08F2F41-F02E-48B5-921A-3FF857F19ED6}"_cry_guid);
		desc.SetLabel("Player");
	}
	
protected:
	
	void UpdateMovementRequest(float frameTime);
	void UpdatePlayerState(float frameTime);
	void UpdateCamera(float frameTime);

	void HandleInputFlagChange(CEnumFlags<EInputFlag> flags, CEnumFlags<EActionActivationMode> activationMode, EInputFlagType type = EInputFlagType::Hold);

	// Called when this entity becomes the local player, to create client specific setup such as the Camera
	void InitializeLocalPlayer();
	
	void EnterState(EPlayerState newState);

	const char* GetPlayerStateName() const
	{
		switch (m_state) {
		case EPlayerState::Idle: return "Idle";
		case EPlayerState::Moving: return "Moving";
		case EPlayerState::Jumping: return "Jumping";
		case EPlayerState::Falling: return "Falling";
		case EPlayerState::Attacking: return "Attacking";
		default: return "Unknown";
		}
	}
	
protected:
	bool m_isAlive = false;

	Cry::DefaultComponents::CCameraComponent* m_pCameraComponent = nullptr;
	Cry::DefaultComponents::CCharacterControllerComponent* m_pCharacterController = nullptr;
	Cry::DefaultComponents::CInputComponent* m_pInputComponent = nullptr;
	Cry::Audio::DefaultComponents::CListenerComponent* m_pAudioListenerComponent = nullptr;
	CSpriteFlipbookComponent* m_pSpriteFlipbookComponent = nullptr;
	

	CEnumFlags<EInputFlag> m_inputFlags;
	Vec2 m_mouseDeltaRotation = ZERO;
	// Should translate to head orientation in the future
	Quat m_lookOrientation = IDENTITY;

	std::unordered_map<int, FFlipbookAnim> m_animations;

	EPlayerState m_state = EPlayerState::Idle;
	float m_stateTime = 0.f;
};
