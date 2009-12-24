/** 
 * @file atmosphericVars.glsl
 *
 * Copyright (c) 2007-$CurrentYear$, Linden Research, Inc.
 * $License$
 */

varying vec3 vary_PositionEye;

varying vec3 vary_SunlitColor;
varying vec3 vary_AmblitColor;
varying vec3 vary_AdditiveColor;
varying vec3 vary_AtmosAttenuation;

vec3 getPositionEye()
{
	return vary_PositionEye;
}
vec3 getSunlitColor()
{
	return vary_SunlitColor;
}
vec3 getAmblitColor()
{
	return vary_AmblitColor;
}
vec3 getAdditiveColor()
{
	return vary_AdditiveColor;
}
vec3 getAtmosAttenuation()
{
	return vary_AtmosAttenuation;
}
