# GLSL Shader Bindings, Locations, Push Constants & UBOs - Deep Dive

This guide explains how shader I/O and resource binding work in your `PBRShader.vert`, including the mysterious location gaps and why data is organized the way it is.

**NEW:** This guide now includes a complete CPU→GPU flow showing how descriptor sets (set 0) are created, configured, and passed to shaders.

---

## Part 1: Vertex Attributes (Input Locations)

### What Are Locations?

`location` specifies **which vertex attribute stream** this variable receives data from. It's a contract between:
- **CPU side** (Vulkan `VkVertexInputAttributeDescription` + `VkVertexInputBindingDescription`)
- **GPU side** (GLSL shader)

### Your Input Locations: PBRShader.vert Lines 3-8

```glsl
layout(location = 0) in vec3 a_Position;    // Location 0
layout(location = 1) in vec2 a_UV;          // Location 1
layout(location = 2) in vec3 a_Normal;      // Location 2
layout(location = 3) in vec3 a_Tangent;     // Location 3
layout(location = 4) in vec3 a_Bitangent;   // Location 4
```

**Sequential locations**: 0, 1, 2, 3, 4 (no gaps needed here)

### What Happens at Runtime

When you render a mesh:

```cpp
// In Vulkan (CPU side), you'd specify:
VkVertexInputAttributeDescription attributes[5] = {
    {0, binding, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)},     // location 0
    {1, binding, VK_FORMAT_R32G32_SFLOAT,    offsetof(Vertex, uv)},           // location 1
    {2, binding, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)},       // location 2
    {3, binding, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, tangent)},      // location 3
    {4, binding, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, bitangent)},    // location 4
};
```

For each vertex the GPU processes:
1. Read value at location 0 → feed into `a_Position`
2. Read value at location 1 → feed into `a_UV`
3. Read value at location 2 → feed into `a_Normal`
4. etc.

The GPU calls your vertex shader once per vertex with these inputs populated.

### Location Size Rules

| Type | Locations Used | Example |
|------|-----------------|---------|
| `float` | 1 | `location = 0` |
| `vec2` | 1 | `location = 0` (holds 2 floats) |
| `vec3` | 1 | `location = 0` (holds 3 floats) |
| `vec4` | 1 | `location = 0` (holds 4 floats) |
| `mat4` | 4 | `location = 0,1,2,3` (4 vec4 rows) |
| `mat3` | 3 | `location = 0,1,2` (3 vec3 rows) |

**This is why your outputs have a gap!**

---

## Part 1B: CPU-Side Vertex Attribute Setup (Complete Flow)

### Where Vertex Data Comes From

**File:** `Engine\src\Model.cpp` Lines 83-151

Vertices are loaded from mesh files (using Assimp) into a flat array:

```cpp
std::vector<float> vertices;

for (unsigned int i = 0; i < InMesh->mNumVertices; i++)
{
    // Each vertex is packed as: [x, y, z] [u, v] [nx, ny, nz] [tx, ty, tz] [bx, by, bz]

    if (_Flags & LOAD_VERTEX_POSITIONS)  // location 0
    {
        vertices.push_back(InMesh->mVertices[i].x);
        vertices.push_back(InMesh->mVertices[i].y);
        vertices.push_back(InMesh->mVertices[i].z);
        _VertexSize += sizeof(float) * 3;  // 12 bytes
    }

    if (_Flags & LOAD_UV)  // location 1
    {
        vertices.push_back(InMesh->mTextureCoords[0][i].x);
        vertices.push_back(InMesh->mTextureCoords[0][i].y);
        _VertexSize += sizeof(float) * 2;  // 8 bytes
    }

    if (_Flags & LOAD_NORMALS)  // location 2
    {
        vertices.push_back(InMesh->mNormals[i].x);
        vertices.push_back(InMesh->mNormals[i].y);
        vertices.push_back(InMesh->mNormals[i].z);
        _VertexSize += sizeof(float) * 3;  // 12 bytes
    }

    if (_Flags & LOAD_TANGENT)  // location 3
    {
        vertices.push_back(InMesh->mTangents[i].x);
        vertices.push_back(InMesh->mTangents[i].y);
        vertices.push_back(InMesh->mTangents[i].z);
        _VertexSize += sizeof(float) * 3;  // 12 bytes
    }

    if (_Flags & LOAD_BITANGENT)  // location 4
    {
        vertices.push_back(InMesh->mBitangents[i].x);
        vertices.push_back(InMesh->mBitangents[i].y);
        vertices.push_back(InMesh->mBitangents[i].z);
        _VertexSize += sizeof(float) * 3;  // 12 bytes
    }
}
// Total per vertex: 12 + 8 + 12 + 12 + 12 = 56 bytes
```

**Structure in Memory:**

```
Vertex 0:  [x, y, z, u, v, nx, ny, nz, tx, ty, tz, bx, by, bz]  (56 bytes)
Vertex 1:  [x, y, z, u, v, nx, ny, nz, tx, ty, tz, bx, by, bz]  (56 bytes)
Vertex 2:  [x, y, z, u, v, nx, ny, nz, tx, ty, tz, bx, by, bz]  (56 bytes)
...
```

The data is **interleaved** — all attributes for one vertex are packed together.

### CPU-Side Vertex Attribute Description

**File:** `Engine\src\Renderer\Renderer.cpp` Lines 542-550

When building the pipeline, you define how to interpret the vertex data:

```cpp
pipeline = PipelineBuilder(_Context)
    .SetVertexBindings({
        // Binding 0: Describes the vertex buffer layout
        {
            0,                                            // Binding index
            sizeof(glm::vec3) +                           // Position (12 bytes)
            sizeof(glm::vec2) +                           // UV (8 bytes)
            sizeof(glm::vec3) * 3,                        // Normal + Tangent + Bitangent (36 bytes)
            VK_VERTEX_INPUT_RATE_VERTEX                   // One vertex per index
        }
        // Total: 56 bytes per vertex
    })
    .SetVertexAttributes({
        // Location 0: Position (vec3)
        { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },

        // Location 1: UV (vec2)
        { 1, 0, VK_FORMAT_R32G32_SFLOAT, sizeof(glm::vec3) },

        // Location 2: Normal (vec3)
        { 2, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(glm::vec3) + sizeof(glm::vec2) },

        // Location 3: Tangent (vec3)
        { 3, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(glm::vec3) + sizeof(glm::vec2) + sizeof(glm::vec3) },

        // Location 4: Bitangent (vec3)
        { 4, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(glm::vec3) + sizeof(glm::vec2) + sizeof(glm::vec3) * 2 },
    })
    .Build();
```

### Decoding VkVertexInputAttributeDescription

Each line has 4 fields:

```cpp
{ location, binding, format, offset }
```

