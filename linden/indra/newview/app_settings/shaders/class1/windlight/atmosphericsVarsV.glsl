/** 
 * @file atmosphericVarsV.glsl
 *
 * Copyright (c) 2007-$CurrentYear$, Linden Research, Inc.
 * $License$
 */

varying vec3 vary_PositionEye;


vec3 getPositionEye()
{
	return vary_PositionEye;
}

void setPositionEye(vec3 v)
{
	vary_PositionEye = v;
}
