/** 
 * @file lightWaterF.glsl
 *
 * Copyright (c) 2007-$CurrentYear$, Linden Research, Inc.
 * $License$
 */

uniform sampler2D diffuseMap;

vec3 atmosLighting(vec3 light);
vec4 applyWaterFog(vec4 color);

void default_lighting_water()
{
	vec4 color = texture2D(diffuseMap, gl_TexCoord[0].xy) * gl_Color;

	color.rgb = atmosLighting(color.rgb);

	gl_FragColor = applyWaterFog(color);
}

