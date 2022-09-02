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
layout(location = 3) out vec4 v_FragPosLightSpace;
layout(location = 4) out vec3 v_DirLightPos;
layout(location = 5) out smooth mat3 v_TBN;

layout(set = 0, binding = 0) uniform ModelMatrix
{
    mat4 ModelMat;
} Model;

layout(set = 0, binding = 1) uniform ViewMatrix
{
    mat4 ViewMat;
} View;

layout(set = 0, binding = 2) uniform ProjectionMatrix
{
    mat4 ProjMat;
} Proj;

layout(set = 0, binding = 3) uniform depthMVP
{
    mat4 Matrix;
} lightMVP;

layout(set = 0, binding = 4) uniform DirectionalLightPosition
{
    vec4 pos;
} dirLightPos;


const mat4 bias = mat4( 
  0.5, 0.0, 0.0, 0.0,
  0.0, 0.5, 0.0, 0.0,
  0.0, 0.0, 1.0, 0.0,
  0.5, 0.5, 0.0, 1.0 );

void main()
{
    v_UV                = a_UV;
    v_Pos               = vec3(Model.ModelMat * vec4(a_Position, 1.0));
    v_Normal            = mat3(Model.ModelMat) * a_Normal;   

    mat3 normalMatrix = transpose(inverse(mat3(Model.ModelMat)));

    vec3 T              = normalize(normalMatrix * a_Tangent);
    vec3 N              = normalize(normalMatrix * a_Normal);
    vec3 B              = normalize(normalMatrix * a_Bitangent);

    //T                   = normalize(T - dot(T, N) * N);

    v_TBN               = transpose(mat3(T, B, N));

    v_FragPosLightSpace = bias * lightMVP.Matrix * Model.ModelMat * vec4(a_Position, 1.0);
    v_DirLightPos       = dirLightPos.pos.xyz;

    gl_Position = Proj.ProjMat * View.ViewMat * Model.ModelMat * vec4(a_Position, 1.0);
}