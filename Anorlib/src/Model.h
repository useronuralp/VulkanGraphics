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
	struct Vertex;
	class Model
	{
	public:
		std::vector<Mesh>& GetMeshes() { return m_Meshes; }
		Model(std::string path, std::initializer_list<ShaderBinding> bindings);
		~Model() {};
		void Draw(const VkCommandBuffer& cmdBuffer);
		void OnResize();
		void UpdateViewProjection(const glm::mat4& viewMat, const glm::mat4& projMat);

		void Rotate(const float& degree, const float& x, const float& y, const float& z);
		void Translate(const float& x, const float& y, const float& z);
		void Scale(const float& x, const float& y, const float& z);
	private:
		void ProcessNode(aiNode* node, const aiScene* scene);
		Mesh ProcessMesh(aiMesh* mesh, const aiScene* scene);
		std::vector<Ref<Texture>> LoadMaterialTextures(aiMaterial* mat, aiTextureType type/*,const std::string& typeName*/, const Ref<DescriptorSet>& meshDescriptorSet);
	private:
		std::vector<Mesh>				 m_Meshes;
		std::vector<ShaderBinding>		 m_DescriptorLayout;
		std::vector<Ref<Texture>>		 m_TextureCache;


		std::string m_FullPath;
		std::string m_Directory;
	};
}