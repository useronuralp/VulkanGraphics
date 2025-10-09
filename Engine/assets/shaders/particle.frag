#version 450

layout (binding = 1) uniform sampler2D samplerFire;

layout (location = 0) in vec4 inColor;
layout (location = 1) in float inAlpha;
layout (location = 2) in float inRotation;
layout (location = 3) in float inRowOffset;
layout (location = 4) in float inColumnOffset;
layout (location = 5) in float inRowCellSize;
layout (location = 6) in float inColumnCellSize;

layout (location = 0) out vec4 outFragColor;

layout( push_constant ) uniform bright
{
	vec4 brightness;
};

void main () 
{
	vec4 color;
	float alpha = (inAlpha <= 1.0) ? inAlpha : 2.0 - inAlpha;
	
	// Rotate texture coordinates
	// Rotate UV	
	float rotCenter = 0.5;
	float rotCos = cos(inRotation);
	float rotSin = sin(inRotation);
	vec2 rotUV = vec2(
		rotCos * (gl_PointCoord.x - rotCenter) + rotSin * (gl_PointCoord.y - rotCenter) + rotCenter,
		rotCos * (gl_PointCoord.y - rotCenter) - rotSin * (gl_PointCoord.x - rotCenter) + rotCenter);

	float mappedX = (gl_PointCoord.x * inRowCellSize) + (inRowOffset - inRowCellSize);
	float mappedY = (gl_PointCoord.y * inColumnCellSize) + (inColumnOffset - inColumnCellSize);
	vec2 UV = vec2(mappedX, mappedY);
	

	// Flame
	color = texture(samplerFire, UV);
	color.a *= inAlpha;
	color.x = color.x * brightness.x;
	color.y = color.y * brightness.x;
	color.z = color.z * brightness.x;

	
	outFragColor = color;
}