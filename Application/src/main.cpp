#include "Engine/Engine.h"

int main()
{
    Engine::Get().Init();
    Engine::Get().Run();
    // TODO: Implement Scene interface
    // auto scene = engine.CreateScene();
    //
    // scene->SetCamera(make_s<Camera>(...));
    // scene->AddModel(ResourceSystem::LoadModel("assets/sponza.obj"));
    // scene->AddLight({ { 0, 10, 0 }, { 1, 1, 1 }, 3.0f });

    return 0;
}
