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

layout (location = 0) out vec2 outUV;

layout (set = 1, binding = 0) uniform SharedBlock { UBOShared ubo; };

layout(push_constant) uniform PushConsts {
	mat4 scale;
	vec4 clipPlane;
	int _dummy;
} pushConsts;

void main(void)
{
	gl_Position = ubo.projection * mat4(mat3(ubo.modelview)) * pushConsts.scale * vec4(inPos.xyz, 1.0);
	outUV = inUV;
	outUV.t = 1.0f - outUV.t;
}
