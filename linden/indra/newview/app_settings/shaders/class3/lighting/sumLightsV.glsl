/**
 * @file sumLightsV.glsl
 *
 * Copyright (c) 2005-$CurrentYear$, Linden Research, Inc.
 * $License$
 */

float calcDirectionalLight(vec3 n, vec3 l);
float calcPointLight(vec3 v, vec3 n, vec4 lp, float la);

vec3 atmosAmbient(vec3 light);
vec3 atmosAffectDirectionalLight(float lightIntensity);
vec3 scaleDownLight(vec3 light);
vec3 scaleUpLight(vec3 light);

vec4 sumLights(vec3 pos, vec3 norm, vec4 color, vec4 baseLight)
{
	vec4 col;
	col.a = color.a;
	
	// Add windlight lights
	col.rgb = atmosAffectDirectionalLight(calcDirectionalLight(norm, gl_LightSource[0].position.xyz));
	col.rgb += atmosAmbient(baseLight.rgb);
	col.rgb = scaleUpLight(col.rgb);

	// Collect normal lights (need to be divided by two, as we later multiply by 2)
	col.rgb += gl_LightSource[2].diffuse.rgb*calcPointLight(pos, norm, gl_LightSource[2].position, gl_LightSource[2].linearAttenuation);
	col.rgb += gl_LightSource[3].diffuse.rgb*calcPointLight(pos, norm, gl_LightSource[3].position, gl_LightSource[3].linearAttenuation);
	col.rgb += gl_LightSource[4].diffuse.rgb*calcPointLight(pos, norm, gl_LightSource[4].position, gl_LightSource[4].linearAttenuation);
	col.rgb += gl_LightSource[5].diffuse.rgb*calcPointLight(pos, norm, gl_LightSource[5].position, gl_LightSource[5].linearAttenuation);
 	col.rgb += gl_LightSource[6].diffuse.rgb*calcPointLight(pos, norm, gl_LightSource[6].position, gl_LightSource[6].linearAttenuation);
 	col.rgb += gl_LightSource[7].diffuse.rgb*calcPointLight(pos, norm, gl_LightSource[7].position, gl_LightSource[7].linearAttenuation);
	col.rgb += gl_LightSource[1].diffuse.rgb*calcDirectionalLight(norm, gl_LightSource[1].position.xyz);
	col.rgb = scaleDownLight(col.rgb);
				

	col.rgb = min(col.rgb*color.rgb, 1.0);
	
	return col;	
}

