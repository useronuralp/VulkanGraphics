# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A toy Vulkan forward renderer (Windows-only, MSVC). `Engine` builds as a static lib (`Engine/lib/Engine.lib`); `Application` is a console exe that links it and lives entirely in [main.cpp](Application/src/main.cpp) — that file is the "scene file" and the only consumer of the public API.

There are no tests, no linter target, and no CI. Verification means building and running the app.

## Build & run

Visual Studio solution only — no CMake/premake. Open `VulkanGraphics.sln`, or:

```bash
msbuild VulkanGraphics.sln /p:Configuration=Debug /p:Platform=x64 /m
```

Note the toolset split in both `.vcxproj` files: **Win32 configs use v143, x64 configs use v145**. Build x64 — it's the only configuration whose preprocessor definitions include `SOLUTION_DIR`, which every asset path in `main.cpp` depends on. Win32 builds will compile the engine but produce an app that can't find assets.

The exe lands in `Application/bin/$(Configuration)/`. Asset paths are absolute at compile time via `SOLUTION_DIR`, so the exe runs from anywhere on the machine that built it but is not relocatable.

### Shaders

GLSL under `Engine/assets/shaders/` is **not** compiled by the build. `.spv` files are committed and loaded at runtime, so after editing any shader you must run:

```bash
cd Engine/assets/shaders && ./compileShaders.bat
```

`compileShaders.bat` is a hardcoded list of `glslc` invocations pinned to `Engine/vendor/VULKAN/1.4.328.1/Bin/`. **A new shader will not be compiled until you add a line to it.** The output naming convention is `<name><STAGE>.spv` (e.g. `PBRShader.vert` → `PBRShaderVERT.spv`) and the renderer's pipeline setup hardcodes those names.

### Vulkan SDK

Vendored in-tree at `Engine/vendor/VULKAN/1.4.328.1/`. The version number appears in include paths in both `.vcxproj` files and in every line of `compileShaders.bat` — bumping the SDK means editing all three places.

## Architecture

### Public API boundary

There's a deliberate pimpl wall so `Application` never sees Vulkan headers:

- `Engine/include/Engine/` — the public surface (`Engine.h`, `Scene.h`, `SceneObject.h`, `StaticMeshObject.h`, `LightObject.h`, `Model.h`, `ParticleSystem.h`, `core.h`). Uses forward declarations only.
- `Engine/src/` — everything Vulkan. `Application.vcxproj` does not add it to its include path.

`Engine` (singleton, `Engine::Get()`) is a thin forwarder holding a `Unique<EngineInternal>`; `EngineInternal` owns the `VulkanContext`, `Swapchain`, `Camera`, active `Scene`, and the `VulkanForwardRenderer`. Most `Engine::` methods delegate straight through to the renderer. **Adding a public API method means touching all three layers** (`Engine.h` → `Engine.cpp` → `EngineInternal` → `VulkanForwardRenderer`).

`core.h` is the shared prelude: `Ref<T>`/`Unique<T>` aliases (`shared_ptr`/`unique_ptr`), `make_s`/`make_u`, the `ENSURE`/`VERIFY_EXPR`/`ENSURE_CHECK` assert macros (all break into the debugger), `PrintInfo`/`PrintWarning`/`PrintError`/`PrintRenderGraph` logging, and `MAX_FRAMES_IN_FLIGHT`.

### Renderer

`RendererInterface` (pure virtual) → `VulkanRenderer` (swapchain, ImGui, frame begin/end, resize) → `VulkanForwardRenderer` (all passes, pipelines, materials, resource loading). The split exists so a deferred path could be added later; only the forward one exists.

`VulkanForwardRenderer::RenderFrame` is ~500 lines and is where the frame is defined. It registers passes into a `RenderGraph` each frame in this order:

`DirectionalShadow` → `PointShadow` → `Cloud Pass` → `HDR Scene` → `Bloom` → `Bokeh / DOF` → `Swapchain / Present`

**The render graph is aspirational, not load-bearing.** `RenderGraph::Compile()` does build a dependency map and topologically sort, but it runs once (guarded by a `printedDependencies` flag) and `Execute()` simply replays passes in insertion order with no barriers inserted. Pass registration order *is* the execution order. Treat `Reads`/`Writes` declarations as documentation until barrier handling lands.

Shadow passes are skipped entirely when `enablePointLightShadows.x == 0`, which also skips the directional pass — an existing quirk, not intentional coupling.

### Uniforms and descriptors

One `GlobalParametersUBO` (declared inline in [Renderer.h](Engine/src/Renderer/Renderer.h)) holds camera matrices, all light data, shadow matrices, and DOF params for every shader. It is std140-sensitive: members are vec4/mat4-aligned with an explicit `float padding[12]` in the middle. **Changing this struct requires matching every GLSL declaration of it** — several shaders declare it independently.

Per-material descriptor binding lives in `MeshBinding::` ([MeshBindingHelper.h](Engine/src/MeshBindingHelper.h)) — `BindPBR`, `BindSkybox`, `BindSimple` and their `BindModel*` wrappers hardcode the descriptor layouts for the engine's four fixed materials (PBR, emissive, skybox, cube). New material types need a new binding function here, not a generic path.

Pipelines are built with the fluent `PipelineBuilder` in [Pipeline.h](Engine/src/Pipeline.h).

### Scene

`Scene` is a flat set of typed buckets, not a graph: mesh objects, emissive objects, debug objects, point lights, skybox, directional light, ambient particles, plus `TorchFireGroup` (mesh + light + spark system + flame system bundled so ImGui and the flicker animation can move them together). Objects derive from `SceneObject` (transform + name) → `StaticMeshObject` / `LightObject`.

`MAX_POINT_LIGHT_COUNT` is 10 and is baked into the UBO array sizes and the shadow map arrays.

## Reference docs

Two long guides at the repo root, both written against this codebase specifically and worth reading before touching shaders or transforms:

- [SHADER_BINDINGS_AND_UNIFORMS_GUIDE.md](SHADER_BINDINGS_AND_UNIFORMS_GUIDE.md) — vertex attribute setup, descriptor set creation flow, push constants, std140 layout, annotated walkthrough of `PBRShader.vert`.
- [SPACE_TRANSFORMATIONS_GUIDE.md](SPACE_TRANSFORMATIONS_GUIDE.md) — model/view/projection conventions, Vulkan clip-space specifics, point light cubemap face matrices.

## Style

`.clang-format` is authoritative: Allman braces, 4-space indent, **170-column limit**, aligned consecutive assignments and declarations, left-aligned pointers.

Naming conventions the codebase follows consistently: `_MemberVariable` for privates, `InParameterName` for function parameters, `PascalCase` methods. Note that `VulkanForwardRenderer`'s public data members break this and use bare camelCase — it's the legacy section of the codebase.
