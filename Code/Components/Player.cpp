// Copyright 2017-2019 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "Player.h"

#include <CryPhysics/IPhysics.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CryCore/StaticInstanceList.h>
#include <CryNetwork/Rmi.h>

#include "GamePlugin.h"

namespace
{
    static void RegisterPlayerComponent(Schematyc::IEnvRegistrar& registrar)
    {
        Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
        {
            Schematyc::CEnvRegistrationScope componentScope = scope.Register(
                SCHEMATYC_MAKE_ENV_COMPONENT(CPlayerComponent));
        }
    }

    CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterPlayerComponent);
}


CPlayerComponent::CPlayerComponent() : m_animations({
    {1, {"idle", 0, 3, 0, 5.f, true}},
    {2, {"moving", 0, 5, 1, 8.f, true}},
    {4, {"jump", 0, 5, 2, 13.5f, false}},
    {5, {"fall", 6, 7, 2, 5.f, true}},
    {6, {"attack", 4, 7, 5, 5.f, false}},

})
{
}

void CPlayerComponent::Initialize()
{
    // The character controller is responsible for maintaining player physics
    m_pCharacterController = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CCharacterControllerComponent>();
    // Offset the default character controller up by one unit
    m_pCharacterController->SetTransformMatrix(
        Matrix34::Create(
            Vec3(1.f),
            IDENTITY,
            Vec3(0.f, 0.f, 0.5f) // смещение вниз по Z
        ) // scale по умолчанию
    );

    m_pSpriteFlipbookComponent = m_pEntity->GetOrCreateComponent<CSpriteFlipbookComponent>();

    InitializeLocalPlayer();
}

void CPlayerComponent::InitializeLocalPlayer()
{
    // Create the camera component, will automatically update the viewport every frame
    m_pCameraComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CCameraComponent>();

    // Create the camera component, will automatically update the viewport every frame
    m_pCameraComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CCameraComponent>();

    // Create the audio listener component.
    m_pAudioListenerComponent = m_pEntity->GetOrCreateComponent<Cry::Audio::DefaultComponents::CListenerComponent>();

    // Get the input component, wraps access to action mapping so we can easily get callbacks when inputs are m_pEntity
    m_pInputComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CInputComponent>();
}

void CPlayerComponent::EnterState(EPlayerState newState)
{
    if (m_state == newState)
        return;

    m_state = newState;
    m_stateTime = 0.f;

    if (newState == EPlayerState::Idle)
        m_pSpriteFlipbookComponent->Play(m_animations.at(1));
    else if (newState == EPlayerState::Moving)
        m_pSpriteFlipbookComponent->Play(m_animations.at(2));
    else if (newState == EPlayerState::Jumping)
        m_pSpriteFlipbookComponent->Play(m_animations.at(4));
    else if (newState == EPlayerState::Falling)
        m_pSpriteFlipbookComponent->Play(m_animations.at(5));
    else if (newState == EPlayerState::Attacking)
        m_pSpriteFlipbookComponent->Play(m_animations.at(6));
}

Cry::Entity::EventFlags CPlayerComponent::GetEventMask() const
{
    return
        Cry::Entity::EEvent::BecomeLocalPlayer |
        Cry::Entity::EEvent::GameplayStarted |
        Cry::Entity::EEvent::Update |
        Cry::Entity::EEvent::Reset;
}