| Field | Meaning | Your Value | Explanation |
|-------|---------|-----------|---|
| `location` | Shader location | 0-4 | Maps to `layout(location = N) in` in shader |
| `binding` | Which vertex buffer | 0 | All attributes from binding 0 (single buffer) |
| `format` | Data type | `VK_FORMAT_R32G32B32_SFLOAT` or `VK_FORMAT_R32G32_SFLOAT` | 3 floats = vec3, 2 floats = vec2 |
| `offset` | Byte offset in vertex | 0, 12, 20, 32, 44 | Where in the 56-byte vertex structure |

### The Key: Offset Calculation

This is **critical**: the offset tells the GPU "start reading from byte N of the vertex data":

```
Vertex in memory (56 bytes):
┌─────────────────────────────────────────────────────────────┐
│ Position (12)  │ UV (8) │ Normal (12) │ Tangent (12) │ Bitan(12) │
├─────────────────────────────────────────────────────────────┤
0              12        20           32             44         56

Location 0: offset = 0           ← Position starts at byte 0
Location 1: offset = 12          ← UV starts at byte 12 (0 + 12)
Location 2: offset = 20          ← Normal starts at byte 20 (12 + 8)
Location 3: offset = 32          ← Tangent starts at byte 32 (20 + 12)
Location 4: offset = 44          ← Bitangent starts at byte 44 (32 + 12)
```

### VkVertexInputBindingDescription

```cpp
{ 0, 56, VK_VERTEX_INPUT_RATE_VERTEX }
```

| Field | Meaning | Value |
|-------|---------|-------|
| `binding` | Binding index | 0 (using single buffer) |
| `stride` | Bytes per vertex | 56 (distance to next vertex in buffer) |
| `inputRate` | Update frequency | `VK_VERTEX_INPUT_RATE_VERTEX` (once per vertex) |

**Alternative:** `VK_VERTEX_INPUT_RATE_INSTANCE` (once per instance, for instancing).

---

## The Complete Vertex Attribute Flow: File → GPU → Shader

```
┌─ Mesh File (FBX, GLTF, OBJ, etc.) ─┐
│                                      │
│ Position: (x, y, z)                  │
│ UV: (u, v)                           │
│ Normal: (nx, ny, nz)                 │
│ Tangent: (tx, ty, tz)                │
│ Bitangent: (bx, by, bz)              │
└──────────────┬───────────────────────┘
               │
               ↓ Assimp Loader (Model.cpp)
┌─ CPU Memory (Interleaved Vertex Data) ─┐
│                                         │
│ Vertex 0: [x y z | u v | nx ny nz | ...] │
│ Vertex 1: [x y z | u v | nx ny nz | ...] │
│ ...                                      │
│ (56 bytes per vertex)                   │
└──────────────┬──────────────────────────┘
               │
               ↓ GPU Upload (Vertex Buffer)
┌─ GPU Memory (Vertex Buffer) ─────────┐
│ Same interleaved layout               │
│ (uploaded via vkCmdBindVertexBuffers) │
└──────────────┬───────────────────────┘
               │
               ↓ Vertex Attribute Description
┌─ VkVertexInputAttributeDescription ──┐
│ Location 0: offset = 0   (Position)   │
│ Location 1: offset = 12  (UV)         │
│ Location 2: offset = 20  (Normal)     │
│ Location 3: offset = 32  (Tangent)    │
│ Location 4: offset = 44  (Bitangent)  │
└──────────────┬───────────────────────┘
               │
               ↓ Pipeline Vertex Input State
┌─ Graphics Pipeline ──────────────────┐
│ "Vertices will have these attributes" │
│ at these locations with these offsets │
└──────────────┬───────────────────────┘
               │
               ↓ Rendering (vkCmdDraw)
┌─ GPU Vertex Shader Execution ────────┐
│                                      │
│ layout(location = 0) in vec3 a_Pos;  │ ← Reads from offset 0
│ layout(location = 1) in vec2 a_UV;   │ ← Reads from offset 12
│ layout(location = 2) in vec3 a_Norm; │ ← Reads from offset 20
│ layout(location = 3) in vec3 a_Tan;  │ ← Reads from offset 32
│ layout(location = 4) in vec3 a_Bitan;│ ← Reads from offset 44
│                                      │
│ void main() {                        │
│     // All attributes now populated  │
│     gl_Position = ... * a_Pos;       │
│ }                                    │
└──────────────────────────────────────┘
```

---

## Key Connection: CPU Layout ↔ Shader Declaration

**MUST MATCH:**

```cpp
// CPU: Location 0, format = 3 floats
{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 }
        ↓
// Shader: Location 0, vec3
layout(location = 0) in vec3 a_Position;
```

```cpp
// CPU: Location 1, format = 2 floats
{ 1, 0, VK_FORMAT_R32G32_SFLOAT, 12 }
        ↓
// Shader: Location 1, vec2
layout(location = 1) in vec2 a_UV;
```

**If they don't match:** shader won't compile or will produce wrong results.

---

## Your PBRShader Example: Complete Breakdown

### Shader Declaration

```glsl
layout(location = 0) in vec3 a_Position;    // 3 floats
layout(location = 1) in vec2 a_UV;          // 2 floats
layout(location = 2) in vec3 a_Normal;      // 3 floats
layout(location = 3) in vec3 a_Tangent;     // 3 floats
layout(location = 4) in vec3 a_Bitangent;   // 3 floats
```

### CPU Setup (from Renderer.cpp)

```cpp
.SetVertexBindings({
    { 0, 56, VK_VERTEX_INPUT_RATE_VERTEX }  // 56 bytes per vertex
})
.SetVertexAttributes({
    { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },      // Location 0: 12 bytes from offset 0
    { 1, 0, VK_FORMAT_R32G32_SFLOAT, 12 },        // Location 1: 8 bytes from offset 12
    { 2, 0, VK_FORMAT_R32G32B32_SFLOAT, 20 },     // Location 2: 12 bytes from offset 20
    { 3, 0, VK_FORMAT_R32G32B32_SFLOAT, 32 },     // Location 3: 12 bytes from offset 32
    { 4, 0, VK_FORMAT_R32G32B32_SFLOAT, 44 },     // Location 4: 12 bytes from offset 44
})
```

### At Runtime: Per Vertex

```
Raw memory (56 bytes):
[12 bytes] [8 bytes] [12 bytes] [12 bytes] [12 bytes]
Position   UV        Normal     Tangent    Bitangent

GPU reads:
└─ Offset 0  → location 0 (vec3 a_Position)
└─ Offset 12 → location 1 (vec2 a_UV)
└─ Offset 20 → location 2 (vec3 a_Normal)
└─ Offset 32 → location 3 (vec3 a_Tangent)
└─ Offset 44 → location 4 (vec3 a_Bitangent)

Shader receives:
a_Position = (x, y, z)
a_UV = (u, v)
a_Normal = (nx, ny, nz)
a_Tangent = (tx, ty, tz)
a_Bitangent = (bx, by, bz)
```

