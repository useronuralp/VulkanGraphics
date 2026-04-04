#define GLM_ENABLE_EXPERIMENTAL
#include "Buffer.h"
#include "Image.h"
#include "Mesh.h"
#include "Model.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

Model::Model(const std::string& InPath, LoadingFlags InFlags) : _FullPath(InPath), _Flags(InFlags)
{
    _Directory = _FullPath.substr(0, _FullPath.find_last_of("\\/"));

    Assimp::Importer importer;
    const aiScene*   scene = importer.ReadFile(_FullPath, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals);

    ENSURE(scene && ~scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE && scene->mRootNode, importer.GetErrorString());

    ProcessNode(scene->mRootNode, scene);

    std::vector<float>    verticesAll;
    std::vector<uint32_t> indicesAll;

    for (const auto* mesh : _Meshes)
    {
        for (const auto& data : mesh->GetIndices())
        {
            indicesAll.push_back(data);
        }

        for (const auto& data : mesh->GetVertices())
        {
            verticesAll.push_back(data);
        }
    }

    _VBO = std::make_unique<VertexBuffer>(verticesAll);
    _IBO = std::make_unique<IndexBuffer>(indicesAll);
}

Model::Model(const float* InVertices, uint32_t InVertexCount, const Ref<Image>& InTexture) : _FullPath("No path. Not loaded from a file"), _Flags(NONE)
{
    _Meshes.emplace_back(new Mesh(InVertices, InVertexCount, InTexture));
    _VertexSize = sizeof(float);
    _VBO        = std::make_unique<VertexBuffer>(_Meshes[0]->GetVertices());
}

Model::~Model()
{
    for (auto* mesh : _Meshes)
    {
        delete mesh;
    }
}

void Model::ProcessNode(aiNode* InNode, const aiScene* InScene)
{
    for (unsigned int i = 0; i < InNode->mNumMeshes; i++)
    {
        aiMesh* mesh = InScene->mMeshes[InNode->mMeshes[i]];
        _Meshes.emplace_back(ProcessMesh(mesh, InScene));
    }

    for (unsigned int i = 0; i < InNode->mNumChildren; i++)
    {
        ProcessNode(InNode->mChildren[i], InScene);
    }
}

Mesh* Model::ProcessMesh(aiMesh* InMesh, const aiScene* InScene)
{
    std::vector<float>    vertices;
    std::vector<uint32_t> indices;
    Ref<Image>            diffuseTexture;
    Ref<Image>            normalTexture;
    Ref<Image>            roughnessMetallicTexture;

    for (unsigned int i = 0; i < InMesh->mNumVertices; i++)
    {
        if (_Flags & LOAD_VERTEX_POSITIONS)
        {
            vertices.push_back(InMesh->mVertices[i].x);
            vertices.push_back(InMesh->mVertices[i].y);
            vertices.push_back(InMesh->mVertices[i].z);
            _VertexSize += sizeof(float) * 3;
        }

        bool dontCalcTangent = false;
        if (_Flags & LOAD_UV)
        {
            if (InMesh->mTextureCoords[0])
            {
                vertices.push_back(InMesh->mTextureCoords[0][i].x);
                vertices.push_back(InMesh->mTextureCoords[0][i].y);
            }
            else
            {
                vertices.push_back(0);
                vertices.push_back(0);
                dontCalcTangent = true;
            }
            _VertexSize += sizeof(float) * 2;
        }

        if (_Flags & LOAD_NORMALS)
        {
            vertices.push_back(InMesh->mNormals[i].x);
            vertices.push_back(InMesh->mNormals[i].y);
            vertices.push_back(InMesh->mNormals[i].z);
            _VertexSize += sizeof(float) * 3;
        }

        if (_Flags & LOAD_TANGENT)
        {
            if (dontCalcTangent)
            {
                vertices.push_back(0);
                vertices.push_back(0);
                vertices.push_back(0);
            }
            else
            {
                vertices.push_back(InMesh->mTangents[i].x);
                vertices.push_back(InMesh->mTangents[i].y);
                vertices.push_back(InMesh->mTangents[i].z);
            }
            _VertexSize += sizeof(float) * 3;
        }

        if (_Flags & LOAD_BITANGENT)
        {
            if (dontCalcTangent)
            {
                vertices.push_back(0);
                vertices.push_back(0);
                vertices.push_back(0);
            }
            else
            {
                vertices.push_back(InMesh->mBitangents[i].x);
                vertices.push_back(InMesh->mBitangents[i].y);
                vertices.push_back(InMesh->mBitangents[i].z);
            }
            _VertexSize += sizeof(float) * 3;
        }
    }

    for (unsigned int i = 0; i < InMesh->mNumFaces; i++)
    {
        aiFace face = InMesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
            indices.push_back((uint32_t)face.mIndices[j]);
    }

    if (InMesh->mMaterialIndex >= 0)
    {
        aiMaterial* material     = InScene->mMaterials[InMesh->mMaterialIndex];
        diffuseTexture           = LoadMaterialTextures(material, aiTextureType_DIFFUSE, _AlbedoCache);
        normalTexture            = LoadMaterialTextures(material, aiTextureType_NORMALS, _NormalsCache);
        roughnessMetallicTexture = LoadMaterialTextures(material, aiTextureType_METALNESS, _RoughnessMetallicCache);
    }

    return new Mesh(vertices, indices, diffuseTexture, normalTexture, roughnessMetallicTexture);
}

