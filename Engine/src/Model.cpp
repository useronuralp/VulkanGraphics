#include "Buffer.h",
#include "CommandBuffer.h"
#include "DescriptorSet.h"
#include "Framebuffer.h"
#include "Image.h"
#include "LogicalDevice.h"
#include "Mesh.h"
#include "Model.h"
#include "PhysicalDevice.h"
#include "Pipeline.h"
#include "Surface.h"
#include "Swapchain.h"
#include "VulkanContext.h"

#include <memory>
#include <string>
#include <unordered_map>
Model::~Model()
{
    for (int i = 0; i < m_Meshes.size(); i++)
    {
        delete m_Meshes[i];
    }
}
Model::Model(
    const std::string&       path,
    LoadingFlags             flags,
    Ref<DescriptorPool>      pool,
    Ref<DescriptorSetLayout> layout,
    Ref<Image>               shadowMap,
    std::vector<Ref<Image>>  pointShadows)
    : m_FullPath(path), m_Flags(flags), m_DefaultShadowMap(shadowMap), m_DefaultPointShadowMaps(pointShadows)
{
    m_Directory = std::string(m_FullPath).substr(0, std::string(m_FullPath).find_last_of("\\/"));
    Assimp::Importer importer;
    const aiScene*   scene = importer.ReadFile(
        m_FullPath, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals);

    ASSERT(scene && ~scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE && scene->mRootNode, importer.GetErrorString());

    // Process the model in a recursive way.
    ProcessNode(scene->mRootNode, scene, pool, layout);

    std::vector<float>    verticesAll;
    std::vector<uint32_t> indicesAll;

    // TO DO : There might be data copying in the vectors check it. Convert to
    // pointers if that is the case.

    for (const auto& mesh : m_Meshes)
    {
        for (const auto& data : mesh->m_Indices)
        {
            indicesAll.push_back(data);
        }
        for (const auto& data : mesh->m_Vertices)
        {
            verticesAll.push_back(data);
        }
    }

    // Create the VB and IB.
    m_VBO = std::make_unique<VertexBuffer>(verticesAll);
    m_IBO = std::make_unique<IndexBuffer>(indicesAll);
}

Model::Model(
    const float*             vertices,
    uint32_t                 vertexCount,
    const Ref<Image>&        cubemapTex,
    Ref<DescriptorPool>      pool,
    Ref<DescriptorSetLayout> layout)
    : m_FullPath("No path. Not loaded from a file"), m_DefaultCubeMap(cubemapTex), m_Flags(NONE)
{
    m_Meshes.emplace_back(new Mesh(vertices, vertexCount, m_DefaultCubeMap, pool, layout));
    m_VertexSize = sizeof(float);
    m_VBO        = std::make_unique<VertexBuffer>(m_Meshes[0]->m_Vertices);
}

void Model::ProcessNode(
    aiNode*                         node,
    const aiScene*                  scene,
    const Ref<DescriptorPool>&      pool,
    const Ref<DescriptorSetLayout>& layout)
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
Mesh* Model::ProcessMesh(
    aiMesh*                         mesh,
    const aiScene*                  scene,
    const Ref<DescriptorPool>&      pool,
    const Ref<DescriptorSetLayout>& layout)
{
    std::vector<float>    vertices;
    std::vector<uint32_t> indices;
    Ref<Image>            diffuseTexture;
    Ref<Image>            normalTexture;
    Ref<Image>            roughnessMetallicTexture;

    for (unsigned int i = 0; i < mesh->mNumVertices; i++)
    {
        if (m_Flags & LOAD_VERTEX_POSITIONS)
        {
            // Vertex Positions
            vertices.push_back(mesh->mVertices[i].x);
            vertices.push_back(mesh->mVertices[i].y);
            vertices.push_back(mesh->mVertices[i].z);
            m_VertexSize += sizeof(float) * 3;
        }

        bool dontCalcTangent = false;
        if (m_Flags & LOAD_UV)
        {
            // UV
            if (mesh->mTextureCoords[0]) // does the mesh contain texture
                                         // coordinates?
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
            m_VertexSize += sizeof(float) * 2;
        }

        if (m_Flags & LOAD_NORMALS)
        {
            // Normals
            vertices.push_back(mesh->mNormals[i].x);
            vertices.push_back(mesh->mNormals[i].y);
            vertices.push_back(mesh->mNormals[i].z);
            m_VertexSize += sizeof(float) * 3;
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
            m_VertexSize += sizeof(float) * 3;
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
            m_VertexSize += sizeof(float) * 3;
        }
    }

    // Process indices
    for (unsigned int i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
            indices.push_back((uint32_t)face.mIndices[j]);
    }
    // Process materials (In our application only textures are loaded.)
    if (mesh->mMaterialIndex >= 0)
    {
        aiMaterial* material     = scene->mMaterials[mesh->mMaterialIndex];
        diffuseTexture           = LoadMaterialTextures(material, aiTextureType_DIFFUSE, m_AlbedoCache); // Load Albedo.
        normalTexture            = LoadMaterialTextures(material, aiTextureType_NORMALS,
                                             m_NormalsCache); // Load Normal map.
        roughnessMetallicTexture = LoadMaterialTextures(
            material,
            aiTextureType_UNKNOWN,
            m_RoughnessMetallicCache); // Load RoughnessMetallic (.gltf) texture
    }
    return new Mesh(
        vertices,
        indices,
        diffuseTexture,
        normalTexture,
        roughnessMetallicTexture,
        pool,
        layout,
        m_DefaultShadowMap,
        m_DefaultPointShadowMaps);
}

