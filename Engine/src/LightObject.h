#pragma once
#include "SceneObject.h"

enum class LightType
{
    Point,
    Directional,
    Spot
};

class LightObject : public SceneObject
{
   public:
    LightObject() = default;
    LightObject(const std::string& InName, LightType InType);

    void      SetLightType(LightType InType);
    LightType GetLightType() const;

    void             SetColor(const glm::vec3& InColor);
    const glm::vec3& GetColor() const;

    void  SetIntensity(float InIntensity);
    float GetIntensity() const;

    // Spot light only
    void  SetInnerAngle(float InDegrees);
    void  SetOuterAngle(float InDegrees);
    float GetInnerAngle() const;
    float GetOuterAngle() const;

    // Direction — for directional and spot lights
    glm::vec3 GetDirection() const;

   private:
    LightType _Type      = LightType::Point;
    glm::vec3 _Color     = glm::vec3(1.0f);
    float     _Intensity = 1.0f;

    // Spot light params
    float _InnerAngle = 30.0f;
    float _OuterAngle = 45.0f;
};