// Scene.h
#pragma once
#include "core.h"
#include "LightObject.h"
#include "StaticMeshObject.h"

#include <string>
#include <vector>

class Camera;
class ParticleSystem;

struct TorchFireGroup
{
    Ref<StaticMeshObject> Torch  = nullptr;
    Ref<LightObject>      Light  = nullptr;
    Ref<ParticleSystem>   Sparks = nullptr;
    Ref<ParticleSystem>   Flame  = nullptr;
};

class Scene
{
   public:
    Scene()  = default;
    ~Scene() = default;

    // Camera
    void        SetCamera(const Ref<Camera>& InCamera);
    Ref<Camera> GetCamera() const;

    // Static mesh objects
    void                                AddMeshObject(const Ref<StaticMeshObject>& InObject);
    std::vector<Ref<StaticMeshObject>>& GetMeshObjects();
    Ref<StaticMeshObject>               FindMeshObject(const std::string& InName) const;

    // Emissive objects
    void                                AddEmissiveObject(const Ref<StaticMeshObject>& InObject);
    std::vector<Ref<StaticMeshObject>>& GetEmissiveObjects();

    // Debug objects
    void                                AddDebugObject(const Ref<StaticMeshObject>& InObject);
    std::vector<Ref<StaticMeshObject>>& GetDebugObjects();

    // Skybox
    void                  SetSkybox(const Ref<StaticMeshObject>& InSkybox);
    Ref<StaticMeshObject> GetSkybox() const;

    // Lights
    void             SetDirectionalLight(const Ref<LightObject>& InLight);
    Ref<LightObject> GetDirectionalLight() const;

    void                           AddPointLight(const Ref<LightObject>& InLight);
    std::vector<Ref<LightObject>>& GetPointLights();
    int                            GetPointLightCount() const;

    // Torch fire groups
    void AddTorchGroup(const Ref<StaticMeshObject>& InTorch, const Ref<LightObject>& InLight, const Ref<ParticleSystem>& InSparks, const Ref<ParticleSystem>& InFlame);
    std::vector<Ref<TorchFireGroup>>& GetTorchGroups();

    // Ambient particles
    void                SetAmbientParticles(const Ref<ParticleSystem>& InParticles);
    Ref<ParticleSystem> GetAmbientParticles() const;

    // Per-frame
    void Update(float InDeltaTime);

    // Shadow casters
    std::vector<Ref<StaticMeshObject>> GetShadowCasters() const;

   private:
    Ref<Camera> _Camera;

    std::vector<Ref<StaticMeshObject>> _MeshObjects;
    std::vector<Ref<StaticMeshObject>> _EmissiveObjects;
    std::vector<Ref<StaticMeshObject>> _DebugObjects;
    std::vector<Ref<LightObject>>      _PointLights;
    std::vector<Ref<TorchFireGroup>>   _TorchGroups;

    Ref<ParticleSystem>   _AmbientParticles = nullptr;
    Ref<LightObject>      _DirectionalLight = nullptr;
    Ref<StaticMeshObject> _Skybox           = nullptr;

    bool _HasSkybox                         = false;
};