/** 
 * @file lightShinyF.glsl
 *
 * Copyright (c) 2007-$CurrentYear$, Linden Research, Inc.
 * $License$
 */

uniform sampler2D diffuseMap;
uniform samplerCube environmentMap;

vec3 scaleSoftClip(vec3 light);
vec3 atmosLighting(vec3 light);
vec4 applyWaterFog(vec4 color);

void shiny_lighting()
{
	vec4 color = texture2D(diffuseMap, gl_TexCoord[0].xy);
	color.rgb *= gl_Color.rgb;
	
	vec3 envColor = textureCube(environmentMap, gl_TexCoord[1].xyz).rgb;	
	color.rgb = mix(color.rgb, envColor.rgb, gl_Color.a);

	color.rgb = atmosLighting(color.rgb);

	color.rgb = scaleSoftClip(color.rgb);
	color.a = max(color.a, gl_Color.a);
	gl_FragColor = color;
}

