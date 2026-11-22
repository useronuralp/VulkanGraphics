#include "Engine.h"
#include "EngineInternal.h"
#include "Image.h"
#include "Model.h"
#include "ParticleSystem.h"
#include "Renderer/Renderer.h"
#include "Scene.h"

// Create with new because EngineInternal constructor is private and can only be accessed from this class
Engine::Engine() : _Internal(Unique<EngineInternal>(new EngineInternal()))
{
}

Engine& Engine::Get()
{
    // This uses the constructor above which initializes _Internal.
    static Engine instance;
    return instance;
}

void Engine::Init(RendererType InRendererType)
{
    _Internal->Init(InRendererType);
}

void Engine::Run()
{
    _Internal->Run();
}

Ref<Camera> Engine::GetCamera()
{
    return _Internal->_Camera;
}

void Engine::SetScene(const Ref<Scene>& InScene)
{
    _Internal->_Renderer->SetScene(InScene);
}

// todo: When loading models/materials, checking rasterizer first and the falling back on path tracer is incorrect behaviour. Caller can't predict what would happen.
Ref<Model> Engine::LoadPBRModel(const std::string& InPath, LoadingFlags InFlags)
{
    if (auto* rasterizer = _Internal->TryGetRasterizer())
    {
        return rasterizer->LoadPBRModel(InPath, InFlags);
    }

    return make_s<Model>(InPath, InFlags);
}

Ref<Model> Engine::LoadSimpleModel(const std::string& InPath, LoadingFlags InFlags)
{
    if (auto* rasterizer = _Internal->TryGetRasterizer())
    {
        return rasterizer->LoadSimpleModel(InPath, InFlags);
    }

    return make_s<Model>(InPath, InFlags);
}

// The skybox cube and debug meshes are raster visualisation. The path tracer's sky will be an
// environment map sampled in the miss shader, and it gets its own API when we get there.
Ref<Model> Engine::LoadSkyboxModel(const float* InVertices, uint32_t InVertexCount, const std::vector<std::string>& InFaces)
{
    if (auto* rasterizer = _Internal->TryGetRasterizer())
    {
        return rasterizer->LoadSkyboxModel(InVertices, InVertexCount, make_s<Image>(InFaces, VK_FORMAT_R8G8B8A8_SRGB));
    }

    ENSURE(false, std::string("Engine::LoadSkyboxModel") + " is raster-only; returning nullptr in path tracer mode.");
    return nullptr;
}

Ref<Model> Engine::LoadDebugModel(const float* InVertices, uint32_t InVertexCount)
{
    if (auto* rasterizer = _Internal->TryGetRasterizer())
    {
        return rasterizer->LoadDebugModel(InVertices, InVertexCount);
    }

    ENSURE(false, std::string("Engine::LoadDebugModel") + " is raster-only; returning nullptr in path tracer mode.");
    return nullptr;
}

// Plain images - meaningful to both renderers.
Ref<Image> Engine::LoadTexture(const std::string& InPath)
{
    return make_s<Image>(std::vector{ InPath }, VK_FORMAT_R8G8B8A8_SRGB);
}

Ref<Image> Engine::LoadCubemap(const std::vector<std::string>& InFaces)
{
    return make_s<Image>(InFaces, VK_FORMAT_R8G8B8A8_SRGB);
}

// Raster materials are pipelines + descriptor layouts; they have no meaning to the path tracer.
Ref<Material> Engine::GetPBRMaterial()
{
    if (auto* rasterizer = _Internal->TryGetRasterizer())
    {
        return rasterizer->GetPBRMaterial();
    }

    ENSURE(false, std::string("Engine::GetPBRMaterial") + " is raster-only; returning nullptr in path tracer mode.");
    return nullptr;
}

Ref<Material> Engine::GetEmissiveMaterial()
{
    if (auto* rasterizer = _Internal->TryGetRasterizer())
    {
        return rasterizer->GetEmissiveMaterial();
    }

    ENSURE(false, std::string("Engine::GetEmissiveMaterial") + " is raster-only; returning nullptr in path tracer mode.");
    return nullptr;
}

Ref<Material> Engine::GetSkyboxMaterial()
{
    if (auto* rasterizer = _Internal->TryGetRasterizer())
    {
        return rasterizer->GetSkyboxMaterial();
    }

    ENSURE(false, std::string("Engine::GetSkyboxMaterial") + " is raster-only; returning nullptr in path tracer mode.");
    return nullptr;
}

Ref<Material> Engine::GetCubeMaterial()
{
    if (auto* rasterizer = _Internal->TryGetRasterizer())
    {
        return rasterizer->GetCubeMaterial();
    }

    ENSURE(false, std::string("Engine::GetCubeMaterial") + " is raster-only; returning nullptr in path tracer mode.");
    return nullptr;
}

Ref<ParticleSystem> Engine::CreateParticleSystem(const ParticleSpecs& InSpecs, const Ref<Image>& InTexture)
{
    if (auto* rasterizer = _Internal->TryGetRasterizer())
    {
        return rasterizer->CreateParticleSystem(InSpecs, InTexture);
    }

    ENSURE(false, std::string("Engine::CreateParticleSystem") + " is raster-only; returning nullptr in path tracer mode.");
    return nullptr;
}
