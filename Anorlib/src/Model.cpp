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
    Model::Model(const std::string& path, std::initializer_list<ShaderBindingSpecs> bindingSpecs, const std::string& vertShaderPath, const std::string& fragShaderPath)
        :m_DescriptorLayout(bindingSpecs), m_FullPath(path), m_VertShaderPath(vertShaderPath), m_FragShaderPath(fragShaderPath)
	{
        m_DescriptorSet = std::make_shared<DescriptorSet>(bindingSpecs);

        m_Directory = std::string(m_FullPath).substr(0, std::string(m_FullPath).find_last_of("\\/"));
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(m_FullPath, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals);

        ASSERT(scene && ~scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE && scene->mRootNode, importer.GetErrorString());

        // Process the model in a recursive way.
        ProcessNode(scene->mRootNode, scene);
	}
    void Model::Draw(const VkCommandBuffer& cmdBuffer)
    {
        for (auto& mesh : m_Meshes)
        {
            mesh->Draw(cmdBuffer);
        }
    }
    void Model::DrawIndexed(const VkCommandBuffer& cmdBuffer)
    {
        for (auto& mesh : m_Meshes)
        {
            mesh->DrawIndexed(cmdBuffer);
        }
    }
    void Model::OnResize()
    {
        for (auto& mesh : m_Meshes)
        {
            mesh->OnResize();
        }
    }
    void Model::UpdateUniformBuffer(uint32_t bufferIndex, void* dataToCopy, size_t dataSize)
    {
        for (auto& mesh : m_Meshes)
        {
            mesh->UpdateUniformBuffer(bufferIndex, dataToCopy, dataSize);
        }
    }
    glm::mat4 Model::GetModelMatrix()
    {
        return m_Meshes[0]->GetModelMatrix();
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
    Ref<Mesh> Model::ProcessMesh(aiMesh* mesh, const aiScene* scene)
    {
        std::vector<Vertex>         vertices;
        std::vector<uint32_t>       indices;
        Ref<Texture>                diffuseTexture;
        Ref<Texture>                normalTexture;
        Ref<Texture>                roughnessMetallicTexture;
        //Ref<Texture>                metallicTexture;
        //Ref<Texture>                AOTexture;

        Ref<DescriptorSet> dscSet = std::make_shared<DescriptorSet>(m_DescriptorLayout);

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
            bool dontCalcTangent = false;
            if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
            {
                glm::vec2 vector;
                vector.x = mesh->mTextureCoords[0][i].x;
                vector.y = mesh->mTextureCoords[0][i].y;
                vertex.texCoord= vector;
            }
            else
            {
                vertex.texCoord = glm::vec2(0.0f, 0.0f);
                dontCalcTangent = true;
            }

            //TODO: Handle the case where the tangents cannot be calculated for the loaded model. That case would be models that don't have UV coordinates.
            //Tangnet


            if (dontCalcTangent)
            {
                vector.x = 0.0f;
                vector.y = 0.0f;
                vector.z = 0.0f;
                vertex.tangent = vector;

                vector.x = 0.0f;
                vector.y = 0.0f;
                vector.z = 0.0f;
                vertex.bitangent = vector;
            
            }
            else
            {
                vector.x = mesh->mTangents[i].x;
                vector.y = mesh->mTangents[i].y;
                vector.z = mesh->mTangents[i].z;
                vertex.tangent = vector;
            

                vector.x = mesh->mBitangents[i].x;
                vector.y = mesh->mBitangents[i].y;
                vector.z = mesh->mBitangents[i].z;
                vertex.bitangent = vector;
            }

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
            diffuseTexture = LoadMaterialTextures(material, aiTextureType_DIFFUSE, m_TextureCache); 
            normalTexture  = LoadMaterialTextures(material, aiTextureType_NORMALS, m_NormalsCache);
            roughnessMetallicTexture = LoadMaterialTextures(material, aiTextureType_UNKNOWN, m_RoughnessCache);
        }
        return std::make_shared<Mesh>(vertices, indices, diffuseTexture, normalTexture, roughnessMetallicTexture, dscSet, m_VertShaderPath, m_FragShaderPath);
    }
    void Model::Rotate(const float& degree, const float& x, const float& y, const float& z)
    {
        for (auto& mesh : m_Meshes)
        {
            mesh->Rotate(degree, x, y, z);
        }
    }
    void Model::Translate(const float& x, const float& y, const float& z)
    {
        for (auto& mesh : m_Meshes)
        {
            mesh->Translate(x, y, z);
        }
    }
    void Model::Scale(const float& x, const float& y, const float& z)
    {
        for (auto& mesh : m_Meshes)
        {
            mesh->Scale(x, y, z);
        }
    }
    Ref<Texture> Model::LoadMaterialTextures(aiMaterial* mat, aiTextureType type, std::vector<Ref<Texture>>& cache)
    {
        
        Ref<Texture> textureOUT;
        std::string folderName = m_Directory.substr(m_Directory.find_last_of("\\/") + 1, m_Directory.length());

        aiString str;
        mat->GetTexture(type, 0, &str);
        
        std::string textureName(str.C_Str());


        bool skip = false;
        for (unsigned int j = 0; j < cache.size(); j++)
        {
            if (std::strcmp(cache[j]->GetPath().c_str(), (m_Directory + "\\" + textureName).c_str()) == 0) // TO DO: Check this part.
            {
                textureOUT = cache[j];
                skip = true;
                break;
            }
        }
        if (!skip)
        {  
            if (!textureName.empty())
            {
                VkFormat format;
                if (type == aiTextureType_NORMALS)
                {
                    format = VK_FORMAT_R8G8B8A8_UNORM;
                }
                else
                {
                    format = VK_FORMAT_R8G8B8A8_SRGB;
                }
                Ref<Texture> texture = std::make_unique<Texture>((m_Directory + "\\" + textureName).c_str(), format);
                textureOUT = texture;
                cache.push_back(texture);
            }
        }

        //Handles the case which the loaded model doesnt contain the specific texture image. In that case, we send the default textures instead.
        if (!textureOUT)
        {
            if (type == aiTextureType_DIFFUSE)
                textureOUT = m_DefaultDiffuse;
            else if (type == aiTextureType_NORMALS || aiTextureType_HEIGHT)
                textureOUT = m_DefaultNormal;
            else if (type == aiTextureType_DIFFUSE_ROUGHNESS)
                textureOUT = m_DefaultRoughness;
            else if (type == aiTextureType_METALNESS)
                textureOUT = m_DefaultMetallic;
            else if (type == aiTextureType_AMBIENT_OCCLUSION)
                textureOUT = m_DefaultAO;
        }
        return textureOUT;
    }
}