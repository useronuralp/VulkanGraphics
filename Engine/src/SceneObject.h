#pragma once
#include "core.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>

class SceneObject
{
   public:
    SceneObject() = default;
    SceneObject(const std::string& InName);
    virtual ~SceneObject() = default;

    void SetPosition(const glm::vec3& InPosition);
    void SetRotation(const glm::vec3& InEulerDegrees);
    void SetScale(const glm::vec3& InScale);

    void Translate(const glm::vec3& InDelta);
    void Rotate(float InAngleDeg, const glm::vec3& InAxis);

    const glm::vec3& GetPosition() const;
    glm::vec3&       GetPositionMutable();
    const glm::vec3& GetRotation() const;
    const glm::vec3& GetScale() const;
    const glm::mat4& GetTransform();
    glm::mat4&       GetTransformMutable();

    void               SetName(const std::string& InName);
    const std::string& GetName() const;

   protected:
    void MarkDirty();

   private:
    void RecalculateTransform();

    glm::vec3 _Position  = glm::vec3(0.0f);
    glm::vec3 _Rotation  = glm::vec3(0.0f); // euler degrees
    glm::vec3 _Scale     = glm::vec3(1.0f);
    glm::mat4 _Transform = glm::mat4(1.0f);
    bool      _Dirty     = true;

    std::string _Name    = "Unnamed";
};