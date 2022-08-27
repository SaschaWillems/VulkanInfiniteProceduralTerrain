/*
 * Copyright (C) 2022 by Sascha Willems - www.saschawillems.de
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#version 450
#extension GL_GOOGLE_include_directive : require

#include "includes/constants.glsl"
#include "includes/types.glsl"

layout (set = 0, binding = 0) uniform sampler2DArray samplerLayers;
layout (set = 0, binding = 1) uniform sampler2DArray shadowMap;

layout (set = 2, binding = 0) uniform ParamBlock { UBOParams params; };
layout (set = 3, binding = 0) uniform UBOCSM { UBOShadowCascades uboCSM; };

//layout (set = 4, binding = 0) uniform sampler2DArray shadowMap;

layout(push_constant) uniform PushConsts {
	mat4 scale;
	vec4 clipPlane;
	uint shadows;
	float alpha;
} pushConsts;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inViewVec;
layout (location = 3) in vec3 inLightVec;
layout (location = 4) in vec3 inEyePos;
layout (location = 5) in vec3 inViewPos;
layout (location = 6) in vec3 inPos;
layout (location = 7) in float inTerrainHeight;

layout (location = 0) out vec4 outFragColor;

#include "includes/fog.glsl"
#include "includes/shadow.glsl"

vec3 triPlanarBlend(vec3 worldNormal){
	vec3 blending = abs(worldNormal);
	blending = normalize(max(blending, 0.00001));
	float b = (blending.x + blending.y + blending.z);
	blending /= vec3(b, b, b);
	return blending;
}

vec3 sampleTerrainLayer()
{
	vec3 color = vec3(0.0);
	float texRepeat = 0.125f;
	vec3 blend = triPlanarBlend(inNormal);
	for (int i = 0; i < params.layers.length(); i++) {
		float start = params.layers[i].x - params.layers[i].y / 2.0;
		float end = params.layers[i].x + params.layers[i].y / 2.0;
		float range = end - start;
		float weight = (range - abs(inTerrainHeight - end)) / range;
		weight = max(0.0, weight);
		// Triplanar mapping
		vec3 xaxis = texture(samplerLayers, vec3(inPos.yz * texRepeat, i)).rgb;
		vec3 yaxis = texture(samplerLayers, vec3(inPos.xz * texRepeat, i)).rgb;
		vec3 zaxis = texture(samplerLayers, vec3(inPos.xy * texRepeat, i)).rgb;
		vec3 texColor = xaxis * blend.x + yaxis * blend.y + zaxis * blend.z;
		color += weight * texColor;
		
//		vec3 uv = vec3(inUV * 32.0, 1);
//		color += weight * texture(samplerLayers, uv).rgb;
	}
	return color;
}

void main()
{
	// Shadows
	float shadow = 1.0;
	if ((params.shadows > 0) && (length(pushConsts.clipPlane) == 0.0)) {
		shadow = shadowMapping(vec4(0.0), inPos, shadowMap);
	}

	// Directional light
	vec3 N = normalize(inNormal);
	vec3 L = normalize(inLightVec);
	float diffuse = dot(N, L);
	vec3 color = (ambient + (shadow * diffuse)) * sampleTerrainLayer();

//	if (params.fog == 1) {
		outFragColor = vec4(applyFog(color), 1.0);
//	} else {
//		outFragColor = vec4(color, 1.0);
//	}
//
	outFragColor.a = clamp(pushConsts.alpha, 0.0, 1.0);
}