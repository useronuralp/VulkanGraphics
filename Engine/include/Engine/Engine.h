#pragma once

#include "core.h"

class EngineInternal;
class Scene;
class Camera;
class Model;
class Image;
class Material;
class ParticleSystem;
struct ParticleSpecs;
enum LoadingFlags;

class Engine
{
   public:
    // public API
    void           Init();
    void           Run();
    static Engine& Get();

    Ref<Camera> GetCamera();
    void        SetScene(const Ref<Scene>& InScene);

    // Model loading
    Ref<Model> LoadPBRModel(const std::string& InPath, LoadingFlags InFlags);
    Ref<Model> LoadSimpleModel(const std::string& InPath, LoadingFlags InFlags);
    Ref<Model> LoadSkyboxModel(const float* InVertices, uint32_t InVertexCount, const std::vector<std::string>& InFaces);
    Ref<Model> LoadDebugModel(const float* InVertices, uint32_t InVertexCount);

    // Texture loading
    Ref<Image> LoadTexture(const std::string& InPath);
    Ref<Image> LoadCubemap(const std::vector<std::string>& InFaces);

    // Materials
    Ref<Material> GetPBRMaterial();
    Ref<Material> GetEmissiveMaterial();
    Ref<Material> GetSkyboxMaterial();
    Ref<Material> GetCubeMaterial();

    // Particles
    Ref<ParticleSystem> CreateParticleSystem(const ParticleSpecs& InSpecs, const Ref<Image>& InTexture);
    //  ~public API end

   private:
    Engine();
    ~Engine()                        = default;
    Engine(const Engine&)            = delete;
    Engine& operator=(const Engine&) = delete;

    Unique<EngineInternal> _Internal;
    friend class EngineInternal;
};