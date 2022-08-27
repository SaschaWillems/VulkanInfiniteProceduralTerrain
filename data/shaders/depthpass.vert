#version 450
#extension GL_EXT_multiview : enable

layout (location = 0) in vec3 inPos;
layout (location = 2) in vec2 inUV;

// todo: pass via specialization constant
// @todo: move to include
#define SHADOW_MAP_CASCADE_COUNT 4

layout(push_constant) uniform PushConsts {
	vec4 position;
} pushConsts;

layout (binding = 0) uniform UBO {
	mat4[SHADOW_MAP_CASCADE_COUNT] cascadeViewProjMat;
} ubo;

layout (location = 0) out vec2 outUV;

void main()
{
	outUV = inUV;
	vec3 pos = inPos + pushConsts.position.xyz;
	gl_Position =  ubo.cascadeViewProjMat[gl_ViewIndex] * vec4(pos, 1.0);
}