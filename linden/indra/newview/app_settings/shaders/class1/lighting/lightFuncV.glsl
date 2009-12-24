/** 
 * @file lightFuncV.glsl
 *
 * Copyright (c) 2007-$CurrentYear$, Linden Research, Inc.
 * $License$
 */


float calcDirectionalLight(vec3 n, vec3 l)
{
	float a = max(dot(n,l),0.0);
	return a;
}

float calcPointLight(vec3 v, vec3 n, vec4 lp, float la)
{
	//get light vector
	vec3 lv = lp.xyz-v;
	
	//get distance
	float d = length(lv);
	
	//normalize light vector
	lv *= 1.0/d;
	
	//distance attenuation
	float da = clamp(1.0/(la * d), 0.0, 1.0);
	
	//angular attenuation
	da *= calcDirectionalLight(n, lv);
	
	return da;	
}

