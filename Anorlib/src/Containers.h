#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include "vulkan/vulkan.h"
namespace Anor
{

	struct Vertex
	{
		glm::vec3 pos; // 0
		glm::vec3 color; // 1
		glm::vec2 texCoord; // 2
		glm::vec3 normal; // 3  WARNING !! There is a possibility that Vulkan could read this data incorrectly because of the alignment rules. Check?

		static VkVertexInputBindingDescription getBindingDescription();
		static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions();

		bool operator==(const Vertex& other) const
		{
			return pos == other.pos && color == other.color && texCoord == other.texCoord;
		}
	};



	struct UniformBufferObject_MVP
	{
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
	};
}

template<>
struct std::hash<Anor::Vertex>
{
	size_t operator()(Anor::Vertex const& vertex) const
	{
		return ((hash<glm::vec3>()(vertex.pos) ^
			(hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
			(hash<glm::vec2>()(vertex.texCoord) << 1) ^
			(hash<glm::vec3>()(vertex.normal) << 1);
	}
};