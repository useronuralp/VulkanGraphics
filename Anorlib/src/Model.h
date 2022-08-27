#pragma once
#include "core.h"
#include <vector>
#include "Mesh.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Texture.h"
namespace Anor
{
	enum LoadingFlags
	{
		LOAD_VERTICES  = (uint32_t(1) << 0),
		LOAD_NORMALS   = (uint32_t(1) << 1),
		LOAD_UV		   = (uint32_t(1) << 2),
		LOAD_TANGENT   = (uint32_t(1) << 3),
		LOAD_BITANGENT = (uint32_t(1) << 4),
	};
	

	inline LoadingFlags operator|(LoadingFlags a, LoadingFlags b)
	{
		return static_cast<LoadingFlags>(static_cast<int>(a) | static_cast<int>(b));
	}
	inline LoadingFlags  operator&(LoadingFlags  a, LoadingFlags  b)
	{
		return static_cast<LoadingFlags>(static_cast<int>(a) & static_cast<int>(b));
	}

	class DescriptorSet;
	class VertexBuffer;
	class IndexBuffer;
	class UniformBuffer;
	class ImageBuffer;
	class RenderPass;
	class Swapchain;
	class Pipeline;
	class Framebuffer;
	class CommandBuffer;
	enum class DescriptorPrimitive;
	class Model
	{
	public:
		std::vector<Ref<Mesh>>& GetMeshes() { return m_Meshes; }
		Model() = default;
		Model(const std::string& path, LoadingFlags flags);
		~Model() {};
		void Draw(const VkCommandBuffer& cmdBuffer);
		void DrawIndexed(const VkCommandBuffer& cmdBuffer);
		void OnResize();
		void UpdateUniformBuffer(uint32_t bufferIndex, void* dataToCopy, size_t dataSize);
		glm::mat4 GetModelMatrix();

		void AddConfiguration(const char* configName, const Pipeline::Specs pipelineCI, std::vector<DescriptorLayout> descriptorLayout);
		void SetActiveConfiguration(const char* configName);
		void SetShadowMap(const Ref<Texture>& shadowMap);

		void Rotate(const float& degree, const float& x, const float& y, const float& z);
		void Translate(const float& x, const float& y, const float& z);
		void Scale(const float& x, const float& y, const float& z);
	private:
		void ProcessNode(aiNode* node, const aiScene* scene);
		Ref<Mesh> ProcessMesh(aiMesh* mesh, const aiScene* scene);
		Ref<Texture> LoadMaterialTextures(aiMaterial* mat, aiTextureType type, std::vector<Ref<Texture>>& cache);
	private:
		std::vector<Ref<Mesh>>				 m_Meshes;
		std::vector<DescriptorLayout>		 m_DescriptorLayout;


		std::vector<Ref<Texture>> m_TextureCache;
		std::vector<Ref<Texture>> m_NormalsCache;
		std::vector<Ref<Texture>> m_RoughnessCache;
		std::vector<Ref<Texture>> m_MetallicCache;
		std::vector<Ref<Texture>> m_AOCache;

		LoadingFlags			  m_Flags;

		std::string m_FullPath;
		std::string m_Directory;

		Ref<Texture> m_DefaultShadowMap = nullptr;
		Ref<Texture> m_DefaultDiffuse = std::make_shared<Texture>((std::string(SOLUTION_DIR) + "Anorlib\\textures\\Magenta_ERROR.png").c_str(), VK_FORMAT_R8G8B8A8_SRGB);
		Ref<Texture> m_DefaultNormal = std::make_shared<Texture>((std::string(SOLUTION_DIR) + "Anorlib\\textures\\NormalMAP_ERROR.png").c_str(), VK_FORMAT_R8G8B8A8_SRGB);
		Ref<Texture> m_DefaultRoughness = std::make_shared<Texture>((std::string(SOLUTION_DIR) + "Anorlib\\textures\\White_Texture.png").c_str(), VK_FORMAT_R8G8B8A8_SRGB);
		Ref<Texture> m_DefaultMetallic = std::make_shared<Texture>((std::string(SOLUTION_DIR) + "Anorlib\\textures\\White_Texture.png").c_str(), VK_FORMAT_R8G8B8A8_SRGB);
		Ref<Texture> m_DefaultAO = std::make_shared<Texture>((std::string(SOLUTION_DIR) + "Anorlib\\textures\\White_Texture.png").c_str(), VK_FORMAT_R8G8B8A8_SRGB);
	};
}