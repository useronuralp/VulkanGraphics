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

void Engine::Init()
{
    _Internal->Init();
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

// Engine.cpp — all forwarding to renderer
Ref<Model> Engine::LoadPBRModel(const std::string& InPath, LoadingFlags InFlags)
{
    return _Internal->_Renderer->LoadPBRModel(InPath, InFlags);
}

Ref<Model> Engine::LoadSimpleModel(const std::string& InPath, LoadingFlags InFlags)
{
    return _Internal->_Renderer->LoadSimpleModel(InPath, InFlags);
}

Ref<Model> Engine::LoadSkyboxModel(const float* InVertices, uint32_t InVertexCount, const std::vector<std::string>& InFaces)
{
    auto cubemap = make_s<Image>(InFaces, VK_FORMAT_R8G8B8A8_SRGB);
    return _Internal->_Renderer->LoadSkyboxModel(InVertices, InVertexCount, cubemap);
}

Ref<Model> Engine::LoadDebugModel(const float* InVertices, uint32_t InVertexCount)
{
    return _Internal->_Renderer->LoadDebugModel(InVertices, InVertexCount);
}

Ref<Image> Engine::LoadTexture(const std::string& InPath)
{
    return make_s<Image>(std::vector{ InPath }, VK_FORMAT_R8G8B8A8_SRGB);
}

Ref<Image> Engine::LoadCubemap(const std::vector<std::string>& InFaces)
{
    return make_s<Image>(InFaces, VK_FORMAT_R8G8B8A8_SRGB);
}

Ref<Material> Engine::GetPBRMaterial()
{
    return _Internal->_Renderer->GetPBRMaterial();
}
Ref<Material> Engine::GetEmissiveMaterial()
{
    return _Internal->_Renderer->GetEmissiveMaterial();
}
Ref<Material> Engine::GetSkyboxMaterial()
{
    return _Internal->_Renderer->GetSkyboxMaterial();
}
Ref<Material> Engine::GetCubeMaterial()
{
    return _Internal->_Renderer->GetCubeMaterial();
}

Ref<ParticleSystem> Engine::CreateParticleSystem(const ParticleSpecs& InSpecs, const Ref<Image>& InTexture)
{
    return _Internal->_Renderer->CreateParticleSystem(InSpecs, InTexture);
}