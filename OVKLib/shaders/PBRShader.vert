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
layout(location = 4) out mat4 v_ViewMatrix;
layout(location = 8) out smooth mat3 v_TBN;


layout(set = 0, binding = 0) uniform globalUBO
{
    mat4 viewMatrix;
    mat4 projMatrix;
    mat4 directionalLightMVP;
};

layout( push_constant ) uniform modelMat
{
	mat4 modelMatrix;
};

const mat4 bias = mat4( 
  0.5, 0.0, 0.0, 0.0,
  0.0, 0.5, 0.0, 0.0,
  0.0, 0.0, 1.0, 0.0,
  0.5, 0.5, 0.0, 1.0 );

void main()
{
    v_UV                        = a_UV;
    v_Pos                       = vec3(modelMatrix * vec4(a_Position, 1.0));
    v_Normal                    = mat3(modelMatrix) * a_Normal;   

    mat3 normalMatrix           = transpose(inverse(mat3(modelMatrix)));

    vec3 T                      = normalize(normalMatrix * a_Tangent);
    vec3 N                      = normalize(normalMatrix * a_Normal);
    vec3 B                      = normalize(normalMatrix * a_Bitangent);

    //T                         = normalize(T - dot(T, N) * N);

    v_TBN                       = transpose(mat3(T, B, N));

    v_FragPosLightSpace         = bias * directionalLightMVP * modelMatrix * vec4(a_Position, 1.0);

    gl_Position                 = projMatrix * viewMatrix * modelMatrix * vec4(a_Position, 1.0);
}