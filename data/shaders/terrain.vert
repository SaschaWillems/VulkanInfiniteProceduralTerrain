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
layout (location = 3) in vec3 inColor;
layout (location = 4) in float inTerrainHeight;

layout (set = 1, binding = 0) uniform SharedBlock { UBOShared ubo; };

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outViewVec;
layout (location = 3) out vec3 outLightVec;
layout (location = 4) out vec3 outEyePos;
layout (location = 5) out vec3 outViewPos;
layout (location = 6) out vec3 outPos;
layout (location = 7) out float outTerrainHeight;

layout(push_constant) uniform PushConsts {
	mat4 scale;
	vec4 clipPlane;
	uint shadows;
	layout(offset = 96) vec3 pos;
} pushConsts;

void main(void)
{
	outUV = inUV;
	outNormal = inNormal;
	vec4 pos = vec4(inPos, 1.0);
	pos.xyz += pushConsts.pos;
	if (pushConsts.scale[1][1] < 0) {
		pos.y *= -1.0f;
	}
	gl_Position = ubo.projection * ubo.modelview * pos;
	outPos = pos.xyz;
	outViewVec = -pos.xyz;
	outLightVec = normalize(-ubo.lightDir.xyz/* + outViewVec*/);
	outEyePos = vec3(ubo.modelview * pos);
	outViewPos = (ubo.modelview * vec4(pos.xyz, 1.0)).xyz;
	outTerrainHeight = inTerrainHeight;

	// Clip against reflection plane
	if (length(pushConsts.clipPlane) != 0.0)  {
		gl_ClipDistance[0] = dot(pos, pushConsts.clipPlane);
	} else {
		gl_ClipDistance[0] = 0.0f;
	}

}