Ref<Image> Model::LoadMaterialTextures(aiMaterial* InMat, aiTextureType InType, std::vector<Ref<Image>>& InCache)
{
    Ref<Image> textureOut;

    aiString str;
    InMat->GetTexture(InType, 0, &str);

    std::string textureName(str.C_Str());

    bool skip = false;
    for (unsigned int j = 0; j < InCache.size(); j++)
    {
        if (std::strcmp(InCache[j]->GetPath().c_str(), (_Directory + "\\" + textureName).c_str()) == 0)
        {
            textureOut = InCache[j];
            skip       = true;
            break;
        }
    }

    if (!skip)
    {
        if (!textureName.empty())
        {
            Ref<Image> texture =
                make_s<Image>(std::vector{ (_Directory + "\\" + textureName) }, InType == aiTextureType_NORMALS ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB);
            textureOut = texture;
            InCache.push_back(texture);
        }
    }

    if (!textureOut)
    {
        if (InType == aiTextureType_DIFFUSE)
        {
            textureOut = _DefaultAlbedo;
        }
        else if (InType == aiTextureType_NORMALS || InType == aiTextureType_HEIGHT)
        {
            textureOut = _DefaultNormal;
        }
        else
        {
            textureOut = _DefaultRoughnessMetallic;
        }
    }

    return textureOut;
}

void Model::DrawIndexed(const VkCommandBuffer& InCmd, const VkPipelineLayout& InLayout)
{
    VkDeviceSize vertexOffset = 0;
    VkDeviceSize indexOffset  = 0;

    for (int i = 0; i < _Meshes.size(); i++)
    {
        vkCmdBindDescriptorSets(InCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, InLayout, 0, 1, &_Meshes[i]->GetDescriptorSet(), 0, nullptr);
        vkCmdBindVertexBuffers(InCmd, 0, 1, &_VBO->GetVKBuffer(), &vertexOffset);
        vkCmdBindIndexBuffer(InCmd, _IBO->GetVKBuffer(), indexOffset, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(InCmd, _Meshes[i]->GetIndexCount(), 1, 0, 0, 0);

        vertexOffset += _Meshes[i]->GetVertexCount() * sizeof(float);
        indexOffset += _Meshes[i]->GetIndexCount() * sizeof(uint32_t);
    }
}

void Model::Draw(const VkCommandBuffer& InCmd, const VkPipelineLayout& InLayout)
{
    VkDeviceSize vertexOffset = 0;
    vkCmdBindDescriptorSets(InCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, InLayout, 0, 1, &_Meshes[0]->GetDescriptorSet(), 0, nullptr);
    vkCmdBindVertexBuffers(InCmd, 0, 1, &_VBO->GetVKBuffer(), &vertexOffset);
    vkCmdDraw(InCmd, 36, 1, 0, 0);
}

const std::vector<Mesh*>& Model::GetMeshes() const
{
    return _Meshes;
}

int Model::GetMeshCount() const
{
    return _Meshes.size();
}

const Unique<VertexBuffer>& Model::GetVBO() const
{
    return _VBO;
}

const Unique<IndexBuffer>& Model::GetIBO() const
{
    return _IBO;
}