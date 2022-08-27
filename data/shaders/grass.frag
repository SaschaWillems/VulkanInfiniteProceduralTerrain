/*
 * Copyright (C) 2022 by Sascha Willems - www.saschawillems.de
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#version 450 core
#extension GL_GOOGLE_include_directive : require

#include "includes/constants.glsl"
#include "includes/types.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inViewVec;
layout (location = 3) in vec3 inLightVec;
layout (location = 4) in vec3 inViewPos;
layout (location = 5) in vec3 inPos;
layout (location = 6) in vec4 inColor;

layout (set = 0, binding = 0) uniform SharedBlock { UBOShared ubo; };

layout (set = 1, binding = 0) uniform sampler2D samplerColorMap;

layout (set = 2, binding = 0) uniform ParamBlock { UBOParams params; };

layout (set = 3, binding = 0) uniform sampler2DArray shadowMap;
layout (set = 4, binding = 0) uniform UBOCSM { UBOShadowCascades uboCSM; };

layout(push_constant) uniform PushConsts {
	mat4 scale;
	vec4 clipPlane;
	uint shadows;
	float alpha;
} pushConsts;

layout (location = 0) out vec4 outFragColor;

#include "includes/fog.glsl"
#include "includes/shadow.glsl"

void main(void)
{
	vec4 colorMap = texture(samplerColorMap, inUV) * vec4(inColor.rgb, 1.0);

	// Shadows
	float shadow = 1.0;
	if ((params.shadows > 0) && (length(pushConsts.clipPlane) == 0.0)) {
		shadow = shadowMapping(vec4(0.0), inPos, shadowMap);
	}

	// Lighting
	float amb = 0.5;
//	vec3 N = normalize(inNormal);
//	vec3 L = normalize(inLightVec);
//	vec3 V = normalize(inViewVec);
//	vec3 R = reflect(-L, N);
//	vec3 diffuse = max(dot(N, L), amb).rrr;
//	const float specular = 0.0f; //pow(max(dot(R, V), 0.0), 32.0);
//
//	vec3 color = (ambient + (shadow * diffuse)) * colorMap.rgb * gColor;

	vec3 color = colorMap.rgb * shadow * params.grassColor.rgb;

//	vec3 color = vec3(diffuse * colorMap.rgb + specular);
//	if (params.fog == 1) {
		outFragColor = vec4(applyFog(color), colorMap.a);
//	} else {
		//outFragColor = vec4(color, colorMap.a);
//	}

//	outFragColor.a *= clamp(pushConsts.alpha, 0.0, 1.0);

	outFragColor.a *= inColor.a;
//	outFragColor.a = 1.0;
//	outFragColor.rgb = inColor.aaa;
}
