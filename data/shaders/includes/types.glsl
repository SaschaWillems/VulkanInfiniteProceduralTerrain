/*
 * Copyright (C) 2022 by Sascha Willems - www.saschawillems.de
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

struct UBOParams {
	uint shadows;
	uint fog;
	float waterAlpha;
	uint shadowPCF;
	vec4 fogColor;
	vec4 waterColor;
	vec4 grassColor;
	vec4 layers[6];
};

struct UBOShared {
	mat4 projection;
	mat4 modelview;
	vec4 lightDir;
	vec4 cameraPos;
	float time;
};

struct UBOShadowCascades {
	vec4 cascadeSplits;
	mat4 cascadeViewProjMat[SHADOW_MAP_CASCADE_COUNT];
	mat4 inverseViewMat;
	vec4 lightDir;
	mat4 biasMat;
};