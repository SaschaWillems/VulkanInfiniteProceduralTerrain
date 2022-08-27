/*
 * Copyright (C) 2022 by Sascha Willems - www.saschawillems.de
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#version 450 core
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_multiview : enable

#include "includes/constants.glsl"

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

// Instanced attributes
layout (location = 3) in vec3 instancePos;
layout (location = 4) in vec3 instanceScale;
layout (location = 5) in vec3 instanceRotation;

layout (location = 0) out vec2 outUV;

layout(push_constant) uniform PushConsts {
	vec4 position;
	uint cascadeIndex;
} pushConsts;

layout (binding = 0) uniform UBO {
	mat4[SHADOW_MAP_CASCADE_COUNT] cascadeViewProjMat;
} ubo;

void main(void)
{
	outUV = inUV;

	mat3 mx, my, mz;
	
	// rotate around x
	float s = sin(instanceRotation.x);
	float c = cos(instanceRotation.x);

	mx[0] = vec3(c, s, 0.0);
	mx[1] = vec3(-s, c, 0.0);
	mx[2] = vec3(0.0, 0.0, 1.0);
	
	// rotate around y
	s = sin(instanceRotation.y);
	c = cos(instanceRotation.y);

	my[0] = vec3(c, 0.0, s);
	my[1] = vec3(0.0, 1.0, 0.0);
	my[2] = vec3(-s, 0.0, c);
	
	// rot around z
	s = sin(instanceRotation.z);
	c = cos(instanceRotation.z);	
	
	mz[0] = vec3(1.0, 0.0, 0.0);
	mz[1] = vec3(0.0, c, s);
	mz[2] = vec3(0.0, -s, c);
	
	mat3 rotMat = mz * my * mx;

	vec4 pos = vec4(inPos * rotMat, 1.0);
	pos.xyz *= instanceScale;
	pos.xyz += instancePos + pushConsts.position.xyz;

	gl_Position = ubo.cascadeViewProjMat[gl_ViewIndex] * pos;
}
