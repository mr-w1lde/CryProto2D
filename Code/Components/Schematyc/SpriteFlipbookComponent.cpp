#include "StdAfx.h"

#include "SpriteFlipbookComponent.h"

#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CryRenderer/IRenderer.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryCore/StaticInstanceList.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>

namespace
{
    static void RegisterSpriteFlipbookComponent(Schematyc::IEnvRegistrar& registrar)
    {
        Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
        {
            Schematyc::CEnvRegistrationScope componentScope = scope.Register(
                SCHEMATYC_MAKE_ENV_COMPONENT(CSpriteFlipbookComponent));
        }
    }

    CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterSpriteFlipbookComponent);
}

void CSpriteFlipbookComponent::Initialize()
{
    m_slotId = m_pEntity->LoadGeometry(
        GetOrMakeEntitySlotId(),
        "%ENGINE%/EngineAssets/Objects/primitive_plane.cgf"
    );

    m_pEntity->SetSlotLocalTM(
        m_slotId,
        Matrix34::Create(
            m_localScale,
            Quat::CreateRotationX(DEG2RAD(90.f)),
            Vec3(0.0f, 0.0f, 0.5f)
        )
    );

    SetType(Cry::DefaultComponents::EMeshType::Render);
    ApplyBaseMeshProperties();
}

Cry::Entity::EventFlags CSpriteFlipbookComponent::GetEventMask() const
{
    return Cry::Entity::EEvent::Update |
        Cry::Entity::EEvent::GameplayStarted |
        Cry::Entity::EEvent::EditorPropertyChanged;
}

void CSpriteFlipbookComponent::ProcessEvent(const SEntityEvent& event)
{
    switch (event.event)
    {
    case Cry::Entity::EEvent::EditorPropertyChanged:
        {
            m_pEntity->SetSlotLocalTM(
                m_slotId,
                Matrix34::Create(
                    m_localScale,
                    Quat::CreateRotationX(DEG2RAD(90.f)),
                    Vec3(0.0f, 0.0f, 0.5f)
                )
            );
            LoadMaterial();
        }
        break;
    case Cry::Entity::EEvent::GameplayStarted:
        {
            LoadMaterial();
        }
        break;
    case Cry::Entity::EEvent::Update:
        {
            Update(event.fParam[0]);
        }
        break;
    }
}

void CSpriteFlipbookComponent::SetFacing(bool facingRight)
{
    Quat base = Quat::CreateRotationX(DEG2RAD(90.f));
    Quat flipY = Quat::CreateRotationZ(facingRight ? 0.f : DEG2RAD(180.f));

    Matrix34 tm = Matrix34::Create(
        m_localScale,
        flipY * base,
        Vec3(0.f, 0.f, 0.5f) // offset
    );
    m_pEntity->SetSlotLocalTM(m_slotId, tm);
}

void CSpriteFlipbookComponent::Play(const FFlipbookAnim& anim)
{
    if (m_currentAnimationData == anim)
    {
        return;
    }

    m_elapsed = 0.f;
    m_currentRow = anim.row;
    m_currentFrame = anim.startFrame;
    m_currentAnimationData = anim;
}

void CSpriteFlipbookComponent::Update(const float frameTime)
{
    if (m_columns == -1 || m_rows == -1)
    {
        return;
    }

    if (m_currentAnimationData.fps > 0.f)
    {
        m_elapsed += frameTime;

        const int frameCount = m_currentAnimationData.endFrame - m_currentAnimationData.startFrame + 1;
        const float frameDuration = 1.f / m_currentAnimationData.fps;

        int currentFrame = int(m_elapsed / frameDuration);

        if (!m_currentAnimationData.loop && currentFrame >= frameCount)
        {
            currentFrame = frameCount - 1; // fix on last frame
        }
        else if (m_currentAnimationData.loop)
        {
            currentFrame %= frameCount;
        }

        m_currentFrame = m_currentAnimationData.startFrame + currentFrame;
    }

    ApplyUVOffset( m_currentFrame % m_columns, m_currentRow);
}

void CSpriteFlipbookComponent::ApplyUVOffset(int frameX, int frameY)
{
    if (!m_pAnimatedMaterial)
    {
        return;
    }

    if (m_pFrameXParam)
        m_pFrameXParam->m_Value.m_Float = static_cast<float>(frameX);

    if (m_pFrameYParam)
        m_pFrameYParam->m_Value.m_Float = static_cast<float>(frameY);

    m_pAnimatedMaterial->GetShaderItem().m_pShaderResources->UpdateConstants(
        m_pAnimatedMaterial->GetShaderItem().m_pShader
    );
}

void CSpriteFlipbookComponent::LoadMaterial()
{
    if (m_materialPath.value.size() <= 0)
    {
        return;
    }

    if (m_slotId == -1)
    {
        CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Failed to load geom for %s!", m_pEntity->GetName());
        return;
    }

    IMaterial* pOriginalMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(m_materialPath.value);

    if (!pOriginalMaterial)
    {
        CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Failed to load material %s!", m_materialPath.value);
        return;
    }

    if (m_pAnimatedMaterial && pOriginalMaterial->GetName() == m_pAnimatedMaterial->GetName())
    {
        CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Animated Material already loaded %s!", m_pAnimatedMaterial->GetName());
        return;
    }

    m_pAnimatedMaterial = gEnv->p3DEngine->GetMaterialManager()->CloneMaterial(pOriginalMaterial);
    m_pEntity->SetMaterial(m_pAnimatedMaterial);

    SShaderItem& shaderItem = m_pAnimatedMaterial->GetShaderItem();
    IRenderShaderResources* pResources = shaderItem.m_pShaderResources;
    
    for (SShaderParam& param : pResources->GetParameters())
    {
        if (strcmp(param.m_Name, "TilesX") == 0)
        {
            m_columns = (int)param.m_Value.m_Float;
        }
        if (strcmp(param.m_Name, "TilesY") == 0)
        {
            m_rows = (int)param.m_Value.m_Float;
        }
        if (strcmp(param.m_Name, "FrameX") == 0)
        {
            m_pFrameXParam = &param; 
        }
        if (strcmp(param.m_Name, "FrameY") == 0)
        {
            m_pFrameYParam = &param; 
        }
    }

    if (m_columns == -1 || m_rows == -1)
    {
        CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Couldn't find any tiles in the Sprite FlipBook");
    }
}
