#pragma once
#include "SceneObject.h"

class Model;

class StaticMeshObject : public SceneObject
{
   public:
    StaticMeshObject() = default;
    StaticMeshObject(const std::string& InName, Ref<Model> InModel);

    void       SetModel(Ref<Model> InModel);
    Ref<Model> GetModel() const;

   private:
    Ref<Model> _Model;
};