---

## Format Enum Reference

| Format | Type | Component Count | Size |
|--------|------|-----------------|------|
| `VK_FORMAT_R32_SFLOAT` | float | 1 | 4 bytes |
| `VK_FORMAT_R32G32_SFLOAT` | vec2 | 2 | 8 bytes |
| `VK_FORMAT_R32G32B32_SFLOAT` | vec3 | 3 | 12 bytes |
| `VK_FORMAT_R32G32B32A32_SFLOAT` | vec4 | 4 | 16 bytes |
| `VK_FORMAT_R8G8B8A8_UNORM` | uvec4 (byte) | 4 | 4 bytes |

---

## Multi-Buffer Vertex Input (Not Used in Your Project)

You could have multiple vertex buffers with different bindings:

```cpp
.SetVertexBindings({
    { 0, 28, VK_VERTEX_INPUT_RATE_VERTEX },  // Binding 0: Position + UV (28 bytes)
    { 1, 36, VK_VERTEX_INPUT_RATE_VERTEX },  // Binding 1: Normal + Tangent + Bitangent (36 bytes)
})
.SetVertexAttributes({
    // From binding 0
    { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },        // Position
    { 1, 0, VK_FORMAT_R32G32_SFLOAT, 12 },          // UV

    // From binding 1
    { 2, 1, VK_FORMAT_R32G32B32_SFLOAT, 0 },        // Normal (offset 0 in binding 1)
    { 3, 1, VK_FORMAT_R32G32B32_SFLOAT, 12 },       // Tangent
    { 4, 1, VK_FORMAT_R32G32B32_SFLOAT, 24 },       // Bitangent
})
```

This allows different update rates:
- Positions/UVs update once per frame
- Normals/Tangents update less frequently
- Trade-off: more complex, but can optimize memory usage.

**Your project uses single-buffer approach (simpler, all attributes in one buffer).**

---

## Summary: Vertex Attributes vs Descriptors vs Push Constants

| Concept | Update | Count | Usage | Example |
|---------|--------|-------|-------|---------|
| **Vertex Attributes** | Per-vertex | Dynamic | Mesh data | Position, UV, Normal |
| **Descriptors** (Set 0) | Per-frame | Fixed (6 bindings) | Shared resources | UBO, textures, samplers |
| **Push Constants** | Per-draw | 64-256 bytes | Per-object data | Model matrix |

All three meet at the shader:

```glsl
// Vertex attributes (from vertex buffer)
layout(location = 0) in vec3 a_Position;

// Descriptors (from descriptor set 0)
layout(set = 0, binding = 0) uniform globalUBO { ... };
layout(set = 0, binding = 1) uniform sampler2D tex;

// Push constants (fast per-draw data)
layout(push_constant) uniform modelMat { mat4 modelMatrix; };

void main() {
    // Combine all three
    gl_Position = globalUBO.projMatrix * globalUBO.viewMatrix * modelMatrix * vec4(a_Position, 1.0);
}
```

---

## Part 2: Output Locations (Fragment Shader Interpolation)

### The Mystery of the Gap: Lines 10-16

```glsl
layout(location = 0) out vec3 v_Pos;                    // Location 0 (1 slot)
layout(location = 1) out vec2 v_UV;                     // Location 1 (1 slot)
layout(location = 2) out vec3 v_Normal;                 // Location 2 (1 slot)
layout(location = 3) out vec4 v_FragPosLightSpace;      // Location 3 (1 slot)
layout(location = 4) out mat4 v_ViewMatrix;             // Location 4-7 (4 slots!)
layout(location = 8) out smooth mat3 v_TBN;             // Location 8-10 (3 slots!)
```

### Why the Gap?

When you output a **mat4**, it takes **4 location slots** (one per row):
- Location 4 = row 0 of mat4
- Location 5 = row 1 of mat4
- Location 6 = row 2 of mat4
- Location 7 = row 3 of mat4

So the next available location is **8**.

Then a **mat3** takes **3 slots**:
- Location 8 = row 0 of mat3
- Location 9 = row 1 of mat3
- Location 10 = row 2 of mat3

### PBRShader.frag Input Locations

Your fragment shader must match these locations:

```glsl
layout(location = 0) in vec3  v_Pos;
layout(location = 1) in vec2  v_UV;
layout(location = 2) in vec3  v_Normal;
layout(location = 3) in vec4  v_FragPosLightSpace;
layout(location = 4) in mat4  v_ViewMatrix;    // Takes locations 4-7
layout(location = 8) in mat3  v_TBN;           // Takes locations 8-10
```

If you don't match, the GPU will complain or produce wrong results.

### What Happens Between Vertex → Fragment Shader

```
Vertex Shader Output:
    v_Pos = (world_x, world_y, world_z)
    v_UV = (u, v)
    v_Normal = (nx, ny, nz)
    v_FragPosLightSpace = (lsx, lsy, lsz, lsw)
    v_ViewMatrix = [4x4 matrix]
    v_TBN = [3x3 matrix]

        ↓ [Rasterization: Triangle Interpolation]
        ↓ Each pixel on triangle gets interpolated values

Fragment Shader Input (interpolated at pixel):
    Same variable names and locations
    Barycentric-weighted values from triangle vertices
```

The GPU **interpolates** these values across the triangle using the fragment's barycentric coordinates. That's why it works for all pixels, not just vertices.

### The `smooth` Keyword

```glsl
layout(location = 8) out smooth mat3 v_TBN;
```

`smooth` means: use **Gouraud shading** (perspective-correct interpolation). Alternatives:
- `flat` = no interpolation, pick from provoking vertex
- (default) = `smooth` (what you want for most data)

### Why Send v_ViewMatrix to Fragment Shader?

Looking at fragment shader, this is likely used for:
- View-space calculations (parallel to surface)
- Specularity calculations
- Screen-space effects

It's computed in vertex shader but needed in fragment shader, so it's passed through.

---

## Part 3: Uniform Buffers (set, binding)

### Layout Syntax Breakdown: Line 19-24

```glsl
layout(set = 0, binding = 0) uniform globalUBO
{
    mat4 viewMatrix;
    mat4 projMatrix;
    mat4 directionalLightMVP;
};
```

Let's decode each part:

| Part | Meaning | In Your Code |
|------|---------|------------|
| `layout` | Vulkan layout specification | Tells GPU where to find data |
| `set = 0` | Descriptor set index | 0 = first set of resources |
| `binding = 0` | Binding index within the set | 0 = first binding in set 0 |
| `uniform` | Read-only GPU memory | Data cannot be modified in shader |
| `globalUBO` | Block name | Name for this uniform buffer object |
| `{ ... }` | Block contents | Structure of data inside UBO |

### What's a Descriptor Set?

A **descriptor set** is a collection of resource bindings. Think of it like a namespace:

