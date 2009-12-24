/** 
 * @file terrainF.glsl
 *
 * Copyright (c) 2007-$CurrentYear$, Linden Research, Inc.
 * $License$
 */

uniform sampler2D detail0;
uniform sampler2D detail1;
uniform sampler2D alphaRamp;

void main() 
{
	float a = texture2D(alphaRamp, gl_TexCoord[1].xy).a;
	vec3 color = mix(texture2D(detail1, gl_TexCoord[2].xy).rgb,
					 texture2D(detail0, gl_TexCoord[0].xy).rgb,
					 a);

	gl_FragColor.rgb = color;
	gl_FragColor.a = texture2D(alphaRamp, gl_TexCoord[3].xy).a;
}
