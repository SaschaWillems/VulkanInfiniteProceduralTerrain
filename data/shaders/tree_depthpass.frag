#version 450

layout (location = 0) in vec2 inUV;

layout (set = 1, binding = 0) uniform sampler2D samplerColorMap;

void main() 
{
	float a = textureLod(samplerColorMap, inUV, 0.0).a;
	if (a < 0.1 /*material.alphaMaskCutoff*/) {
		discard;
	}
}