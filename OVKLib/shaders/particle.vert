#version 450

layout (location = 0) in vec4 inPos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in float inAlpha;
layout (location = 3) in float inSize;
layout (location = 4) in float inRotation;
layout (location = 5) in float inRowOffset;
layout (location = 6) in float inColumnOffset;
layout (location = 7) in float inRowCellSize;
layout (location = 8) in float inColumnCellSize;

layout (location = 0) out vec4 outColor;
layout (location = 1) out float outAlpha;
layout (location = 2) out float outRotation;
layout (location = 3) out float outRowOffset;
layout (location = 4) out float outColumnOffset;
layout (location = 5) out float outRowCellSize;
layout (location = 6) out float outColumnCellSize;


layout(set = 0, binding = 0) uniform UBO
{
	// Most of these members are not being used by this shader since it is shared buffer. Consider optimizing.
    mat4 viewMat;
    mat4 projMat;
    mat4 lightMVP;
    vec4 dirLightPosition;
    vec4 cameraPos;
	vec4 viewportDimension;
};

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
};

void main () 
{
	outColor = inColor;
	outAlpha = inAlpha;
	outRotation = inRotation;
	outRowOffset = inRowOffset;
	outColumnOffset = inColumnOffset;
	outRowCellSize = inRowCellSize;
	outColumnCellSize = inColumnCellSize;
	  
	gl_Position = projMat * viewMat * vec4(inPos.xyz, 1.0);	
	
	// Base size of the point sprites
	float spriteSize = 0.02 * inSize;

	// Scale particle size depending on camera projection
	vec4 eyePos = viewMat * vec4(inPos.xyz, 1.0);
	vec4 projectedCorner = projMat * vec4(0.5 * spriteSize, 0.5 * spriteSize, eyePos.z, eyePos.w);
	gl_PointSize = viewportDimension.x * projectedCorner.x / projectedCorner.w;	
}