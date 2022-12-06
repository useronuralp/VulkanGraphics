#pragma once
#include "core.h"
// External
#define GLM_ENABLE_EXPERIMENTAL
#include <vector>
#include <vulkan/vulkan.h>
#include <glm/gtx/quaternion.hpp>
#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace OVK
{
	enum LoadingFlags
	{
		NONE				   = -1,
		LOAD_VERTEX_POSITIONS  = (uint32_t(1) << 0),
		LOAD_NORMALS		   = (uint32_t(1) << 1),
		LOAD_UV				   = (uint32_t(1) << 2),
		LOAD_TANGENT		   = (uint32_t(1) << 3),
		LOAD_BITANGENT		   = (uint32_t(1) << 4),
	};
	

	inline LoadingFlags operator|(LoadingFlags a, LoadingFlags b)
	{
		return static_cast<LoadingFlags>(static_cast<int>(a) | static_cast<int>(b));
	}
	inline LoadingFlags  operator&(LoadingFlags  a, LoadingFlags  b)
	{
		return static_cast<LoadingFlags>(static_cast<int>(a) & static_cast<int>(b));
	}

	class CubemapTexture;
	class Mesh;
	class Image;
	class DescriptorSet;
	class VertexBuffer;
	class IndexBuffer;
	class UniformBuffer;
	class ImageBuffer;
	class RenderPass;
	class Swapchain;
	class DescriptorPool;
	class DescriptorLayout;
	class Pipeline;
	class Framebuffer;
	class CommandBuffer;
	enum class DescriptorPrimitive;
	class Model
	{
	public:
		Model()  = default;
		~Model();
		// This constructor is used to construct a model that contains at least one mesh.
		Model(const std::string& path, LoadingFlags flags, Ref<DescriptorPool> pool, Ref<DescriptorLayout> layout, Ref<Image> shadowMap = nullptr, std::vector<Ref<Image>> pointShadows = std::vector<Ref<Image>>());
		// This constructor is used to construct a single meshed model (skybox).
		Model(const float* vertices, uint32_t vertexCount, const Ref<Image>& cubemapTex, Ref<DescriptorPool> pool, Ref<DescriptorLayout> layout);
		const std::vector<Mesh*>&		GetMeshes() { return m_Meshes; }

		glm::mat4& GetTransform()  { return m_Transform; }

		void Rotate(const float degree, const float& x, const float& y, const float& z);
		void Translate(const float& x, const float& y, const float& z);
		void Scale(const float& x, const float& y, const float& z);


		const Unique<VertexBuffer>&		GetVBO() { return m_VBO; }
		const Unique<IndexBuffer>&		GetIBO() { return m_IBO; }
		void							DrawIndexed(const VkCommandBuffer& commandBuffer, const VkPipelineLayout& pipelineLayout);
		void							Draw(const VkCommandBuffer& commandBuffer, const VkPipelineLayout& pipelineLayout);

	private:
		void		ProcessNode(aiNode* node, const aiScene* scene, const Ref<DescriptorPool>& pool, const Ref<DescriptorLayout>& layout);
		Mesh*		ProcessMesh(aiMesh* mesh, const aiScene* scene, const Ref<DescriptorPool>& pool, const Ref<DescriptorLayout>& layout);
		Ref<Image>	LoadMaterialTextures(aiMaterial* mat, aiTextureType type, std::vector<Ref<Image>>& cache);
	private:

		mutable glm::mat4		  m_Transform = glm::mat4(1.0f);
		std::vector<Mesh*>		  m_Meshes;
		LoadingFlags			  m_Flags;

		std::vector<Ref<Image>> m_AlbedoCache;
		std::vector<Ref<Image>> m_NormalsCache;
		std::vector<Ref<Image>> m_RoughnessMetallicCache;

		// Vertex & Index Buffers
		Unique<VertexBuffer>			m_VBO = nullptr;
		Unique<IndexBuffer>			    m_IBO = nullptr;

		size_t							m_VertexSize = 0;

		std::string  m_FullPath;
		std::string  m_Directory;

		std::vector<Ref<Image>> m_DefaultPointShadowMaps;
		Ref<Image>		m_DefaultShadowMap = nullptr;
		Ref<Image>		m_DefaultCubeMap = nullptr;
		Ref<Image>		m_DefaultAlbedo =				std::make_shared<Image>(std::vector{ (std::string(SOLUTION_DIR) + "OVKLib\\textures\\Magenta_ERROR.png") }, VK_FORMAT_R8G8B8A8_SRGB);
		Ref<Image>		m_DefaultNormal =				std::make_shared<Image>(std::vector{ (std::string(SOLUTION_DIR) + "OVKLib\\textures\\NormalMAP_ERROR.png") }, VK_FORMAT_R8G8B8A8_UNORM);
		Ref<Image>		m_DefaultRoughnessMetallic =	std::make_shared<Image>(std::vector{ (std::string(SOLUTION_DIR) + "OVKLib\\textures\\White_Texture.png") }, VK_FORMAT_R8G8B8A8_SRGB);
	};
}