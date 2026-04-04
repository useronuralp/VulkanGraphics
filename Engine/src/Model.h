#pragma once
#include "core.h"

class Mesh;
class Image;
class VertexBuffer;
class IndexBuffer;
struct aiMaterial;
struct aiMesh;
struct aiNode;
struct aiScene;
enum aiTextureType;

enum LoadingFlags
{
    NONE                  = -1,
    LOAD_VERTEX_POSITIONS = (uint32_t(1) << 0),
    LOAD_NORMALS          = (uint32_t(1) << 1),
    LOAD_UV               = (uint32_t(1) << 2),
    LOAD_TANGENT          = (uint32_t(1) << 3),
    LOAD_BITANGENT        = (uint32_t(1) << 4),
};

inline LoadingFlags operator|(LoadingFlags a, LoadingFlags b)
{
    return static_cast<LoadingFlags>(static_cast<int>(a) | static_cast<int>(b));
}
inline LoadingFlags operator&(LoadingFlags a, LoadingFlags b)
{
    return static_cast<LoadingFlags>(static_cast<int>(a) & static_cast<int>(b));
}

class Model
{
   public:
    Model() = default;
    ~Model();

    Model(const std::string& InPath, LoadingFlags InFlags);
    Model(const float* InVertices, uint32_t InVertexCount, const Ref<Image>& InTexture = nullptr);

    const std::vector<Mesh*>&   GetMeshes() const;
    int                         GetMeshCount() const;
    const Unique<VertexBuffer>& GetVBO() const;
    const Unique<IndexBuffer>&  GetIBO() const;

    void DrawIndexed(const VkCommandBuffer& InCmd, const VkPipelineLayout& InLayout);
    void Draw(const VkCommandBuffer& InCmd, const VkPipelineLayout& InLayout);

   private:
    void       ProcessNode(aiNode* InNode, const aiScene* InScene);
    Mesh*      ProcessMesh(aiMesh* InMesh, const aiScene* InScene);
    Ref<Image> LoadMaterialTextures(aiMaterial* InMat, aiTextureType InType, std::vector<Ref<Image>>& InCache);

    std::vector<Mesh*> _Meshes;
    LoadingFlags       _Flags;

    std::vector<Ref<Image>> _AlbedoCache;
    std::vector<Ref<Image>> _NormalsCache;
    std::vector<Ref<Image>> _RoughnessMetallicCache;

    Unique<VertexBuffer> _VBO = nullptr;
    Unique<IndexBuffer>  _IBO = nullptr;

    size_t      _VertexSize   = 0;
    std::string _FullPath;
    std::string _Directory;

    Ref<Image> _DefaultAlbedo = make_s<Image>(std::vector{ (std::string(SOLUTION_DIR) + "Engine/assets/textures/Magenta_ERROR.png") }, VK_FORMAT_R8G8B8A8_SRGB);
    Ref<Image> _DefaultNormal = make_s<Image>(std::vector{ (std::string(SOLUTION_DIR) + "Engine/assets/textures/NormalMAP_ERROR.png") }, VK_FORMAT_R8G8B8A8_UNORM);
    Ref<Image> _DefaultRoughnessMetallic =
        make_s<Image>(std::vector{ (std::string(SOLUTION_DIR) + "Engine/assets/textures/White_Texture.png") }, VK_FORMAT_R8G8B8A8_SRGB);
};