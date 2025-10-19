#include "Camera.h"
#include "core.h"
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

void Scene::AddModel(Ref<Model> InModel)
{
    _Models.push_back(InModel);
}

const std::vector<Ref<Model>>& Scene::GetModels() const
{
    return _Models;
}

void Scene::AddLight(const Light& InLight)
{
    _Lights.push_back(InLight);
}

const std::vector<Light>& Scene::GetLights() const
{
    return _Lights;
}

void Scene::Update(float deltaTime)
{
    // TODO: Maybe update camera, animations, etc.
}
