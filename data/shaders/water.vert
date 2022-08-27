/*
 * Copyright (C) 2022 by Sascha Willems - www.saschawillems.de
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#version 450
#extension GL_GOOGLE_include_directive : require

#include "includes/constants.glsl"
#include "includes/types.glsl"

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

layout (set = 1, binding = 0) uniform SharedBlock { UBOShared ubo; };

layout(push_constant) uniform PushConsts {
	mat4 scale;
	vec4 clipPlane;
	uint shadows;
	layout(offset = 96) vec3 pos;
} pushConsts;

layout (location = 0) out vec2 outUV;
layout (location = 1) out vec4 outPos;
layout (location = 2) out vec3 outNormal;
layout (location = 3) out vec3 outEyePos;
layout (location = 5) out vec3 outViewPos;
layout (location = 6) out vec3 outLPos;

void main() 
{
	outUV = inUV * 8.0f;
	vec3 pos = inPos;
	pos.xz *= 24.1 / 2.0;
	pos.xyz += pushConsts.pos.xyz;
	outPos = ubo.projection * ubo.modelview * vec4(pos, 1.0);
	outLPos = pos;
	outNormal = inNormal;
	outEyePos = ubo.cameraPos.xyz - pos;
	outViewPos = (ubo.modelview * vec4(pos, 1.0)).xyz;
	gl_Position = outPos;		
}