void CPlayerComponent::ProcessEvent(const SEntityEvent& event)
{
    switch (event.event)
    {
    case Cry::Entity::EEvent::GameplayStarted:
        {
            m_inputFlags.Clear();
            m_isAlive = true;

            // Register an action, and the callback that will be sent when it's m_pEntity
            m_pInputComponent->RegisterAction("player", "moveleft", [this](int activationMode, float value)
            {
                HandleInputFlagChange(EInputFlag::MoveLeft,
                                      (EActionActivationMode)activationMode);
            });
            // Bind the 'A' key the "moveleft" action
            m_pInputComponent->BindAction("player", "moveleft", eAID_KeyboardMouse, EKeyId::eKI_A);

            m_pInputComponent->RegisterAction("player", "moveright", [this](int activationMode, float value)
            {
                HandleInputFlagChange(EInputFlag::MoveRight,
                                      (EActionActivationMode)activationMode);
            });
            m_pInputComponent->BindAction("player", "moveright", eAID_KeyboardMouse, EKeyId::eKI_D);

            m_pInputComponent->RegisterAction("player", "moveforward", [this](int activationMode, float value)
            {
                HandleInputFlagChange(EInputFlag::MoveForward,
                                      (EActionActivationMode)activationMode);
            });
            m_pInputComponent->BindAction("player", "moveforward", eAID_KeyboardMouse, EKeyId::eKI_W);

            m_pInputComponent->RegisterAction("player", "moveback", [this](int activationMode, float value)
            {
                HandleInputFlagChange(EInputFlag::MoveBack,
                                      (EActionActivationMode)activationMode);
            });
            m_pInputComponent->BindAction("player", "moveback", eAID_KeyboardMouse, EKeyId::eKI_S);

            m_pInputComponent->RegisterAction("player", "mouse_rotateyaw", [this](int activationMode, float value)
            {
                m_mouseDeltaRotation.x -= value;
            });
            m_pInputComponent->BindAction("player", "mouse_rotateyaw", eAID_KeyboardMouse, EKeyId::eKI_MouseX);

            m_pInputComponent->RegisterAction("player", "mouse_rotatepitch", [this](int activationMode, float value)
            {
                m_mouseDeltaRotation.y -= value;
            });
            m_pInputComponent->BindAction("player", "mouse_rotatepitch", eAID_KeyboardMouse, EKeyId::eKI_MouseY);

            // Register the shoot action
            m_pInputComponent->RegisterAction("player", "jump", [this](int activationMode, float value)
            {
                // Only jump if the button was pressed
                if (activationMode == eAAM_OnPress && m_pCharacterController->IsOnGround())
                {
                    m_pCharacterController->AddVelocity(Vec3(0, 0, 5.f));
                }
            });

            // Bind the jump action to the space bar
            m_pInputComponent->BindAction("player", "jump", eAID_KeyboardMouse, EKeyId::eKI_Space);
            m_pInputComponent->RegisterAction("player", "attack", [this](int activationMode, float value)
            {
                if (activationMode == eAAM_OnPress && m_state != EPlayerState::Attacking)
                {
                    EnterState(EPlayerState::Attacking);
                }
            });
            m_pInputComponent->BindAction("player", "attack", eAID_KeyboardMouse, EKeyId::eKI_Mouse1);
        }
        break;
    case Cry::Entity::EEvent::Update:
        {
            // Don't update the player if we haven't spawned yet
            if (!m_isAlive)
                return;

            const float frameTime = event.fParam[0];

            // Start by updating the movement request we want to send to the character controller
            // This results in the physical representation of the character moving
            UpdateMovementRequest(frameTime);

            // Update the player state
            UpdatePlayerState(frameTime);

            // Update the camera component offset
            UpdateCamera(frameTime);
        }
        break;
    case Cry::Entity::EEvent::Reset:
        {
            // Disable player when leaving game mode.
            m_isAlive = event.nParam[0] != 0;
        }
        break;
    }
}


void CPlayerComponent::UpdateMovementRequest(float frameTime)
{
    // Don't handle input if we are in air
    if (!m_pCharacterController->IsOnGround())
    {
        if (m_state != EPlayerState::Jumping && m_state != EPlayerState::Falling)
        {
            EnterState(EPlayerState::Falling);
        }
        return;
    }

    if (m_state == EPlayerState::Attacking)
        return;

    Vec3 velocity = ZERO;
    const float moveSpeed = 20.5f;

    if (m_inputFlags & EInputFlag::MoveLeft)
    {
        m_pSpriteFlipbookComponent->SetFacing(false);
        velocity.x -= moveSpeed * frameTime;
    }
    if (m_inputFlags & EInputFlag::MoveRight)
    {
        m_pSpriteFlipbookComponent->SetFacing(true);
        velocity.x += moveSpeed * frameTime;
    }
    if (m_inputFlags & EInputFlag::MoveForward)
    {
        velocity.y += moveSpeed * frameTime;
    }
    if (m_inputFlags & EInputFlag::MoveBack)
    {
        velocity.y -= moveSpeed * frameTime;
    }

    if (!velocity.IsZero())
    {
        EnterState(EPlayerState::Moving);
    }

    if (velocity.IsZero() &&
        m_state != EPlayerState::Idle &&
        m_state != EPlayerState::Jumping &&
        m_state != EPlayerState::Falling)
    {
        EnterState(EPlayerState::Idle);
    }

    m_pCharacterController->AddVelocity(GetEntity()->GetWorldRotation() * velocity);
}