```
Descriptor Set 0:
  ├─ Binding 0: Uniform buffer "globalUBO"
  ├─ Binding 1: Sampler "u_DiffuseSampler"
  ├─ Binding 2: Sampler "u_NormalSampler"
  └─ Binding 3-5: More samplers...

Descriptor Set 1:
  ├─ Binding 0: Storage buffer
  └─ Binding 1: Another resource
```

You can have multiple sets, but your shader only uses **set 0** currently.

### How It Maps to Vulkan CPU Code

On the CPU side (pseudocode):

```cpp
// 1. Create UBO buffer
VkBuffer uboBuffer = CreateBuffer(
    size = sizeof(mat4) * 3,  // 3 matrices
    usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
);

// 2. Write data to UBO
CopyDataToBuffer(uboBuffer, &viewMatrix, sizeof(mat4));
CopyDataToBuffer(uboBuffer, &projMatrix, sizeof(mat4));
CopyDataToBuffer(uboBuffer, &directionalLightMVP, sizeof(mat4));

// 3. Create descriptor set layout
VkDescriptorSetLayoutBinding binding = {
    .binding = 0,
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
};

// 4. Create descriptor set and write binding
VkWriteDescriptorSet write = {
    .dstSet = descriptorSet,
    .dstBinding = 0,
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    .pBufferInfo = &bufferInfo,  // Points to uboBuffer
};
vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

// 5. Bind descriptor set before rendering
vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                        pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
```

Now when the shader runs, `globalUBO` contains the UBO data.

### All Bindings in Your PBRShader.frag

```glsl
layout(set = 0, binding = 0) uniform globalUBO { ... };           // Uniform buffer
layout(set = 0, binding = 1) uniform sampler2D u_DiffuseSampler;  // Sampler
layout(set = 0, binding = 2) uniform sampler2D u_NormalSampler;   // Sampler
layout(set = 0, binding = 3) uniform sampler2D u_RoughnessMetallicSampler;
layout(set = 0, binding = 4) uniform sampler2D u_DirectionalShadowMap;
layout(set = 0, binding = 5) uniform samplerCube[5] u_PointShadowMap;
```

All in set 0, but bindings 0-5. Each is a different resource type:
- Binding 0: Uniform buffer
- Bindings 1-5: Samplers (read-only textures)

### Member Layout: std140 vs std430

GLSL uses **std140** by default for uniform buffers. This affects padding:

```glsl
layout(set = 0, binding = 0, std140) uniform globalUBO
{
    mat4 viewMatrix;        // Offset 0, size 64 bytes (4x4x4)
    mat4 projMatrix;        // Offset 64, size 64 bytes
    mat4 directionalLightMVP; // Offset 128, size 64 bytes
};  // Total: 192 bytes
```

std140 rules:
- `mat4` = 4 × `vec4` (each vec4 is 16 bytes)
- Scalars align to 4 bytes
- Vectors align to their size (vec3 = 12 bytes)

**std140 is more conservative with padding but wastes space.** If you want tighter packing, use `std430`, but then you must also specify it on CPU side.

### When to Use UBOs

Use uniform buffers for data that:
- Changes **per-frame** or **per-object** but not per-fragment
- Is **read-only**
- Is **large** (128 bytes or more)

Examples in your code:
- View matrix (per-frame)
- Projection matrix (per-frame)
- All point light shadow matrices (per-frame)
- Samplers/textures (rarely change)

---

## Part 4: Push Constants

### The Quick Path: Lines 26-29

```glsl
layout( push_constant ) uniform modelMat
{
    mat4 modelMatrix;
};
```

### What Are Push Constants?