Ref<Image> Model::LoadMaterialTextures(aiMaterial* mat, aiTextureType type, std::vector<Ref<Image>>& cache)
{
    Ref<Image>  textureOUT;
    std::string folderName = m_Directory.substr(m_Directory.find_last_of("\\/") + 1, m_Directory.length());

    aiString str;
    mat->GetTexture(type, 0, &str);

    std::string textureName(str.C_Str());

    bool skip = false;
    for (unsigned int j = 0; j < cache.size(); j++)
    {
        if (std::strcmp(cache[j]->GetPath().c_str(), (m_Directory + "\\" + textureName).c_str()) == 0)
        {
            textureOUT = cache[j];
            skip       = true;
            break;
        }
    }
    if (!skip)
    {
        if (!textureName.empty())
        {
            Ref<Image> texture = make_s<Image>(
                std::vector{ (m_Directory + "\\" + textureName) },
                type == aiTextureType_NORMALS ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB);
            textureOUT = texture;
            cache.push_back(texture);
        }
    }

    // Handles the case which the loaded model doesnt contain the specific
    // texture image. In that case, we send the default textures instead.
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

void Model::DrawIndexed(const VkCommandBuffer& commandBuffer, const VkPipelineLayout& pipelineLayout)
{
    VkDeviceSize vertexOffset = 0;
    VkDeviceSize indexOffset  = 0;

    for (int i = 0; i < m_Meshes.size(); i++)
    {
        vkCmdBindDescriptorSets(
            commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &m_Meshes[i]->GetDescriptorSet(), 0, nullptr);
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_VBO->GetVKBuffer(), &vertexOffset);
        vkCmdBindIndexBuffer(commandBuffer, m_IBO->GetVKBuffer(), indexOffset, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, GetMeshes()[i]->GetIndexCount(), 1, 0, 0, 0);

        vertexOffset += m_Meshes[i]->GetVertexCount() * sizeof(float);
        indexOffset += m_Meshes[i]->GetIndexCount() * sizeof(uint32_t);
    }
}

void Model::Draw(const VkCommandBuffer& commandBuffer, const VkPipelineLayout& pipelineLayout)
{
    // Currently used only to draw skyboxes/cubes. Extend if you need it.
    VkDeviceSize vertexOffset = 0;
    vkCmdBindDescriptorSets(
        commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &m_Meshes[0]->GetDescriptorSet(), 0, nullptr);
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_VBO->GetVKBuffer(), &vertexOffset);
    vkCmdDraw(commandBuffer, 36, 1, 0, 0);
}

void Model::Rotate(const float degree, const float& x, const float& y, const float& z)
{
    m_Transform = glm::rotate(m_Transform, glm::radians(degree), glm::vec3(x, y, z));
}

void Model::Translate(const float& x, const float& y, const float& z)
{
    m_Transform = glm::translate(m_Transform, glm::vec3(x, y, z));
}

void Model::Scale(const float& x, const float& y, const float& z)
{
    m_Transform = glm::scale(m_Transform, glm::vec3(x, y, z));
}
