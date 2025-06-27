#pragma once

#include <CryEntitySystem/IEntityComponent.h>
#include <DefaultComponents/Geometry/BaseMeshComponent.h>

#include "FFlipbookAnim.h"

class CSpriteFlipbookComponent  final : public Cry::DefaultComponents::CBaseMeshComponent
{
public:
    CSpriteFlipbookComponent() = default;
    virtual ~CSpriteFlipbookComponent() = default;

    // IEntityComponent
    virtual void Initialize() override;

    virtual Cry::Entity::EventFlags GetEventMask() const override;
    virtual void ProcessEvent(const SEntityEvent& event) override;

    // IEditorEntityComponent
    virtual bool SetMaterial(int slotId, const char* szMaterial) override { return false; }

    // Reflect type to set a unique identifier for this component
    static void ReflectType(Schematyc::CTypeDesc<CSpriteFlipbookComponent>& desc)
    {
        desc.SetGUID("{F0E9C3FD-2CAF-49E9-A199-6749951DEABE}"_cry_guid);
        desc.SetLabel("Sprite Flipbook");
        desc.SetEditorCategory("Rendering");
        desc.SetDescription("2D Flipbook animation component");
        desc.AddMember(&CSpriteFlipbookComponent::m_materialPath, 'mat', "Material", "Sprite Material", "Specifies the override material for the selected object", "");
        desc.AddMember(&CSpriteFlipbookComponent::m_localScale, 'scal', "LocalScale", "Local Scale", "Per-component scale override", Vec3(1.0f));
    }

    void LoadMaterial();
    
    void Play(const FFlipbookAnim& anim);
    void SetFacing(bool facingRight);
private:
    void Update(float frameTime);
    void ApplyUVOffset(int frameX, int frameY);


    int m_slotId = -1;
    int m_columns = -1;
    int m_rows = -1;

    float m_elapsed = 0.f;     
    int m_currentFrame = 0;
    int m_currentRow = 0;

    Vec3 m_localScale = Vec3(1.0f, 1.0f, 1.0f);

    FFlipbookAnim m_currentAnimationData;

    Schematyc::MaterialFileName m_materialPath;
    IMaterial* m_pAnimatedMaterial = nullptr;

    // cache
    SShaderParam* m_pFrameXParam = nullptr;
    SShaderParam* m_pFrameYParam = nullptr;
};
