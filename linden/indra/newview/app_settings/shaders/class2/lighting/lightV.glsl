/** 
 * @file lightV.glsl
 *
 * Copyright (c) 2007-$CurrentYear$, Linden Research, Inc.
 * $License$
 */

// All lights, no specular highlights

vec4 sumLights(vec3 pos, vec3 norm, vec4 color, vec4 baseLight);

vec4 calcLighting(vec3 pos, vec3 norm, vec4 color, vec4 baseLight)
{
	return sumLights(pos, norm, color, baseLight);
}

