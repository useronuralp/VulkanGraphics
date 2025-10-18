#pragma once
#include "core.h"
#include "glm/glm.hpp"

#include <vector>

class Camera;
class Model;
class Light;

struct Light
{
    glm::vec3 position;
    glm::vec3 color;
    float     intensity;
    bool      castsShadows = false;
};

class Scene
{
   public:
    Scene()  = default;
    ~Scene() = default;

    // Global camera for now — later this can be per-viewport or per-entity
    void        SetCamera(Ref<Camera> camera);
    Ref<Camera> GetCamera() const;

    void                           AddModel(Ref<Model> model);
    const std::vector<Ref<Model>>& GetModels() const;

    void                      AddLight(const Light& light);
    const std::vector<Light>& GetLights() const;

   private:
    Ref<Camera>             _Camera;
    std::vector<Ref<Model>> _Models;
    std::vector<Light>      _Lights; // simple struct for now
};
