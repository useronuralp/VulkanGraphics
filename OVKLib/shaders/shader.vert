#version 450

// INs
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_UV;
layout(location = 2) in vec3 a_Normal;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec3 a_Bitangent;

//OUTs
layout(location = 0) out vec3 v_Pos;
layout(location = 1) out vec2 v_UV;
layout(location = 2) out vec3 v_Normal;
layout(Location = 3) out mat3 v_TBN;

layout(binding = 0) uniform ModelMatrix
{
    mat4 ModelMat;
} Model;

layout(binding = 1) uniform ViewMatrix
{
    mat4 ViewMat;
} View;

layout(binding = 2) uniform ProjectionMatrix
{
    mat4 ProjMat;
} Proj;

void main()
{
    v_Pos = vec3(Model.ModelMat * vec4(a_Position, 1.0));
    v_UV = a_UV;
    
    mat3 normalMatrix = transpose(inverse(mat3(Model.ModelMat)));
    vec3 T = normalize(normalMatrix * a_Tangent);
    vec3 N = normalize(normalMatrix * a_Normal);

    //vec3 B = normalize(normalMatrix * a_Bitangent);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    mat3 TBN = transpose(mat3(T, B, N));
    v_TBN = TBN;
   
    gl_Position = Proj.ProjMat * View.ViewMat * Model.ModelMat * vec4(a_Position, 1.0);
}