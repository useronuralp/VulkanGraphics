#include "Model.h"
#include "VulkanApplication.h"
#include <string>
#include "Buffer.h",
#include <memory>
#include "LogicalDevice.h"
#include "PhysicalDevice.h"
#include "DescriptorSet.h"
#include "CommandBuffer.h"
#include <unordered_map>
#include "Framebuffer.h"
#include "Surface.h"
#include "Pipeline.h"
#include "Swapchain.h"
#include "Mesh.h"
#include "Texture.h"
namespace OVK
{
    Model::~Model()
    {
        for (int i = 0; i < m_Meshes.size(); i++)
        {
            delete m_Meshes[i];
        }
    }
    Model::Model(const std::string& path, LoadingFlags flags, Ref<DescriptorPool> pool, Ref<DescriptorLayout> layout, Ref<Texture> shadowMap)
        :m_FullPath(path), m_Flags(flags), m_DefaultShadowMap(shadowMap)
	{
        m_Directory = std::string(m_FullPath).substr(0, std::string(m_FullPath).find_last_of("\\/"));
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(m_FullPath, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals);

        ASSERT(scene && ~scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE && scene->mRootNode, importer.GetErrorString());

        // Process the model in a recursive way.
        ProcessNode(scene->mRootNode, scene, pool, layout);
	}

    Model::Model(const float* vertices, size_t vertexBufferSize, uint32_t vertexCount, const Ref<CubemapTexture>& cubemapTex, Ref<DescriptorPool> pool, Ref<DescriptorLayout> layout)
        : m_FullPath("No path. Not loaded from a file"), m_DefaultCubeMap(cubemapTex), m_Flags(NONE)
    {
        m_Meshes.emplace_back(new Mesh(vertices, vertexBufferSize, vertexCount, cubemapTex, pool, layout));
    }

    void Model::ProcessNode(aiNode* node, const aiScene* scene, const Ref<DescriptorPool>& pool, const Ref<DescriptorLayout>& layout)
    {
        // process all the node's meshes (if any)
        for (unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            m_Meshes.emplace_back(ProcessMesh(mesh, scene, pool, layout));
        }
        // then do the same for each of its children
        for (unsigned int i = 0; i < node->mNumChildren; i++)
        {
            ProcessNode(node->mChildren[i], scene, pool, layout);
        }
    }
    Mesh* Model::ProcessMesh(aiMesh* mesh, const aiScene* scene, const Ref<DescriptorPool>& pool, const Ref<DescriptorLayout>& layout)
    {
        std::vector<float>     vertices;
        std::vector<uint32_t>  indices;
        Ref<Texture>           diffuseTexture;
        Ref<Texture>           normalTexture;
        Ref<Texture>           roughnessMetallicTexture;

        for (unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            if (m_Flags & LOAD_VERTICES)
            {
                //Vertex Positions
                vertices.push_back(mesh->mVertices[i].x);
                vertices.push_back(mesh->mVertices[i].y);
                vertices.push_back(mesh->mVertices[i].z);
            }

            bool dontCalcTangent = false;
            if (m_Flags & LOAD_UV)
            {
                //UV
                if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
                {
                    vertices.push_back(mesh->mTextureCoords[0][i].x);
                    vertices.push_back(mesh->mTextureCoords[0][i].y);
                }
                else
                {
                    vertices.push_back(0);
                    vertices.push_back(0);
                    dontCalcTangent = true;
                }
            }

            if (m_Flags & LOAD_NORMALS)
            {
                //Normals
                vertices.push_back(mesh->mNormals[i].x);
                vertices.push_back(mesh->mNormals[i].y);
                vertices.push_back(mesh->mNormals[i].z);
            }


            if (m_Flags & LOAD_TANGENT)
            {
                if (dontCalcTangent)
                {
                    // Tangnet
                    vertices.push_back(0);
                    vertices.push_back(0);
                    vertices.push_back(0);
                }
                else
                {
                    // Tangent
                    vertices.push_back(mesh->mTangents[i].x);
                    vertices.push_back(mesh->mTangents[i].y);
                    vertices.push_back(mesh->mTangents[i].z);
                }
            }
            if (m_Flags & LOAD_BITANGENT)
            {
                if (dontCalcTangent)
                {
                    // Bitangent
                    vertices.push_back(0);
                    vertices.push_back(0);
                    vertices.push_back(0);
                }
                else
                {
                    // Bitangent
                    vertices.push_back(mesh->mBitangents[i].x);
                    vertices.push_back(mesh->mBitangents[i].y);
                    vertices.push_back(mesh->mBitangents[i].z);
                }
            }
        }

        // Process indices
        for (unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back((uint32_t)face.mIndices[j]);
        }
        //Mesh mesh2 = Mesh();
        //mesh2.
        // Process materials (In our application only textures are loaded.)
        if (mesh->mMaterialIndex >= 0)
        {
            aiMaterial* material     = scene->mMaterials[mesh->mMaterialIndex];
            diffuseTexture           = LoadMaterialTextures(material, aiTextureType_DIFFUSE, m_AlbedoCache);   // Load Albedo.
            normalTexture            = LoadMaterialTextures(material, aiTextureType_NORMALS, m_NormalsCache);   // Load Normal map.
            roughnessMetallicTexture = LoadMaterialTextures(material, aiTextureType_UNKNOWN, m_RoughnessMetallicCache); // Load RoughnessMetallic (.gltf) texture
        }
        return new Mesh(vertices, indices, diffuseTexture, normalTexture, roughnessMetallicTexture, pool, layout, m_DefaultShadowMap);
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
                Ref<Texture> texture = std::make_shared<Texture>((m_Directory + "\\" + textureName).c_str(), type == aiTextureType_NORMALS ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB);
                textureOUT = texture;
                cache.push_back(texture);
            }
        }

        //Handles the case which the loaded model doesnt contain the specific texture image. In that case, we send the default textures instead.
        if (!textureOUT)
        {
            if (type == aiTextureType_DIFFUSE)
                textureOUT = m_DefaultAlbedo;
            else if (type == aiTextureType_NORMALS || aiTextureType_HEIGHT)
                textureOUT = m_DefaultNormal;
            else if (type == aiTextureType_DIFFUSE_ROUGHNESS)
                textureOUT = m_DefaultRoughnessMetallic;
        }
        return textureOUT;
    }

    void Model::Rotate(const float& degree, const float& x, const float& y, const float& z)
    {
        m_ModelMatrix = glm::rotate(m_ModelMatrix, glm::radians(degree), glm::vec3(x, y, z));
    }

    void Model::Translate(const float& x, const float& y, const float& z)
    {
        m_ModelMatrix = glm::translate(m_ModelMatrix, glm::vec3(x, y, z));
    }

    void Model::Scale(const float& x, const float& y, const float& z)
    {
        m_ModelMatrix = glm::scale(m_ModelMatrix, glm::vec3(x, y, z));
    }
}