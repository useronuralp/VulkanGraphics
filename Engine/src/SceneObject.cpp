// SceneObject.cpp
#include "SceneObject.h"

SceneObject::SceneObject(const std::string& InName) : _Name(InName)
{
}

void SceneObject::SetPosition(const glm::vec3& InPosition)
{
    _Position = InPosition;
    MarkDirty();
}

void SceneObject::SetRotation(const glm::vec3& InEulerDegrees)
{
    _Rotation = InEulerDegrees;
    MarkDirty();
}

void SceneObject::SetScale(const glm::vec3& InScale)
{
    _Scale = InScale;
    MarkDirty();
}

void SceneObject::Translate(const glm::vec3& InDelta)
{
    _Position += InDelta;
    MarkDirty();
}

void SceneObject::Rotate(float InAngleDeg, const glm::vec3& InAxis)
{
    _Rotation += InAngleDeg * InAxis;
    MarkDirty();
}

void SceneObject::SetCastsShadow(bool InCasts)
{
    _CastsShadow = InCasts;
}

bool SceneObject::GetCastsShadow() const
{
    return _CastsShadow;
}

const glm::vec3& SceneObject::GetPosition() const
{
    return _Position;
}

const glm::vec3& SceneObject::GetRotation() const
{
    return _Rotation;
}

const glm::vec3& SceneObject::GetScale() const
{
    return _Scale;
}

const glm::mat4& SceneObject::GetTransform()
{
    if (_Dirty)
    {
        RecalculateTransform();
    }
    return _Transform;
}

void SceneObject::SetName(const std::string& InName)
{
    _Name = InName;
}

const std::string& SceneObject::GetName() const
{
    return _Name;
}

void SceneObject::MarkDirty()
{
    _Dirty = true;
}

void SceneObject::RecalculateTransform()
{
    _Transform = glm::mat4(1.0f);
    _Transform = glm::translate(_Transform, _Position);
    _Transform = glm::rotate(_Transform, glm::radians(_Rotation.x), glm::vec3(1, 0, 0));
    _Transform = glm::rotate(_Transform, glm::radians(_Rotation.y), glm::vec3(0, 1, 0));
    _Transform = glm::rotate(_Transform, glm::radians(_Rotation.z), glm::vec3(0, 0, 1));
    _Transform = glm::scale(_Transform, _Scale);
    _Dirty     = false;
}