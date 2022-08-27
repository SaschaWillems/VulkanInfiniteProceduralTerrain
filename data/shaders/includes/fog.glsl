/*
 * Copyright (C) 2022 by Sascha Willems - www.saschawillems.de
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

const float fogDensity = 0.05;

vec3 applyFog(vec3 color)
{
	const float LOG2 = -1.442695;
	float dist = gl_FragCoord.z / gl_FragCoord.w * 0.1;
	float d = fogDensity * dist;
	float factor = 1.0 - clamp(exp2(d * d * LOG2), 0.0, 1.0);
	return mix(color, params.fogColor.rgb, factor);
}