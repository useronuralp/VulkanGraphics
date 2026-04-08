#include "Engine/Engine.h"
#include "Engine/LightObject.h"
#include "Engine/Model.h"
#include "Engine/ParticleSystem.h"
#include "Engine/Scene.h"
#include "Engine/SceneObject.h"
#include "Engine/StaticMeshObject.h"

Ref<Scene> CreateScene()
{
    auto& engine = Engine::Get();
    auto  scene  = make_s<Scene>();
    scene->SetCamera(engine.GetCamera());

    // ── Sponza ──────────────────────────────────────────────

    auto sponzaModel = engine.LoadPBRModel(
        std::string(SOLUTION_DIR) + "Engine/assets/models/Sponza/scene.gltf", LOAD_VERTEX_POSITIONS | LOAD_NORMALS | LOAD_BITANGENT | LOAD_TANGENT | LOAD_UV);

    auto sponza = make_s<StaticMeshObject>("Sponza", sponzaModel);
    sponza->SetScale(glm::vec3(0.005f));
    sponza->SetMaterial(engine.GetPBRMaterial());
    sponza->SetCastsShadow(true);
    scene->AddMeshObject(sponza);

    // ── Helmet ──────────────────────────────────────────────

    auto helmetModel = engine.LoadPBRModel(
        std::string(SOLUTION_DIR) + "Engine/assets/models/MaleniaHelmet/scene.gltf", LOAD_VERTEX_POSITIONS | LOAD_NORMALS | LOAD_BITANGENT | LOAD_TANGENT | LOAD_UV);

    auto helmet = make_s<StaticMeshObject>("Helmet", helmetModel);
    helmet->SetPosition(glm::vec3(0.0f, 2.0f, 0.0f));
    helmet->Rotate(90, glm::vec3(0, 1, 0));
    helmet->SetScale(glm::vec3(0.7f));
    helmet->SetMaterial(engine.GetPBRMaterial());
    helmet->SetCastsShadow(true);
    scene->AddMeshObject(helmet);

    // ── Torches ─────────────────────────────────────────────

    auto torchModel = engine.LoadPBRModel(
        std::string(SOLUTION_DIR) + "Engine/assets/models/torch/scene.gltf", LOAD_VERTEX_POSITIONS | LOAD_NORMALS | LOAD_BITANGENT | LOAD_TANGENT | LOAD_UV);

    auto particleTexture = engine.LoadTexture(std::string(SOLUTION_DIR) + "Engine/assets/textures/spark.png");
    auto fireTexture     = engine.LoadTexture(std::string(SOLUTION_DIR) + "Engine/assets/textures/fire_sprite_sheet.png");
    auto dustTexture     = engine.LoadTexture(std::string(SOLUTION_DIR) + "Engine/assets/textures/dust.png");

    struct TorchPlacement
    {
        glm::vec3 Position;
        float     RotationDeg;
    };

    std::vector<TorchPlacement> torchPlacements = {
        { { 2.450f, 1.3f, 0.810f }, 90.0f },
        { { 0.610f, 1.3f, -1.170f }, -90.0f },
        { { 0.610f, 1.3f, 0.810f }, 90.0f },
        { { 2.450f, 1.3f, -1.170f }, -90.0f },
    };

    for (int i = 0; i < torchPlacements.size(); i++)
    {
        auto& placement = torchPlacements[i];

        auto torchObj   = make_s<StaticMeshObject>("Torch " + std::to_string(i + 1), torchModel);
        torchObj->SetPosition(placement.Position);
        torchObj->Rotate(placement.RotationDeg, glm::vec3(0, 1, 0));
        torchObj->SetScale(glm::vec3(0.3f));
        torchObj->SetMaterial(engine.GetPBRMaterial());
        torchObj->SetCastsShadow(false);
        scene->AddMeshObject(torchObj);

        auto light = make_s<LightObject>("Torch Light " + std::to_string(i + 1), LightType::Point);
        light->SetPosition(placement.Position + glm::vec3(0.0f, 0.22f, 0.0f));
        light->SetColor(glm::vec3(0.97f, 0.76f, 0.46f));
        light->SetIntensity(25.0f);
        light->SetCastsShadow(true);
        scene->AddPointLight(light);

        ParticleSpecs sparkSpecs{};
        sparkSpecs.ParticleCount       = 10;
        sparkSpecs.EnableNoise         = true;
        sparkSpecs.TrailLength         = 2;
        sparkSpecs.SphereRadius        = 0.05f;
        sparkSpecs.ImmortalParticle    = false;
        sparkSpecs.ParticleSize        = 0.5f;
        sparkSpecs.EmitterPos          = placement.Position + glm::vec3(0.0f, 0.22f, 0.0f);
        sparkSpecs.ParticleMinLifetime = 0.1f;
        sparkSpecs.ParticleMaxLifetime = 3.0f;
        sparkSpecs.MinVel              = glm::vec3(-1.0f, 0.1f, -1.0f);
        sparkSpecs.MaxVel              = glm::vec3(1.0f, 2.0f, 1.0f);

        auto sparks                    = engine.CreateParticleSystem(sparkSpecs, particleTexture);

        ParticleSpecs flameSpecs{};
        flameSpecs.ParticleCount       = 1;
        flameSpecs.ImmortalParticle    = true;
        flameSpecs.ParticleSize        = 13.0f;
        flameSpecs.EnableNoise         = false;
        flameSpecs.SphereRadius        = 0.0f;
        flameSpecs.TrailLength         = 0;
        flameSpecs.EmitterPos          = placement.Position + glm::vec3(0.0f, 0.28f, 0.0f);
        flameSpecs.ParticleMinLifetime = 0.1f;
        flameSpecs.ParticleMaxLifetime = 1.5f;
        flameSpecs.MinVel              = glm::vec3(0.0f);
        flameSpecs.MaxVel              = glm::vec3(0.0f);

        auto flame                     = engine.CreateParticleSystem(flameSpecs, fireTexture);
        flame->RowOffset               = 0.0f;
        flame->RowCellSize             = 0.0833333333333333333333f;
        flame->ColumnCellSize          = 0.166666666666666f;
        flame->ColumnOffset            = 0.0f;

        scene->AddTorchGroup(torchObj, light, sparks, flame);
    }

    // ── Directional Light ───────────────────────────────────

    auto dirLight = make_s<LightObject>("Directional Light", LightType::Directional);
    dirLight->SetPosition(glm::vec3(-10.0f, 35.0f, -22.0f));
    dirLight->SetIntensity(10.0f);
    dirLight->SetColor(glm::vec3(1.0f));
    dirLight->SetCastsShadow(true);
    scene->SetDirectionalLight(dirLight);

    // ── Sword ───────────────────────────────────────────────

    auto swordModel = engine.LoadSimpleModel(std::string(SOLUTION_DIR) + "Engine/assets/models/sword/scene.gltf", LOAD_VERTEX_POSITIONS);

    auto sword      = make_s<StaticMeshObject>("Sword", swordModel);
    sword->SetPosition(glm::vec3(-2, 7, 0));
    sword->Rotate(54, glm::vec3(0, 0, 1));
    sword->Rotate(90, glm::vec3(0, 1, 0));
    sword->SetScale(glm::vec3(0.7f));
    sword->SetMaterial(engine.GetEmissiveMaterial());
    scene->AddEmissiveObject(sword);

    // ── Skybox ──────────────────────────────────────────────

    const uint32_t vertexCount               = 3 * 6 * 6;
    const float    cubeVertices[vertexCount] = {
        -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, 1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f, 1.0f,
        1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, 1.0f,
        1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f, -1.0f,
        1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,
    };

    std::vector<std::string> skyboxFaces{
        std::string(SOLUTION_DIR) + "Engine/assets/textures/skybox/Night/right.png", std::string(SOLUTION_DIR) + "Engine/assets/textures/skybox/Night/left.png",
        std::string(SOLUTION_DIR) + "Engine/assets/textures/skybox/Night/top.png",   std::string(SOLUTION_DIR) + "Engine/assets/textures/skybox/Night/bottom.png",
        std::string(SOLUTION_DIR) + "Engine/assets/textures/skybox/Night/front.png", std::string(SOLUTION_DIR) + "Engine/assets/textures/skybox/Night/back.png",
    };

    auto skyboxModel = engine.LoadSkyboxModel(cubeVertices, vertexCount, skyboxFaces);

    auto skybox      = make_s<StaticMeshObject>("Skybox", skyboxModel);
    skybox->SetMaterial(engine.GetSkyboxMaterial());
    scene->SetSkybox(skybox);

    // ── Debug Cube ──────────────────────────────────────────

    auto cubeModel = engine.LoadDebugModel(cubeVertices, vertexCount);

    auto debugCube = make_s<StaticMeshObject>("Light Cube", cubeModel);
    debugCube->SetPosition(glm::vec3(-0.3f, 3.190f, -0.180f));
    debugCube->SetScale(glm::vec3(0.05f));
    debugCube->SetMaterial(engine.GetCubeMaterial());
    scene->AddDebugObject(debugCube);

    // ── Red Point Light ─────────────────────────────────────

    auto redLight = make_s<LightObject>("Red Light", LightType::Point);
    redLight->SetPosition(glm::vec3(-0.3f, 3.190f, -0.180f));
    redLight->SetColor(glm::vec3(1.0f, 0.0f, 0.0f));
    redLight->SetIntensity(500.0f);
    redLight->SetCastsShadow(true);
    scene->AddPointLight(redLight);

    // ── Ambient Particles ───────────────────────────────────

    ParticleSpecs ambientSpecs{};
    ambientSpecs.ParticleCount       = 500;
    ambientSpecs.EnableNoise         = true;
    ambientSpecs.TrailLength         = 0;
    ambientSpecs.SphereRadius        = 5.0f;
    ambientSpecs.ImmortalParticle    = true;
    ambientSpecs.ParticleSize        = 0.5f;
    ambientSpecs.EmitterPos          = glm::vec3(0, 2.0f, 0);
    ambientSpecs.ParticleMinLifetime = 5.0f;
    ambientSpecs.ParticleMaxLifetime = 10.0f;
    ambientSpecs.MinVel              = glm::vec3(-0.3f, -0.3f, -0.3f);
    ambientSpecs.MaxVel              = glm::vec3(0.3f, 0.3f, 0.3f);

    scene->SetAmbientParticles(engine.CreateParticleSystem(ambientSpecs, dustTexture));

    return scene;
}

int main()
{
    Engine::Get().Init();
    Engine::Get().SetScene(CreateScene());
    Engine::Get().Run();

    return 0;
}