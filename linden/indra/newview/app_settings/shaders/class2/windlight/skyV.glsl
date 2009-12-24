/** 
 * @file WLSkyV.glsl
 *
 * Copyright (c) 2005-$CurrentYear$, Linden Research, Inc.
 * $License$
 */

// SKY ////////////////////////////////////////////////////////////////////////
// The vertex shader for creating the atmospheric sky
///////////////////////////////////////////////////////////////////////////////

// Output parameters
varying vec4 vary_HazeColor;

// Inputs
uniform vec3 camPosLocal;

uniform vec4 lightnorm;
uniform vec4 sunlight_color;
uniform vec4 ambient;
uniform vec4 blue_horizon;
uniform vec4 blue_density;
uniform vec4 haze_horizon;
uniform vec4 haze_density;

uniform vec4 cloud_shadow;
uniform vec4 density_multiplier;
uniform vec4 max_y;

uniform vec4 glow;

uniform vec4 cloud_color;

uniform vec4 cloud_scale;

void main()
{

	// World / view / projection
	gl_Position = ftransform();
	gl_TexCoord[0] = gl_MultiTexCoord0;

	// Get relative position
	vec3 P = gl_Vertex.xyz - camPosLocal.xyz + vec3(0,50,0);
	//vec3 P = gl_Vertex.xyz + vec3(0,50,0);

	// Set altitude
	if (P.y > 0.)
	{
		P *= (max_y.x / P.y);
	}
	else
	{
		P *= (-32000. / P.y);
	}

	// Can normalize then
	vec3 Pn = normalize(P);
	float  Plen = length(P);

	// Initialize temp variables
	vec4 temp1 = vec4(0.);
	vec4 temp2 = vec4(0.);
	vec4 blue_weight;
	vec4 haze_weight;
	vec4 sunlight = sunlight_color;
	vec4 light_atten;


	// Sunlight attenuation effect (hue and brightness) due to atmosphere
	// this is used later for sunlight modulation at various altitudes
	light_atten = (blue_density * 1.0 + haze_density.x * 0.25) * (density_multiplier.x * max_y.x);

	// Calculate relative weights
	temp1 = blue_density + haze_density.x;
	blue_weight = blue_density / temp1;
	haze_weight = haze_density.x / temp1;

	// Compute sunlight from P & lightnorm (for long rays like sky)
	temp2.y = max(0., max(0., Pn.y) * 1.0 + lightnorm.y );
	temp2.y = 1. / temp2.y;
	sunlight *= exp( - light_atten * temp2.y);

	// Distance
	temp2.z = Plen * density_multiplier.x;

	// Transparency (-> temp1)
	// ATI Bugfix -- can't store temp1*temp2.z in a variable because the ati
	// compiler gets confused.
	temp1 = exp(-temp1 * temp2.z);


	// Compute haze glow
	temp2.x = dot(Pn, lightnorm.xyz);
	temp2.x = 1. - temp2.x;
		// temp2.x is 0 at the sun and increases away from sun
	temp2.x = max(temp2.x, .001);	
		// Set a minimum "angle" (smaller glow.y allows tighter, brighter hotspot)
	temp2.x *= glow.x;
		// Higher glow.x gives dimmer glow (because next step is 1 / "angle")
	temp2.x = pow(temp2.x, glow.z);
		// glow.z should be negative, so we're doing a sort of (1 / "angle") function

	// Add "minimum anti-solar illumination"
	temp2.x += .25;


	// Haze color above cloud
	vary_HazeColor = (	  blue_horizon * blue_weight * (sunlight + ambient)
				+ (haze_horizon.r * haze_weight) * (sunlight * temp2.x + ambient)
			 );	


	// Increase ambient when there are more clouds
	vec4 tmpAmbient = ambient;
	tmpAmbient += (1. - tmpAmbient) * cloud_shadow.x * 0.5; 

	// Dim sunlight by cloud shadow percentage
	sunlight *= (1. - cloud_shadow.x);

	// Haze color below cloud
	vec4 additiveColorBelowCloud = (	  blue_horizon * blue_weight * (sunlight + tmpAmbient)
				+ (haze_horizon.r * haze_weight) * (sunlight * temp2.x + tmpAmbient)
			 );	

	// Final atmosphere additive
	vary_HazeColor *= (1. - temp1);
	
	// Attenuate cloud color by atmosphere
	temp1 = sqrt(temp1);	//less atmos opacity (more transparency) below clouds

	// At horizon, blend high altitude sky color towards the darker color below the clouds
	vary_HazeColor += (additiveColorBelowCloud - vary_HazeColor) * (1. - sqrt(temp1));
	
	// won't compile on mac without this being set
	//vary_AtmosAttenuation = vec3(0.0,0.0,0.0);
}

