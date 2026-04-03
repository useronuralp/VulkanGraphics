// Scene.h
#pragma once
#include "core.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>

class Camera;
class Model;
class ParticleSystem;
class Image;

struct DirectionalLight
{
    glm::vec4 Position    = glm::vec4(0.0f, 10.0f, 0.0f, 1.0f);
    float     Intensity   = 10.0f;
    bool      CastsShadow = true;
};

struct PointLight
{
    glm::vec3 Position    = glm::vec3(0.0f);
    glm::vec3 Color       = glm::vec3(1.0f);
    float     Intensity   = 1.0f;
    bool      CastsShadow = true;
};

struct RenderObject
{
    Ref<Model> Model;
    glm::mat4  Transform     = glm::mat4(1.0f);
    glm::vec4  EmissiveColor = glm::vec4(0.0f); // non-zero = emissive
};

struct ParticleEmitterGroup
{
    Ref<ParticleSystem> Sparks;
    Ref<ParticleSystem> FlameBase;
};

class Scene
{
   public:
    Scene()  = default;
    ~Scene() = default;

    void        SetCamera(Ref<Camera> InCamera);
    Ref<Camera> GetCamera() const;

    void                             AddRenderObject(const RenderObject& InObject);
    const std::vector<RenderObject>& GetRenderObjects() const;

    void                             AddEmissiveObject(const RenderObject& InObject);
    const std::vector<RenderObject>& GetEmissiveObjects() const;

    void                    SetDirectionalLight(const DirectionalLight& InLight);
    const DirectionalLight& GetDirectionalLight() const;

    void                           AddPointLight(const PointLight& InLight);
    const std::vector<PointLight>& GetPointLights() const;
    std::vector<PointLight>&       GetPointLightsMutable();

    void       SetSkybox(Ref<Model> InSkybox);
    Ref<Model> GetSkybox() const;

    void                                     AddParticleEmitter(const ParticleEmitterGroup& InEmitter);
    const std::vector<ParticleEmitterGroup>& GetParticleEmitters() const;

    void                SetAmbientParticles(Ref<ParticleSystem> InParticles);
    Ref<ParticleSystem> GetAmbientParticles() const;

    void Update(float InDeltaTime);

   private:
    Ref<Camera> _Camera;

    std::vector<RenderObject> _RenderObjects;
    std::vector<RenderObject> _EmissiveObjects;

    DirectionalLight        _DirectionalLight;
    std::vector<PointLight> _PointLights;

    Ref<Model> _Skybox;

    std::vector<ParticleEmitterGroup> _ParticleEmitters;
    Ref<ParticleSystem>               _AmbientParticles;
};