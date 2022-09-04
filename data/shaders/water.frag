/*
 * Copyright (C) 2022 by Sascha Willems - www.saschawillems.de
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#version 450
#extension GL_GOOGLE_include_directive : require

#include "includes/constants.glsl"
#include "includes/types.glsl"

layout (set = 0, binding = 0) uniform sampler2D samplerRefraction;
layout (set = 0, binding = 1) uniform sampler2D samplerReflection;
layout (set = 0, binding = 2) uniform sampler2D samplerRefractionDepth;
layout (set = 0, binding = 3) uniform sampler2D samplerWaterNormalMap;
layout (set = 0, binding = 4) uniform sampler2DArray shadowMap;

layout (set = 1, binding = 0) uniform SharedBlock { UBOShared ubo; };
layout (set = 2, binding = 0) uniform ParamBlock { UBOParams params; };
layout (set = 3, binding = 0) uniform UBOCSM { UBOShadowCascades uboCSM; };

layout(push_constant) uniform PushConsts {
	mat4 scale;
	vec4 clipPlane;
	uint shadows;
	float alpha;
} pushConsts;

layout (location = 0) in vec2 inUV;
layout (location = 1) in vec4 inPos;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec3 inEyePos;
layout (location = 5) in vec3 inViewPos;
layout (location = 6) in vec3 inLPos;

layout (location = 0) out vec4 outFragColor;

#include "includes/fog.glsl"
#include "includes/shadow.glsl"

float LinearizeDepth(float depth)
{
  float n = 0.5;
  float f = 1024.0;
  float z = depth;
  return (2.0 * n) / (f + n - z * (f - n));	
}

void main() 
{
	const vec4 tangent = vec4(1.0, 0.0, 0.0, 0.0);
	const vec4 viewNormal = vec4(0.0, -1.0, 0.0, 0.0);
	const vec4 bitangent = vec4(0.0, 0.0, 1.0, 0.0);
	const float distortAmount = 0.05;
	
	// Convert to clip space
//	vec4 projCoord = inPos / inPos.w;
	vec2 projCoord = (inPos.xy / inPos.w) / 2.0 + 0.5;

	// Scale and bias
//	projCoord += 1.0;
//	projCoord *= 0.5;
//
	vec4 normal = texture(samplerWaterNormalMap, inUV * 8.0 + ubo.time);
	normal = normalize(normal * 2.0 - 1.0);

	vec4 viewDir = normalize(vec4(inEyePos, 1.0));
	vec4 viewTanSpace = normalize(vec4(dot(viewDir, tangent), dot(viewDir, bitangent), dot(viewDir, viewNormal), 1.0));	
	vec4 viewReflection = normalize(reflect(-1.0 * viewTanSpace, normal));
	float fresnel = dot(normal, viewReflection);	

	vec4 dudv = normal * distortAmount;

	//projCoord.xy += dudv.st;

	//@todo: pass via uniform
	const float near = 0.5;
	const float far = 1024.0;

	vec4 color = params.waterColor;
	if (gl_FrontFacing) {
		float shadow = 1.0;
		if (params.shadows > 0) {
			shadow = shadowMapping(dudv, inLPos, shadowMap);
		}
		vec4 refraction = texture(samplerRefraction, projCoord.xy);
		vec4 reflection = texture(samplerReflection, projCoord.xy);
		float rdepth = texture(samplerRefractionDepth, projCoord.xy).r;
		// @todo: linear depth
		float floorDistance = LinearizeDepth(rdepth);

		float depth  = gl_FragCoord.z;
		float waterDistance = LinearizeDepth(depth);

		float waterDepth = floorDistance - waterDistance;

		//(2.0 * near) / (far + near - depth * (far - near)); 
		//floorDistance = 2.0 * near * far / (far + near - (2.0 * depth - 1.0) * (far - near));
		color = mix(refraction, reflection, fresnel) * shadow;
		//color.rgb = waterDepth.rrr * 50.0f;

		color.a = waterDepth;

	}

//	if (params.fog == 1) {
		outFragColor = vec4(applyFog(color.rgb), 1.0);
//	} else {
//		outFragColor = color;
//	}

	if (params.fog == 1) {
		outFragColor.a = clamp(color.a * params.waterAlpha * pushConsts.alpha, 0.0, 1.0);
	} else {
		outFragColor.a = clamp(pushConsts.alpha, 0.0, 1.0);
	}
}