#include "Model.h"
#include "VulkanApplication.h"
#include <string>
#include "Buffer.h",
#include <memory>
#include "LogicalDevice.h"
#include "PhysicalDevice.h"
#include "DescriptorSet.h"
#include "Containers.h"
#include "CommandBuffer.h"
#include <unordered_map>
#include "Framebuffer.h"
#include "RenderPass.h"
#include "Surface.h"
#include "Pipeline.h"
#include "Swapchain.h"
namespace Anor
{
    Model::Model(std::string path, std::initializer_list<ShaderBinding> bindings) 
        :m_DescriptorLayout(bindings), m_FullPath(path)
	{
        m_Directory = std::string(m_FullPath).substr(0, std::string(m_FullPath).find_last_of("\\/"));
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(m_FullPath, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

        ASSERT(scene && ~scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE && scene->mRootNode, importer.GetErrorString());

        // Process the model in a recursive way.
        ProcessNode(scene->mRootNode, scene);

        // Free all the texture pixels after creating the ImageBuffers.
        for (auto& texture : m_TextureCache)
        {
            texture->FreePixels();
        }

	}
    void Model::Draw(const VkCommandBuffer& cmdBuffer)
    {
        for (auto& mesh : m_Meshes)
        {
            mesh.Draw(cmdBuffer);
        }
    }
    void Model::OnResize()
    {
        for (auto& mesh : m_Meshes)
        {
            mesh.OnResize();
        }
    }
    void Model::UpdateViewProjection(const glm::mat4& viewMat, const glm::mat4& projMat)
    {
        for (auto& mesh : m_Meshes)
        {
            mesh.UpdateViewProjection(viewMat, projMat);
        }
    }
    void Model::ProcessNode(aiNode* node, const aiScene* scene)
    {
        // process all the node's meshes (if any)
        for (unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            m_Meshes.push_back(ProcessMesh(mesh, scene));
        }
        // then do the same for each of its children
        for (unsigned int i = 0; i < node->mNumChildren; i++)
        {
            ProcessNode(node->mChildren[i], scene);
        }
    }
    Mesh Model::ProcessMesh(aiMesh* mesh, const aiScene* scene)
    {
        std::vector<Vertex>         vertices;
        std::vector<uint32_t>       indices;
        std::vector<Ref<Texture>>   diffuseTextures;

        Ref<DescriptorSet> meshDescriptorSet = std::make_shared<DescriptorSet>(m_DescriptorLayout);

        for (unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            Vertex vertex;
            //Vertex Positions
            glm::vec3 vector;
            vector.x = mesh->mVertices[i].x;
            vector.y = mesh->mVertices[i].y;
            vector.z = mesh->mVertices[i].z;
            vertex.pos = vector;

            //Normals
            vector.x = mesh->mNormals[i].x;
            vector.y = mesh->mNormals[i].y;
            vector.z = mesh->mNormals[i].z;
            vertex.normal = vector;

            //UV
            if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
            {
                glm::vec2 vector;
                vector.x = mesh->mTextureCoords[0][i].x;
                vector.y = mesh->mTextureCoords[0][i].y;
                vertex.texCoord = vector;
            }
            else
                vertex.texCoord = glm::vec2(0.0f, 0.0f);

            // Color.
            vector.x = 0.9f;
            vector.y = 0.0f;
            vector.z = 0.8f;
            vertex.color = vector;

            ////TODO: Handle the case where the tangents cannot be calculated for the loaded model. That case would be models that don't have UV coordinates.
            ////Tangnet
            //vector.x = mesh->mTangents[i].x;
            //vector.y = mesh->mTangents[i].y;
            //vector.z = mesh->mTangents[i].z;
            //vertex.Tangent = vector;
            //
            ////Biangnet
            //vector.x = mesh->mBitangents[i].x;
            //vector.y = mesh->mBitangents[i].y;
            //vector.z = mesh->mBitangents[i].z;
            //vertex.Bitangent = vector;

            vertices.push_back(vertex);
        }
        // process indices
        for (unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back((uint32_t)face.mIndices[j]);
        }
        // process material
        if (mesh->mMaterialIndex >= 0)
        {
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
            std::vector<Ref<Texture>> diffuseMaps = LoadMaterialTextures(material, aiTextureType_DIFFUSE/*, "texture_diffuse"*/, meshDescriptorSet);
            diffuseTextures.insert(diffuseTextures.end(), diffuseMaps.begin(), diffuseMaps.end());
            //std::vector<Ref<ImageBuffer>> specularMaps = LoadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
            //textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
        
            //std::vector<Ref<Texture>> normalMaps;
            //normalMaps = LoadMaterialTextures(material, type, "texture_normalMap");
            //textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
        }
        return Mesh(vertices, indices, diffuseTextures, meshDescriptorSet);
    }
    void Model::Rotate(const float& degree, const float& x, const float& y, const float& z)
    {
        for (auto& mesh : m_Meshes)
        {
            mesh.Rotate(degree, x, y, z);
        }
    }
    void Model::Translate(const float& x, const float& y, const float& z)
    {
        for (auto& mesh : m_Meshes)
        {
            mesh.Translate(x, y, z);
        }
    }
    void Model::Scale(const float& x, const float& y, const float& z)
    {
        for (auto& mesh : m_Meshes)
        {
            mesh.Scale(x, y, z);
        }
    }
    std::vector<Ref<Texture>> Model::LoadMaterialTextures(aiMaterial* mat, aiTextureType type/*, const std::string& typeName*/, const Ref<DescriptorSet>& meshDescriptorSet)
    {
        std::vector<Ref<Texture>> diffuseTextures;
        std::string folderName = m_Directory.substr(m_Directory.find_last_of("\\/") + 1, m_Directory.length());
        for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
        {
            aiString str;
            mat->GetTexture(type, i, &str);
            std::string textureName(str.C_Str());


            bool skip = false;
            for (unsigned int j = 0; j < m_TextureCache.size(); j++)
            {
                if (std::strcmp(m_TextureCache[j]->GetPath().c_str(), (m_Directory + "\\" + textureName).c_str()) == 0) // TO DO: Check this part.
                {
                    diffuseTextures.push_back(m_TextureCache[j]);
                    skip = true;
                    break;
                }
            }
            if (!skip)
            {  
                Ref<Texture> texture = std::make_shared<Texture>((m_Directory + "\\" + textureName).c_str());
                diffuseTextures.push_back(texture);
                m_TextureCache.push_back(texture);
            }
        }
        //Handles the case which the loaded model doesnt contain the specific texture image. In that case, we send the default textures instead.
        //if (textures.empty())
        //{
        //    if (typeName == "texture_diffuse")
        //        textures.push_back(m_DefaultDiffuse);
        //    else if (typeName == "texture_specular")
        //        textures.push_back(m_DefaultSpecular);
        //    else if (typeName == "texture_normalMap")
        //        textures.push_back(m_DefaultNormal);
        //}
        return diffuseTextures;
    }
}