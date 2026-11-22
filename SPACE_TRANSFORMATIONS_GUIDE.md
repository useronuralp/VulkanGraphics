# Space Transformations in VulkanGraphics - Complete Guide

This document traces space transformations through your VulkanGraphics project, from CPU code to GPU shaders, showing exactly where each transformation happens and why.

---

## Overview: The Transformation Pipeline

In your project, vertices move through these spaces:

1. **Object Space** → Vertex positions in model files (e.g., helmet mesh)
2. **World Space** → After applying model matrix (object's position, rotation, scale)
3. **View Space** → After applying camera view matrix (camera's "look at" transform)
4. **Clip Space** → After applying projection matrix (maps to -w ≤ x,y,z ≤ w)
5. **NDC Space** → After perspective divide (maps to Vulkan's 0-1 depth, flipped Y)

### Transformation Order in GLSL (Column Vectors)
Your shaders use Vulkan's GLSL with **column vectors**, so the order is:

```glsl
gl_Position = P * V * M * v
```

Where:
- `P` = Projection Matrix
- `V` = View Matrix  
- `M` = Model Matrix
- `v` = Vertex in object space (w=1 for positions, w=0 for directions)

---

## 1. MODEL MATRIX (Object → World Space)

### Where It's Created: `Engine\src\SceneObject.cpp`

```cpp
// SceneObject::RecalculateTransform() - Lines 78-88
void SceneObject::RecalculateTransform()
{
    _Transform = glm::mat4(1.0f);
    _Transform = glm::translate(_Transform, _Position);      // Translation
    _Transform = glm::rotate(_Transform, glm::radians(_Rotation.x), glm::vec3(1, 0, 0));  // X rotation
    _Transform = glm::rotate(_Transform, glm::radians(_Rotation.y), glm::vec3(0, 1, 0));  // Y rotation
    _Transform = glm::rotate(_Transform, glm::radians(_Rotation.z), glm::vec3(0, 0, 1));  // Z rotation
    _Transform = glm::scale(_Transform, _Scale);            // Scale
    _Dirty     = false;
}
```

**The Order Matters!**
- Translate first, then rotate, then scale
- This gives: `M = T * R * S`
- Why? Because matrix multiplication is right-to-left: vertex is multiplied `M * v = T * R * S * v`, so scale applies first, then rotation, then translation

**Example from Your Code:**
```cpp
// In Renderer.cpp, rotating the helmet around Y axis:
auto helmet = _Scene->FindMeshObject("Helmet");
if (helmet)
    helmet->Rotate(2.0f * _DeltaTime, glm::vec3(0, 1, 0));
```

This accumulates into the model matrix and is passed to shaders via push constant.

### Where It's Used in Shaders: `PBRShader.vert` (Line 42)

```glsl
void main()
{
    // Transform vertex from object space to world space
    v_Pos = vec3(modelMatrix * vec4(a_Position, 1.0));

    // Transform normal from object space to world space
    v_Normal = mat3(modelMatrix) * a_Normal;
}
```

**Key Detail:** `a_Position` has `w=1.0` because it's a position. Translation DOES apply.

---

## 2. VIEW MATRIX (World → View Space)

### Where It's Created: `Engine\src\Camera.cpp`

```cpp
// Camera::UpdateView() - Lines 47-53
void Camera::UpdateView()
{
    _Position             = CalculatePosition();

    // Create world-to-camera transform
    glm::quat orientation = GetOrientation();
    _ViewMatrix           = glm::translate(glm::mat4(1.0f), _Position) * glm::toMat4(orientation);

    // INVERT to get the view matrix (camera-relative coordinates)
    _ViewMatrix           = glm::inverse(_ViewMatrix);

    _ViewProjectionMatrix = _ProjectionMatrix * _ViewMatrix;
}
```

**Why Invert?**
- First line builds: "where is camera in world" (camera's transform)
- Inverse gives: "where is world relative to camera" (what we need for shaders)
- This is the heart of camera systems

**Example Values at Runtime:**
- Camera looks at scene from `_Position` (far from origin)
- Orientation from mouse input (yaw, pitch)
- Result: everything transforms to be relative to camera

### Projection Matrix Creation: `Engine\src\Camera.cpp`

```cpp
// Camera::UpdateProjection() - Lines 39-43
void Camera::UpdateProjection()
{
    _AspectRatio          = _ViewportWidth / _ViewportHeight;
    // glm::perspective creates P with standard conventions
    _ProjectionMatrix     = glm::perspective(glm::radians(_FOV), _AspectRatio, _NearClip, _FarClip);
    _ViewProjectionMatrix = _ProjectionMatrix * _ViewMatrix;
}
```

**Camera Settings:**
- FOV: 45 degrees
- Near Clip: 0.1 (anything closer is clipped)
- Far Clip: 1500.0 (anything farther is clipped)
- Aspect Ratio: Automatically computed from viewport size

**What This Does:**
- Maps view-space frustum to clip space
- `w` after projection = original view-space depth (z)
- This is why clipping happens *before* perspective divide!

---

## 3. VERTEX TRANSFORMATION IN MAIN PASS: `PBRShader.vert`

This is where all matrices combine:

```glsl
#version 450

// From push constants (updated per object)
layout( push_constant ) uniform modelMat
{
    mat4 modelMatrix;
};

// From global UBO (updated once per frame)
layout(set = 0, binding = 0) uniform globalUBO
{
    mat4 viewMatrix;
    mat4 projMatrix;
    mat4 directionalLightMVP;
    // ... other uniforms
};

void main()
{
    // ===== OBJECT SPACE → WORLD SPACE =====
    v_Pos = vec3(modelMatrix * vec4(a_Position, 1.0));

    // ===== NORMAL TRANSFORMATION: INVERSE TRANSPOSE =====
    mat3 normalMatrix = transpose(inverse(mat3(modelMatrix)));

    vec3 T = normalize(normalMatrix * a_Tangent);
    vec3 N = normalize(normalMatrix * a_Normal);
    vec3 B = normalize(normalMatrix * a_Bitangent);

    v_TBN = transpose(mat3(T, B, N));

    // ===== SHADOW MAPPING =====
    v_FragPosLightSpace = bias * directionalLightMVP * modelMatrix * vec4(a_Position, 1.0);

    // ===== OBJECT SPACE → CLIP SPACE =====
    // This is: P * V * M * v (column vector convention)
    gl_Position = projMatrix * viewMatrix * modelMatrix * vec4(a_Position, 1.0);
}
```

### Breaking Down Each Line:

#### 1. World-Space Position (Line 42)
```glsl
v_Pos = vec3(modelMatrix * vec4(a_Position, 1.0));
```
- Input: vertex in object space (from mesh file)
- Operation: `M * v` 
- Output: world-space position (sent to fragment shader)
- Used for: lighting calculations in world space

#### 2. Normal Transformation (Line 46)
```glsl
mat3 normalMatrix = transpose(inverse(mat3(modelMatrix)));
```

**This Is Critical Knowledge:**
- If you just did `modelMatrix * normal`, non-uniform scaling would shear the normal
- Proof: if `n · t = 0` (normal perpendicular to tangent), after transform you'd have `n' · Mt ≠ 0`
- Solution: use `(M⁻¹)ᵀ` to restore perpendicularity
- Your code does exactly this: `transpose(inverse(M))`

**Performance Note in Your Code:**
- This is computed per-vertex
- Alternative: compute on CPU and pass as separate attribute
- Only necessary if M contains non-uniform scale

#### 3. TBN Matrix for Normal Mapping (Lines 50-54)
```glsl
vec3 T = normalize(normalMatrix * a_Tangent);
vec3 N = normalize(normalMatrix * a_Normal);
vec3 B = normalize(normalMatrix * a_Bitangent);
v_TBN = transpose(mat3(T, B, N));  // Store as transpose for fragment shader
```

- Transforms tangent space basis vectors to world space
- Transposed because fragment shader will use: `normal = TBN * sampledNormal` (row vector convention for efficiency)

#### 4. Shadow Space Transformation (Line 57)
```glsl
v_FragPosLightSpace = bias * directionalLightMVP * modelMatrix * vec4(a_Position, 1.0);
```

Chain:
1. `modelMatrix * v` → world space
2. `directionalLightMVP * (world pos)` → light clip space
3. `bias * (clip space)` → Vulkan NDC space (0-1 depth, flipped Y)

The `bias` matrix:
```glsl
const mat4 bias = mat4( 
  0.5, 0.0, 0.0, 0.0,
  0.0, 0.5, 0.0, 0.0,
  0.0, 0.0, 1.0, 0.0,
  0.5, 0.5, 0.0, 1.0 );
```
Converts from [-1, 1] NDC to [0, 1] texture coordinates.

#### 5. Final Position in Clip Space (Line 59)
```glsl
gl_Position = projMatrix * viewMatrix * modelMatrix * vec4(a_Position, 1.0);
```

The complete chain:
- `modelMatrix * v` → world space
- `viewMatrix * (world)` → view space (camera-relative)
- `projMatrix * (view)` → clip space
- Hardware then does perspective divide by `w` → NDC space

---

## 4. DIRECTIONAL SHADOW PASS: `shadowPass.vert`

This uses the **same M**, but different **V** and **P**:

```glsl
layout(set = 0, binding = 0) uniform globalUBO
{
    mat4 viewMat;
    mat4 projMat;
    mat4 lightMVP;  // Pre-computed P * V for light
};

void main()
{
    gl_Position = lightMVP * modelMatrix * vec4(a_Position, 1.0);
}
```

### Where lightMVP Comes From: `Renderer.cpp` Line 884

```cpp
auto      dirLight            = _Scene->GetDirectionalLight();
glm::mat4 directionalLightMVP = directionalLightProjectionMatrix * glm::lookAt(
    dirLight->GetPosition(),      // Camera position (light's position)
    glm::vec3(0.0f),               // Look at origin
    glm::vec3(0.0f, 1.0f, 0.0f)    // Up vector
);
```

**Key Difference from Camera:**
- Light view: looking FROM light position AT scene origin
- Camera view: looking from camera AT scene
- Same `modelMatrix` used, but different V and P
- Result: vertices in light's clip space (for shadow map)

---

## 5. POINT LIGHT SHADOWS: 6 Faces of Cubemap

### Geometry Shader Approach: `pointShadowPass.geom`

```glsl
#define MAX_POINT_LIGHT 10

layout (triangles) in;
layout (triangle_strip, max_vertices = 18) out;

layout(set = 0, binding = 0) uniform globalUBO
{
    // ... 
    mat4 shadowMatrices[MAX_POINT_LIGHT][6];  // 6 faces per light
};

layout (push_constant) uniform pointLightIndex
{   
    vec4 index;  // Which light (0-9)
};

void main()
{
    // For each of 6 cubemap faces
    for(int face = 0; face < 6; ++face)
    {
        gl_Layer = face;  // Output to face N

        for(int i = 0; i < 3; ++i)  // For each vertex of triangle
        {
            FragPos = gl_in[i].gl_Position;  // In world space (from VS)

            // Transform world position to THIS face's clip space
            gl_Position = shadowMatrices[int(index.x)][face] * FragPos;

            EmitVertex();
        }    
        EndPrimitive();
    }
}
```

**One Triangle → 6 Triangle Copies:**
- Input: 1 triangle
- Output: 6 identical triangles (one per cubemap face)
- Each copies to different layer
- Each uses different MVP (different view direction)

### Where Shadow Matrices Come From: `Renderer.cpp` Lines 953-963

```cpp
glm::vec3 position = glm::vec3(
    globalParametersUBO.pointLightPositions[i].x,
    globalParametersUBO.pointLightPositions[i].y,
    globalParametersUBO.pointLightPositions[i].z
);

// Face 0: +X (looking right)
globalParametersUBO.shadowMatrices[i][0] =
    pointLightProjectionMatrix * glm::lookAt(position, position + glm::vec3(1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0));

// Face 1: -X (looking left)
globalParametersUBO.shadowMatrices[i][1] =
    pointLightProjectionMatrix * glm::lookAt(position, position + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0));

// Face 2: +Y (looking up)
globalParametersUBO.shadowMatrices[i][2] =
    pointLightProjectionMatrix * glm::lookAt(position, position + glm::vec3(0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0));

// Face 3: -Y (looking down)
globalParametersUBO.shadowMatrices[i][3] =
    pointLightProjectionMatrix * glm::lookAt(position, position + glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 0.0, -1.0));

// Face 4: +Z (looking forward)
globalParametersUBO.shadowMatrices[i][4] =
    pointLightProjectionMatrix * glm::lookAt(position, position + glm::vec3(0.0, 0.0, 1.0), glm::vec3(0.0, -1.0, 0.0));

// Face 5: -Z (looking backward)
globalParametersUBO.shadowMatrices[i][5] =
    pointLightProjectionMatrix * glm::lookAt(position, position + glm::vec3(0.0, 0.0, -1.0), glm::vec3(0.0, -1.0, 0.0));
```

**Key Pattern:**
- Same light position for all 6
- Different `lookAt()` direction for each face
- Each gives its own V matrix
- All use same P (pointLightProjectionMatrix)
- Result: 6 different MVP matrices, one per cubemap face

---

## 6. NORMAL TRANSFORM IN FRAGMENT SHADER: `PBRShader.frag`

```glsl
vec3 getNormalFromMap()
{
    vec3 tangentNormal = texture(u_NormalSampler, v_UV).xyz * 2.0 - 1.0;

    // Reconstruct TBN from derivatives (alternative method)
    vec3 Q1  = dFdx(v_Pos);
    vec3 Q2  = dFdy(v_Pos);
    vec2 st1 = dFdx(v_UV);
    vec2 st2 = dFdy(v_UV);

    vec3 N   = normalize(v_Normal);
    vec3 T   = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B   = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}
```

**Two Methods for TBN:**
1. **From vertex shader** (your code uses this too): tangent, bitangent, normal pre-computed
2. **From derivatives** (shown here): computed per-fragment using ddx/ddy

Derivatives method is more stable under extreme stretching but more expensive.

---

## 7. VULKAN-SPECIFIC CONVENTIONS

### NDC Space Differences
Your project uses Vulkan, which has specific conventions:

| Convention | OpenGL | Vulkan |
|-----------|--------|--------|
| Depth Range | [-1, 1] | [0, 1] |
| Y Axis | +Y up | +Y down (in NDC) |
| Clip Range | -w ≤ z ≤ w | 0 ≤ z ≤ w |

### Bias Matrix Explanation

```glsl
const mat4 bias = mat4( 
  0.5, 0.0, 0.0, 0.0,   // Scale X by 0.5
  0.0, 0.5, 0.0, 0.0,   // Scale Y by 0.5
  0.0, 0.0, 1.0, 0.0,   // Keep Z as-is
  0.5, 0.5, 0.0, 1.0 ); // Translate by (0.5, 0.5)
```

Transforms `[-1, 1]` → `[0, 1]`:
- `x' = 0.5 * x + 0.5`
- `y' = 0.5 * y + 0.5`
- Maps clip-space coordinates to texture coordinates for shadow map lookup

---

## 8. COMPLETE VERTEX JOURNEY EXAMPLE

Let's trace one vertex of the helmet:

### Input
```
Helmet mesh: vertex at position (0.0, 1.0, 0.0) in model space
Helmet transform: position=(5.0, 0.0, 0.0), rotation=45° Y, scale=(1.0, 1.0, 1.0)
Camera: position=(0.0, 5.0, 10.0), looking at origin
```

### Step 1: Model Matrix Applied
```
Object space vertex:     v_obj = (0.0, 1.0, 0.0, 1)
Model matrix:            M = Translate(5,0,0) * Rotate(45°,Y) * Scale(1,1,1)
World space vertex:      v_world = M * v_obj ≈ (3.54, 1.0, 3.54, 1)
```
(Exact values depend on rotation calculation)

### Step 2: View Matrix Applied
```
Camera view matrix:      V = inverse(translate(0,5,10) * rotationFromPitch/Yaw)
View space vertex:       v_view = V * v_world (now relative to camera)
```

### Step 3: Projection Applied
```
Projection matrix:       P = perspective(45°, 16:9, 0.1, 1500.0)
Clip space vertex:       v_clip = P * v_view
After divide by w:       v_ndc ≈ (0.4, 0.5, 0.8)  (example NDC coords)
```

### Step 4: Rasterization
```
gl_Position set to v_clip
Hardware divides by w automatically
Produces pixel position on screen
```

---

## 9. COORDINATE SYSTEM SUMMARY

Your project uses **right-handed coordinate system** with:
- **X**: right
- **Y**: up
- **Z**: backward (into screen)

This is standard for graphics. You can see it in camera code:
- `GetRightDirection()` returns X axis
- `GetUpDirection()` returns Y axis
- `GetForwardDirection()` returns -Z (into scene)

---

## 10. PERFORMANCE OBSERVATIONS

### Matrix Updates Per Frame
From `Renderer.cpp` lines 887-896:

```cpp
globalParametersUBO.viewMatrix           = cameraView;      // Updated once
globalParametersUBO.projMatrix           = cameraProj;      // Updated once
globalParametersUBO.directionalLightMVP  = directionalLightMVP;  // Updated once
globalParametersUBO.pointLightCount      = /* ... */;       // Updated once

// For each point light (up to 10):
for (int i = 0; i < POINT_LIGHT_COUNT; i++)
{
    globalParametersUBO.shadowMatrices[i][0..5] = /* lookAt for each face */;  // 6x per light
}
```

- Global UBO: written once per frame
- Push constants (model matrix): written once per object draw

### Inverse Transpose Cost
In vertex shader:
```glsl
mat3 normalMatrix = transpose(inverse(mat3(modelMatrix)));
```

- Per-vertex cost: 1 matrix inverse + 1 transpose
- Alternative: compute on CPU once per object (dirty flag optimization already implemented)
- Your code recomputes every vertex (acceptable for most scenarios)

---

## 11. KEY TAKEAWAYS FOR YOUR PROJECT

1. **Model Matrix** (SceneObject.cpp)
   - Built from: Translate → Rotate → Scale
   - Passed via push constants per object
   - Transforms object space → world space

2. **View Matrix** (Camera.cpp)
   - Built with `glm::lookAt()` then inverted
   - Updated once per frame
   - Transforms world space → view space

3. **Projection Matrix** (Camera.cpp)
   - Built with `glm::perspective()`
   - Settings: 45° FOV, 0.1-1500 far, aspect from viewport
   - Transforms view space → clip space
   - After hardware divide by w: NDC space

4. **Normal Transformation**
   - Uses `transpose(inverse(M))` for correctness under non-uniform scale
   - Your code implements this per-vertex in shader
   - Could optimize by computing on CPU

5. **Shadow Passes**
   - Directional: single MVP from light's perspective
   - Point lights: 6 MVPs (one per cubemap face) via geometry shader
   - Different V and P, same M (model matrix)

6. **Vulkan NDC**
   - Depth 0-1 (not -1 to 1)
   - Y flipped in NDC
   - Bias matrix handles NDC coordinate remapping

This architecture is solid and follows graphics programming best practices!
