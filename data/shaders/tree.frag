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

float CalcMipLevel(vec2 texture_coord)
{
    vec2 dx = dFdx(texture_coord);
    vec2 dy = dFdy(texture_coord);
    float delta_max_sqr = max(dot(dx, dx), dot(dy, dy));
                
    return max(0.0, 0.5 * log2(delta_max_sqr));
}

void main(void)
{
	vec4 colorMap = texture(samplerColorMap, inUV) * inColor;

	// Shadows
	float shadow = 1.0;
	if ((params.shadows > 0) && (length(pushConsts.clipPlane) == 0.0)) {
		shadow = shadowMapping(vec4(0.0), inPos, shadowMap);
	}

	// Lighting
	float amb = 0.2;
	vec3 N = normalize(inNormal);
	vec3 L = normalize(inLightVec);
	float diffuse = dot(N, L);
	vec3 color = (amb + diffuse) * shadow * colorMap.rgb; 
	
    color = colorMap.rgb * shadow * 0.75;

	// @todo: doesn't seem to work, check param block binding
//	if (params.fog == 1) {
		outFragColor = vec4(applyFog(color), colorMap.a);
//	} else {
//		outFragColor = vec4(color, colorMap.a);
//	}

	vec2 texSize = vec2(1.0) / textureSize(samplerColorMap, 0);
	float _Cutoff = 0.15;
	float _MipScale = 0.25;
    outFragColor.a *= 1 + max(0, CalcMipLevel(inUV * texSize.xy)) * _MipScale;
	outFragColor.a = (outFragColor.a - _Cutoff) / max(fwidth(outFragColor.a), 0.0001) + 0.5;

	outFragColor.a *= inColor.a;
}
