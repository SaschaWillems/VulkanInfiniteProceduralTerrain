/*
 * Copyright (C) 2022 by Sascha Willems - www.saschawillems.de
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

float textureProj(vec4 shadowCoord, vec2 offset, uint cascadeIndex, sampler2DArray shadowCascades)
{
	float shadow = 1.0;
	float bias = 0.005;
	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 ) {
		float dist = texture(shadowCascades, vec3(shadowCoord.st + offset, cascadeIndex)).r;
		if (shadowCoord.w > 0 && dist < shadowCoord.z - bias) {
			shadow = 0.5;
		}
	}
	return shadow;

}

float filterPCF(vec4 sc, uint cascadeIndex, sampler2DArray shadowCascades)
{
	ivec2 texDim = textureSize(shadowCascades, 0).xy;
	float scale = 0.75;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;
	
	for (int x = -range; x <= range; x++) {
		for (int y = -range; y <= range; y++) {
			shadowFactor += textureProj(sc, vec2(dx*x, dy*y), cascadeIndex, shadowCascades);
			count++;
		}
	}
	return shadowFactor / count;
}

float shadowMapping(vec4 dist, vec3 pos, sampler2DArray shadowCascades)
{
	// Get cascade index for the current fragment's view position
	uint cascadeIndex = 0;
	for(uint i = 0; i < SHADOW_MAP_CASCADE_COUNT - 1; ++i) {
		if(inViewPos.z < uboCSM.cascadeSplits[i]) {	
			cascadeIndex = i + 1;
		}
	}

	// Depth compare for shadowing
	vec4 shadowCoord = (uboCSM.biasMat * uboCSM.cascadeViewProjMat[cascadeIndex]) * vec4(pos.xyz + dist.xyz, 1.0);	

#define _PCF

#ifdef PCF
	return filterPCF(shadowCoord / shadowCoord.w, cascadeIndex, shadowCascades);
#else
	return textureProj(shadowCoord / shadowCoord.w, vec2(0.0), cascadeIndex, shadowCascades);
#endif
}