/*
* Intended to be included in the client main files. This file includes all the inclusions that a typical user would need.
*/

#pragma once
#include "VulkanApplication.h"
#include "Model.h"
#include "Image.h"
#include "Framebuffer.h"
#include "Pipeline.h"
#include "Surface.h"
#include "Swapchain.h"
#include "LogicalDevice.h"
#include "Utils.h"
#include "PhysicalDevice.h"
#include "CommandBuffer.h"
#include "Window.h"
#include "CommandBuffer.h"
#include "Camera.h"
#include "Buffer.h"
#include "Mesh.h"
#include "ParticleSystem.h"
#include "Bloom.h"
#include "Instance.h"

// External
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <simplexnoise.h>
#include <random>
#include <Curl.h>
#include <filesystem>

#include <glm/gtx/matrix_decompose.hpp>