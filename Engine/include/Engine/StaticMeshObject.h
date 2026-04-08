#pragma once
#include "SceneObject.h"

class Model;
class Material;

class StaticMeshObject : public SceneObject
{
   public:
    StaticMeshObject() = default;
    StaticMeshObject(const std::string& InName, Ref<Model> InModel);

    void          SetMaterial(Ref<Material> InMaterial);
    Ref<Material> GetMaterial() const;

    void       SetModel(Ref<Model> InModel);
    Ref<Model> GetModel() const;

   private:
    Ref<Model>    _Model;
    Ref<Material> _Material;
};