void CPlayerComponent::UpdatePlayerState(float frameTime)
{
    m_stateTime += frameTime;
    gEnv->pRenderer->GetIRenderAuxGeom()->
          Draw2dLabel(50, 50, 1.5f, Col_White, false, "State: %s", GetPlayerStateName());
    Vec3 vel = m_pCharacterController->GetVelocity();

    switch (m_state)
    {
    case EPlayerState::Attacking:
        {
            if (m_stateTime > 0.4f) // animation timing
                EnterState(EPlayerState::Idle);
        }
        break;
    case EPlayerState::Jumping:
        {
            if (m_stateTime > 0.1f && vel.z < 0.0f)
            {
                EnterState(EPlayerState::Falling);
            }
        }
        break;
    case EPlayerState::Falling:
        // Funny hack to switch to jump
        if (vel.z > 0.1f && m_stateTime < 0.25f)
        {
            EnterState(EPlayerState::Jumping);
            break;
        }
        if (m_pCharacterController->IsOnGround())
            EnterState(EPlayerState::Idle);
    default:
        break;
    }
}

void CPlayerComponent::UpdateCamera(float frameTime)
{
    // Start with updating look orientation from the latest input
    Ang3 ypr = CCamera::CreateAnglesYPR(Matrix33(m_lookOrientation));

    if (!m_mouseDeltaRotation.IsZero())
    {
        const float rotationSpeed = 0.002f;

        ypr.x += m_mouseDeltaRotation.x * rotationSpeed;

        const float rotationLimitsMinPitch = -1.2;
        const float rotationLimitsMaxPitch = 0.05f;

        // TODO: Perform soft clamp here instead of hard wall, should reduce rot speed in this direction when close to limit.
        ypr.y = CLAMP(ypr.y + m_mouseDeltaRotation.y * rotationSpeed, rotationLimitsMinPitch, rotationLimitsMaxPitch);

        // Look direction needs to be synced to server to calculate the movement in
        // the right direction.
        m_lookOrientation = Quat(CCamera::CreateOrientationYPR(ypr));

        // Reset every frame
        m_mouseDeltaRotation = ZERO;
    }

    // Start with changing view rotation to the requested mouse look orientation
    Matrix34 localTransform = IDENTITY;
    localTransform.SetRotation33(
        Matrix33(m_pEntity->GetWorldRotation().GetInverted()) * CCamera::CreateOrientationYPR(ypr));

    const float viewDistance = 4.f;

    // Offset the player along the forward axis (normally back)
    /// Also offset upwards
    localTransform.SetTranslation(-localTransform.GetColumn1() * viewDistance);

    m_pCameraComponent->SetTransformMatrix(localTransform);
    m_pAudioListenerComponent->SetOffset(localTransform.GetTranslation());
}

void CPlayerComponent::HandleInputFlagChange(const CEnumFlags<EInputFlag> flags,
                                             const CEnumFlags<EActionActivationMode> activationMode,
                                             const EInputFlagType type)
{
    switch (type)
    {
    case EInputFlagType::Hold:
        {
            if (activationMode == eAAM_OnRelease)
            {
                m_inputFlags &= ~flags;
            }
            else
            {
                m_inputFlags |= flags;
            }
        }
        break;
    case EInputFlagType::Toggle:
        {
            if (activationMode == eAAM_OnRelease)
            {
                // Toggle the bit(s)
                m_inputFlags ^= flags;
            }
        }
        break;
    }
}
