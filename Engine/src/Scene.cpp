// Scene.cpp
#include "Camera.h"
#include "LightObject.h"
#include "ParticleSystem.h"
#include "Scene.h"
#include "StaticMeshObject.h"

void Scene::SetCamera(const Ref<Camera>& InCamera)
{
    _Camera = InCamera;
}

Ref<Camera> Scene::GetCamera() const
{
    return _Camera;
}

void Scene::AddMeshObject(const Ref<StaticMeshObject>& InObject)
{
    _MeshObjects.emplace_back(InObject);
}

std::vector<Ref<StaticMeshObject>>& Scene::GetMeshObjects()
{
    return _MeshObjects;
}

Ref<StaticMeshObject> Scene::FindMeshObject(const std::string& InName) const
{
    for (auto& obj : _MeshObjects)
    {
        if (obj->GetName() == InName)
        {
            return obj;
        }
    }

    for (auto& obj : _EmissiveObjects)
    {
        if (obj->GetName() == InName)
        {
            return obj;
        }
    }

    for (auto& obj : _DebugObjects)
    {
        if (obj->GetName() == InName)
        {
            return obj;
        }
    }

    return nullptr;
}

void Scene::AddEmissiveObject(const Ref<StaticMeshObject>& InObject)
{
    _EmissiveObjects.emplace_back(InObject);
}

std::vector<Ref<StaticMeshObject>>& Scene::GetEmissiveObjects()
{
    return _EmissiveObjects;
}

void Scene::SetDirectionalLight(const Ref<LightObject>& InLight)
{
    _DirectionalLight = InLight;
}

Ref<LightObject> Scene::GetDirectionalLight() const
{
    return _DirectionalLight;
}

void Scene::AddPointLight(const Ref<LightObject>& InLight)
{
    _PointLights.emplace_back(InLight);
}

std::vector<Ref<LightObject>>& Scene::GetPointLights()
{
    return _PointLights;
}

int Scene::GetPointLightCount() const
{
    return static_cast<int>(_PointLights.size());
}

void Scene::SetSkybox(const Ref<StaticMeshObject>& InSkybox)
{
    _Skybox    = InSkybox;
    _HasSkybox = true;
}

Ref<StaticMeshObject> Scene::GetSkybox() const
{
    return _HasSkybox ? _Skybox : nullptr;
}

void Scene::AddDebugObject(const Ref<StaticMeshObject>& InObject)
{
    _DebugObjects.emplace_back(InObject);
}

std::vector<Ref<StaticMeshObject>>& Scene::GetDebugObjects()
{
    return _DebugObjects;
}

void Scene::AddTorchGroup(const Ref<StaticMeshObject>& InTorch, const Ref<LightObject>& InLight, const Ref<ParticleSystem>& InSparks, const Ref<ParticleSystem>& InFlame)
{
    _TorchGroups.push_back(make_s<TorchFireGroup>(InTorch, InLight, InSparks, InFlame));
}

std::vector<Ref<TorchFireGroup>>& Scene::GetTorchGroups()
{
    return _TorchGroups;
}

void Scene::SetAmbientParticles(const Ref<ParticleSystem>& InParticles)
{
    _AmbientParticles = InParticles;
}

Ref<ParticleSystem> Scene::GetAmbientParticles() const
{
    return _AmbientParticles;
}

void Scene::Update(float InDeltaTime)
{
    for (auto& group : _TorchGroups)
    {
        if (group->Sparks)
        {
            group->Sparks->UpdateParticles(InDeltaTime);
        }
        if (group->Flame)
        {
            group->Flame->UpdateParticles(InDeltaTime);
        }
    }

    if (_AmbientParticles)
    {
        _AmbientParticles->UpdateParticles(InDeltaTime);
    }
}

std::vector<Ref<StaticMeshObject>> Scene::GetShadowCasters() const
{
    std::vector<Ref<StaticMeshObject>> casters;

    for (auto obj : _MeshObjects)
    {
        if (obj->GetCastsShadow())
        {
            casters.push_back(obj);
        }
    }

    return casters;
}