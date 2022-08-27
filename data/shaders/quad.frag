#version 450

layout (binding = 1) uniform sampler2D samplerReflection;
layout (binding = 2) uniform sampler2D samplerRefraction;

layout(push_constant) uniform PushConsts {
	uint samplerIndex;
} pushConsts;

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outFragColor;

void main()
{
	switch(pushConsts.samplerIndex) {
		case 0: 
		outFragColor = texture(samplerReflection, inUV);
		break;
		case 1:
		outFragColor = texture(samplerRefraction, inUV);
		break;
	}
}