Push constants are a **tiny fast memory** for small data that changes frequently:
- **Typical size**: 128-256 bytes (check device limits)
- **Speed**: Faster than UBOs (don't need descriptor set indirection)
- **Ideal for**: Per-object data that changes every draw call
- **Your use case**: Model matrix (one per object)

### Size Comparison

| Data | Size | Storage | Usage |
|------|------|---------|-------|
| Model Matrix | 64 bytes | Push constant | ✓ Per-object |
| View + Projection | 128 bytes | UBO | ✓ Per-frame (shared) |
| Single texture coordinate | 8 bytes | Push constant | ✓ Could go here |
| Array of 100 matrices | 6400 bytes | UBO or SSBO | ✗ Too large for push const |

### CPU Side: How Model Matrix Gets There

```cpp
// In Renderer.cpp, during rendering each object:
for (const auto& meshObject : objectsToRender)
{
    glm::mat4 transform = meshObject->GetTransform();

    // Push this object's model matrix
    CommandBuffer::PushConstants(
        cmd, 
        pipeline->GetPipelineLayout(), 
        VK_SHADER_STAGE_VERTEX_BIT,  // Available in vertex shader
        0,                            // Offset 0
        sizeof(glm::mat4),            // Size
        &transform                    // Data to push
    );

    // Draw this object
    meshObject->GetModel()->DrawIndexed(cmd, pipeline->GetPipelineLayout());

    // Next object's push constants will overwrite this
}
```

Each `PushConstants()` call sends 64 bytes (one matrix) directly to the GPU, which makes it available in the shader for that one draw call.

### Why Push Constants Are Used Here

```glsl
layout( push_constant ) uniform modelMat
{
    mat4 modelMatrix;
};
```

**Perfect for model matrix because:**
- Different for every object (helmet, ground, skybox, etc.)
- Same object for both vertex and fragment shaders
- Changes once per draw call
- Exactly 64 bytes (fits in push constant limits)
- Much faster than UBO indirection for per-object data

### Push Constant vs UBO Trade-off

| Aspect | Push Constant | Uniform Buffer |
|--------|---------------|-----------------|
| Size Limit | 128-256 bytes | Megabytes |
| Access Speed | Very fast (register) | Fast (cache) |
| Update Frequency | Per draw call | Per frame or less |
| Descriptor Binding | No (direct) | Yes (via descriptor set) |
| Use Case | **Per-object** | **Shared/per-frame** |
| Your Usage | Model matrix | View, Projection matrices |

### The Shader Access

In vertex shader, you access it simply:

```glsl
v_Pos = vec3(modelMatrix * vec4(a_Position, 1.0));
```

The GLSL compiler knows `modelMatrix` comes from push constants and generates optimal code to access it from the fast push constant memory.

---

## Part 5: Memory Layout Summary

Here's how data flows from CPU to your shader:

```
┌─────────────────────────────────────────────────────────┐
│                  CPU (Renderer.cpp)                      │
│                                                          │
│  Frame Update:                                           │
│  ├─ UBO: viewMatrix, projMatrix (once per frame)       │
│  ├─ UBO: directionalLightMVP (once per frame)          │
│  ├─ UBO: shadowMatrices[10][6] (once per frame)        │
│  └─ UBO: point light positions (once per frame)        │
│                                                          │
│  Per-Object:                                             │
│  └─ Push Constant: modelMatrix (once per draw call)    │
│                                                          │
│  Per-Vertex (from mesh file):                            │
│  ├─ Vertex Buffer: position, uv, normal, tangent       │
│  └─ Index Buffer: triangle indices                      │
└────────────────┬──────────────────────────────────────┘
                 │
                 ↓ [Vulkan Command Buffer]
┌────────────────────────────────────────────────────────┐
│              GPU (Shader Execution)                      │
│                                                          │
│  For each vertex:                                        │
│  ├─ Fetch input: a_Position (location 0) = ✓ From VB   │
│  ├─ Read uniform: viewMatrix (set=0, binding=0) = ✓   │
│  ├─ Read push const: modelMatrix = ✓                    │
│  └─ Compute: gl_Position = P * V * M * v               │
│                                                          │
│  Output to Framebuffer:                                  │
│  ├─ gl_Position (built-in)                              │
│  ├─ v_Pos (location 0) → Fragment Shader               │
│  ├─ v_ViewMatrix (location 4) → Fragment Shader        │
│  └─ v_TBN (location 8) → Fragment Shader               │
└────────────────────────────────────────────────────────┘
```

---

## Part 6: Practical Example: Full Transformation

Let's trace one vertex through your shader with real values:

### Input Data (from mesh file)
```glsl
a_Position = vec3(0.5, 0.0, -0.5);  // (location 0)
a_UV = vec2(0.25, 0.75);             // (location 1)
a_Normal = vec3(0.0, 1.0, 0.0);      // (location 2)
a_Tangent = vec3(1.0, 0.0, 0.0);     // (location 3)
a_Bitangent = vec3(0.0, 0.0, 1.0);   // (location 4)
```

### Uniform Data (from UBO, set 0, binding 0)
```glsl
viewMatrix = [ ... ];        // Camera transformation
projMatrix = [ ... ];        // Perspective transformation
directionalLightMVP = [ ... ]; // Light view-projection
```

### Push Constant Data (sent per draw call)
```glsl
modelMatrix = [
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    5, 2, 3, 1   // Translation: (5, 2, 3)
];
```

### Shader Execution

```glsl
void main()
{
    // Output 1: UV (unchanged)
    v_UV = a_UV;  // = (0.25, 0.75) → location 1

    // Output 2: Transform position to world space (using push constant)
    v_Pos = vec3(modelMatrix * vec4(a_Position, 1.0));
    // = (5, 2, 3) + (0.5, 0.0, -0.5)
    // = (5.5, 2.0, 2.5) → location 0

    // Output 3: Transform normal to world space (ignoring translation)
    v_Normal = mat3(modelMatrix) * a_Normal;
    // = (0.0, 1.0, 0.0) (unchanged, identity rotation) → location 2

    // Output 4: Compute normal matrix for proper normal transformation
    mat3 normalMatrix = transpose(inverse(mat3(modelMatrix)));

    vec3 T = normalize(normalMatrix * a_Tangent);      // (1.0, 0.0, 0.0)
    vec3 N = normalize(normalMatrix * a_Normal);       // (0.0, 1.0, 0.0)
    vec3 B = normalize(normalMatrix * a_Bitangent);    // (0.0, 0.0, 1.0)

    v_TBN = transpose(mat3(T, B, N));  // Store transposed → location 8-10

    // Output 5: Transform to light space (using UBO data)
    v_FragPosLightSpace = bias * directionalLightMVP * modelMatrix * vec4(a_Position, 1.0);
    // Complex transform for shadow map lookup → location 3

    // Output 6: Send view matrix to fragment shader (from UBO)
    // (This is unusual - normally computed in fragment, but passed here)
    v_ViewMatrix = viewMatrix;  // → location 4-7

    // Built-in output: Final position in clip space
    gl_Position = projMatrix * viewMatrix * modelMatrix * vec4(a_Position, 1.0);
    // Result in clip space, hardware will divide by w
}
```

### Output Sent to Rasterizer

```
Location 0: v_Pos = (5.5, 2.0, 2.5)
Location 1: v_UV = (0.25, 0.75)
Location 2: v_Normal = (0.0, 1.0, 0.0)
Location 3: v_FragPosLightSpace = [computed]
Location 4-7: v_ViewMatrix = [16 floats from UBO]
Location 8-10: v_TBN = [9 floats from normal transform]

Built-in: gl_Position = [computed clip space position]
```

During rasterization, these values are **interpolated** across the triangle and passed to the fragment shader.

---

## Part 7: Why This Organization?

### Why separate UBO from Push Constants?

**Per-Frame Data (UBO):**
- View, Projection matrices (same for all objects)
- Light positions (same for all objects)
- Camera position (same for all objects)
- Send once to GPU, reuse in 1000 draw calls

**Per-Object Data (Push Constant):**
- Model matrix (different for each object)
- Object ID or color tint (different for each object)
- Send once per draw call, used for just one object

This separation is a **massive optimization**. Without it, you'd need to update all matrices for every single object!

### Why Locations Have Gaps?

Matrix types consume multiple location slots:
- `mat4` = 4 slots (one per row/vec4)
- `mat3` = 3 slots (one per row/vec3)

**Layout explicitly after matrices prevents location conflicts:**

```glsl
layout(location = 4) out mat4 v_ViewMatrix;    // Takes 4-7
layout(location = 8) out smooth mat3 v_TBN;    // Takes 8-10, no overlap
```

If you didn't specify `location = 8`, the compiler would auto-assign and could create unexpected gaps.

### Why Send v_ViewMatrix to Fragment?

This is actually **unusual** in most shaders. Typically:
- Vertex shader computes world-space data
- Fragment shader uses that world-space data

Sending the view matrix suggests the fragment shader needs it for:
- Computing eye-relative vectors
- Screen-space calculations
- View-dependent effects (rim lighting, parallax)

Without seeing the fragment shader's use of it, it's hard to say, but it's being passed because some per-fragment calculation needs it.

---

## Part 8: Binding Types Reference

| Layout Qualifier | Type | Example | Use |
|------------------|------|---------|-----|
| `layout(location = n)` | Vertex attribute | `in vec3 position` | Mesh data |
| `layout(location = n)` | Varying | `out vec3 v_Normal` | VS → FS |
| `layout(set = s, binding = b)` | Uniform Buffer | `uniform globalUBO { ... }` | Per-frame data |
| `layout(set = s, binding = b)` | Sampler | `uniform sampler2D tex` | Textures |
| `layout(push_constant)` | Push Constant | `uniform modelMat { ... }` | Per-object fast data |
| `layout(set = s, binding = b)` | Storage Buffer | `buffer storageData { ... }` | Read/write GPU data |
| `layout(set = s, binding = b)` | Image | `layout(rgba32f) uniform image2D` | Compute shader images |

---

## Part 9: Common Mistakes

1. **Location Gaps Without Matrix Awareness**
   ```glsl
   // WRONG: Location 5 skipped!
   layout(location = 4) out mat4 v_Matrix;  // Takes 4-7
   layout(location = 5) out vec3 v_Normal;  // ERROR: 5 is already used!

   // RIGHT: Specify next available
   layout(location = 4) out mat4 v_Matrix;  // Takes 4-7
   layout(location = 8) out vec3 v_Normal;  // OK
   ```

2. **Mismatched Vertex/Fragment Layouts**
   ```glsl
   // Vertex Shader
   layout(location = 0) out vec3 v_Pos;
   layout(location = 1) out mat4 v_Data;    // Takes 1-4
   layout(location = 5) out vec3 v_Normal;

   // Fragment Shader (WRONG)
   layout(location = 0) in vec3 v_Pos;
   layout(location = 1) in mat4 v_Data;     // Takes 1-4
   layout(location = 4) in vec3 v_Normal;   // ERROR: 4 already used!

   // Fragment Shader (CORRECT)
   layout(location = 0) in vec3 v_Pos;
   layout(location = 1) in mat4 v_Data;     // Takes 1-4
   layout(location = 5) in vec3 v_Normal;   // Matches vertex
   ```

3. **Forgetting Push Constant Must Match Pipeline**
   ```glsl
   // Shader
   layout(push_constant) uniform { mat4 model; };

   // CPU: WRONG - pipeline layout doesn't define push constant
   vkCmdPushConstants(cmd, pipelineLayout, ...);  // CRASH or ignored

   // CPU: RIGHT - pipeline layout includes push constant range
   VkPushConstantRange range = {
       .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
       .offset = 0,
       .size = sizeof(glm::mat4)
   };
   CreatePipelineLayout(descriptorSetLayout, &range, 1);
   ```

4. **UBO Size Mismatch (std140)**
   ```glsl
   // Shader
   layout(std140) uniform ubo {
       vec3 data1;   // 12 bytes, then 4 byte padding
       float data2;  // 4 bytes
       mat4 data3;   // 64 bytes
   };  // Total: 84 bytes (not 80!)

   // CPU: WRONG - only allocated 80 bytes
   CreateBuffer(80, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

   // CPU: RIGHT - allocate full size
   CreateBuffer(96, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);  // Aligned to 16
   ```

---

## Summary Reference

| Concept | Where | Size | Update | Use |
|---------|-------|------|--------|-----|
| **Locations** | `layout(location = n)` | 1-4 slots | Per-vertex | Vertex input/output |
| **Sets/Bindings** | `layout(set = s, binding = b)` | Variable | Per-frame | UBOs, samplers |
| **Push Constants** | `layout(push_constant)` | 64-256 B | Per-draw | Per-object data |
| **Matrix Slots** | mat4 = 4, mat3 = 3 | Additive | N/A | Layout planning |

---

## Your PBRShader.vert Annotated

```glsl
#version 450

// ===== VERTEX ATTRIBUTES (Per-Vertex, from Mesh) =====
layout(location = 0) in vec3 a_Position;      // Location 0: xyz position
layout(location = 1) in vec2 a_UV;            // Location 1: uv texture coords
layout(location = 2) in vec3 a_Normal;        // Location 2: surface normal
layout(location = 3) in vec3 a_Tangent;       // Location 3: tangent for normal mapping
layout(location = 4) in vec3 a_Bitangent;     // Location 4: bitangent for normal mapping

// ===== VERTEX SHADER OUTPUTS (Interpolated to Fragment) =====
layout(location = 0) out vec3 v_Pos;                // Slot 0: world position
layout(location = 1) out vec2 v_UV;                // Slot 1: uv coords
layout(location = 2) out vec3 v_Normal;            // Slot 2: world normal
layout(location = 3) out vec4 v_FragPosLightSpace; // Slot 3: shadow map coords
layout(location = 4) out mat4 v_ViewMatrix;        // Slots 4-7: camera view matrix (4 rows)
layout(location = 8) out smooth mat3 v_TBN;        // Slots 8-10: tangent-bitangent-normal (3 rows)
// NOTE: Slot gap (5,6,7) occupied by mat4 rows. Next mat3 starts at 8.

// ===== PER-FRAME UNIFORMS (Set 0, Binding 0) =====
// Updated once per frame on CPU, shared across all objects
layout(set = 0, binding = 0) uniform globalUBO
{
    mat4 viewMatrix;           // World-to-camera transform
    mat4 projMatrix;           // Camera-to-clip transform
    mat4 directionalLightMVP;  // Light-space transform
};

// ===== PER-OBJECT DATA (Push Constant) =====
// Updated once per draw call on CPU, different for each object
layout( push_constant ) uniform modelMat
{
    mat4 modelMatrix;  // Object-to-world transform (64 bytes)
};

// ===== CONSTANTS =====
const mat4 bias = mat4(  // Maps NDC [-1,1] to texture [0,1]
  0.5, 0.0, 0.0, 0.0,
  0.0, 0.5, 0.0, 0.0,
  0.0, 0.0, 1.0, 0.0,
  0.5, 0.5, 0.0, 1.0 );

void main()
{
    // Interpolate texture coordinates unchanged
    v_UV = a_UV;

    // Transform vertex position: Object Space → World Space
    // w=1 because this is a position (translation applies)
    v_Pos = vec3(modelMatrix * vec4(a_Position, 1.0));

    // Transform normal: Object Space → World Space
    // Note: This is naive, should use inverse-transpose!
    v_Normal = mat3(modelMatrix) * a_Normal;

    // Properly transform TBN basis (handles non-uniform scale)
    mat3 normalMatrix = transpose(inverse(mat3(modelMatrix)));
    vec3 T = normalize(normalMatrix * a_Tangent);
    vec3 N = normalize(normalMatrix * a_Normal);
    vec3 B = normalize(normalMatrix * a_Bitangent);
    v_TBN = transpose(mat3(T, B, N));

    // Transform vertex to light space for shadow mapping
    v_FragPosLightSpace = bias * directionalLightMVP * modelMatrix * vec4(a_Position, 1.0);

    // Pass camera view matrix to fragment shader (unusual but needed there)
    v_ViewMatrix = viewMatrix;

    // Final position: Object Space → Clip Space
    // P * V * M * v (column vector convention)
    gl_Position = projMatrix * viewMatrix * modelMatrix * vec4(a_Position, 1.0);
}
```

This should clarify exactly how all the pieces fit together!

---

# Part 10: CPU-Side Descriptor Set Creation (The Complete Flow)

This section shows the **complete chain** from CPU code to GPU shader access, using your actual codebase as examples.

## The Complete Pipeline: CPU → GPU

```
CPU Code (Renderer.cpp, etc.)
    ↓
1. Define DescriptorBinding array
    ↓
2. Create DescriptorSetLayout (GPU-side blueprint)
    ↓
3. Create DescriptorPool (memory pool for allocations)
    ↓
4. Create Pipeline with this layout
    ↓
5. Allocate VkDescriptorSet from pool
    ↓
6. Write actual data to descriptor set (vkUpdateDescriptorSets)
    ↓
7. Bind descriptor set during rendering (vkCmdBindDescriptorSets)
    ↓
GPU Shader Execution
    ↓
8. Shader accesses data via layout(set = 0, binding = n)
```

---

## Step 1: Define DescriptorBinding Array

**File:** `Engine\src\Renderer\Renderer.cpp` Lines 384-391

This is the **template** that describes what you want to bind. From your code:

```cpp
// Define the layout for PBR materials
std::vector<DescriptorBinding> hdrLayout{
    { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
    { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
    { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
    { 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
    { 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
    { 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5, VK_SHADER_STAGE_FRAGMENT_BIT },
};
```

**Each line is one binding.**

| Binding | Type | Count | Shaders | Shader Declaration |
|---------|------|-------|---------|---|
| 0 | UNIFORM_BUFFER | 1 | V/G/F | `layout(set=0, binding=0) uniform globalUBO { ... }` |
| 1 | COMBINED_IMAGE_SAMPLER | 1 | F | `layout(set=0, binding=1) uniform sampler2D u_DiffuseSampler` |
| 2 | COMBINED_IMAGE_SAMPLER | 1 | F | `layout(set=0, binding=2) uniform sampler2D u_NormalSampler` |
| 3 | COMBINED_IMAGE_SAMPLER | 1 | F | `layout(set=0, binding=3) uniform sampler2D u_RoughnessMetallicSampler` |
| 4 | COMBINED_IMAGE_SAMPLER | 1 | F | `layout(set=0, binding=4) uniform sampler2D u_DirectionalShadowMap` |
| 5 | COMBINED_IMAGE_SAMPLER | 5 | F | `layout(set=0, binding=5) uniform samplerCube[5] u_PointShadowMap` |

**Key Insight:** Each CPU `DescriptorBinding` struct corresponds to ONE `layout(binding = N)` in the shader.

---

## Step 2 → 8: Full CPU-to-GPU Flow

**File:** `Engine\src\DescriptorSet.cpp`, `Pipeline.cpp`, `MeshBindingHelper.cpp`, `Utils.cpp`

### DescriptorSetLayout Creation (Step 2)

```cpp
// Converts your DescriptorBinding[] into GPU blueprint
DescriptorSetLayout::DescriptorSetLayout(const std::vector<DescriptorBinding>& InBindings)
{
    // Convert to Vulkan format
    std::vector<VkDescriptorSetLayoutBinding> vkBindings(InBindings.size());
    for (int i = 0; i < InBindings.size(); i++)
    {
        vkBindings[i].binding = InBindings[i].Binding;           // 0, 1, 2, ..., 5
        vkBindings[i].descriptorType = InBindings[i].Type;       // UNIFORM_BUFFER, SAMPLER
        vkBindings[i].descriptorCount = InBindings[i].Count;     // 1 or 5
        vkBindings[i].stageFlags = InBindings[i].ShaderStage;    // Which shaders
    }

    // Tell GPU: create layout with these bindings
    vkCreateDescriptorSetLayout(...);  // GPU now knows about bindings 0-5
}
```

### DescriptorPool Creation (Step 3)

```cpp
// Allocate GPU memory for descriptor sets
DescriptorPool::DescriptorPool(uint32_t count, std::vector<VkDescriptorType> types)
{
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.maxSets = count;              // Max 10 sets
    poolInfo.poolSizes = ...;              // How many of each type

    vkCreateDescriptorPool(...);           // GPU memory reserved
}
```

### Pipeline Creation (Step 4)

```cpp
// Link layout to pipeline
Ref<Pipeline> pipeline = PipelineBuilder(_Context)
    .SetDescriptorSetLayout(PBRLayout)     // ← Link layout here
    .SetVertexShader("...")
    .SetFragmentShader("...")
    .Build();

// Inside Build():
// 1. Create VkPipelineLayout with descriptor set layout
// 2. Create VkGraphicsPipeline using that layout
// Now pipeline "knows" shaders expect set 0 with bindings 0-5
```

### Descriptor Set Allocation (Step 5)

```cpp
VkDescriptorSetAllocateInfo allocInfo{};
allocInfo.descriptorPool = pool->GetDescriptorPool();      // From step 3
allocInfo.pSetLayouts = &PBRLayout->GetDescriptorLayout(); // From step 2

VkDescriptorSet set;
vkAllocateDescriptorSets(..., &set);  // Empty set allocated from pool
```

### Writing Data (Step 6)

```cpp
// Binding 0: Write the UBO buffer
Utils::UpdateDescriptorSet(set, globalUBOBuffer, 0, sizeof(UBO), 0);
//                         ^^^                                      ^^^
//                         descriptor set                    binding index

// Binding 1: Write diffuse sampler
Utils::UpdateDescriptorSet(set, diffuseSampler, diffuseImage->GetImageView(), 1, ...);
//                                                                               ^^^
//                                                                        binding index

// Bindings 2-5: Repeat pattern...
```

Inside `UpdateDescriptorSet`:

```cpp
// For samplers (bindings 1-5):
VkDescriptorImageInfo imageInfo{};
imageInfo.imageView = imageView;   // Which texture
imageInfo.sampler = sampler;       // How to filter it

VkWriteDescriptorSet write{};
write.dstSet = set;                // Which descriptor set
write.dstBinding = bindingIndex;   // Which binding (1, 2, 3, ...)
write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
write.pImageInfo = &imageInfo;

vkUpdateDescriptorSets(...);       // Write to GPU

// For UBO (binding 0):
VkDescriptorBufferInfo bufferInfo{};
bufferInfo.buffer = buffer;        // The UBO buffer
bufferInfo.range = size;           // How much data

VkWriteDescriptorSet write{};
write.dstSet = set;
write.dstBinding = 0;              // Binding 0
write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
write.pBufferInfo = &bufferInfo;

vkUpdateDescriptorSets(...);       // Write to GPU
```

### Binding During Rendering (Step 7)

```cpp
// Before drawing:
VkDescriptorSet sets[] = { set };
vkCmdBindDescriptorSets(
    cmd,
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    pipeline->GetPipelineLayout(),      // From step 4
    0,                                  // Set index (0)
    1,                                  // Number of sets
    sets,                               // The sets to bind
    0, nullptr);

// GPU now knows: when shaders run, use this descriptor set for all bindings
```

### Shader Access (Step 8)

```glsl
layout(set = 0, binding = 0) uniform globalUBO {
    mat4 viewMatrix;
    mat4 projMatrix;
    // ...
};

layout(set = 0, binding = 1) uniform sampler2D u_DiffuseSampler;
layout(set = 0, binding = 2) uniform sampler2D u_NormalSampler;
// ... bindings 3-5 similarly

void main() {
    // Access UBO (binding 0)
    mat4 view = viewMatrix;

    // Sample textures (bindings 1-5)
    vec3 albedo = texture(u_DiffuseSampler, uv).rgb;
    vec3 normal = texture(u_NormalSampler, uv).rgb;
    // ...
}
```

---

## The Connection: CPU Binding Index ↔ GPU Binding Number

**This is the critical link:**

```
CPU: DescriptorBinding { binding: 1, type: SAMPLER, ... }
              ↓
CPU: vkUpdateDescriptorSets(..., dstBinding = 1, ...)
              ↓
CPU: Utils::UpdateDescriptorSet(..., bindingIndex = 1, ...)
              ↓
GPU: layout(set = 0, binding = 1) uniform sampler2D ...
```

**They must match!** If CPU writes to binding 1, shader must read from binding 1.

---

## Visual Architecture Diagram

```
┌─── CPU INITIALIZATION ───────────────────────────────┐
│                                                       │
│  DescriptorBinding hdrLayout[]                        │
│  ├─ [0] UNIFORM_BUFFER                              │
│  ├─ [1] SAMPLER (diffuse)                           │
│  ├─ [2] SAMPLER (normal)                            │
│  ├─ [3] SAMPLER (roughness)                         │
│  ├─ [4] SAMPLER (shadow)                            │
│  └─ [5] SAMPLER[5] (cubemaps)                       │
│         │                                            │
│         ↓                                            │
│  DescriptorSetLayout (GPU blueprint)                │
│         │                                            │
│         ↓                                            │
│  DescriptorPool (GPU memory bank)                   │
│         │                                            │
│         ↓                                            │
│  Pipeline + PipelineLayout (links everything)       │
│                                                       │
└───────────────────────────────────────────────────────┘
                         │
                         ↓
┌─── PER-OBJECT RENDERING ─────────────────────────────┐
│                                                       │
│  Allocate VkDescriptorSet (empty from pool)         │
│         │                                            │
│         ├─ vkUpdateDescriptorSets(binding=0, UBO)  │
│         ├─ vkUpdateDescriptorSets(binding=1, tex1) │
│         ├─ vkUpdateDescriptorSets(binding=2, tex2) │
│         ├─ vkUpdateDescriptorSets(binding=3, tex3) │
│         ├─ vkUpdateDescriptorSets(binding=4, tex4) │
│         └─ vkUpdateDescriptorSets(binding=5, tex5) │
│         │                                            │
│         ↓                                            │
│  vkCmdBindDescriptorSets(set 0)                     │
│         │                                            │
│         ↓                                            │
│  vkCmdDrawIndexed() → GPU execution                 │
│                                                       │
└───────────────────────────────────────────────────────┘
                         │
                         ↓
┌─── GPU SHADER EXECUTION ─────────────────────────────┐
│                                                       │
│  Vertex Shader:                                      │
│  layout(set=0, binding=0) uniform globalUBO { ... } │
│  layout(push_constant) uniform modelMat { ... }     │
│         │                                            │
│         ↓ Per-vertex transform                      │
│  Fragment Shader:                                    │
│  layout(set=0, binding=0) uniform globalUBO { ... } │
│  layout(set=0, binding=1) uniform sampler2D u_Dif. │
│  layout(set=0, binding=2) uniform sampler2D u_Norm │
│  ... bindings 3-5 ...                              │
│         │                                            │
│         ↓ Per-pixel shading                         │
│  Output pixel                                        │
│                                                       │
└───────────────────────────────────────────────────────┘
```

---

## Reference: All Descriptor Types

Your project uses these descriptor types:

| Type | Usage | Count | Example |
|------|-------|-------|---------|
| `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER` | Read-only data (UBO) | 1 | globalUBO with matrices |
| `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER` | Texture sampling | 1 or array | Diffuse, normal, shadow maps |
| `VK_DESCRIPTOR_TYPE_STORAGE_BUFFER` | Read/write data | 1+ | Compute shader buffers |
| `VK_DESCRIPTOR_TYPE_STORAGE_IMAGE` | Read/write images | 1+ | Compute shader output |

Your PBR shader uses only **UNIFORM_BUFFER** and **COMBINED_IMAGE_SAMPLER**.

---

## Why "set 0"?

Multiple descriptor sets allow organizational structure:

```glsl
// Set 0: Frame-wide data (view, projection, lights)
layout(set = 0, binding = 0) uniform FrameData { ... };

// Set 1: Object-specific data
layout(set = 1, binding = 0) uniform ObjectData { ... };

// Set 2: Material-specific data
layout(set = 2, binding = 0) uniform MaterialData { ... };
```

Update per-frame, per-object, per-material independently. **Your project uses only set 0**, which is simpler but less organized.

---

## Complete Example: From Definition to Shader

```cpp
// ===== CPU CODE =====

// 1. Define layout
std::vector<DescriptorBinding> layout{
    { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
    { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
};

// 2. Create layout
auto descriptorLayout = make_s<DescriptorSetLayout>(layout);

// 3. Create pool
auto pool = make_s<DescriptorPool>(10, { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER });

// 4. Create pipeline
auto pipeline = PipelineBuilder(context)
    .SetDescriptorSetLayout(descriptorLayout)
    .SetVertexShader("shader.vert")
    .SetFragmentShader("shader.frag")
    .Build();

// 5-6. Allocate and fill descriptor set
VkDescriptorSet set = AllocateSet(pool, descriptorLayout);
Utils::UpdateDescriptorSet(set, uboBuffer, 0, sizeof(UBO), 0);
Utils::UpdateDescriptorSet(set, sampler, imageView, 1, layout);

// 7. Bind during rendering
vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipelineLayout(), 0, 1, &set, 0, nullptr);
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetHandle());
vkCmdDrawIndexed(cmd, ...);
```

```glsl
// ===== SHADER CODE =====

#version 450

// Binding 0 from CPU DescriptorBinding
layout(set = 0, binding = 0) uniform UniformBuffer {
    mat4 view;
    mat4 proj;
} ubo;

// Binding 1 from CPU DescriptorBinding
layout(set = 0, binding = 1) uniform sampler2D tex;

void main() {
    // Use UBO (from binding 0)
    gl_Position = ubo.proj * ubo.view * vec4(...);

    // Sample texture (from binding 1)
    vec4 color = texture(tex, uv);
}
```

---

## Summary

**set 0** contains all your shader bindings:
- Created on CPU via `DescriptorSetLayout` + `DescriptorPool`
- Populated via `vkUpdateDescriptorSets`
- Bound via `vkCmdBindDescriptorSets`
- Accessed in shader via `layout(set = 0, binding = N)`

The binding number **must match** between CPU and shader, and the data type must match too.
