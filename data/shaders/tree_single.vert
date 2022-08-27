/*
 * Copyright (C) 2022 by Sascha Willems - www.saschawillems.de
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#version 450 core
#extension GL_GOOGLE_include_directive : require

#include "includes/constants.glsl"
#include "includes/types.glsl"

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

// Instanced attributes
//layout (location = 3) in vec3 instancePos;
//layout (location = 4) in vec3 instanceScale;
//layout (location = 5) in vec3 instanceRotation;
//layout (location = 6) in vec2 instanceUV;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outViewVec;
layout (location = 3) out vec3 outLightVec;
layout (location = 4) out vec3 outViewPos;
layout (location = 5) out vec3 outPos;

layout (set = 0, binding = 0) uniform SharedBlock { UBOShared ubo; };

layout(push_constant) uniform PushConsts {
	mat4 scale;
	vec4 clipPlane;
	int _dummy;
	layout(offset = 96) vec3 pos;
} pushConsts;

void main(void)
{
	outUV = inUV;// + instanceUV;
	outNormal = inNormal;

	mat3 mx, my, mz;
	
	// rotate around x
//	float s = sin(instanceRotation.x);
//	float c = cos(instanceRotation.x);
//
//	mx[0] = vec3(c, s, 0.0);
//	mx[1] = vec3(-s, c, 0.0);
//	mx[2] = vec3(0.0, 0.0, 1.0);
//	
//	// rotate around y
//	s = sin(instanceRotation.y);
//	c = cos(instanceRotation.y);
//
//	my[0] = vec3(c, 0.0, s);
//	my[1] = vec3(0.0, 1.0, 0.0);
//	my[2] = vec3(-s, 0.0, c);
//	
//	// rot around z
//	s = sin(instanceRotation.z);
//	c = cos(instanceRotation.z);	
//	
//	mz[0] = vec3(1.0, 0.0, 0.0);
//	mz[1] = vec3(0.0, c, s);
//	mz[2] = vec3(0.0, -s, c);
//	
//	mat3 rotMat = mz * my * mx;


	vec4 pos = vec4(inPos, 1.0);
//	pos.xyz *= instanceScale;
	pos.xyz += pushConsts.pos;
	if (pushConsts.scale[1][1] < 0) {
		pos.y *= -1.0f;
	}

	gl_Position = ubo.projection * ubo.modelview * pos;

	outPos = pos.xyz;
	outViewVec = -pos.xyz;
	outLightVec = normalize(ubo.lightDir.xyz + outViewVec);
	outViewPos = (ubo.modelview * vec4(pos.xyz, 1.0)).xyz;

	outNormal = inNormal;

	// Clip against reflection plane
	if (length(pushConsts.clipPlane) != 0.0)  {
		gl_ClipDistance[0] = dot(pos, pushConsts.clipPlane);
	} else {
		gl_ClipDistance[0] = 0.0f;
	}

}
