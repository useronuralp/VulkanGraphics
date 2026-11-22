#pragma once

// Which renderer the engine boots with. Chosen once at startup (Engine::Init) because it decides which
// device extensions/features get enabled, and those can't change after the logical device is created.
enum class RendererType
{
    Rasterizer,
    PathTracer,
    Undefined
};
