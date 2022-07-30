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
		glm::vec2 texCoord; // 1
		glm::vec3 normal; // 2  WARNING !! There is a possibility that Vulkan could read this data incorrectly because of the alignment rules. Check?
		glm::vec3 tangent;
		glm::vec3 bitangent;

		static VkVertexInputBindingDescription getBindingDescription();
		static std::array<VkVertexInputAttributeDescription, 5> getAttributeDescriptions();

		bool operator==(const Vertex& other) const
		{
			return pos == other.pos && normal == other.normal && texCoord == other.texCoord && tangent == other.tangent && bitangent == other.bitangent;
		}
	};
}

template<>
struct std::hash<Anor::Vertex>
{
	size_t operator()(Anor::Vertex const& vertex) const
	{
		return ((hash<glm::vec3>()(vertex.pos) ^
			(hash<glm::vec2>()(vertex.texCoord) << 1) ^
			(hash<glm::vec3>()(vertex.normal) << 1)));
	}
};