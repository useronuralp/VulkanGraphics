// LightObject.cpp
#include "LightObject.h"

LightObject::LightObject(const std::string& InName, LightType InType) : SceneObject(InName), _Type(InType)
{
}

void LightObject::SetLightType(LightType InType)
{
    _Type = InType;
}

LightType LightObject::GetLightType() const
{
    return _Type;
}

void LightObject::SetColor(const glm::vec3& InColor)
{
    _Color = InColor;
}

const glm::vec3& LightObject::GetColor() const
{
    return _Color;
}

void LightObject::SetIntensity(float InIntensity)
{
    _Intensity = InIntensity;
}

float LightObject::GetIntensity() const
{
    return _Intensity;
}

void LightObject::SetCastsShadow(bool InCasts)
{
    _CastsShadow = InCasts;
}

bool LightObject::GetCastsShadow() const
{
    return _CastsShadow;
}

void LightObject::SetInnerAngle(float InDegrees)
{
    _InnerAngle = InDegrees;
}

void LightObject::SetOuterAngle(float InDegrees)
{
    _OuterAngle = InDegrees;
}

float LightObject::GetInnerAngle() const
{
    return _InnerAngle;
}

float LightObject::GetOuterAngle() const
{
    return _OuterAngle;
}

glm::vec3 LightObject::GetDirection() const
{
    glm::mat4 rot = glm::mat4(1.0f);
    rot           = glm::rotate(rot, glm::radians(GetRotation().x), glm::vec3(1, 0, 0));
    rot           = glm::rotate(rot, glm::radians(GetRotation().y), glm::vec3(0, 1, 0));
    rot           = glm::rotate(rot, glm::radians(GetRotation().z), glm::vec3(0, 0, 1));
    return glm::normalize(glm::vec3(rot * glm::vec4(0, -1, 0, 0)));
}