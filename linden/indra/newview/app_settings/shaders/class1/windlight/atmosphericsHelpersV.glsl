/** 
 * @file atmosphericsHelpersV.glsl 
 *
 * Copyright (c) 2005-$CurrentYear$, Linden Research, Inc.
 * $License$
 */

vec3 atmosAmbient(vec3 light)
{
	return gl_LightModel.ambient.rgb + light;
}

vec3 atmosAffectDirectionalLight(float lightIntensity)
{
	return gl_LightSource[0].diffuse.rgb * lightIntensity;
}

vec3 atmosGetDiffuseSunlightColor()
{
	return gl_LightSource[0].diffuse.rgb;
}

vec3 scaleDownLight(vec3 light)
{
	/* stub function for fallback compatibility on class1 hardware */
	return light;
}

vec3 scaleUpLight(vec3 light)
{
	/* stub function for fallback compatibility on class1 hardware */
	return light;
}

