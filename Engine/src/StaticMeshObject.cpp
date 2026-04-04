#include "Material.h"
#include "StaticMeshObject.h"
StaticMeshObject::StaticMeshObject(const std::string& InName, Ref<Model> InModel) : SceneObject(InName), _Model(InModel)
{
}

void StaticMeshObject::SetModel(Ref<Model> InModel)
{
    _Model = InModel;
}

Ref<Model> StaticMeshObject::GetModel() const
{
    return _Model;
}

void StaticMeshObject::SetMaterial(Ref<Material> InMaterial)
{
    _Material = InMaterial;
}

Ref<Material> StaticMeshObject::GetMaterial() const
{
    return _Material;
}