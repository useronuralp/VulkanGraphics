// Scene.cpp
#include "Camera.h"
#include "Model.h"
#include "ParticleSystem.h"
#include "Scene.h"

void Scene::SetCamera(Ref<Camera> InCamera)
{
    _Camera = InCamera;
}

Ref<Camera> Scene::GetCamera() const
{
    return _Camera;
}

void Scene::AddRenderObject(const RenderObject& InObject)
{
    _RenderObjects.push_back(InObject);
}

const std::vector<RenderObject>& Scene::GetRenderObjects() const
{
    return _RenderObjects;
}

void Scene::AddEmissiveObject(const RenderObject& InObject)
{
    _EmissiveObjects.push_back(InObject);
}

const std::vector<RenderObject>& Scene::GetEmissiveObjects() const
{
    return _EmissiveObjects;
}

void Scene::SetDirectionalLight(const DirectionalLight& InLight)
{
    _DirectionalLight = InLight;
}

const DirectionalLight& Scene::GetDirectionalLight() const
{
    return _DirectionalLight;
}

void Scene::AddPointLight(const PointLight& InLight)
{
    _PointLights.push_back(InLight);
}

const std::vector<PointLight>& Scene::GetPointLights() const
{
    return _PointLights;
}

std::vector<PointLight>& Scene::GetPointLightsMutable()
{
    return _PointLights;
}

void Scene::SetSkybox(Ref<Model> InSkybox)
{
    _Skybox = InSkybox;
}

Ref<Model> Scene::GetSkybox() const
{
    return _Skybox;
}

void Scene::AddParticleEmitter(const ParticleEmitterGroup& InEmitter)
{
    _ParticleEmitters.push_back(InEmitter);
}

const std::vector<ParticleEmitterGroup>& Scene::GetParticleEmitters() const
{
    return _ParticleEmitters;
}

void Scene::SetAmbientParticles(Ref<ParticleSystem> InParticles)
{
    _AmbientParticles = InParticles;
}

Ref<ParticleSystem> Scene::GetAmbientParticles() const
{
    return _AmbientParticles;
}

void Scene::Update(float InDeltaTime)
{
    for (auto& emitter : _ParticleEmitters)
    {
        if (emitter.Sparks)
        {
            emitter.Sparks->UpdateParticles(InDeltaTime);
        }
        if (emitter.FlameBase)
        {
            emitter.FlameBase->UpdateParticles(InDeltaTime);
        }
    }

    if (_AmbientParticles)
    {
        _AmbientParticles->UpdateParticles(InDeltaTime);
    }
}