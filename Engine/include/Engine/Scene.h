#pragma once
#include "core.h"

#include <glm/glm.hpp>
#include <vector>

class Camera;
class Model;
class Light;

struct Light
{
    glm::vec3 Position{ 0.0f };
    glm::vec3 Color{ 1.0f };
    float     Intensity   = 1.0f;
    bool      CastsShadow = true;
};

class Scene
{
   public:
    Scene()  = default;
    ~Scene() = default;

    // Global camera for now — later this can be per-viewport or per-entity
    void        SetCamera(Ref<Camera> InCamera);
    Ref<Camera> GetCamera() const;

    void                           AddModel(Ref<Model> InModel);
    const std::vector<Ref<Model>>& GetModels() const;

    void                      AddLight(const Light& InLight);
    const std::vector<Light>& GetLights() const;

    void Update(float deltaTime);

   private:
    Ref<Camera>             _Camera;
    std::vector<Ref<Model>> _Models;
    std::vector<Light>      _Lights; // simple struct for now
};
