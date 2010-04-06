/** 
 * @file llvolume.cpp
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "linden_common.h"
#include "llmath.h"

#include <set>

#include "llerror.h"
#include "llmemtype.h"

#include "llvolumemgr.h"
#include "v2math.h"
#include "v3math.h"
#include "v4math.h"
#include "m4math.h"
#include "m3math.h"
#include "lldarray.h"
#include "llvolume.h"
#include "llstl.h"

#define DEBUG_SILHOUETTE_BINORMALS 0
#define DEBUG_SILHOUETTE_NORMALS 0 // TomY: Use this to display normals using the silhouette
#define DEBUG_SILHOUETTE_EDGE_MAP 0 // DaveP: Use this to display edge map using the silhouette

const F32 CUT_MIN = 0.f;
const F32 CUT_MAX = 1.f;
const F32 MIN_CUT_DELTA = 0.02f;

const F32 HOLLOW_MIN = 0.f;
const F32 HOLLOW_MAX = 0.95f;
const F32 HOLLOW_MAX_SQUARE	= 0.7f;

const F32 TWIST_MIN = -1.f;
const F32 TWIST_MAX =  1.f;

const F32 RATIO_MIN = 0.f;
const F32 RATIO_MAX = 2.f; // Tom Y: Inverted sense here: 0 = top taper, 2 = bottom taper

const F32 HOLE_X_MIN= 0.05f;
const F32 HOLE_X_MAX= 1.0f;

const F32 HOLE_Y_MIN= 0.05f;
const F32 HOLE_Y_MAX= 0.5f;

const F32 SHEAR_MIN = -0.5f;
const F32 SHEAR_MAX =  0.5f;

const F32 REV_MIN = 1.f;
const F32 REV_MAX = 4.f;

const F32 TAPER_MIN = -1.f;
const F32 TAPER_MAX =  1.f;

const F32 SKEW_MIN	= -0.95f;
const F32 SKEW_MAX	=  0.95f;

const F32 SCULPT_MIN_AREA = 0.002f;

BOOL check_same_clock_dir( const LLVector3& pt1, const LLVector3& pt2, const LLVector3& pt3, const LLVector3& norm)
{    
	LLVector3 test = (pt2-pt1)%(pt3-pt2);

	//answer
	if(test * norm < 0) 
	{
		return FALSE;
	}
	else 
	{
		return TRUE;
	}
} 

BOOL LLLineSegmentBoxIntersect(const LLVector3& start, const LLVector3& end, const LLVector3& center, const LLVector3& size)
{
	float fAWdU[3];
	LLVector3 dir;
	LLVector3 diff;

	for (U32 i = 0; i < 3; i++)
	{
		dir.mV[i] = 0.5f * (end.mV[i] - start.mV[i]);
		diff.mV[i] = (0.5f * (end.mV[i] + start.mV[i])) - center.mV[i];
		fAWdU[i] = fabsf(dir.mV[i]);
		if(fabsf(diff.mV[i])>size.mV[i] + fAWdU[i]) return false;
	}

	float f;
	f = dir.mV[1] * diff.mV[2] - dir.mV[2] * diff.mV[1];    if(fabsf(f)>size.mV[1]*fAWdU[2] + size.mV[2]*fAWdU[1])  return false;
	f = dir.mV[2] * diff.mV[0] - dir.mV[0] * diff.mV[2];    if(fabsf(f)>size.mV[0]*fAWdU[2] + size.mV[2]*fAWdU[0])  return false;
	f = dir.mV[0] * diff.mV[1] - dir.mV[1] * diff.mV[0];    if(fabsf(f)>size.mV[0]*fAWdU[1] + size.mV[1]*fAWdU[0])  return false;
	
	return true;
}


// intersect test between triangle vert0, vert1, vert2 and a ray from orig in direction dir.
// returns TRUE if intersecting and returns barycentric coordinates in intersection_a, intersection_b,
// and returns the intersection point along dir in intersection_t.

// Moller-Trumbore algorithm
BOOL LLTriangleRayIntersect(const LLVector3& vert0, const LLVector3& vert1, const LLVector3& vert2, const LLVector3& orig, const LLVector3& dir,
							F32* intersection_a, F32* intersection_b, F32* intersection_t, BOOL two_sided)
{
	F32 u, v, t;
	
	/* find vectors for two edges sharing vert0 */
	LLVector3 edge1 = vert1 - vert0;
	
	LLVector3 edge2 = vert2 - vert0;;

	/* begin calculating determinant - also used to calculate U parameter */
	LLVector3 pvec = dir % edge2;
	
	/* if determinant is near zero, ray lies in plane of triangle */
	F32 det = edge1 * pvec;

	if (!two_sided)
	{
		if (det < F_APPROXIMATELY_ZERO)
		{
			return FALSE;
		}

		/* calculate distance from vert0 to ray origin */
		LLVector3 tvec = orig - vert0;

		/* calculate U parameter and test bounds */
		u = tvec * pvec;	

		if (u < 0.f || u > det)
		{
			return FALSE;
		}
	
		/* prepare to test V parameter */
		LLVector3 qvec = tvec % edge1;
		
		/* calculate V parameter and test bounds */
		v = dir * qvec;
		if (v < 0.f || u + v > det)
		{
			return FALSE;
		}

		/* calculate t, scale parameters, ray intersects triangle */
		t = edge2 * qvec;
		F32 inv_det = 1.0 / det;
		t *= inv_det;
		u *= inv_det;
		v *= inv_det;
	}
	
	else // two sided
			{
		if (det > -F_APPROXIMATELY_ZERO && det < F_APPROXIMATELY_ZERO)
				{
			return FALSE;
				}
		F32 inv_det = 1.0 / det;

		/* calculate distance from vert0 to ray origin */
		LLVector3 tvec = orig - vert0;
		
		/* calculate U parameter and test bounds */
		u = (tvec * pvec) * inv_det;
		if (u < 0.f || u > 1.f)
		{
			return FALSE;
			}

		/* prepare to test V parameter */
		LLVector3 qvec = tvec - edge1;
		
		/* calculate V parameter and test bounds */
		v = (dir * qvec) * inv_det;
		
		if (v < 0.f || u + v > 1.f)
		{
			return FALSE;
		}

		/* calculate t, ray intersects triangle */
		t = (edge2 * qvec) * inv_det;
	}
	
	if (intersection_a != NULL)
		*intersection_a = u;
	if (intersection_b != NULL)
		*intersection_b = v;
	if (intersection_t != NULL)
		*intersection_t = t;
	
	
	return TRUE;
} 


//-------------------------------------------------------------------
// statics
//-------------------------------------------------------------------


//----------------------------------------------------

LLProfile::Face* LLProfile::addCap(S16 faceID)
{
	LLMemType m1(LLMemType::MTYPE_VOLUME);
	
	Face *face   = vector_append(mFaces, 1);
	
	face->mIndex = 0;
	face->mCount = mTotal;
	face->mScaleU= 1.0f;
	face->mCap   = TRUE;
	face->mFaceID = faceID;
	return face;
}

LLProfile::Face* LLProfile::addFace(S32 i, S32 count, F32 scaleU, S16 faceID, BOOL flat)
{
	LLMemType m1(LLMemType::MTYPE_VOLUME);
	
	Face *face   = vector_append(mFaces, 1);
	
	face->mIndex = i;
	face->mCount = count;
	face->mScaleU= scaleU;

	face->mFlat = flat;
	face->mCap   = FALSE;
	face->mFaceID = faceID;
	return face;
}

// What is the bevel parameter used for? - DJS 04/05/02
// Bevel parameter is currently unused but presumedly would support
// filleted and chamfered corners
void LLProfile::genNGon(const LLProfileParams& params, S32 sides, F32 offset, F32 bevel, F32 ang_scale, S32 split)
{
	LLMemType m1(LLMemType::MTYPE_VOLUME);
	
	// Generate an n-sided "circular" path.
	// 0 is (1,0), and we go counter-clockwise along a circular path from there.
	const F32 tableScale[] = { 1, 1, 1, 0.5f, 0.707107f, 0.53f, 0.525f, 0.5f };
	F32 scale = 0.5f;
	F32 t, t_step, t_first, t_fraction, ang, ang_step;
	LLVector3 pt1,pt2;

	F32 begin  = params.getBegin();
	F32 end    = params.getEnd();

	t_step = 1.0f / sides;
	ang_step = 2.0f*F_PI*t_step*ang_scale;

	// Scale to have size "match" scale.  Compensates to get object to generally fill bounding box.

	S32 total_sides = llround(sides / ang_scale);	// Total number of sides all around

	if (total_sides < 8)
	{
		scale = tableScale[total_sides];
	}

	t_first = floor(begin * sides) / (F32)sides;

	// pt1 is the first point on the fractional face.
	// Starting t and ang values for the first face
	t = t_first;
	ang = 2.0f*F_PI*(t*ang_scale + offset);
	pt1.setVec(cos(ang)*scale,sin(ang)*scale, t);

	// Increment to the next point.
	// pt2 is the end point on the fractional face
	t += t_step;
	ang += ang_step;
	pt2.setVec(cos(ang)*scale,sin(ang)*scale,t);

	t_fraction = (begin - t_first)*sides;

	// Only use if it's not almost exactly on an edge.
	if (t_fraction < 0.9999f)
	{
		LLVector3 new_pt = lerp(pt1, pt2, t_fraction);
		mProfile.push_back(new_pt);
	}

	// There's lots of potential here for floating point error to generate unneeded extra points - DJS 04/05/02
	while (t < end)
	{
		// Iterate through all the integer steps of t.
		pt1.setVec(cos(ang)*scale,sin(ang)*scale,t);

		if (mProfile.size() > 0) {
			LLVector3 p = mProfile[mProfile.size()-1];
			for (S32 i = 0; i < split && mProfile.size() > 0; i++) {
				mProfile.push_back(p+(pt1-p) * 1.0f/(float)(split+1) * (float)(i+1));
			}
		}
		mProfile.push_back(pt1);

		t += t_step;
		ang += ang_step;
	}

	t_fraction = (end - (t - t_step))*sides;

	// pt1 is the first point on the fractional face
	// pt2 is the end point on the fractional face
	pt2.setVec(cos(ang)*scale,sin(ang)*scale,t);

	// Find the fraction that we need to add to the end point.
	t_fraction = (end - (t - t_step))*sides;
	if (t_fraction > 0.0001f)
	{
		LLVector3 new_pt = lerp(pt1, pt2, t_fraction);
		
		if (mProfile.size() > 0) {
			LLVector3 p = mProfile[mProfile.size()-1];
			for (S32 i = 0; i < split && mProfile.size() > 0; i++) {
				mProfile.push_back(p+(new_pt-p) * 1.0f/(float)(split+1) * (float)(i+1));
			}
		}
		mProfile.push_back(new_pt);
	}

	// If we're sliced, the profile is open.
	if ((end - begin)*ang_scale < 0.99f)
	{
		if ((end - begin)*ang_scale > 0.5f)
		{
			mConcave = TRUE;
		}
		else
		{
			mConcave = FALSE;
		}
		mOpen = TRUE;
		if (params.getHollow() <= 0)
		{
			// put center point if not hollow.
			mProfile.push_back(LLVector3(0,0,0));
		}
	}
	else
	{
		// The profile isn't open.
		mOpen = FALSE;
		mConcave = FALSE;
	}

	mTotal = mProfile.size();
}

void LLProfile::genNormals(const LLProfileParams& params)
{
	S32 count = mProfile.size();

	S32 outer_count;
	if (mTotalOut)
	{
		outer_count = mTotalOut;
	}
	else
	{
		outer_count = mTotal / 2;
	}

	mEdgeNormals.resize(count * 2);
	mEdgeCenters.resize(count * 2);
	mNormals.resize(count);

	LLVector2 pt0,pt1;

	BOOL hollow = (params.getHollow() > 0);

	S32 i0, i1, i2, i3, i4;

	// Parametrically generate normal
	for (i2 = 0; i2 < count; i2++)
	{
		mNormals[i2].mV[0] = mProfile[i2].mV[0];
		mNormals[i2].mV[1] = mProfile[i2].mV[1];
		if (hollow && (i2 >= outer_count))
		{
			mNormals[i2] *= -1.f;
		}
		if (mNormals[i2].magVec() < 0.001)
		{
			// Special case for point at center, get adjacent points.
			i1 = (i2 - 1) >= 0 ? i2 - 1 : count - 1;
			i0 = (i1 - 1) >= 0 ? i1 - 1 : count - 1;
			i3 = (i2 + 1) < count ? i2 + 1 : 0;
			i4 = (i3 + 1) < count ? i3 + 1 : 0;

			pt0.setVec(mProfile[i1].mV[VX] + mProfile[i1].mV[VX] - mProfile[i0].mV[VX], 
				mProfile[i1].mV[VY] + mProfile[i1].mV[VY] - mProfile[i0].mV[VY]);
			pt1.setVec(mProfile[i3].mV[VX] + mProfile[i3].mV[VX] - mProfile[i4].mV[VX], 
				mProfile[i3].mV[VY] + mProfile[i3].mV[VY] - mProfile[i4].mV[VY]);

			mNormals[i2] = pt0 + pt1;
			mNormals[i2] *= 0.5f;
		}
		mNormals[i2].normVec();
	}

	S32 num_normal_sets = isConcave() ? 2 : 1;
	for (S32 normal_set = 0; normal_set < num_normal_sets; normal_set++)
	{
		S32 point_num;
		for (point_num = 0; point_num < mTotal; point_num++)
		{
			LLVector3 point_1 = mProfile[point_num];
			point_1.mV[VZ] = 0.f;

			LLVector3 point_2;
			
			if (isConcave() && normal_set == 0 && point_num == (mTotal - 1) / 2)
			{
				point_2 = mProfile[mTotal - 1];
			}
			else if (isConcave() && normal_set == 1 && point_num == mTotal - 1)
			{
				point_2 = mProfile[(mTotal - 1) / 2];
			}
			else
			{
				LLVector3 delta_pos;
				S32 neighbor_point = (point_num + 1) % mTotal;
				while(delta_pos.magVecSquared() < 0.01f * 0.01f)
				{
					point_2 = mProfile[neighbor_point];
					delta_pos = point_2 - point_1;
					neighbor_point = (neighbor_point + 1) % mTotal;
					if (neighbor_point == point_num)
					{
						break;
					}
				}
			}

			point_2.mV[VZ] = 0.f;
			LLVector3 face_normal = (point_2 - point_1) % LLVector3::z_axis;
			face_normal.normVec();
			mEdgeNormals[normal_set * count + point_num] = face_normal;
			mEdgeCenters[normal_set * count + point_num] = lerp(point_1, point_2, 0.5f);
		}
	}
}


// Hollow is percent of the original bounding box, not of this particular
// profile's geometry.  Thus, a swept triangle needs lower hollow values than
// a swept square.
LLProfile::Face* LLProfile::addHole(const LLProfileParams& params, BOOL flat, F32 sides, F32 offset, F32 box_hollow, F32 ang_scale, S32 split)
{
	// Note that addHole will NOT work for non-"circular" profiles, if we ever decide to use them.

	// Total add has number of vertices on outside.
	mTotalOut = mTotal;

	// Why is the "bevel" parameter -1? DJS 04/05/02
	genNGon(params, llfloor(sides),offset,-1, ang_scale, split);

	Face *face = addFace(mTotalOut, mTotal-mTotalOut,0,LL_FACE_INNER_SIDE, flat);

	std::vector<LLVector3> pt;
	pt.resize(mTotal) ;

	for (S32 i=mTotalOut;i<mTotal;i++)
	{
		pt[i] = mProfile[i] * box_hollow;
	}

	S32 j=mTotal-1;
	for (S32 i=mTotalOut;i<mTotal;i++)
	{
		mProfile[i] = pt[j--];
	}

	for (S32 i=0;i<(S32)mFaces.size();i++) 
	{
		if (mFaces[i].mCap)
		{
			mFaces[i].mCount *= 2;
		}
	}

	return face;
}



BOOL LLProfile::generate(const LLProfileParams& params, BOOL path_open,F32 detail, S32 split,
						 BOOL is_sculpted, S32 sculpt_size)
{
	LLMemType m1(LLMemType::MTYPE_VOLUME);
	
	if ((!mDirty) && (!is_sculpted))
	{
		return FALSE;
	}
	mDirty = FALSE;

	if (detail < MIN_LOD)
	{
		llinfos << "Generating profile with LOD < MIN_LOD.  CLAMPING" << llendl;
		detail = MIN_LOD;
	}

	mProfile.clear();
	mFaces.clear();

	// Generate the face data
	S32 i;
	F32 begin = params.getBegin();
	F32 end = params.getEnd();
	F32 hollow = params.getHollow();

	// Quick validation to eliminate some server crashes.
	if (begin > end - 0.01f)
	{
		llwarns << "LLProfile::generate() assertion failed (begin >= end)" << llendl;
		return FALSE;
	}

	S32 face_num = 0;

	switch (params.getCurveType() & LL_PCODE_PROFILE_MASK)
	{
	case LL_PCODE_PROFILE_SQUARE:
		{
			genNGon(params, 4,-0.375, 0, 1, split);
			if (path_open)
			{
				addCap (LL_FACE_PATH_BEGIN);
			}

			for (i = llfloor(begin * 4.f); i < llfloor(end * 4.f + .999f); i++)
			{
				addFace((face_num++) * (split +1), split+2, 1, LL_FACE_OUTER_SIDE_0 << i, TRUE);
			}

			for (i = 0; i <(S32) mProfile.size(); i++)
			{
				// Scale by 4 to generate proper tex coords.
				mProfile[i].mV[2] *= 4.f;
			}

			if (hollow)
			{
				switch (params.getCurveType() & LL_PCODE_HOLE_MASK)
				{
				case LL_PCODE_HOLE_TRIANGLE:
					// This offset is not correct, but we can't change it now... DK 11/17/04
				  	addHole(params, TRUE, 3, -0.375f, hollow, 1.f, split);
					break;
				case LL_PCODE_HOLE_CIRCLE:
					// TODO: Compute actual detail levels for cubes
				  	addHole(params, FALSE, MIN_DETAIL_FACES * detail, -0.375f, hollow, 1.f);
					break;
				case LL_PCODE_HOLE_SAME:
				case LL_PCODE_HOLE_SQUARE:
				default:
					addHole(params, TRUE, 4, -0.375f, hollow, 1.f, split);
					break;
				}
			}
			
			if (path_open) {
				mFaces[0].mCount = mTotal;
			}
		}
		break;
	case  LL_PCODE_PROFILE_ISOTRI:
	case  LL_PCODE_PROFILE_RIGHTTRI:
	case  LL_PCODE_PROFILE_EQUALTRI:
		{
			genNGon(params, 3,0, 0, 1, split);
			for (i = 0; i <(S32) mProfile.size(); i++)
			{
				// Scale by 3 to generate proper tex coords.
				mProfile[i].mV[2] *= 3.f;
			}

			if (path_open)
			{
				addCap(LL_FACE_PATH_BEGIN);
			}

			for (i = llfloor(begin * 3.f); i < llfloor(end * 3.f + .999f); i++)
			{
				addFace((face_num++) * (split +1), split+2, 1, LL_FACE_OUTER_SIDE_0 << i, TRUE);
			}
			if (hollow)
			{
				// Swept triangles need smaller hollowness values,
				// because the triangle doesn't fill the bounding box.
				F32 triangle_hollow = hollow / 2.f;

				switch (params.getCurveType() & LL_PCODE_HOLE_MASK)
				{
				case LL_PCODE_HOLE_CIRCLE:
					// TODO: Actually generate level of detail for triangles
					addHole(params, FALSE, MIN_DETAIL_FACES * detail, 0, triangle_hollow, 1.f);
					break;
				case LL_PCODE_HOLE_SQUARE:
					addHole(params, TRUE, 4, 0, triangle_hollow, 1.f, split);
					break;
				case LL_PCODE_HOLE_SAME:
				case LL_PCODE_HOLE_TRIANGLE:
				default:
					addHole(params, TRUE, 3, 0, triangle_hollow, 1.f, split);
					break;
				}
			}
		}
		break;
	case LL_PCODE_PROFILE_CIRCLE:
		{
			// If this has a square hollow, we should adjust the
			// number of faces a bit so that the geometry lines up.
			U8 hole_type=0;
			F32 circle_detail = MIN_DETAIL_FACES * detail;
			if (hollow)
			{
				hole_type = params.getCurveType() & LL_PCODE_HOLE_MASK;
				if (hole_type == LL_PCODE_HOLE_SQUARE)
				{
					// Snap to the next multiple of four sides,
					// so that corners line up.
					circle_detail = llceil(circle_detail / 4.0f) * 4.0f;
				}
			}

			S32 sides = (S32)circle_detail;

			if (is_sculpted)
				sides = sculpt_size;
			
			genNGon(params, sides);
			
			if (path_open)
			{
				addCap (LL_FACE_PATH_BEGIN);
			}

			if (mOpen && !hollow)
			{
				addFace(0,mTotal-1,0,LL_FACE_OUTER_SIDE_0, FALSE);
			}
			else
			{
				addFace(0,mTotal,0,LL_FACE_OUTER_SIDE_0, FALSE);
			}

			if (hollow)
			{
				switch (hole_type)
				{
				case LL_PCODE_HOLE_SQUARE:
					addHole(params, TRUE, 4, 0, hollow, 1.f, split);
					break;
				case LL_PCODE_HOLE_TRIANGLE:
					addHole(params, TRUE, 3, 0, hollow, 1.f, split);
					break;
				case LL_PCODE_HOLE_CIRCLE:
				case LL_PCODE_HOLE_SAME:
				default:
					addHole(params, FALSE, circle_detail, 0, hollow, 1.f);
					break;
				}
			}
		}
		break;
	case LL_PCODE_PROFILE_CIRCLE_HALF:
		{
			// If this has a square hollow, we should adjust the
			// number of faces a bit so that the geometry lines up.
			U8 hole_type=0;
			// Number of faces is cut in half because it's only a half-circle.
			F32 circle_detail = MIN_DETAIL_FACES * detail * 0.5f;
			if (hollow)
			{
				hole_type = params.getCurveType() & LL_PCODE_HOLE_MASK;
				if (hole_type == LL_PCODE_HOLE_SQUARE)
				{
					// Snap to the next multiple of four sides (div 2),
					// so that corners line up.
					circle_detail = llceil(circle_detail / 2.0f) * 2.0f;
				}
			}
			genNGon(params, llfloor(circle_detail), 0.5f, 0.f, 0.5f);
			if (path_open)
			{
				addCap(LL_FACE_PATH_BEGIN);
			}
			if (mOpen && !params.getHollow())
			{
				addFace(0,mTotal-1,0,LL_FACE_OUTER_SIDE_0, FALSE);
			}
			else
			{
				addFace(0,mTotal,0,LL_FACE_OUTER_SIDE_0, FALSE);
			}

			if (hollow)
			{
				switch (hole_type)
				{
				case LL_PCODE_HOLE_SQUARE:
					addHole(params, TRUE, 2, 0.5f, hollow, 0.5f, split);
					break;
				case LL_PCODE_HOLE_TRIANGLE:
					addHole(params, TRUE, 3,  0.5f, hollow, 0.5f, split);
					break;
				case LL_PCODE_HOLE_CIRCLE:
				case LL_PCODE_HOLE_SAME:
				default:
					addHole(params, FALSE, circle_detail,  0.5f, hollow, 0.5f);
					break;
				}
			}

			// Special case for openness of sphere
			if ((params.getEnd() - params.getBegin()) < 1.f)
			{
				mOpen = TRUE;
			}
			else if (!hollow)
			{
				mOpen = FALSE;
				mProfile.push_back(mProfile[0]);
				mTotal++;
			}
		}
		break;
	default:
	    llerrs << "Unknown profile: getCurveType()=" << params.getCurveType() << llendl;
		break;
	};

	if (path_open)
	{
		addCap(LL_FACE_PATH_END); // bottom
	}
	
	if ( mOpen) // interior edge caps
	{
		addFace(mTotal-1, 2,0.5,LL_FACE_PROFILE_BEGIN, TRUE); 

		if (hollow)
		{
			addFace(mTotalOut-1, 2,0.5,LL_FACE_PROFILE_END, TRUE);
		}
		else
		{
			addFace(mTotal-2, 2,0.5,LL_FACE_PROFILE_END, TRUE);
		}
	}
	
	//genNormals(params);

	return TRUE;
}



BOOL LLProfileParams::importFile(LLFILE *fp)
{
	LLMemType m1(LLMemType::MTYPE_VOLUME);
	
	const S32 BUFSIZE = 16384;
	char buffer[BUFSIZE];	/* Flawfinder: ignore */
	// *NOTE: changing the size or type of these buffers will require
	// changing the sscanf below.
	char keyword[256];	/* Flawfinder: ignore */
	char valuestr[256];	/* Flawfinder: ignore */
	keyword[0] = 0;
	valuestr[0] = 0;
	F32 tempF32;
	U32 tempU32;

	while (!feof(fp))
	{
		if (fgets(buffer, BUFSIZE, fp) == NULL)
		{
			buffer[0] = '\0';
		}
		
		sscanf(	/* Flawfinder: ignore */
			buffer,
			" %255s %255s",
			keyword, valuestr);
		if (!strcmp("{", keyword))
		{
			continue;
		}
		if (!strcmp("}",keyword))
		{
			break;
		}
		else if (!strcmp("curve", keyword))
		{
			sscanf(valuestr,"%d",&tempU32);
			setCurveType((U8) tempU32);
		}
		else if (!strcmp("begin",keyword))
		{
			sscanf(valuestr,"%g",&tempF32);
			setBegin(tempF32);
		}
		else if (!strcmp("end",keyword))
		{
			sscanf(valuestr,"%g",&tempF32);
			setEnd(tempF32);
		}
		else if (!strcmp("hollow",keyword))
		{
			sscanf(valuestr,"%g",&tempF32);
			setHollow(tempF32);
		}
		else
		{
			llwarns << "unknown keyword " << keyword << " in profile import" << llendl;
		}
	}

	return TRUE;
}


BOOL LLProfileParams::exportFile(LLFILE *fp) const
{
	fprintf(fp,"\t\tprofile 0\n");
	fprintf(fp,"\t\t{\n");
	fprintf(fp,"\t\t\tcurve\t%d\n", getCurveType());
	fprintf(fp,"\t\t\tbegin\t%g\n", getBegin());
	fprintf(fp,"\t\t\tend\t%g\n", getEnd());
	fprintf(fp,"\t\t\thollow\t%g\n", getHollow());
	fprintf(fp, "\t\t}\n");
	return TRUE;
}


BOOL LLProfileParams::importLegacyStream(std::istream& input_stream)
{
	LLMemType m1(LLMemType::MTYPE_VOLUME);
	
	const S32 BUFSIZE = 16384;
	char buffer[BUFSIZE];	/* Flawfinder: ignore */
	// *NOTE: changing the size or type of these buffers will require
	// changing the sscanf below.
	char keyword[256];	/* Flawfinder: ignore */
	char valuestr[256];	/* Flawfinder: ignore */
	keyword[0] = 0;
	valuestr[0] = 0;
	F32 tempF32;
	U32 tempU32;

	while (input_stream.good())
	{
		input_stream.getline(buffer, BUFSIZE);
		sscanf(	/* Flawfinder: ignore */
			buffer,
			" %255s %255s",
			keyword,
			valuestr);
		if (!strcmp("{", keyword))
		{
			continue;
		}
		if (!strcmp("}",keyword))
		{
			break;
		}
		else if (!strcmp("curve", keyword))
		{
			sscanf(valuestr,"%d",&tempU32);
			setCurveType((U8) tempU32);
		}
		else if (!strcmp("begin",keyword))
		{
			sscanf(valuestr,"%g",&tempF32);
			setBegin(tempF32);
		}
		else if (!strcmp("end",keyword))
		{
			sscanf(valuestr,"%g",&tempF32);
			setEnd(tempF32);
		}
		else if (!strcmp("hollow",keyword))
		{
			sscanf(valuestr,"%g",&tempF32);
			setHollow(tempF32);
		}
		else
		{
 		llwarns << "unknown keyword " << keyword << " in profile import" << llendl;
		}
	}

	return TRUE;
}


BOOL LLProfileParams::exportLegacyStream(std::ostream& output_stream) const
{
	output_stream <<"\t\tprofile 0\n";
	output_stream <<"\t\t{\n";
	output_stream <<"\t\t\tcurve\t" << (S32) getCurveType() << "\n";
	output_stream <<"\t\t\tbegin\t" << getBegin() << "\n";
	output_stream <<"\t\t\tend\t" << getEnd() << "\n";
	output_stream <<"\t\t\thollow\t" << getHollow() << "\n";
	output_stream << "\t\t}\n";
	return TRUE;
}

LLSD LLProfileParams::asLLSD() const
{
	LLSD sd;

	sd["curve"] = getCurveType();
	sd["begin"] = getBegin();
	sd["end"] = getEnd();
	sd["hollow"] = getHollow();
	return sd;
}

bool LLProfileParams::fromLLSD(LLSD& sd)
{
	setCurveType(sd["curve"].asInteger());
	setBegin((F32)sd["begin"].asReal());
	setEnd((F32)sd["end"].asReal());
	setHollow((F32)sd["hollow"].asReal());
	return true;
}

void LLProfileParams::copyParams(const LLProfileParams &params)
{
	LLMemType m1(LLMemType::MTYPE_VOLUME);
	setCurveType(params.getCurveType());
	setBegin(params.getBegin());
	setEnd(params.getEnd());
	setHollow(params.getHollow());
}


LLPath::~LLPath()
{
}

void LLPath::genNGon(const LLPathParams& params, S32 sides, F32 startOff, F32 end_scale, F32 twist_scale)
{
	// Generates a circular path, starting at (1, 0, 0), counterclockwise along the xz plane.
	const F32 tableScale[] = { 1, 1, 1, 0.5f, 0.707107f, 0.53f, 0.525f, 0.5f };

	F32 revolutions = params.getRevolutions();
	F32 skew		= params.getSkew();
	F32 skew_mag	= fabs(skew);
	F32 hole_x		= params.getScaleX() * (1.0f - skew_mag);
	F32 hole_y		= params.getScaleY();

	// Calculate taper begin/end for x,y (Negative means taper the beginning)
	F32 taper_x_begin	= 1.0f;
	F32 taper_x_end		= 1.0f - params.getTaperX();
	F32	taper_y_begin	= 1.0f;
	F32	taper_y_end		= 1.0f - params.getTaperY();

	if ( taper_x_end > 1.0f )
	{
		// Flip tapering.
		taper_x_begin	= 2.0f - taper_x_end;
		taper_x_end		= 1.0f;
	}
	if ( taper_y_end > 1.0f )
	{
		// Flip tapering.
		taper_y_begin	= 2.0f - taper_y_end;
		taper_y_end		= 1.0f;
	}

	// For spheres, the radius is usually zero.
	F32 radius_start = 0.5f;
	if (sides < 8)
	{
		radius_start = tableScale[sides];
	}

	// Scale the radius to take the hole size into account.
	radius_start *= 1.0f - hole_y;
	
	// Now check the radius offset to calculate the start,end radius.  (Negative means
	// decrease the start radius instead).
	F32 radius_end    = radius_start;
	F32 radius_offset = params.getRadiusOffset();
	if (radius_offset < 0.f)
	{
		radius_start *= 1.f + radius_offset;
	}
	else
	{
		radius_end   *= 1.f - radius_offset;
	}	

	// Is the path NOT a closed loop?
	mOpen = ( (params.getEnd()*end_scale - params.getBegin() < 1.0f) ||
		      (skew_mag > 0.001f) ||
			  (fabs(taper_x_end - taper_x_begin) > 0.001f) ||
			  (fabs(taper_y_end - taper_y_begin) > 0.001f) ||
			  (fabs(radius_end - radius_start) > 0.001f) );

	F32 ang, c, s;
	LLQuaternion twist, qang;
	PathPt *pt;
	LLVector3 path_axis (1.f, 0.f, 0.f);
	//LLVector3 twist_axis(0.f, 0.f, 1.f);
	F32 twist_begin = params.getTwistBegin() * twist_scale;
	F32 twist_end	= params.getTwist() * twist_scale;

	// We run through this once before the main loop, to make sure
	// the path begins at the correct cut.
	F32 step= 1.0f / sides;
	F32 t	= params.getBegin();
	pt		= vector_append(mPath, 1);
	ang		= 2.0f*F_PI*revolutions * t;
	s		= sin(ang)*lerp(radius_start, radius_end, t);	
	c		= cos(ang)*lerp(radius_start, radius_end, t);


	pt->mPos.setVec(0 + lerp(0,params.getShear().mV[0],s)
					  + lerp(-skew ,skew, t) * 0.5f,
					c + lerp(0,params.getShear().mV[1],s), 
					s);
	pt->mScale.mV[VX] = hole_x * lerp(taper_x_begin, taper_x_end, t);
	pt->mScale.mV[VY] = hole_y * lerp(taper_y_begin, taper_y_end, t);
	pt->mTexT  = t;
	
	// Twist rotates the path along the x,y plane (I think) - DJS 04/05/02
	twist.setQuat  (lerp(twist_begin,twist_end,t) * 2.f * F_PI - F_PI,0,0,1);
	// Rotate the point around the circle's center.
	qang.setQuat   (ang,path_axis);
	pt->mRot   = twist * qang;

	t+=step;

	// Snap to a quantized parameter, so that cut does not
	// affect most sample points.
	t = ((S32)(t * sides)) / (F32)sides;

	// Run through the non-cut dependent points.
	while (t < params.getEnd())
	{
		pt		= vector_append(mPath, 1);

		ang = 2.0f*F_PI*revolutions * t;
		c   = cos(ang)*lerp(radius_start, radius_end, t);
		s   = sin(ang)*lerp(radius_start, radius_end, t);

		pt->mPos.setVec(0 + lerp(0,params.getShear().mV[0],s)
					      + lerp(-skew ,skew, t) * 0.5f,
						c + lerp(0,params.getShear().mV[1],s), 
						s);

		pt->mScale.mV[VX] = hole_x * lerp(taper_x_begin, taper_x_end, t);
		pt->mScale.mV[VY] = hole_y * lerp(taper_y_begin, taper_y_end, t);
		pt->mTexT  = t;

		// Twist rotates the path along the x,y plane (I think) - DJS 04/05/02
		twist.setQuat  (lerp(twist_begin,twist_end,t) * 2.f * F_PI - F_PI,0,0,1);
		// Rotate the point around the circle's center.
		qang.setQuat   (ang,path_axis);
		pt->mRot	= twist * qang;

		t+=step;
	}

	// Make one final pass for the end cut.
	t = params.getEnd();
	pt		= vector_append(mPath, 1);
	ang = 2.0f*F_PI*revolutions * t;
	c   = cos(ang)*lerp(radius_start, radius_end, t);
	s   = sin(ang)*lerp(radius_start, radius_end, t);

	pt->mPos.setVec(0 + lerp(0,params.getShear().mV[0],s)
					  + lerp(-skew ,skew, t) * 0.5f,
					c + lerp(0,params.getShear().mV[1],s), 
					s);
	pt->mScale.mV[VX] = hole_x * lerp(taper_x_begin, taper_x_end, t);
	pt->mScale.mV[VY] = hole_y * lerp(taper_y_begin, taper_y_end, t);
	pt->mTexT  = t;
	
	// Twist rotates the path along the x,y plane (I think) - DJS 04/05/02
	twist.setQuat  (lerp(twist_begin,twist_end,t) * 2.f * F_PI - F_PI,0,0,1);
	// Rotate the point around the circle's center.
	qang.setQuat   (ang,path_axis);
	pt->mRot   = twist * qang;

	mTotal = mPath.size();
}

const LLVector2 LLPathParams::getBeginScale() const
{
	LLVector2 begin_scale(1.f, 1.f);
	if (getScaleX() > 1)
	{
		begin_scale.mV[0] = 2-getScaleX();
	}
	if (getScaleY() > 1)
	{
		begin_scale.mV[1] = 2-getScaleY();
	}
	return begin_scale;
}

const LLVector2 LLPathParams::getEndScale() const
{
	LLVector2 end_scale(1.f, 1.f);
	if (getScaleX() < 1)
	{
		end_scale.mV[0] = getScaleX();
	}
	if (getScaleY() < 1)
	{
		end_scale.mV[1] = getScaleY();
	}
	return end_scale;
}

BOOL LLPath::generate(const LLPathParams& params, F32 detail, S32 split,
					  BOOL is_sculpted, S32 sculpt_size)
{
	LLMemType m1(LLMemType::MTYPE_VOLUME);
	
	if ((!mDirty) && (!is_sculpted))
	{
		return FALSE;
	}

	if (detail < MIN_LOD)
	{
		llinfos << "Generating path with LOD < MIN!  Clamping to 1" << llendl;
		detail = MIN_LOD;
	}

	mDirty = FALSE;
	S32 np = 2; // hardcode for line

	mPath.clear();
	mOpen = TRUE;

	// Is this 0xf0 mask really necessary?  DK 03/02/05
	switch (params.getCurveType() & 0xf0)
	{
	default:
	case LL_PCODE_PATH_LINE:
		{
			// Take the begin/end twist into account for detail.
			np    = llfloor(fabs(params.getTwistBegin() - params.getTwist()) * 3.5f * (detail-0.5f)) + 2;
			if (np < split+2)
			{
				np = split+2;
			}

			mStep = 1.0f / (np-1);
			
			mPath.resize(np);

			LLVector2 start_scale = params.getBeginScale();
			LLVector2 end_scale = params.getEndScale();

			for (S32 i=0;i<np;i++)
			{
				F32 t = lerp(params.getBegin(),params.getEnd(),(F32)i * mStep);
				mPath[i].mPos.setVec(lerp(0,params.getShear().mV[0],t),
									 lerp(0,params.getShear().mV[1],t),
									 t - 0.5f);
				mPath[i].mRot.setQuat(lerp(F_PI * params.getTwistBegin(),F_PI * params.getTwist(),t),0,0,1);
				mPath[i].mScale.mV[0] = lerp(start_scale.mV[0],end_scale.mV[0],t);
				mPath[i].mScale.mV[1] = lerp(start_scale.mV[1],end_scale.mV[1],t);
				mPath[i].mTexT        = t;
			}
		}
		break;

	case LL_PCODE_PATH_CIRCLE:
		{
			// Increase the detail as the revolutions and twist increase.
			F32 twist_mag = fabs(params.getTwistBegin() - params.getTwist());

			S32 sides = (S32)llfloor(llfloor((MIN_DETAIL_FACES * detail + twist_mag * 3.5f * (detail-0.5f))) * params.getRevolutions());

			if (is_sculpted)
				sides = sculpt_size;
			
			genNGon(params, sides);
		}
		break;

	case LL_PCODE_PATH_CIRCLE2:
		{
			if (params.getEnd() - params.getBegin() >= 0.99f &&
				params.getScaleX() >= .99f)
			{
				mOpen = FALSE;
			}

			//genNGon(params, llfloor(MIN_DETAIL_FACES * detail), 4.f, 0.f);
			genNGon(params, llfloor(MIN_DETAIL_FACES * detail));

			F32 t     = 0.f;
			F32 tStep = 1.0f / mPath.size();

			F32 toggle = 0.5f;
			for (S32 i=0;i<(S32)mPath.size();i++)
			{
				mPath[i].mPos.mV[0] = toggle;
				if (toggle == 0.5f)
					toggle = -0.5f;
				else
					toggle = 0.5f;
				t += tStep;
			}
		}

		break;

	case LL_PCODE_PATH_TEST:

		np     = 5;
		mStep = 1.0f / (np-1);
		
		mPath.resize(np);

		for (S32 i=0;i<np;i++)
		{
			F32 t = (F32)i * mStep;
			mPath[i].mPos.setVec(0,
								lerp(0,   -sin(F_PI*params.getTwist()*t)*0.5f,t),
								lerp(-0.5, cos(F_PI*params.getTwist()*t)*0.5f,t));
			mPath[i].mScale.mV[0] = lerp(1,params.getScale().mV[0],t);
			mPath[i].mScale.mV[1] = lerp(1,params.getScale().mV[1],t);
			mPath[i].mTexT  = t;
			mPath[i].mRot.setQuat(F_PI * params.getTwist() * t,1,0,0);
		}

		break;
	};

	if (params.getTwist() != params.getTwistBegin()) mOpen = TRUE;

	//if ((int(fabsf(params.getTwist() - params.getTwistBegin())*100))%100 != 0) {
	//	mOpen = TRUE;
	//}
	
	return TRUE;
}

BOOL LLDynamicPath::generate(const LLPathParams& params, F32 detail, S32 split,
							 BOOL is_sculpted, S32 sculpt_size)
{
	LLMemType m1(LLMemType::MTYPE_VOLUME);
	
	mOpen = TRUE; // Draw end caps
	if (getPathLength() == 0)
	{
		// Path hasn't been generated yet.
		// Some algorithms later assume at least TWO path points.
		resizePath(2);
		for (U32 i = 0; i < 2; i++)
		{
			mPath[i].mPos.setVec(0, 0, 0);
			mPath[i].mRot.setQuat(0, 0, 0);
			mPath[i].mScale.setVec(1, 1);
			mPath[i].mTexT = 0;
		}
	}

	return TRUE;
}


BOOL LLPathParams::importFile(LLFILE *fp)
{
	LLMemType m1(LLMemType::MTYPE_VOLUME);
	
	const S32 BUFSIZE = 16384;
	char buffer[BUFSIZE];	/* Flawfinder: ignore */
	// *NOTE: changing the size or type of these buffers will require
	// changing the sscanf below.
	char keyword[256];	/* Flawfinder: ignore */
	char valuestr[256];	/* Flawfinder: ignore */
	keyword[0] = 0;
	valuestr[0] = 0;

	F32 tempF32;
	F32 x, y;
	U32 tempU32;

	while (!feof(fp))
	{
		if (fgets(buffer, BUFSIZE, fp) == NULL)
		{
			buffer[0] = '\0';
		}
		
		sscanf(	/* Flawfinder: ignore */
			buffer,
			" %255s %255s",
			keyword, valuestr);
		if (!strcmp("{", keyword))
		{
			continue;
		}
		if (!strcmp("}",keyword))
		{
			break;
		}
		else if (!strcmp("curve", keyword))
		{
			sscanf(valuestr,"%d",&tempU32);
			setCurveType((U8) tempU32);
		}
		else if (!strcmp("begin",keyword))
		{
			sscanf(valuestr,"%g",&tempF32);
			setBegin(tempF32);
		}
		else if (!strcmp("end",keyword))
		{
			sscanf(valuestr,"%g",&tempF32);
			setEnd(tempF32);
		}
		else if (!strcmp("scale",keyword))
		{
			// Legacy for one dimensional scale per path
			sscanf(valuestr,"%g",&tempF32);
			setScale(tempF32, tempF32);
		}
		else if (!strcmp("scale_x", keyword))
		{
			sscanf(valuestr, "%g", &x);
			setScaleX(x);
		}
		else if (!strcmp("scale_y", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setScaleY(y);
		}
		else if (!strcmp("shear_x", keyword))
		{
			sscanf(valuestr, "%g", &x);
			setShearX(x);
		}
		else if (!strcmp("shear_y", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setShearY(y);
		}
		else if (!strcmp("twist",keyword))
		{
			sscanf(valuestr,"%g",&tempF32);
			setTwist(tempF32);
		}
		else if (!strcmp("twist_begin", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setTwistBegin(y);
		}
		else if (!strcmp("radius_offset", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setRadiusOffset(y);
		}
		else if (!strcmp("taper_x", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setTaperX(y);
		}
		else if (!strcmp("taper_y", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setTaperY(y);
		}
		else if (!strcmp("revolutions", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setRevolutions(y);
		}
		else if (!strcmp("skew", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setSkew(y);
		}
		else
		{
			llwarns << "unknown keyword " << " in path import" << llendl;
		}
	}
	return TRUE;
}


BOOL LLPathParams::exportFile(LLFILE *fp) const
{
	fprintf(fp, "\t\tpath 0\n");
	fprintf(fp, "\t\t{\n");
	fprintf(fp, "\t\t\tcurve\t%d\n", getCurveType());
	fprintf(fp, "\t\t\tbegin\t%g\n", getBegin());
	fprintf(fp, "\t\t\tend\t%g\n", getEnd());
	fprintf(fp, "\t\t\tscale_x\t%g\n", getScaleX() );
	fprintf(fp, "\t\t\tscale_y\t%g\n", getScaleY() );
	fprintf(fp, "\t\t\tshear_x\t%g\n", getShearX() );
	fprintf(fp, "\t\t\tshear_y\t%g\n", getShearY() );
	fprintf(fp,"\t\t\ttwist\t%g\n", getTwist());
	
	fprintf(fp,"\t\t\ttwist_begin\t%g\n", getTwistBegin());
	fprintf(fp,"\t\t\tradius_offset\t%g\n", getRadiusOffset());
	fprintf(fp,"\t\t\ttaper_x\t%g\n", getTaperX());
	fprintf(fp,"\t\t\ttaper_y\t%g\n", getTaperY());
	fprintf(fp,"\t\t\trevolutions\t%g\n", getRevolutions());
	fprintf(fp,"\t\t\tskew\t%g\n", getSkew());

	fprintf(fp, "\t\t}\n");
	return TRUE;
}


BOOL LLPathParams::importLegacyStream(std::istream& input_stream)
{
	LLMemType m1(LLMemType::MTYPE_VOLUME);
	
	const S32 BUFSIZE = 16384;
	char buffer[BUFSIZE];	/* Flawfinder: ignore */
	// *NOTE: changing the size or type of these buffers will require
	// changing the sscanf below.
	char keyword[256];	/* Flawfinder: ignore */
	char valuestr[256];	/* Flawfinder: ignore */
	keyword[0] = 0;
	valuestr[0] = 0;

	F32 tempF32;
	F32 x, y;
	U32 tempU32;

	while (input_stream.good())
	{
		input_stream.getline(buffer, BUFSIZE);
		sscanf(	/* Flawfinder: ignore */
			buffer,
			" %255s %255s",
			keyword, valuestr);
		if (!strcmp("{", keyword))
		{
			continue;
		}
		if (!strcmp("}",keyword))
		{
			break;
		}
		else if (!strcmp("curve", keyword))
		{
			sscanf(valuestr,"%d",&tempU32);
			setCurveType((U8) tempU32);
		}
		else if (!strcmp("begin",keyword))
		{
			sscanf(valuestr,"%g",&tempF32);
			setBegin(tempF32);
		}
		else if (!strcmp("end",keyword))
		{
			sscanf(valuestr,"%g",&tempF32);
			setEnd(tempF32);
		}
		else if (!strcmp("scale",keyword))
		{
			// Legacy for one dimensional scale per path
			sscanf(valuestr,"%g",&tempF32);
			setScale(tempF32, tempF32);
		}
		else if (!strcmp("scale_x", keyword))
		{
			sscanf(valuestr, "%g", &x);
			setScaleX(x);
		}
		else if (!strcmp("scale_y", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setScaleY(y);
		}
		else if (!strcmp("shear_x", keyword))
		{
			sscanf(valuestr, "%g", &x);
			setShearX(x);
		}
		else if (!strcmp("shear_y", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setShearY(y);
		}
		else if (!strcmp("twist",keyword))
		{
			sscanf(valuestr,"%g",&tempF32);
			setTwist(tempF32);
		}
		else if (!strcmp("twist_begin", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setTwistBegin(y);
		}
		else if (!strcmp("radius_offset", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setRadiusOffset(y);
		}
		else if (!strcmp("taper_x", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setTaperX(y);
		}
		else if (!strcmp("taper_y", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setTaperY(y);
		}
		else if (!strcmp("revolutions", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setRevolutions(y);
		}
		else if (!strcmp("skew", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setSkew(y);
		}
		else
		{
			llwarns << "unknown keyword " << " in path import" << llendl;
		}
	}
	return TRUE;
}


BOOL LLPathParams::exportLegacyStream(std::ostream& output_stream) const
{
	output_stream << "\t\tpath 0\n";
	output_stream << "\t\t{\n";
	output_stream << "\t\t\tcurve\t" << (S32) getCurveType() << "\n";
	output_stream << "\t\t\tbegin\t" << getBegin() << "\n";
	output_stream << "\t\t\tend\t" << getEnd() << "\n";
	output_stream << "\t\t\tscale_x\t" << getScaleX()  << "\n";
	output_stream << "\t\t\tscale_y\t" << getScaleY()  << "\n";
	output_stream << "\t\t\tshear_x\t" << getShearX()  << "\n";
	output_stream << "\t\t\tshear_y\t" << getShearY()  << "\n";
	output_stream <<"\t\t\ttwist\t" << getTwist() << "\n";
	
	output_stream <<"\t\t\ttwist_begin\t" << getTwistBegin() << "\n";
	output_stream <<"\t\t\tradius_offset\t" << getRadiusOffset() << "\n";
	output_stream <<"\t\t\ttaper_x\t" << getTaperX() << "\n";
	output_stream <<"\t\t\ttaper_y\t" << getTaperY() << "\n";
	output_stream <<"\t\t\trevolutions\t" << getRevolutions() << "\n";
	output_stream <<"\t\t\tskew\t" << getSkew() << "\n";

	output_stream << "\t\t}\n";
	return TRUE;
}

LLSD LLPathParams::asLLSD() const
{
	LLSD sd = LLSD();
	sd["curve"] = getCurveType();
	sd["begin"] = getBegin();
	sd["end"] = getEnd();
	sd["scale_x"] = getScaleX();
	sd["scale_y"] = getScaleY();
	sd["shear_x"] = getShearX();
	sd["shear_y"] = getShearY();
	sd["twist"] = getTwist();
	sd["twist_begin"] = getTwistBegin();
	sd["radius_offset"] = getRadiusOffset();
	sd["taper_x"] = getTaperX();
	sd["taper_y"] = getTaperY();
	sd["revolutions"] = getRevolutions();
	sd["skew"] = getSkew();

	return sd;
}

bool LLPathParams::fromLLSD(LLSD& sd)
{
	setCurveType(sd["curve"].asInteger());
	setBegin((F32)sd["begin"].asReal());
	setEnd((F32)sd["end"].asReal());
	setScaleX((F32)sd["scale_x"].asReal());
	setScaleY((F32)sd["scale_y"].asReal());
	setShearX((F32)sd["shear_x"].asReal());
	setShearY((F32)sd["shear_y"].asReal());
	setTwist((F32)sd["twist"].asReal());
	setTwistBegin((F32)sd["twist_begin"].asReal());
	setRadiusOffset((F32)sd["radius_offset"].asReal());
	setTaperX((F32)sd["taper_x"].asReal());
	setTaperY((F32)sd["taper_y"].asReal());
	setRevolutions((F32)sd["revolutions"].asReal());
	setSkew((F32)sd["skew"].asReal());
	return true;
}

void LLPathParams::copyParams(const LLPathParams &params)
{
	setCurveType(params.getCurveType());
	setBegin(params.getBegin());
	setEnd(params.getEnd());
	setScale(params.getScaleX(), params.getScaleY() );
	setShear(params.getShearX(), params.getShearY() );
	setTwist(params.getTwist());
	setTwistBegin(params.getTwistBegin());
	setRadiusOffset(params.getRadiusOffset());
	setTaper( params.getTaperX(), params.getTaperY() );
	setRevolutions(params.getRevolutions());
	setSkew(params.getSkew());
}

S32 profile_delete_lock = 1 ; 
LLProfile::~LLProfile()
{
	if(profile_delete_lock)
	{
		llerrs << "LLProfile should not be deleted here!" << llendl ;
	}
}


S32 LLVolume::sNumMeshPoints = 0;

LLVolume::LLVolume(const LLVolumeParams &params, const F32 detail, const BOOL generate_single_face, const BOOL is_unique)
	: mParams(params)
{
	LLMemType m1(LLMemType::MTYPE_VOLUME);
	
	mUnique = is_unique;
	mFaceMask = 0x0;
	mDetail = detail;
	mSculptLevel = -2;
	
	// set defaults
	if (mParams.getPathParams().getCurveType() == LL_PCODE_PATH_FLEXIBLE)
	{
		mPathp = new LLDynamicPath();
	}
	else
	{
		mPathp = new LLPath();
	}
	mProfilep = new LLProfile();

	mGenerateSingleFace = generate_single_face;

	generate();
	if (mParams.getSculptID().isNull())
	{
		createVolumeFaces();
	}
}

void LLVolume::resizePath(S32 length)
{
	mPathp->resizePath(length);
	mVolumeFaces.clear();
}

void LLVolume::regen()
{
	generate();
	createVolumeFaces();
}

void LLVolume::genBinormals(S32 face)
{
	mVolumeFaces[face].createBinormals();
}

LLVolume::~LLVolume()
{
	sNumMeshPoints -= mMesh.size();
	delete mPathp;

	profile_delete_lock = 0 ;
	delete mProfilep;
	profile_delete_lock = 1 ;

	mPathp = NULL;
	mProfilep = NULL;
	mVolumeFaces.clear();
}

BOOL LLVolume::generate()
{
	LLMemType m1(LLMemType::MTYPE_VOLUME);
	llassert_always(mProfilep);
	
	//Added 10.03.05 Dave Parks
	// Split is a parameter to LLProfile::generate that tesselates edges on the profile 
	// to prevent lighting and texture interpolation errors on triangles that are 
	// stretched due to twisting or scaling on the path.  
	S32 split = (S32) ((mDetail)*0.66f);
	
	if (mParams.getPathParams().getCurveType() == LL_PCODE_PATH_LINE &&
		(mParams.getPathParams().getScale().mV[0] != 1.0f ||
		 mParams.getPathParams().getScale().mV[1] != 1.0f) &&
		(mParams.getProfileParams().getCurveType() == LL_PCODE_PROFILE_SQUARE ||
		 mParams.getProfileParams().getCurveType() == LL_PCODE_PROFILE_ISOTRI ||
		 mParams.getProfileParams().getCurveType() == LL_PCODE_PROFILE_EQUALTRI ||
		 mParams.getProfileParams().getCurveType() == LL_PCODE_PROFILE_RIGHTTRI))
	{
		split = 0;
	}
		 
	mLODScaleBias.setVec(0.5f, 0.5f, 0.5f);
	
	F32 profile_detail = mDetail;
	F32 path_detail = mDetail;
	
	U8 path_type = mParams.getPathParams().getCurveType();
	U8 profile_type = mParams.getProfileParams().getCurveType();
	
	if (path_type == LL_PCODE_PATH_LINE && profile_type == LL_PCODE_PROFILE_CIRCLE)
	{ //cylinders don't care about Z-Axis
		mLODScaleBias.setVec(0.6f, 0.6f, 0.0f);
	}
	else if (path_type == LL_PCODE_PATH_CIRCLE) 
	{	
		mLODScaleBias.setVec(0.6f, 0.6f, 0.6f);
	}
	
	//********************************************************************
	//debug info, to be removed
	if((U32)(mPathp->mPath.size() * mProfilep->mProfile.size()) > (1u << 20))
	{
		llinfos << "sizeS: " << mPathp->mPath.size() << " sizeT: " << mProfilep->mProfile.size() << llendl ;
		llinfos << "path_detail : " << path_detail << " split: " << split << " profile_detail: " << profile_detail << llendl ;
		llinfos << mParams << llendl ;
		llinfos << "more info to check if mProfilep is deleted or not." << llendl ;
		llinfos << mProfilep->mNormals.size() << " : " << mProfilep->mFaces.size() << " : " << mProfilep->mEdgeNormals.size() << " : " << mProfilep->mEdgeCenters.size() << llendl ;

		llerrs << "LLVolume corrupted!" << llendl ;
	}
	//********************************************************************

	BOOL regenPath = mPathp->generate(mParams.getPathParams(), path_detail, split);
	BOOL regenProf = mProfilep->generate(mParams.getProfileParams(), mPathp->isOpen(),profile_detail, split);

	if (regenPath || regenProf ) 
	{
		S32 sizeS = mPathp->mPath.size();
		S32 sizeT = mProfilep->mProfile.size();

		//********************************************************************
		//debug info, to be removed
		if((U32)(sizeS * sizeT) > (1u << 20))
		{
			llinfos << "regenPath: " << (S32)regenPath << " regenProf: " << (S32)regenProf << llendl ;
			llinfos << "sizeS: " << sizeS << " sizeT: " << sizeT << llendl ;
			llinfos << "path_detail : " << path_detail << " split: " << split << " profile_detail: " << profile_detail << llendl ;
			llinfos << mParams << llendl ;
			llinfos << "more info to check if mProfilep is deleted or not." << llendl ;
			llinfos << mProfilep->mNormals.size() << " : " << mProfilep->mFaces.size() << " : " << mProfilep->mEdgeNormals.size() << " : " << mProfilep->mEdgeCenters.size() << llendl ;

			llerrs << "LLVolume corrupted!" << llendl ;
		}
		//********************************************************************

		sNumMeshPoints -= mMesh.size();
		mMesh.resize(sizeT * sizeS);
		sNumMeshPoints += mMesh.size();		

		//generate vertex positions

		// Run along the path.
		for (S32 s = 0; s < sizeS; ++s)
		{
			LLVector2  scale = mPathp->mPath[s].mScale;
			LLQuaternion rot = mPathp->mPath[s].mRot;

			// Run along the profile.
			for (S32 t = 0; t < sizeT; ++t)
			{
				S32 m = s*sizeT + t;
				Point& pt = mMesh[m];
				
				pt.mPos.mV[0] = mProfilep->mProfile[t].mV[0] * scale.mV[0];
				pt.mPos.mV[1] = mProfilep->mProfile[t].mV[1] * scale.mV[1];
				pt.mPos.mV[2] = 0.0f;
				pt.mPos       = pt.mPos * rot;
				pt.mPos      += mPathp->mPath[s].mPos;
			}
		}

		for (std::vector<LLProfile::Face>::iterator iter = mProfilep->mFaces.begin();
			 iter != mProfilep->mFaces.end(); ++iter)
		{
			LLFaceID id = iter->mFaceID;
			mFaceMask |= id;
		}
		
		return TRUE;
	}
	return FALSE;
}


void LLVolume::createVolumeFaces()
{
	LLMemType m1(LLMemType::MTYPE_VOLUME);

	if (mGenerateSingleFace)
	{
		// do nothing
	}
	else
	{
		S32 num_faces = getNumFaces();
		BOOL partial_build = TRUE;
		if (num_faces != mVolumeFaces.size())
		{
			partial_build = FALSE;
			mVolumeFaces.resize(num_faces);
		}
		// Initialize volume faces with parameter data
		for (S32 i = 0; i < (S32)mVolumeFaces.size(); i++)
		{
			LLVolumeFace& vf = mVolumeFaces[i];
			LLProfile::Face& face = mProfilep->mFaces[i];
			vf.mBeginS = face.mIndex;
			vf.mNumS = face.mCount;
			vf.mBeginT = 0;
			vf.mNumT= getPath().mPath.size();
			vf.mID = i;

			// Set the type mask bits correctly
			if (mParams.getProfileParams().getHollow() > 0)
			{
				vf.mTypeMask |= LLVolumeFace::HOLLOW_MASK;
			}
			if (mProfilep->isOpen())
			{
				vf.mTypeMask |= LLVolumeFace::OPEN_MASK;
			}
			if (face.mCap)
			{
				vf.mTypeMask |= LLVolumeFace::CAP_MASK;
				if (face.mFaceID == LL_FACE_PATH_BEGIN)
				{
					vf.mTypeMask |= LLVolumeFace::TOP_MASK;
				}
				else
				{
					llassert(face.mFaceID == LL_FACE_PATH_END);
					vf.mTypeMask |= LLVolumeFace::BOTTOM_MASK;
				}
			}
			else if (face.mFaceID & (LL_FACE_PROFILE_BEGIN | LL_FACE_PROFILE_END))
			{
				vf.mTypeMask |= LLVolumeFace::FLAT_MASK | LLVolumeFace::END_MASK;
			}
			else
			{
				vf.mTypeMask |= LLVolumeFace::SIDE_MASK;
				if (face.mFlat)
				{
					vf.mTypeMask |= LLVolumeFace::FLAT_MASK;
				}
				if (face.mFaceID & LL_FACE_INNER_SIDE)
				{
					vf.mTypeMask |= LLVolumeFace::INNER_MASK;
					if (face.mFlat && vf.mNumS > 2)
					{ //flat inner faces have to copy vert normals
						vf.mNumS = vf.mNumS*2;
					}
				}
				else
				{
					vf.mTypeMask |= LLVolumeFace::OUTER_MASK;
				}
			}
		}

		for (face_list_t::iterator iter = mVolumeFaces.begin();
			 iter != mVolumeFaces.end(); ++iter)
		{
			(*iter).create(this, partial_build);
		}
	}
}


inline LLVector3 sculpt_rgb_to_vector(U8 r, U8 g, U8 b)
{
	// maps RGB values to vector values [0..255] -> [-0.5..0.5]
	LLVector3 value;
	value.mV[VX] = r / 255.f - 0.5f;
	value.mV[VY] = g / 255.f - 0.5f;
	value.mV[VZ] = b / 255.f - 0.5f;

	return value;
}

inline U32 sculpt_xy_to_index(U32 x, U32 y, U16 sculpt_width, U16 sculpt_height, S8 sculpt_components)
{
	U32 index = (x + y * sculpt_width) * sculpt_components;
	return index;
}


inline U32 sculpt_st_to_index(S32 s, S32 t, S32 size_s, S32 size_t, U16 sculpt_width, U16 sculpt_height, S8 sculpt_components)
{
	U32 x = (U32) ((F32)s/(size_s) * (F32) sculpt_width);
	U32 y = (U32) ((F32)t/(size_t) * (F32) sculpt_height);

	return sculpt_xy_to_index(x, y, sculpt_width, sculpt_height, sculpt_components);
}


inline LLVector3 sculpt_index_to_vector(U32 index, const U8* sculpt_data)
{
	LLVector3 v = sculpt_rgb_to_vector(sculpt_data[index], sculpt_data[index+1], sculpt_data[index+2]);

	return v;
}

inline LLVector3 sculpt_st_to_vector(S32 s, S32 t, S32 size_s, S32 size_t, U16 sculpt_width, U16 sculpt_height, S8 sculpt_components, const U8* sculpt_data)
{
	U32 index = sculpt_st_to_index(s, t, size_s, size_t, sculpt_width, sculpt_height, sculpt_components);

	return sculpt_index_to_vector(index, sculpt_data);
}

inline LLVector3 sculpt_xy_to_vector(U32 x, U32 y, U16 sculpt_width, U16 sculpt_height, S8 sculpt_components, const U8* sculpt_data)
{
	U32 index = sculpt_xy_to_index(x, y, sculpt_width, sculpt_height, sculpt_components);

	return sculpt_index_to_vector(index, sculpt_data);
}


F32 LLVolume::sculptGetSurfaceArea()
{
	// test to see if image has enough variation to create non-degenerate geometry

	F32 area = 0;

	S32 sizeS = mPathp->mPath.size();
	S32 sizeT = mProfilep->mProfile.size();
			
	for (S32 s = 0; s < sizeS-1; s++)
	{
		for (S32 t = 0; t < sizeT-1; t++)
		{
			// get four corners of quad
			LLVector3 p1 = mMesh[(s  )*sizeT + (t  )].mPos;
			LLVector3 p2 = mMesh[(s+1)*sizeT + (t  )].mPos;
			LLVector3 p3 = mMesh[(s  )*sizeT + (t+1)].mPos;
			LLVector3 p4 = mMesh[(s+1)*sizeT + (t+1)].mPos;

			// compute the area of the quad by taking the length of the cross product of the two triangles
			LLVector3 cross1 = (p1 - p2) % (p1 - p3);
			LLVector3 cross2 = (p4 - p2) % (p4 - p3);
			area += (cross1.magVec() + cross2.magVec()) / 2.0;
		}
	}

	return area;
}

// create placeholder shape
void LLVolume::sculptGeneratePlaceholder()
{
	LLMemType m1(LLMemType::MTYPE_VOLUME);
	
	S32 sizeS = mPathp->mPath.size();
	S32 sizeT = mProfilep->mProfile.size();
	
	S32 line = 0;

	// for now, this is a sphere.
	for (S32 s = 0; s < sizeS; s++)
	{
		for (S32 t = 0; t < sizeT; t++)
		{
			S32 i = t + line;
			Point& pt = mMesh[i];

			
			F32 u = (F32)s/(sizeS-1);
			F32 v = (F32)t/(sizeT-1);

			const F32 RADIUS = (F32) 0.3;
					
			pt.mPos.mV[0] = (F32)(sin(F_PI * v) * cos(2.0 * F_PI * u) * RADIUS);
			pt.mPos.mV[1] = (F32)(sin(F_PI * v) * sin(2.0 * F_PI * u) * RADIUS);
			pt.mPos.mV[2] = (F32)(cos(F_PI * v) * RADIUS);

		}
		line += sizeT;
	}
}

// create the vertices from the map
void LLVolume::sculptGenerateMapVertices(U16 sculpt_width,
										 U16 sculpt_height,
										 S8 sculpt_components,
										 const U8* sculpt_data,
										 U8 sculpt_type,
										 BOOL is_flexible)
{
	U8 sculpt_stitching = sculpt_type & LL_SCULPT_TYPE_MASK;
	BOOL sculpt_invert = sculpt_type & LL_SCULPT_FLAG_INVERT;
	BOOL sculpt_mirror = sculpt_type & LL_SCULPT_FLAG_MIRROR;
	BOOL reverse_horizontal = (sculpt_invert ? !sculpt_mirror : sculpt_mirror);  // XOR
	
	
	LLMemType m1(LLMemType::MTYPE_VOLUME);
	
	S32 sizeS = mPathp->mPath.size();
	S32 sizeT = mProfilep->mProfile.size();
	
	S32 line = 0;
	for (S32 s = 0; s < sizeS; s++)
	{
		// Run along the profile.
		for (S32 t = 0; t < sizeT; t++)
		{
			S32 i = t + line;
			Point& pt = mMesh[i];

			S32 reversed_t = t;

			if (reverse_horizontal)
			{
				reversed_t = sizeT - t - 1;
			}
			
			U32 x = (U32) ((F32)reversed_t/(sizeT-1) * (F32) sculpt_width);
			U32 y = (U32) ((F32)s/(sizeS-1) * (F32) sculpt_height);

			
			if (y == 0)  // top row stitching
			{
				// pinch?
				if (sculpt_stitching == LL_SCULPT_TYPE_SPHERE)
				{
					x = sculpt_width / 2;
				}
			}

			if (y == sculpt_height)  // bottom row stitching
			{
				// wrap?
				if (sculpt_stitching == LL_SCULPT_TYPE_TORUS)
				{
					y = 0;
				}
				else
				{
					y = sculpt_height - 1;
				}

				// pinch?
				if (sculpt_stitching == LL_SCULPT_TYPE_SPHERE)
				{
					x = sculpt_width / 2;
				}
			}

			if (x == sculpt_width)   // side stitching
			{
				// wrap?
				if ((sculpt_stitching == LL_SCULPT_TYPE_SPHERE) ||
					(sculpt_stitching == LL_SCULPT_TYPE_TORUS) ||
					(sculpt_stitching == LL_SCULPT_TYPE_CYLINDER))
				{
					x = 0;
				}
					
				else
				{
					x = sculpt_width - 1;
				}
			}

			pt.mPos = sculpt_xy_to_vector(x, y, sculpt_width, sculpt_height, sculpt_components, sculpt_data);

			if (sculpt_mirror)
			{
				pt.mPos.mV[VX] *= -1.f;
			}


			if(is_flexible)
			{
				// apply path deformation to position
				LLQuaternion rotation;
				LLVector3 position;
				LLVector2 scale;

				F32 path_dist = pt.mPos[VZ] + 0.5;  // in interval [0..1]

				S32 p1 = (sizeS-1) * (S32)path_dist;
				if (p1 == (sizeS-1))
				{
					rotation = mPathp->mPath[p1].mRot;
					position = mPathp->mPath[p1].mPos;
				}
				else
				{
					// point is somewhere between p1 and p1+1.  so lerp.
					
					S32 p2 = p1+1;
					F32 remainder = path_dist * (sizeS-1) - p1;
					rotation = lerp(remainder, mPathp->mPath[p1].mRot, mPathp->mPath[p2].mRot);
					position = lerp(mPathp->mPath[p1].mPos, mPathp->mPath[p2].mPos, remainder);
				}
				
				// scale doesn't vary (sculpties ignore taper)
				scale = mPathp->mPath[0].mScale;
				
				pt.mPos[VZ] = 0;
				pt.mPos[VX] = pt.mPos[VX] * scale.mV[VX];
				pt.mPos[VY] = pt.mPos[VY] * scale.mV[VY];
				pt.mPos = pt.mPos * rotation;
				pt.mPos += position;
			}

		}
		
		line += sizeT;
	}
}


const S32 SCULPT_REZ_1 = 6;  // changed from 4 to 6 - 6 looks round whereas 4 looks square
const S32 SCULPT_REZ_2 = 8;
const S32 SCULPT_REZ_3 = 16;
const S32 SCULPT_REZ_4 = 32;

S32 sculpt_sides(F32 detail)
{

	// detail is usually one of: 1, 1.5, 2.5, 4.0.
	
	if (detail <= 1.0)
	{
		return SCULPT_REZ_1;
	}
	if (detail <= 2.0)
	{
		return SCULPT_REZ_2;
	}
	if (detail <= 3.0)
	{
		return SCULPT_REZ_3;
	}
	else
	{
		return SCULPT_REZ_4;
	}
}



// determine the number of vertices in both s and t direction for this sculpt
void sculpt_calc_mesh_resolution(U16 width, U16 height, U8 type, F32 detail, S32& s, S32& t)
{
	// this code has the following properties:
	// 1) the aspect ratio of the mesh is as close as possible to the ratio of the map
	//    while still using all available verts
	// 2) the mesh cannot have more verts than is allowed by LOD
	// 3) the mesh cannot have more verts than is allowed by the map
	
	S32 max_vertices_lod = (S32)pow((double)sculpt_sides(detail), 2.0);
	S32 max_vertices_map = width * height / 4;
	
	S32 vertices;
	if (max_vertices_map > 0)
		vertices = llmin(max_vertices_lod, max_vertices_map);
	else
		vertices = max_vertices_lod;
	

	F32 ratio;
	if ((width == 0) || (height == 0))
		ratio = 1.f;
	else
		ratio = (F32) width / (F32) height;

	
	s = (S32)fsqrtf(((F32)vertices / ratio));

	s = llmax(s, 4);              // no degenerate sizes, please
	t = vertices / s;

	t = llmax(t, 4);              // no degenerate sizes, please
	s = vertices / t;
}

// sculpt replaces generate() for sculpted surfaces
void LLVolume::sculpt(U16 sculpt_width,
					  U16 sculpt_height,
					  S8 sculpt_components,
					  const U8* sculpt_data,
					  S32 sculpt_level,
					  BOOL is_flexible)
{
	LLMemType m1(LLMemType::MTYPE_VOLUME);
	U8 sculpt_type = mParams.getSculptType();

	BOOL data_is_empty = FALSE;

	if (sculpt_width == 0 || sculpt_height == 0 || sculpt_components < 3 || sculpt_data == NULL)
	{
		sculpt_level = -1;
		data_is_empty = TRUE;
	}

	S32 requested_sizeS = 0;
	S32 requested_sizeT = 0;

	// create oblong sculpties with high LOD always
	F32 sculpt_detail = mDetail;
	if (sculpt_width != sculpt_height && sculpt_detail < 4.0)
	{
		sculpt_detail = 4.0;
	}

	sculpt_calc_mesh_resolution(sculpt_width, sculpt_height, sculpt_type, sculpt_detail, requested_sizeS, requested_sizeT);

	mPathp->generate(mParams.getPathParams(), sculpt_detail, 0, TRUE, requested_sizeS);
	mProfilep->generate(mParams.getProfileParams(), mPathp->isOpen(), sculpt_detail, 0, TRUE, requested_sizeT);

	S32 sizeS = mPathp->mPath.size();         // we requested a specific size, now see what we really got
	S32 sizeT = mProfilep->mProfile.size();   // we requested a specific size, now see what we really got

	// weird crash bug - DEV-11158 - trying to collect more data:
	if ((sizeS == 0) || (sizeT == 0))
	{
		llwarns << "sculpt bad mesh size " << sizeS << " " << sizeT << llendl;
	}
	
	sNumMeshPoints -= mMesh.size();
	mMesh.resize(sizeS * sizeT);
	sNumMeshPoints += mMesh.size();

	//generate vertex positions
	if (!data_is_empty)
	{
		sculptGenerateMapVertices(sculpt_width, sculpt_height, sculpt_components, sculpt_data, sculpt_type, is_flexible);
		
		if (sculptGetSurfaceArea() < SCULPT_MIN_AREA)
		{
			data_is_empty = TRUE;
		}
	}

	if (data_is_empty)
	{
		sculptGeneratePlaceholder();
	}


	
	for (S32 i = 0; i < (S32)mProfilep->mFaces.size(); i++)
	{
		mFaceMask |= mProfilep->mFaces[i].mFaceID;
	}

	mSculptLevel = sculpt_level;

	// Delete any existing faces so that they get regenerated
	mVolumeFaces.clear();
	
	createVolumeFaces();
}




BOOL LLVolume::isCap(S32 face)
{
	return mProfilep->mFaces[face].mCap; 
}

BOOL LLVolume::isFlat(S32 face)
{
	return mProfilep->mFaces[face].mFlat;
}


bool LLVolumeParams::operator==(const LLVolumeParams &params) const
{
	return ( (getPathParams() == params.getPathParams()) &&
			 (getProfileParams() == params.getProfileParams()) &&
			 (mSculptID == params.mSculptID) &&
			 (mSculptType == params.mSculptType) );
}

bool LLVolumeParams::operator!=(const LLVolumeParams &params) const
{
	return ( (getPathParams() != params.getPathParams()) ||
			 (getProfileParams() != params.getProfileParams()) ||
			 (mSculptID != params.mSculptID) ||
			 (mSculptType != params.mSculptType) );
}

bool LLVolumeParams::operator<(const LLVolumeParams &params) const
{
	if( getPathParams() != params.getPathParams() )
	{
		return getPathParams() < params.getPathParams();
	}
	
	if (getProfileParams() != params.getProfileParams())
	{
		return getProfileParams() < params.getProfileParams();
	}
	
	if (mSculptID != params.mSculptID)
	{
		return mSculptID < params.mSculptID;
	}


	return mSculptType < params.mSculptType;


}

void LLVolumeParams::copyParams(const LLVolumeParams &params)
{
	LLMemType m1(LLMemType::MTYPE_VOLUME);
	mProfileParams.copyParams(params.mProfileParams);
	mPathParams.copyParams(params.mPathParams);
	mSculptID = params.getSculptID();
	mSculptType = params.getSculptType();
}

// Less restricitve approx 0 for volumes
const F32 APPROXIMATELY_ZERO = 0.001f;
bool approx_zero( F32 f, F32 tolerance = APPROXIMATELY_ZERO)
{
	return (f >= -tolerance) && (f <= tolerance);
}

// return true if in range (or nearly so)
static bool limit_range(F32& v, F32 min, F32 max, F32 tolerance = APPROXIMATELY_ZERO)
{
	F32 min_delta = v - min;
	if (min_delta < 0.f)
	{
		v = min;
		if (!approx_zero(min_delta, tolerance))
			return false;
	}
	F32 max_delta = max - v;
	if (max_delta < 0.f)
	{
		v = max;
		if (!approx_zero(max_delta, tolerance))
			return false;
	}
	return true;
}

bool LLVolumeParams::setBeginAndEndS(const F32 b, const F32 e)
{
	bool valid = true;

	// First, clamp to valid ranges.
	F32 begin = b;
	valid &= limit_range(begin, 0.f, 1.f - MIN_CUT_DELTA);

	F32 end = e;
	if (end >= .0149f && end < MIN_CUT_DELTA) end = MIN_CUT_DELTA; // eliminate warning for common rounding error
	valid &= limit_range(end, MIN_CUT_DELTA, 1.f);

	valid &= limit_range(begin, 0.f, end - MIN_CUT_DELTA, .01f);

	// Now set them.
	mProfileParams.setBegin(begin);
	mProfileParams.setEnd(end);

	return valid;
}

bool LLVolumeParams::setBeginAndEndT(const F32 b, const F32 e)
{
	bool valid = true;

	// First, clamp to valid ranges.
	F32 begin = b;
	valid &= limit_range(begin, 0.f, 1.f - MIN_CUT_DELTA);

	F32 end = e;
	valid &= limit_range(end, MIN_CUT_DELTA, 1.f);

	valid &= limit_range(begin, 0.f, end - MIN_CUT_DELTA, .01f);

	// Now set them.
	mPathParams.setBegin(begin);
	mPathParams.setEnd(end);

	return valid;
}			

bool LLVolumeParams::setHollow(const F32 h)
{
	// Validate the hollow based on path and profile.
	U8 profile 	= mProfileParams.getCurveType() & LL_PCODE_PROFILE_MASK;
	U8 hole_type 	= mProfileParams.getCurveType() & LL_PCODE_HOLE_MASK;
	
	F32 max_hollow = HOLLOW_MAX;

	// Only square holes have trouble.
	if (LL_PCODE_HOLE_SQUARE == hole_type)
	{
		switch(profile)
		{
		case LL_PCODE_PROFILE_CIRCLE:
		case LL_PCODE_PROFILE_CIRCLE_HALF:
		case LL_PCODE_PROFILE_EQUALTRI:
			max_hollow = HOLLOW_MAX_SQUARE;
		}
	}

	F32 hollow = h;
	bool valid = limit_range(hollow, HOLLOW_MIN, max_hollow);
	mProfileParams.setHollow(hollow); 

	return valid;
}	

bool LLVolumeParams::setTwistBegin(const F32 b)
{
	F32 twist_begin = b;
	bool valid = limit_range(twist_begin, TWIST_MIN, TWIST_MAX);
	mPathParams.setTwistBegin(twist_begin);
	return valid;
}

bool LLVolumeParams::setTwistEnd(const F32 e)
{	
	F32 twist_end = e;
	bool valid = limit_range(twist_end, TWIST_MIN, TWIST_MAX);
	mPathParams.setTwistEnd(twist_end);
	return valid;
}

bool LLVolumeParams::setRatio(const F32 x, const F32 y)
{
	F32 min_x = RATIO_MIN;
	F32 max_x = RATIO_MAX;
	F32 min_y = RATIO_MIN;
	F32 max_y = RATIO_MAX;
	// If this is a circular path (and not a sphere) then 'ratio' is actually hole size.
	U8 path_type 	= mPathParams.getCurveType();
	U8 profile_type = mProfileParams.getCurveType() & LL_PCODE_PROFILE_MASK;
	if ( LL_PCODE_PATH_CIRCLE == path_type &&
		 LL_PCODE_PROFILE_CIRCLE_HALF != profile_type)
	{
		// Holes are more restricted...
		min_x = HOLE_X_MIN;
		max_x = HOLE_X_MAX;
		min_y = HOLE_Y_MIN;
		max_y = HOLE_Y_MAX;
	}

	F32 ratio_x = x;
	bool valid = limit_range(ratio_x, min_x, max_x);
	F32 ratio_y = y;
	valid &= limit_range(ratio_y, min_y, max_y);

	mPathParams.setScale(ratio_x, ratio_y);

	return valid;
}

bool LLVolumeParams::setShear(const F32 x, const F32 y)
{
	F32 shear_x = x;
	bool valid = limit_range(shear_x, SHEAR_MIN, SHEAR_MAX);
	F32 shear_y = y;
	valid &= limit_range(shear_y, SHEAR_MIN, SHEAR_MAX);
	mPathParams.setShear(shear_x, shear_y);
	return valid;
}

bool LLVolumeParams::setTaperX(const F32 v)
{
	F32 taper = v;
	bool valid = limit_range(taper, TAPER_MIN, TAPER_MAX);
	mPathParams.setTaperX(taper);
	return valid;
}

bool LLVolumeParams::setTaperY(const F32 v)
{
	F32 taper = v;
	bool valid = limit_range(taper, TAPER_MIN, TAPER_MAX);
	mPathParams.setTaperY(taper);
	return valid;
}

bool LLVolumeParams::setRevolutions(const F32 r)
{
	F32 revolutions = r;
	bool valid = limit_range(revolutions, REV_MIN, REV_MAX);
	mPathParams.setRevolutions(revolutions);
	return valid;
}

bool LLVolumeParams::setRadiusOffset(const F32 offset)
{
	bool valid = true;

	// If this is a sphere, just set it to 0 and get out.
	U8 path_type 	= mPathParams.getCurveType();
	U8 profile_type = mProfileParams.getCurveType() & LL_PCODE_PROFILE_MASK;
	if ( LL_PCODE_PROFILE_CIRCLE_HALF == profile_type ||
		LL_PCODE_PATH_CIRCLE != path_type )
	{
		mPathParams.setRadiusOffset(0.f);
		return true;
	}

	// Limit radius offset, based on taper and hole size y.
	F32 radius_offset	= offset;
	F32 taper_y    		= getTaperY();
	F32 radius_mag		= fabs(radius_offset);
	F32 hole_y_mag 		= fabs(getRatioY());
	F32 taper_y_mag		= fabs(taper_y);
	// Check to see if the taper effects us.
	if ( (radius_offset > 0.f && taper_y < 0.f) ||
			(radius_offset < 0.f && taper_y > 0.f) )
	{
		// The taper does not help increase the radius offset range.
		taper_y_mag = 0.f;
	}
	F32 max_radius_mag = 1.f - hole_y_mag * (1.f - taper_y_mag) / (1.f - hole_y_mag);

	// Enforce the maximum magnitude.
	F32 delta = max_radius_mag - radius_mag;
	if (delta < 0.f)
	{
		// Check radius offset sign.
		if (radius_offset < 0.f)
		{
			radius_offset = -max_radius_mag;
		}
		else
		{
			radius_offset = max_radius_mag;
		}
		valid = approx_zero(delta, .1f);
	}

	mPathParams.setRadiusOffset(radius_offset);
	return valid;
}

bool LLVolumeParams::setSkew(const F32 skew_value)
{
	bool valid = true;

	// Check the skew value against the revolutions.
	F32 skew		= llclamp(skew_value, SKEW_MIN, SKEW_MAX);
	F32 skew_mag	= fabs(skew);
	F32 revolutions = getRevolutions();
	F32 scale_x		= getRatioX();
	F32 min_skew_mag = 1.0f - 1.0f / (revolutions * scale_x + 1.0f);
	// Discontinuity; A revolution of 1 allows skews below 0.5.
	if ( fabs(revolutions - 1.0f) < 0.001)
		min_skew_mag = 0.0f;

	// Clip skew.
	F32 delta = skew_mag - min_skew_mag;
	if (delta < 0.f)
	{
		// Check skew sign.
		if (skew < 0.0f)
		{
			skew = -min_skew_mag;
		}
		else 
		{
			skew = min_skew_mag;
		}
		valid = approx_zero(delta, .01f);
	}

	mPathParams.setSkew(skew);
	return valid;
}

bool LLVolumeParams::setSculptID(const LLUUID sculpt_id, U8 sculpt_type)
{
	mSculptID = sculpt_id;
	mSculptType = sculpt_type;
	return true;
}

bool LLVolumeParams::setType(U8 profile, U8 path)
{
	bool result = true;
	// First, check profile and path for validity.
	U8 profile_type	= profile & LL_PCODE_PROFILE_MASK;
	U8 hole_type 	= (profile & LL_PCODE_HOLE_MASK) >> 4;
	U8 path_type	= path >> 4;

	if (profile_type > LL_PCODE_PROFILE_MAX)
	{
		// Bad profile.  Make it square.
		profile = LL_PCODE_PROFILE_SQUARE;
		result = false;
		llwarns << "LLVolumeParams::setType changing bad profile type (" << profile_type
			 	<< ") to be LL_PCODE_PROFILE_SQUARE" << llendl;
	}
	else if (hole_type > LL_PCODE_HOLE_MAX)
	{
		// Bad hole.  Make it the same.
		profile = profile_type;
		result = false;
		llwarns << "LLVolumeParams::setType changing bad hole type (" << hole_type
			 	<< ") to be LL_PCODE_HOLE_SAME" << llendl;
	}

	if (path_type < LL_PCODE_PATH_MIN ||
		path_type > LL_PCODE_PATH_MAX)
	{
		// Bad path.  Make it linear.
		result = false;
		llwarns << "LLVolumeParams::setType changing bad path (" << path
			 	<< ") to be LL_PCODE_PATH_LINE" << llendl;
		path = LL_PCODE_PATH_LINE;
	}

	mProfileParams.setCurveType(profile);
	mPathParams.setCurveType(path);
	return result;
}

// static 
bool LLVolumeParams::validate(U8 prof_curve, F32 prof_begin, F32 prof_end, F32 hollow,
		U8 path_curve, F32 path_begin, F32 path_end,
		F32 scx, F32 scy, F32 shx, F32 shy,
		F32 twistend, F32 twistbegin, F32 radiusoffset,
		F32 tx, F32 ty, F32 revolutions, F32 skew)
{
	LLVolumeParams test_params;
	if (!test_params.setType		(prof_curve, path_curve))
	{
	    	return false;
	}
	if (!test_params.setBeginAndEndS	(prof_begin, prof_end))
	{
	    	return false;
	}
	if (!test_params.setBeginAndEndT	(path_begin, path_end))
	{
	    	return false;
	}
	if (!test_params.setHollow		(hollow))
	{
	    	return false;
	}
	if (!test_params.setTwistBegin		(twistbegin))
	{
	    	return false;
	}
	if (!test_params.setTwistEnd		(twistend))
	{
	    	return false;
	}
	if (!test_params.setRatio		(scx, scy))
	{
	    	return false;
	}
	if (!test_params.setShear		(shx, shy))
	{
	    	return false;
	}
	if (!test_params.setTaper		(tx, ty))
	{
	    	return false;
	}
	if (!test_params.setRevolutions		(revolutions))
	{
	    	return false;
	}
	if (!test_params.setRadiusOffset	(radiusoffset))
	{
	    	return false;
	}
	if (!test_params.setSkew		(skew))
	{
	    	return false;
	}
	return true;
}

S32 *LLVolume::getTriangleIndices(U32 &num_indices) const
{
	LLMemType m1(LLMemType::MTYPE_VOLUME);
	
	S32 expected_num_triangle_indices = getNumTriangleIndices();
	if (expected_num_triangle_indices > MAX_VOLUME_TRIANGLE_INDICES)
	{
		// we don't allow LLVolumes with this many vertices
		llwarns << "Couldn't allocate triangle indices" << llendl;
		num_indices = 0;
		return NULL;
	}

	S32* index = new S32[expected_num_triangle_indices];
	S32 count = 0;

	// Let's do this totally diffently, as we don't care about faces...
	// Counter-clockwise triangles are forward facing...

	BOOL open = getProfile().isOpen();
	BOOL hollow = (mParams.getProfileParams().getHollow() > 0);
	BOOL path_open = getPath().isOpen();
	S32 size_s, size_s_out, size_t;
	S32 s, t, i;
	size_s = getProfile().getTotal();
	size_s_out = getProfile().getTotalOut();
	size_t = getPath().mPath.size();

	// NOTE -- if the construction of the triangles below ever changes
	// then getNumTriangleIndices() method may also have to be updated.

	if (open)		/* Flawfinder: ignore */
	{
		if (hollow)
		{
			// Open hollow -- much like the closed solid, except we 
			// we need to stitch up the gap between s=0 and s=size_s-1

			for (t = 0; t < size_t - 1; t++)
			{
				// The outer face, first cut, and inner face
				for (s = 0; s < size_s - 1; s++)
				{
					i  = s + t*size_s;
					index[count++]  = i;				// x,y
					index[count++]  = i + 1;			// x+1,y
					index[count++]  = i + size_s;		// x,y+1
	
					index[count++]  = i + size_s;		// x,y+1
					index[count++]  = i + 1;			// x+1,y
					index[count++]  = i + size_s + 1;	// x+1,y+1
				}

				// The other cut face
				index[count++]  = s + t*size_s;		// x,y
				index[count++]  = 0 + t*size_s;		// x+1,y
				index[count++]  = s + (t+1)*size_s;	// x,y+1
	
				index[count++]  = s + (t+1)*size_s;	// x,y+1
				index[count++]  = 0 + t*size_s;		// x+1,y
				index[count++]  = 0 + (t+1)*size_s;	// x+1,y+1
			}

			// Do the top and bottom caps, if necessary
			if (path_open)
			{
				// Top cap
				S32 pt1 = 0;
				S32 pt2 = size_s-1;
				S32 i   = (size_t - 1)*size_s;

				while (pt2 - pt1 > 1)
				{
					// Use the profile points instead of the mesh, since you want
					// the un-transformed profile distances.
					LLVector3 p1 = getProfile().mProfile[pt1];
					LLVector3 p2 = getProfile().mProfile[pt2];
					LLVector3 pa = getProfile().mProfile[pt1+1];
					LLVector3 pb = getProfile().mProfile[pt2-1];

					p1.mV[VZ] = 0.f;
					p2.mV[VZ] = 0.f;
					pa.mV[VZ] = 0.f;
					pb.mV[VZ] = 0.f;

					// Use area of triangle to determine backfacing
					F32 area_1a2, area_1ba, area_21b, area_2ab;
					area_1a2 =  (p1.mV[0]*pa.mV[1] - pa.mV[0]*p1.mV[1]) +
								(pa.mV[0]*p2.mV[1] - p2.mV[0]*pa.mV[1]) +
								(p2.mV[0]*p1.mV[1] - p1.mV[0]*p2.mV[1]);

					area_1ba =  (p1.mV[0]*pb.mV[1] - pb.mV[0]*p1.mV[1]) +
								(pb.mV[0]*pa.mV[1] - pa.mV[0]*pb.mV[1]) +
								(pa.mV[0]*p1.mV[1] - p1.mV[0]*pa.mV[1]);

					area_21b =  (p2.mV[0]*p1.mV[1] - p1.mV[0]*p2.mV[1]) +
								(p1.mV[0]*pb.mV[1] - pb.mV[0]*p1.mV[1]) +
								(pb.mV[0]*p2.mV[1] - p2.mV[0]*pb.mV[1]);

					area_2ab =  (p2.mV[0]*pa.mV[1] - pa.mV[0]*p2.mV[1]) +
								(pa.mV[0]*pb.mV[1] - pb.mV[0]*pa.mV[1]) +
								(pb.mV[0]*p2.mV[1] - p2.mV[0]*pb.mV[1]);

					BOOL use_tri1a2 = TRUE;
					BOOL tri_1a2 = TRUE;
					BOOL tri_21b = TRUE;

					if (area_1a2 < 0)
					{
						tri_1a2 = FALSE;
					}
					if (area_2ab < 0)
					{
						// Can't use, because it contains point b
						tri_1a2 = FALSE;
					}
					if (area_21b < 0)
					{
						tri_21b = FALSE;
					}
					if (area_1ba < 0)
					{
						// Can't use, because it contains point b
						tri_21b = FALSE;
					}

					if (!tri_1a2)
					{
						use_tri1a2 = FALSE;
					}
					else if (!tri_21b)
					{
						use_tri1a2 = TRUE;
					}
					else
					{
						LLVector3 d1 = p1 - pa;
						LLVector3 d2 = p2 - pb;

						if (d1.magVecSquared() < d2.magVecSquared())
						{
							use_tri1a2 = TRUE;
						}
						else
						{
							use_tri1a2 = FALSE;
						}
					}

					if (use_tri1a2)
					{
						index[count++] = pt1 + i;
						index[count++] = pt1 + 1 + i;
						index[count++] = pt2 + i;
						pt1++;
					}
					else
					{
						index[count++] = pt1 + i;
						index[count++] = pt2 - 1 + i;
						index[count++] = pt2 + i;
						pt2--;
					}
				}

				// Bottom cap
				pt1          = 0;
				pt2          = size_s-1;
				while (pt2 - pt1 > 1)
				{
					// Use the profile points instead of the mesh, since you want
					// the un-transformed profile distances.
					LLVector3 p1 = getProfile().mProfile[pt1];
					LLVector3 p2 = getProfile().mProfile[pt2];
					LLVector3 pa = getProfile().mProfile[pt1+1];
					LLVector3 pb = getProfile().mProfile[pt2-1];

					p1.mV[VZ] = 0.f;
					p2.mV[VZ] = 0.f;
					pa.mV[VZ] = 0.f;
					pb.mV[VZ] = 0.f;

					// Use area of triangle to determine backfacing
					F32 area_1a2, area_1ba, area_21b, area_2ab;
					area_1a2 =  (p1.mV[0]*pa.mV[1] - pa.mV[0]*p1.mV[1]) +
								(pa.mV[0]*p2.mV[1] - p2.mV[0]*pa.mV[1]) +
								(p2.mV[0]*p1.mV[1] - p1.mV[0]*p2.mV[1]);

					area_1ba =  (p1.mV[0]*pb.mV[1] - pb.mV[0]*p1.mV[1]) +
								(pb.mV[0]*pa.mV[1] - pa.mV[0]*pb.mV[1]) +
								(pa.mV[0]*p1.mV[1] - p1.mV[0]*pa.mV[1]);

					area_21b =  (p2.mV[0]*p1.mV[1] - p1.mV[0]*p2.mV[1]) +
								(p1.mV[0]*pb.mV[1] - pb.mV[0]*p1.mV[1]) +
								(pb.mV[0]*p2.mV[1] - p2.mV[0]*pb.mV[1]);

					area_2ab =  (p2.mV[0]*pa.mV[1] - pa.mV[0]*p2.mV[1]) +
								(pa.mV[0]*pb.mV[1] - pb.mV[0]*pa.mV[1]) +
								(pb.mV[0]*p2.mV[1] - p2.mV[0]*pb.mV[1]);

					BOOL use_tri1a2 = TRUE;
					BOOL tri_1a2 = TRUE;
					BOOL tri_21b = TRUE;

					if (area_1a2 < 0)
					{
						tri_1a2 = FALSE;
					}
					if (area_2ab < 0)
					{
						// Can't use, because it contains point b
						tri_1a2 = FALSE;
					}
					if (area_21b < 0)
					{
						tri_21b = FALSE;
					}
					if (area_1ba < 0)
					{
						// Can't use, because it contains point b
						tri_21b = FALSE;
					}

					if (!tri_1a2)
					{
						use_tri1a2 = FALSE;
					}
					else if (!tri_21b)
					{
						use_tri1a2 = TRUE;
					}
					else
					{
						LLVector3 d1 = p1 - pa;
						LLVector3 d2 = p2 - pb;

						if (d1.magVecSquared() < d2.magVecSquared())
						{
							use_tri1a2 = TRUE;
						}
						else
						{
							use_tri1a2 = FALSE;
						}
					}

					if (use_tri1a2)
					{
						index[count++] = pt1;
						index[count++] = pt2;
						index[count++] = pt1 + 1;
						pt1++;
					}
					else
					{
						index[count++] = pt1;
						index[count++] = pt2;
						index[count++] = pt2 - 1;
						pt2--;
					}
				}
			}
		}
		else
		{
			// Open solid

			for (t = 0; t < size_t - 1; t++)
			{
				// Outer face + 1 cut face
				for (s = 0; s < size_s - 1; s++)
				{
					i  = s + t*size_s;

					index[count++]  = i;				// x,y
					index[count++]  = i + 1;			// x+1,y
					index[count++]  = i + size_s;		// x,y+1

					index[count++]  = i + size_s;		// x,y+1
					index[count++]  = i + 1;			// x+1,y
					index[count++]  = i + size_s + 1;	// x+1,y+1
				}

				// The other cut face
				index[count++] = (size_s - 1) + (t*size_s);		// x,y
				index[count++] = 0 + t*size_s;					// x+1,y
				index[count++] = (size_s - 1) + (t+1)*size_s;	// x,y+1

				index[count++] = (size_s - 1) + (t+1)*size_s;	// x,y+1
				index[count++] = 0 + (t*size_s);				// x+1,y
				index[count++] = 0 + (t+1)*size_s;				// x+1,y+1
			}

			// Do the top and bottom caps, if necessary
			if (path_open)
			{
				for (s = 0; s < size_s - 2; s++)
				{
					index[count++] = s+1;
					index[count++] = s;
					index[count++] = size_s - 1;
				}

				// We've got a top cap
				S32 offset = (size_t - 1)*size_s;
				for (s = 0; s < size_s - 2; s++)
				{
					// Inverted ordering from bottom cap.
					index[count++] = offset + size_s - 1;
					index[count++] = offset + s;
					index[count++] = offset + s + 1;
				}
			}
		}
	}
	else if (hollow)
	{
		// Closed hollow
		// Outer face
		
		for (t = 0; t < size_t - 1; t++)
		{
			for (s = 0; s < size_s_out - 1; s++)
			{
				i  = s + t*size_s;

				index[count++]  = i;				// x,y
				index[count++]  = i + 1;			// x+1,y
				index[count++]  = i + size_s;		// x,y+1

				index[count++]  = i + size_s;		// x,y+1
				index[count++]  = i + 1;			// x+1,y
				index[count++]  = i + 1 + size_s;	// x+1,y+1
			}
		}

		// Inner face
		// Invert facing from outer face
		for (t = 0; t < size_t - 1; t++)
		{
			for (s = size_s_out; s < size_s - 1; s++)
			{
				i  = s + t*size_s;

				index[count++]  = i;				// x,y
				index[count++]  = i + 1;			// x+1,y
				index[count++]  = i + size_s;		// x,y+1

				index[count++]  = i + size_s;		// x,y+1
				index[count++]  = i + 1;			// x+1,y
				index[count++]  = i + 1 + size_s;	// x+1,y+1
			}
		}

		// Do the top and bottom caps, if necessary
		if (path_open)
		{
			// Top cap
			S32 pt1 = 0;
			S32 pt2 = size_s-1;
			S32 i   = (size_t - 1)*size_s;

			while (pt2 - pt1 > 1)
			{
				// Use the profile points instead of the mesh, since you want
				// the un-transformed profile distances.
				LLVector3 p1 = getProfile().mProfile[pt1];
				LLVector3 p2 = getProfile().mProfile[pt2];
				LLVector3 pa = getProfile().mProfile[pt1+1];
				LLVector3 pb = getProfile().mProfile[pt2-1];

				p1.mV[VZ] = 0.f;
				p2.mV[VZ] = 0.f;
				pa.mV[VZ] = 0.f;
				pb.mV[VZ] = 0.f;

				// Use area of triangle to determine backfacing
				F32 area_1a2, area_1ba, area_21b, area_2ab;
				area_1a2 =  (p1.mV[0]*pa.mV[1] - pa.mV[0]*p1.mV[1]) +
							(pa.mV[0]*p2.mV[1] - p2.mV[0]*pa.mV[1]) +
							(p2.mV[0]*p1.mV[1] - p1.mV[0]*p2.mV[1]);

				area_1ba =  (p1.mV[0]*pb.mV[1] - pb.mV[0]*p1.mV[1]) +
							(pb.mV[0]*pa.mV[1] - pa.mV[0]*pb.mV[1]) +
							(pa.mV[0]*p1.mV[1] - p1.mV[0]*pa.mV[1]);

				area_21b =  (p2.mV[0]*p1.mV[1] - p1.mV[0]*p2.mV[1]) +
							(p1.mV[0]*pb.mV[1] - pb.mV[0]*p1.mV[1]) +
							(pb.mV[0]*p2.mV[1] - p2.mV[0]*pb.mV[1]);

				area_2ab =  (p2.mV[0]*pa.mV[1] - pa.mV[0]*p2.mV[1]) +
							(pa.mV[0]*pb.mV[1] - pb.mV[0]*pa.mV[1]) +
							(pb.mV[0]*p2.mV[1] - p2.mV[0]*pb.mV[1]);

				BOOL use_tri1a2 = TRUE;
				BOOL tri_1a2 = TRUE;
				BOOL tri_21b = TRUE;

				if (area_1a2 < 0)
				{
					tri_1a2 = FALSE;
				}
				if (area_2ab < 0)
				{
					// Can't use, because it contains point b
					tri_1a2 = FALSE;
				}
				if (area_21b < 0)
				{
					tri_21b = FALSE;
				}
				if (area_1ba < 0)
				{
					// Can't use, because it contains point b
					tri_21b = FALSE;
				}

				if (!tri_1a2)
				{
					use_tri1a2 = FALSE;
				}
				else if (!tri_21b)
				{
					use_tri1a2 = TRUE;
				}
				else
				{
					LLVector3 d1 = p1 - pa;
					LLVector3 d2 = p2 - pb;

					if (d1.magVecSquared() < d2.magVecSquared())
					{
						use_tri1a2 = TRUE;
					}
					else
					{
						use_tri1a2 = FALSE;
					}
				}

				if (use_tri1a2)
				{
					index[count++] = pt1 + i;
					index[count++] = pt1 + 1 + i;
					index[count++] = pt2 + i;
					pt1++;
				}
				else
				{
					index[count++] = pt1 + i;
					index[count++] = pt2 - 1 + i;
					index[count++] = pt2 + i;
					pt2--;
				}
			}

			// Bottom cap
			pt1          = 0;
			pt2          = size_s-1;
			while (pt2 - pt1 > 1)
			{
				// Use the profile points instead of the mesh, since you want
				// the un-transformed profile distances.
				LLVector3 p1 = getProfile().mProfile[pt1];
				LLVector3 p2 = getProfile().mProfile[pt2];
				LLVector3 pa = getProfile().mProfile[pt1+1];
				LLVector3 pb = getProfile().mProfile[pt2-1];

				p1.mV[VZ] = 0.f;
				p2.mV[VZ] = 0.f;
				pa.mV[VZ] = 0.f;
				pb.mV[VZ] = 0.f;

				// Use area of triangle to determine backfacing
				F32 area_1a2, area_1ba, area_21b, area_2ab;
				area_1a2 =  (p1.mV[0]*pa.mV[1] - pa.mV[0]*p1.mV[1]) +
							(pa.mV[0]*p2.mV[1] - p2.mV[0]*pa.mV[1]) +
							(p2.mV[0]*p1.mV[1] - p1.mV[0]*p2.mV[1]);

				area_1ba =  (p1.mV[0]*pb.mV[1] - pb.mV[0]*p1.mV[1]) +
							(pb.mV[0]*pa.mV[1] - pa.mV[0]*pb.mV[1]) +
							(pa.mV[0]*p1.mV[1] - p1.mV[0]*pa.mV[1]);

				area_21b =  (p2.mV[0]*p1.mV[1] - p1.mV[0]*p2.mV[1]) +
							(p1.mV[0]*pb.mV[1] - pb.mV[0]*p1.mV[1]) +
							(pb.mV[0]*p2.mV[1] - p2.mV[0]*pb.mV[1]);

				area_2ab =  (p2.mV[0]*pa.mV[1] - pa.mV[0]*p2.mV[1]) +
							(pa.mV[0]*pb.mV[1] - pb.mV[0]*pa.mV[1]) +
							(pb.mV[0]*p2.mV[1] - p2.mV[0]*pb.mV[1]);

				BOOL use_tri1a2 = TRUE;
				BOOL tri_1a2 = TRUE;
				BOOL tri_21b = TRUE;

				if (area_1a2 < 0)
				{
					tri_1a2 = FALSE;
				}
				if (area_2ab < 0)
				{
					// Can't use, because it contains point b
					tri_1a2 = FALSE;
				}
				if (area_21b < 0)
				{
					tri_21b = FALSE;
				}
				if (area_1ba < 0)
				{
					// Can't use, because it contains point b
					tri_21b = FALSE;
				}

				if (!tri_1a2)
				{
					use_tri1a2 = FALSE;
				}
				else if (!tri_21b)
				{
					use_tri1a2 = TRUE;
				}
				else
				{
					LLVector3 d1 = p1 - pa;
					LLVector3 d2 = p2 - pb;

					if (d1.magVecSquared() < d2.magVecSquared())
					{
						use_tri1a2 = TRUE;
					}
					else
					{
						use_tri1a2 = FALSE;
					}
				}

				if (use_tri1a2)
				{
					index[count++] = pt1;
					index[count++] = pt2;
					index[count++] = pt1 + 1;
					pt1++;
				}
				else
				{
					index[count++] = pt1;
					index[count++] = pt2;
					index[count++] = pt2 - 1;
					pt2--;
				}
			}
		}		
	}
	else
	{
		// Closed solid.  Easy case.
		for (t = 0; t < size_t - 1; t++)
		{
			for (s = 0; s < size_s - 1; s++)
			{
				// Should wrap properly, but for now...
				i  = s + t*size_s;

				index[count++]  = i;				// x,y
				index[count++]  = i + 1;			// x+1,y
				index[count++]  = i + size_s;		// x,y+1

				index[count++]  = i + size_s;		// x,y+1
				index[count++]  = i + 1;			// x+1,y
				index[count++]  = i + size_s + 1;	// x+1,y+1
			}
		}

		// Do the top and bottom caps, if necessary
		if (path_open)
		{
			// bottom cap
			for (s = 1; s < size_s - 2; s++)
			{
				index[count++] = s+1;
				index[count++] = s;
				index[count++] = 0;
			}

			// top cap
			S32 offset = (size_t - 1)*size_s;
			for (s = 1; s < size_s - 2; s++)
			{
				// Inverted ordering from bottom cap.
				index[count++] = offset;
				index[count++] = offset + s;
				index[count++] = offset + s + 1;
			}
		}
	}

#ifdef LL_DEBUG
	// assert that we computed the correct number of indices
	if (count != expected_num_triangle_indices )
	{
		llerrs << "bad index count prediciton:"
			<< "  expected=" << expected_num_triangle_indices 
			<< " actual=" << count << llendl;
	}
#endif

#if 0
	// verify that each index does not point beyond the size of the mesh
	S32 num_vertices = mMesh.size();
	for (i = 0; i < count; i+=3)
	{
		llinfos << index[i] << ":" << index[i+1] << ":" << index[i+2] << llendl;
		llassert(index[i] < num_vertices);
		llassert(index[i+1] < num_vertices);
		llassert(index[i+2] < num_vertices);
	}
#endif

	num_indices = count;
	return index;
}

S32 LLVolume::getNumTriangleIndices() const
{
	BOOL profile_open = getProfile().isOpen();
	BOOL hollow = (mParams.getProfileParams().getHollow() > 0);
	BOOL path_open = getPath().isOpen();

	S32 size_s, size_s_out, size_t;
	size_s = getProfile().getTotal();
	size_s_out = getProfile().getTotalOut();
	size_t = getPath().mPath.size();

	S32 count = 0;
	if (profile_open)		/* Flawfinder: ignore */
	{
		if (hollow)
		{
			// Open hollow -- much like the closed solid, except we 
			// we need to stitch up the gap between s=0 and s=size_s-1
			count = (size_t - 1) * (((size_s -1) * 6) + 6);
		}
		else
		{
			count = (size_t - 1) * (((size_s -1) * 6) + 6); 
		}
	}
	else if (hollow)
	{
		// Closed hollow
		// Outer face
		count = (size_t - 1) * (size_s_out - 1) * 6;

		// Inner face
		count += (size_t - 1) * ((size_s - 1) - size_s_out) * 6;
	}
	else
	{
		// Closed solid.  Easy case.
		count = (size_t - 1) * (size_s - 1) * 6;
	}

	if (path_open)
	{
		S32 cap_triangle_count = size_s - 3;
		if ( profile_open
			|| hollow )
		{
			cap_triangle_count = size_s - 2;
		}
		if ( cap_triangle_count > 0 )
		{
			// top and bottom caps
			count += cap_triangle_count * 2 * 3;
		}
	}
	return count;
}

//-----------------------------------------------------------------------------
// generateSilhouetteVertices()
//-----------------------------------------------------------------------------
void LLVolume::generateSilhouetteVertices(std::vector<LLVector3> &vertices,
										  std::vector<LLVector3> &normals,
										  std::vector<S32> &segments,
										  const LLVector3& obj_cam_vec,
										  const LLMatrix4& mat,
										  const LLMatrix3& norm_mat)
{
	LLMemType m1(LLMemType::MTYPE_VOLUME);
	
	vertices.clear();
	normals.clear();
	segments.clear();

	//for each face
	for (face_list_t::iterator iter = mVolumeFaces.begin();
		 iter != mVolumeFaces.end(); ++iter)
	{
		const LLVolumeFace& face = *iter;
	
		if (face.mTypeMask & (LLVolumeFace::CAP_MASK)) {
	
		}
		else {

			//==============================================
			//DEBUG draw edge map instead of silhouette edge
			//==============================================

#if DEBUG_SILHOUETTE_EDGE_MAP

			//for each triangle
			U32 count = face.mIndices.size();
			for (U32 j = 0; j < count/3; j++) {
				//get vertices
				S32 v1 = face.mIndices[j*3+0];
				S32 v2 = face.mIndices[j*3+1];
				S32 v3 = face.mIndices[j*3+2];

				//get current face center
				LLVector3 cCenter = (face.mVertices[v1].mPosition + 
									face.mVertices[v2].mPosition + 
									face.mVertices[v3].mPosition) / 3.0f;

				//for each edge
				for (S32 k = 0; k < 3; k++) {
                    S32 nIndex = face.mEdge[j*3+k];
					if (nIndex <= -1) {
						continue;
					}

					if (nIndex >= (S32) count/3) {
						continue;
					}
					//get neighbor vertices
					v1 = face.mIndices[nIndex*3+0];
					v2 = face.mIndices[nIndex*3+1];
					v3 = face.mIndices[nIndex*3+2];

					//get neighbor face center
					LLVector3 nCenter = (face.mVertices[v1].mPosition + 
									face.mVertices[v2].mPosition + 
									face.mVertices[v3].mPosition) / 3.0f;

					//draw line
					vertices.push_back(cCenter);
					vertices.push_back(nCenter);
					normals.push_back(LLVector3(1,1,1));
					normals.push_back(LLVector3(1,1,1));
					segments.push_back(vertices.size());
				}
			}
		
			continue;

			//==============================================
			//DEBUG
			//==============================================

			//==============================================
			//DEBUG draw normals instead of silhouette edge
			//==============================================
#elif DEBUG_SILHOUETTE_NORMALS

			//for each vertex
			for (U32 j = 0; j < face.mVertices.size(); j++) {
				vertices.push_back(face.mVertices[j].mPosition);
				vertices.push_back(face.mVertices[j].mPosition + face.mVertices[j].mNormal*0.1f);
				normals.push_back(LLVector3(0,0,1));
				normals.push_back(LLVector3(0,0,1));
				segments.push_back(vertices.size());
#if DEBUG_SILHOUETTE_BINORMALS
				vertices.push_back(face.mVertices[j].mPosition);
				vertices.push_back(face.mVertices[j].mPosition + face.mVertices[j].mBinormal*0.1f);
				normals.push_back(LLVector3(0,0,1));
				normals.push_back(LLVector3(0,0,1));
				segments.push_back(vertices.size());
#endif
			}
						
			continue;
#else
			//==============================================
			//DEBUG
			//==============================================

			static const U8 AWAY = 0x01,
							TOWARDS = 0x02;

			//for each triangle
			std::vector<U8> fFacing;
			vector_append(fFacing, face.mIndices.size()/3);
			for (U32 j = 0; j < face.mIndices.size()/3; j++) 
			{
				//approximate normal
				S32 v1 = face.mIndices[j*3+0];
				S32 v2 = face.mIndices[j*3+1];
				S32 v3 = face.mIndices[j*3+2];

				LLVector3 norm = (face.mVertices[v1].mPosition - face.mVertices[v2].mPosition) % 
					(face.mVertices[v2].mPosition - face.mVertices[v3].mPosition);
				
				if (norm.magVecSquared() < 0.00000001f) 
				{
					fFacing[j] = AWAY | TOWARDS;
				}
				else 
				{
					//get view vector
					LLVector3 view = (obj_cam_vec-face.mVertices[v1].mPosition);
					bool away = view * norm > 0.0f; 
					if (away) 
					{
						fFacing[j] = AWAY;
					}
					else 
					{
						fFacing[j] = TOWARDS;
					}
				}
			}
			
			//for each triangle
			for (U32 j = 0; j < face.mIndices.size()/3; j++) 
			{
				if (fFacing[j] == (AWAY | TOWARDS)) 
				{ //this is a degenerate triangle
					//take neighbor facing (degenerate faces get facing of one of their neighbors)
					// *FIX IF NEEDED:  this does not deal with neighboring degenerate faces
					for (S32 k = 0; k < 3; k++) 
					{
						S32 index = face.mEdge[j*3+k];
						if (index != -1) 
						{
							fFacing[j] = fFacing[index];
							break;
						}
					}
					continue; //skip degenerate face
				}

				//for each edge
				for (S32 k = 0; k < 3; k++) {
					S32 index = face.mEdge[j*3+k];
					if (index != -1 && fFacing[index] == (AWAY | TOWARDS)) {
						//our neighbor is degenerate, make him face our direction
						fFacing[face.mEdge[j*3+k]] = fFacing[j];
						continue;
					}

					if (index == -1 ||		//edge has no neighbor, MUST be a silhouette edge
						(fFacing[index] & fFacing[j]) == 0) { 	//we found a silhouette edge

						S32 v1 = face.mIndices[j*3+k];
						S32 v2 = face.mIndices[j*3+((k+1)%3)];
						
						vertices.push_back(face.mVertices[v1].mPosition*mat);
						LLVector3 norm1 = face.mVertices[v1].mNormal * norm_mat;
						norm1.normVec();
						normals.push_back(norm1);

						vertices.push_back(face.mVertices[v2].mPosition*mat);
						LLVector3 norm2 = face.mVertices[v2].mNormal * norm_mat;
						norm2.normVec();
						normals.push_back(norm2);

						segments.push_back(vertices.size());
					}
				}		
			}
#endif
		}
	}
}

S32 LLVolume::lineSegmentIntersect(const LLVector3& start, const LLVector3& end, 
								   S32 face,
								   LLVector3* intersection,LLVector2* tex_coord, LLVector3* normal, LLVector3* bi_normal)
{
	S32 hit_face = -1;
	
	S32 start_face;
	S32 end_face;
	
	if (face == -1) // ALL_SIDES
	{
		start_face = 0;
		end_face = getNumFaces() - 1;
	}
	else
	{
		start_face = face;
		end_face = face;
	}

	LLVector3 dir = end - start;

	F32 closest_t = 2.f; // must be larger than 1
	
	for (S32 i = start_face; i <= end_face; i++)
	{
		const LLVolumeFace &face = getVolumeFace((U32)i);

		LLVector3 box_center = (face.mExtents[0] + face.mExtents[1]) / 2.f;
		LLVector3 box_size   = face.mExtents[1] - face.mExtents[0];

        if (LLLineSegmentBoxIntersect(start, end, box_center, box_size))
		{
			if (bi_normal != NULL) // if the caller wants binormals, we may need to generate them
			{
				genBinormals(i);
			}
			
			for (U32 tri = 0; tri < face.mIndices.size()/3; tri++) 
			{
				S32 index1 = face.mIndices[tri*3+0];
				S32 index2 = face.mIndices[tri*3+1];
				S32 index3 = face.mIndices[tri*3+2];

				F32 a, b, t;
			
				if (LLTriangleRayIntersect(face.mVertices[index1].mPosition,
										   face.mVertices[index2].mPosition,
										   face.mVertices[index3].mPosition,
										   start, dir, &a, &b, &t, FALSE))
				{
					if ((t >= 0.f) &&      // if hit is after start
						(t <= 1.f) &&      // and before end
						(t < closest_t))   // and this hit is closer
		{
						closest_t = t;
						hit_face = i;

						if (intersection != NULL)
						{
							*intersection = start + dir * closest_t;
						}
			
						if (tex_coord != NULL)
			{
							*tex_coord = ((1.f - a - b)  * face.mVertices[index1].mTexCoord +
										  a              * face.mVertices[index2].mTexCoord +
										  b              * face.mVertices[index3].mTexCoord);

						}

						if (normal != NULL)
				{
							*normal    = ((1.f - a - b)  * face.mVertices[index1].mNormal + 
										  a              * face.mVertices[index2].mNormal +
										  b              * face.mVertices[index3].mNormal);
						}

						if (bi_normal != NULL)
					{
							*bi_normal = ((1.f - a - b)  * face.mVertices[index1].mBinormal + 
										  a              * face.mVertices[index2].mBinormal +
										  b              * face.mVertices[index3].mBinormal);
						}

					}
				}
			}
		}		
	}
	
	
	return hit_face;
}

class LLVertexIndexPair
{
public:
	LLVertexIndexPair(const LLVector3 &vertex, const S32 index);

	LLVector3 mVertex;
	S32	mIndex;
};

LLVertexIndexPair::LLVertexIndexPair(const LLVector3 &vertex, const S32 index)
{
	mVertex = vertex;
	mIndex = index;
}

const F32 VERTEX_SLOP = 0.00001f;
const F32 VERTEX_SLOP_SQRD = VERTEX_SLOP * VERTEX_SLOP;

struct lessVertex
{
	bool operator()(const LLVertexIndexPair *a, const LLVertexIndexPair *b)
	{
		const F32 slop = VERTEX_SLOP;

		if (a->mVertex.mV[0] + slop < b->mVertex.mV[0])
		{
			return TRUE;
		}
		else if (a->mVertex.mV[0] - slop > b->mVertex.mV[0])
		{
			return FALSE;
		}
		
		if (a->mVertex.mV[1] + slop < b->mVertex.mV[1])
		{
			return TRUE;
		}
		else if (a->mVertex.mV[1] - slop > b->mVertex.mV[1])
		{
			return FALSE;
		}
		
		if (a->mVertex.mV[2] + slop < b->mVertex.mV[2])
		{
			return TRUE;
		}
		else if (a->mVertex.mV[2] - slop > b->mVertex.mV[2])
		{
			return FALSE;
		}
		
		return FALSE;
	}
};

struct lessTriangle
{
	bool operator()(const S32 *a, const S32 *b)
	{
		if (*a < *b)
		{
			return TRUE;
		}
		else if (*a > *b)
		{
			return FALSE;
		}

		if (*(a+1) < *(b+1))
		{
			return TRUE;
		}
		else if (*(a+1) > *(b+1))
		{
			return FALSE;
		}

		if (*(a+2) < *(b+2))
		{
			return TRUE;
		}
		else if (*(a+2) > *(b+2))
		{
			return FALSE;
		}

		return FALSE;
	}
};

BOOL equalTriangle(const S32 *a, const S32 *b)
{
	if ((*a == *b) && (*(a+1) == *(b+1)) && (*(a+2) == *(b+2)))
	{
		return TRUE;
	}
	return FALSE;
}

BOOL LLVolume::cleanupTriangleData( const S32 num_input_vertices,
									const std::vector<Point>& input_vertices,
									const S32 num_input_triangles,
									S32 *input_triangles,
									S32 &num_output_vertices,
									LLVector3 **output_vertices,
									S32 &num_output_triangles,
									S32 **output_triangles)
{
	LLMemType m1(LLMemType::MTYPE_VOLUME);
	
	/* Testing: avoid any cleanup
	static BOOL skip_cleanup = TRUE;
	if ( skip_cleanup )
	{
		num_output_vertices = num_input_vertices;
		num_output_triangles = num_input_triangles;

		*output_vertices = new LLVector3[num_input_vertices];
		for (S32 index = 0; index < num_input_vertices; index++)
		{
			(*output_vertices)[index] = input_vertices[index].mPos;
		}

		*output_triangles = new S32[num_input_triangles*3];
		memcpy(*output_triangles, input_triangles, 3*num_input_triangles*sizeof(S32));		// Flawfinder: ignore
		return TRUE;
	}
	*/

	// Here's how we do this:
	// Create a structure which contains the original vertex index and the
	// LLVector3 data.
	// "Sort" the data by the vectors
	// Create an array the size of the old vertex list, with a mapping of
	// old indices to new indices.
	// Go through triangles, shift so the lowest index is first
	// Sort triangles by first index
	// Remove duplicate triangles
	// Allocate and pack new triangle data.

	//LLTimer cleanupTimer;
	//llinfos << "In vertices: " << num_input_vertices << llendl;
	//llinfos << "In triangles: " << num_input_triangles << llendl;

	S32 i;
	typedef std::multiset<LLVertexIndexPair*, lessVertex> vertex_set_t;
	vertex_set_t vertex_list;

	LLVertexIndexPair *pairp = NULL;
	for (i = 0; i < num_input_vertices; i++)
	{
		LLVertexIndexPair *new_pairp = new LLVertexIndexPair(input_vertices[i].mPos, i);
		vertex_list.insert(new_pairp);
	}

	// Generate the vertex mapping and the list of vertices without
	// duplicates.  This will crash if there are no vertices.
	S32 *vertex_mapping = new S32[num_input_vertices];
	LLVector3 *new_vertices = new LLVector3[num_input_vertices];
	LLVertexIndexPair *prev_pairp = NULL;

	S32 new_num_vertices;

	new_num_vertices = 0;
	for (vertex_set_t::iterator iter = vertex_list.begin(),
			 end = vertex_list.end();
		 iter != end; iter++)
	{
		pairp = *iter;
		if (!prev_pairp || ((pairp->mVertex - prev_pairp->mVertex).magVecSquared() >= VERTEX_SLOP_SQRD))	
		{
			new_vertices[new_num_vertices] = pairp->mVertex;
			//llinfos << "Added vertex " << new_num_vertices << " : " << pairp->mVertex << llendl;
			new_num_vertices++;
			// Update the previous
			prev_pairp = pairp;
		}
		else
		{
			//llinfos << "Removed duplicate vertex " << pairp->mVertex << ", distance magVecSquared() is " << (pairp->mVertex - prev_pairp->mVertex).magVecSquared() << llendl;
		}
		vertex_mapping[pairp->mIndex] = new_num_vertices - 1;
	}

	// Iterate through triangles and remove degenerates, re-ordering vertices
	// along the way.
	S32 *new_triangles = new S32[num_input_triangles * 3];
	S32 new_num_triangles = 0;

	for (i = 0; i < num_input_triangles; i++)
	{
		S32 v1 = i*3;
		S32 v2 = v1 + 1;
		S32 v3 = v1 + 2;

		//llinfos << "Checking triangle " << input_triangles[v1] << ":" << input_triangles[v2] << ":" << input_triangles[v3] << llendl;
		input_triangles[v1] = vertex_mapping[input_triangles[v1]];
		input_triangles[v2] = vertex_mapping[input_triangles[v2]];
		input_triangles[v3] = vertex_mapping[input_triangles[v3]];

		if ((input_triangles[v1] == input_triangles[v2])
			|| (input_triangles[v1] == input_triangles[v3])
			|| (input_triangles[v2] == input_triangles[v3]))
		{
			//llinfos << "Removing degenerate triangle " << input_triangles[v1] << ":" << input_triangles[v2] << ":" << input_triangles[v3] << llendl;
			// Degenerate triangle, skip
			continue;
		}

		if (input_triangles[v1] < input_triangles[v2])
		{
			if (input_triangles[v1] < input_triangles[v3])
			{
				// (0 < 1) && (0 < 2)
				new_triangles[new_num_triangles*3] = input_triangles[v1];
				new_triangles[new_num_triangles*3+1] = input_triangles[v2];
				new_triangles[new_num_triangles*3+2] = input_triangles[v3];
			}
			else
			{
				// (0 < 1) && (2 < 0)
				new_triangles[new_num_triangles*3] = input_triangles[v3];
				new_triangles[new_num_triangles*3+1] = input_triangles[v1];
				new_triangles[new_num_triangles*3+2] = input_triangles[v2];
			}
		}
		else if (input_triangles[v2] < input_triangles[v3])
		{
			// (1 < 0) && (1 < 2)
			new_triangles[new_num_triangles*3] = input_triangles[v2];
			new_triangles[new_num_triangles*3+1] = input_triangles[v3];
			new_triangles[new_num_triangles*3+2] = input_triangles[v1];
		}
		else
		{
			// (1 < 0) && (2 < 1)
			new_triangles[new_num_triangles*3] = input_triangles[v3];
			new_triangles[new_num_triangles*3+1] = input_triangles[v1];
			new_triangles[new_num_triangles*3+2] = input_triangles[v2];
		}
		new_num_triangles++;
	}

	if (new_num_triangles == 0)
	{
		llwarns << "Created volume object with 0 faces." << llendl;
		delete[] new_triangles;
		delete[] vertex_mapping;
		delete[] new_vertices;
		return FALSE;
	}

	typedef std::set<S32*, lessTriangle> triangle_set_t;
	triangle_set_t triangle_list;

	for (i = 0; i < new_num_triangles; i++)
	{
		triangle_list.insert(&new_triangles[i*3]);
	}

	// Sort through the triangle list, and delete duplicates

	S32 *prevp = NULL;
	S32 *curp = NULL;

	S32 *sorted_tris = new S32[new_num_triangles*3];
	S32 cur_tri = 0;
	for (triangle_set_t::iterator iter = triangle_list.begin(),
			 end = triangle_list.end();
		 iter != end; iter++)
	{
		curp = *iter;
		if (!prevp || !equalTriangle(prevp, curp))
		{
			//llinfos << "Added triangle " << *curp << ":" << *(curp+1) << ":" << *(curp+2) << llendl;
			sorted_tris[cur_tri*3] = *curp;
			sorted_tris[cur_tri*3+1] = *(curp+1);
			sorted_tris[cur_tri*3+2] = *(curp+2);
			cur_tri++;
			prevp = curp;
		}
		else
		{
			//llinfos << "Skipped triangle " << *curp << ":" << *(curp+1) << ":" << *(curp+2) << llendl;
		}
	}

	*output_vertices = new LLVector3[new_num_vertices];
	num_output_vertices = new_num_vertices;
	for (i = 0; i < new_num_vertices; i++)
	{
		(*output_vertices)[i] = new_vertices[i];
	}

	*output_triangles = new S32[cur_tri*3];
	num_output_triangles = cur_tri;
	memcpy(*output_triangles, sorted_tris, 3*cur_tri*sizeof(S32));		/* Flawfinder: ignore */

	/*
	llinfos << "Out vertices: " << num_output_vertices << llendl;
	llinfos << "Out triangles: " << num_output_triangles << llendl;
	for (i = 0; i < num_output_vertices; i++)
	{
		llinfos << i << ":" << (*output_vertices)[i] << llendl;
	}
	for (i = 0; i < num_output_triangles; i++)
	{
		llinfos << i << ":" << (*output_triangles)[i*3] << ":" << (*output_triangles)[i*3+1] << ":" << (*output_triangles)[i*3+2] << llendl;
	}
	*/

	//llinfos << "Out vertices: " << num_output_vertices << llendl;
	//llinfos << "Out triangles: " << num_output_triangles << llendl;
	delete[] vertex_mapping;
	vertex_mapping = NULL;
	delete[] new_vertices;
	new_vertices = NULL;
	delete[] new_triangles;
	new_triangles = NULL;
	delete[] sorted_tris;
	sorted_tris = NULL;
	triangle_list.clear();
	std::for_each(vertex_list.begin(), vertex_list.end(), DeletePointer());
	vertex_list.clear();
	
	return TRUE;
}


BOOL LLVolumeParams::importFile(LLFILE *fp)
{
	LLMemType m1(LLMemType::MTYPE_VOLUME);
	
	//llinfos << "importing volume" << llendl;
	const S32 BUFSIZE = 16384;
	char buffer[BUFSIZE];	/* Flawfinder: ignore */
	// *NOTE: changing the size or type of this buffer will require
	// changing the sscanf below.
	char keyword[256];	/* Flawfinder: ignore */
	keyword[0] = 0;

	while (!feof(fp))
	{
		if (fgets(buffer, BUFSIZE, fp) == NULL)
		{
			buffer[0] = '\0';
		}
		
		sscanf(buffer, " %255s", keyword);	/* Flawfinder: ignore */
		if (!strcmp("{", keyword))
		{
			continue;
		}
		if (!strcmp("}",keyword))
		{
			break;
		}
		else if (!strcmp("profile", keyword))
		{
			mProfileParams.importFile(fp);
		}
		else if (!strcmp("path",keyword))
		{
			mPathParams.importFile(fp);
		}
		else
		{
			llwarns << "unknown keyword " << keyword << " in volume import" << llendl;
		}
	}

	return TRUE;
}

BOOL LLVolumeParams::exportFile(LLFILE *fp) const
{
	fprintf(fp,"\tshape 0\n");
	fprintf(fp,"\t{\n");
	mPathParams.exportFile(fp);
	mProfileParams.exportFile(fp);
	fprintf(fp, "\t}\n");
	return TRUE;
}


BOOL LLVolumeParams::importLegacyStream(std::istream& input_stream)
{
	LLMemType m1(LLMemType::MTYPE_VOLUME);
	
	//llinfos << "importing volume" << llendl;
	const S32 BUFSIZE = 16384;
	// *NOTE: changing the size or type of this buffer will require
	// changing the sscanf below.
	char buffer[BUFSIZE];		/* Flawfinder: ignore */
	char keyword[256];		/* Flawfinder: ignore */
	keyword[0] = 0;

	while (input_stream.good())
	{
		input_stream.getline(buffer, BUFSIZE);
		sscanf(buffer, " %255s", keyword);
		if (!strcmp("{", keyword))
		{
			continue;
		}
		if (!strcmp("}",keyword))
		{
			break;
		}
		else if (!strcmp("profile", keyword))
		{
			mProfileParams.importLegacyStream(input_stream);
		}
		else if (!strcmp("path",keyword))
		{
			mPathParams.importLegacyStream(input_stream);
		}
		else
		{
			llwarns << "unknown keyword " << keyword << " in volume import" << llendl;
		}
	}

	return TRUE;
}

BOOL LLVolumeParams::exportLegacyStream(std::ostream& output_stream) const
{
	LLMemType m1(LLMemType::MTYPE_VOLUME);
	
	output_stream <<"\tshape 0\n";
	output_stream <<"\t{\n";
	mPathParams.exportLegacyStream(output_stream);
	mProfileParams.exportLegacyStream(output_stream);
	output_stream << "\t}\n";
	return TRUE;
}

LLSD LLVolumeParams::asLLSD() const
{
	LLSD sd = LLSD();
	sd["path"] = mPathParams;
	sd["profile"] = mProfileParams;
	return sd;
}

bool LLVolumeParams::fromLLSD(LLSD& sd)
{
	mPathParams.fromLLSD(sd["path"]);
	mProfileParams.fromLLSD(sd["profile"]);
	return true;
}

void LLVolumeParams::reduceS(F32 begin, F32 end)
{
	begin = llclampf(begin);
	end = llclampf(end);
	if (begin > end)
	{
		F32 temp = begin;
		begin = end;
		end = temp;
	}
	F32 a = mProfileParams.getBegin();
	F32 b = mProfileParams.getEnd();
	mProfileParams.setBegin(a + begin * (b - a));
	mProfileParams.setEnd(a + end * (b - a));
}

void LLVolumeParams::reduceT(F32 begin, F32 end)
{
	begin = llclampf(begin);
	end = llclampf(end);
	if (begin > end)
	{
		F32 temp = begin;
		begin = end;
		end = temp;
	}
	F32 a = mPathParams.getBegin();
	F32 b = mPathParams.getEnd();
	mPathParams.setBegin(a + begin * (b - a));
	mPathParams.setEnd(a + end * (b - a));
}

const F32 MIN_CONCAVE_PROFILE_WEDGE = 0.125f;	// 1/8 unity
const F32 MIN_CONCAVE_PATH_WEDGE = 0.111111f;	// 1/9 unity

// returns TRUE if the shape can be approximated with a convex shape 
// for collison purposes
BOOL LLVolumeParams::isConvex() const
{
	F32 path_length = mPathParams.getEnd() - mPathParams.getBegin();
	F32 hollow = mProfileParams.getHollow();
	 
	U8 path_type = mPathParams.getCurveType();
	if ( path_length > MIN_CONCAVE_PATH_WEDGE
		&& ( mPathParams.getTwist() != mPathParams.getTwistBegin()
		     || (hollow > 0.f 
				 && LL_PCODE_PATH_LINE != path_type) ) )
	{
		// twist along a "not too short" path is concave
		return FALSE;
	}

	F32 profile_length = mProfileParams.getEnd() - mProfileParams.getBegin();
	BOOL same_hole = hollow == 0.f 
					 || (mProfileParams.getCurveType() & LL_PCODE_HOLE_MASK) == LL_PCODE_HOLE_SAME;

	F32 min_profile_wedge = MIN_CONCAVE_PROFILE_WEDGE;
	U8 profile_type = mProfileParams.getCurveType() & LL_PCODE_PROFILE_MASK;
	if ( LL_PCODE_PROFILE_CIRCLE_HALF == profile_type )
	{
		// it is a sphere and spheres get twice the minimum profile wedge
		min_profile_wedge = 2.f * MIN_CONCAVE_PROFILE_WEDGE;
	}

	BOOL convex_profile = ( ( profile_length == 1.f
						     || profile_length <= 0.5f )
						   && hollow == 0.f )						// trivially convex
						  || ( profile_length <= min_profile_wedge
							  && same_hole );						// effectvely convex (even when hollow)

	if (!convex_profile)
	{
		// profile is concave
		return FALSE;
	}

	if ( LL_PCODE_PATH_LINE == path_type )
	{
		// straight paths with convex profile
		return TRUE;
	}

	BOOL concave_path = (path_length < 1.0f) && (path_length > 0.5f);
	if (concave_path)
	{
		return FALSE;
	}

	// we're left with spheres, toroids and tubes
	if ( LL_PCODE_PROFILE_CIRCLE_HALF == profile_type )
	{
		// at this stage all spheres must be convex
		return TRUE;
	}

	// it's a toroid or tube		
	if ( path_length <= MIN_CONCAVE_PATH_WEDGE )
	{
		// effectively convex
		return TRUE;
	}

	return FALSE;
}

// debug
void LLVolumeParams::setCube()
{
	mProfileParams.setCurveType(LL_PCODE_PROFILE_SQUARE);
	mProfileParams.setBegin(0.f);
	mProfileParams.setEnd(1.f);
	mProfileParams.setHollow(0.f);

	mPathParams.setBegin(0.f);
	mPathParams.setEnd(1.f);
	mPathParams.setScale(1.f, 1.f);
	mPathParams.setShear(0.f, 0.f);
	mPathParams.setCurveType(LL_PCODE_PATH_LINE);
	mPathParams.setTwistBegin(0.f);
	mPathParams.setTwistEnd(0.f);
	mPathParams.setRadiusOffset(0.f);
	mPathParams.setTaper(0.f, 0.f);
	mPathParams.setRevolutions(0.f);
	mPathParams.setSkew(0.f);
}

LLFaceID LLVolume::generateFaceMask()
{
	LLFaceID new_mask = 0x0000;

	switch(mParams.getProfileParams().getCurveType() & LL_PCODE_PROFILE_MASK)
	{
	case LL_PCODE_PROFILE_CIRCLE:
	case LL_PCODE_PROFILE_CIRCLE_HALF:
		new_mask |= LL_FACE_OUTER_SIDE_0;
		break;
	case LL_PCODE_PROFILE_SQUARE:
		{
			for(S32 side = (S32)(mParams.getProfileParams().getBegin() * 4.f); side < llceil(mParams.getProfileParams().getEnd() * 4.f); side++)
			{
				new_mask |= LL_FACE_OUTER_SIDE_0 << side;
			}
		}
		break;
	case LL_PCODE_PROFILE_ISOTRI:
	case LL_PCODE_PROFILE_EQUALTRI:
	case LL_PCODE_PROFILE_RIGHTTRI:
		{
			for(S32 side = (S32)(mParams.getProfileParams().getBegin() * 3.f); side < llceil(mParams.getProfileParams().getEnd() * 3.f); side++)
			{
				new_mask |= LL_FACE_OUTER_SIDE_0 << side;
			}
		}
		break;
	default:
		llerrs << "Unknown profile!" << llendl
		break;
	}

	// handle hollow objects
	if (mParams.getProfileParams().getHollow() > 0)
	{
		new_mask |= LL_FACE_INNER_SIDE;
	}

	// handle open profile curves
	if (mProfilep->isOpen())
	{
		new_mask |= LL_FACE_PROFILE_BEGIN | LL_FACE_PROFILE_END;
	}

	// handle open path curves
	if (mPathp->isOpen())
	{
		new_mask |= LL_FACE_PATH_BEGIN | LL_FACE_PATH_END;
	}

	return new_mask;
}

BOOL LLVolume::isFaceMaskValid(LLFaceID face_mask)
{
	LLFaceID test_mask = 0;
	for(S32 i = 0; i < getNumFaces(); i++)
	{
		test_mask |= mProfilep->mFaces[i].mFaceID;
	}

	return test_mask == face_mask;
}

BOOL LLVolume::isConvex() const
{
	// mParams.isConvex() may return FALSE even though the final
	// geometry is actually convex due to LOD approximations.
	// TODO -- provide LLPath and LLProfile with isConvex() methods
	// that correctly determine convexity. -- Leviathan
	return mParams.isConvex();
}


std::ostream& operator<<(std::ostream &s, const LLProfileParams &profile_params)
{
	s << "{type=" << (U32) profile_params.mCurveType;
	s << ", begin=" << profile_params.mBegin;
	s << ", end=" << profile_params.mEnd;
	s << ", hollow=" << profile_params.mHollow;
	s << "}";
	return s;
}


std::ostream& operator<<(std::ostream &s, const LLPathParams &path_params)
{
	s << "{type=" << (U32) path_params.mCurveType;
	s << ", begin=" << path_params.mBegin;
	s << ", end=" << path_params.mEnd;
	s << ", twist=" << path_params.mTwistEnd;
	s << ", scale=" << path_params.mScale;
	s << ", shear=" << path_params.mShear;
	s << ", twist_begin=" << path_params.mTwistBegin;
	s << ", radius_offset=" << path_params.mRadiusOffset;
	s << ", taper=" << path_params.mTaper;
	s << ", revolutions=" << path_params.mRevolutions;
	s << ", skew=" << path_params.mSkew;
	s << "}";
	return s;
}


std::ostream& operator<<(std::ostream &s, const LLVolumeParams &volume_params)
{
	s << "{profileparams = " << volume_params.mProfileParams;
	s << ", pathparams = " << volume_params.mPathParams;
	s << "}";
	return s;
}


std::ostream& operator<<(std::ostream &s, const LLProfile &profile)
{
	s << " {open=" << (U32) profile.mOpen;
	s << ", dirty=" << profile.mDirty;
	s << ", totalout=" << profile.mTotalOut;
	s << ", total=" << profile.mTotal;
	s << "}";
	return s;
}


std::ostream& operator<<(std::ostream &s, const LLPath &path)
{
	s << "{open=" << (U32) path.mOpen;
	s << ", dirty=" << path.mDirty;
	s << ", step=" << path.mStep;
	s << ", total=" << path.mTotal;
	s << "}";
	return s;
}

std::ostream& operator<<(std::ostream &s, const LLVolume &volume)
{
	s << "{params = " << volume.getParams();
	s << ", path = " << *volume.mPathp;
	s << ", profile = " << *volume.mProfilep;
	s << "}";
	return s;
}


std::ostream& operator<<(std::ostream &s, const LLVolume *volumep)
{
	s << "{params = " << volumep->getParams();
	s << ", path = " << *(volumep->mPathp);
	s << ", profile = " << *(volumep->mProfilep);
	s << "}";
	return s;
}


BOOL LLVolumeFace::create(LLVolume* volume, BOOL partial_build)
{
	if (mTypeMask & CAP_MASK)
	{
		return createCap(volume, partial_build);
	}
	else if ((mTypeMask & END_MASK) || (mTypeMask & SIDE_MASK))
	{
		return createSide(volume, partial_build);
	}
	else
	{
		llerrs << "Unknown/uninitialized face type!" << llendl;
		return FALSE;
	}
}

void	LerpPlanarVertex(LLVolumeFace::VertexData& v0,
				   LLVolumeFace::VertexData& v1,
				   LLVolumeFace::VertexData& v2,
				   LLVolumeFace::VertexData& vout,
				   F32	coef01,
				   F32	coef02)
{
	vout.mPosition = v0.mPosition + ((v1.mPosition-v0.mPosition)*coef01)+((v2.mPosition-v0.mPosition)*coef02);
	vout.mTexCoord = v0.mTexCoord + ((v1.mTexCoord-v0.mTexCoord)*coef01)+((v2.mTexCoord-v0.mTexCoord)*coef02);
	vout.mNormal = v0.mNormal;
	vout.mBinormal = v0.mBinormal;
}

BOOL LLVolumeFace::createUnCutCubeCap(LLVolume* volume, BOOL partial_build)
{
	LLMemType m1(LLMemType::MTYPE_VOLUME);
	
	const std::vector<LLVolume::Point>& mesh = volume->getMesh();
	const std::vector<LLVector3>& profile = volume->getProfile().mProfile;
	S32 max_s = volume->getProfile().getTotal();
	S32 max_t = volume->getPath().mPath.size();

	// S32 i;
	S32 num_vertices = 0, num_indices = 0;
	S32	grid_size = (profile.size()-1)/4;
	S32	quad_count = (grid_size * grid_size);

	num_vertices = (grid_size+1)*(grid_size+1);
	num_indices = quad_count * 4;

	LLVector3& min = mExtents[0];
	LLVector3& max = mExtents[1];

	S32 offset = 0;
	if (mTypeMask & TOP_MASK)
		offset = (max_t-1) * max_s;
	else
		offset = mBeginS;

	VertexData	corners[4];
	VertexData baseVert;
	for(int t = 0; t < 4; t++){
		corners[t].mPosition = mesh[offset + (grid_size*t)].mPos;
		corners[t].mTexCoord.mV[0] = profile[grid_size*t].mV[0]+0.5f;
		corners[t].mTexCoord.mV[1] = 0.5f - profile[grid_size*t].mV[1];
	}
	baseVert.mNormal = 
		((corners[1].mPosition-corners[0].mPosition) % 
		(corners[2].mPosition-corners[1].mPosition));
	baseVert.mNormal.normVec();
	if(!(mTypeMask & TOP_MASK)){
		baseVert.mNormal *= -1.0f;
	}else{
		//Swap the UVs on the U(X) axis for top face
		LLVector2 swap;
		swap = corners[0].mTexCoord;
		corners[0].mTexCoord=corners[3].mTexCoord;
		corners[3].mTexCoord=swap;
		swap = corners[1].mTexCoord;
		corners[1].mTexCoord=corners[2].mTexCoord;
		corners[2].mTexCoord=swap;
	}
	baseVert.mBinormal = calc_binormal_from_triangle( 
		corners[0].mPosition, corners[0].mTexCoord,
		corners[1].mPosition, corners[1].mTexCoord,
		corners[2].mPosition, corners[2].mTexCoord);
	for(int t = 0; t < 4; t++){
		corners[t].mBinormal = baseVert.mBinormal;
		corners[t].mNormal = baseVert.mNormal;
	}
	mHasBinormals = TRUE;

	if (partial_build)
	{
		mVertices.clear();
	}

	S32	vtop = mVertices.size();
	for(int gx = 0;gx<grid_size+1;gx++){
		for(int gy = 0;gy<grid_size+1;gy++){
			VertexData newVert;
			LerpPlanarVertex(
				corners[0],
				corners[1],
				corners[3],
				newVert,
				(F32)gx/(F32)grid_size,
				(F32)gy/(F32)grid_size);
			mVertices.push_back(newVert);

			if (gx == 0 && gy == 0)
			{
				min = max = newVert.mPosition;
			}
			else
			{
				update_min_max(min,max,newVert.mPosition);
			}
		}
	}
	
	mCenter = (min + max) * 0.5f;

	if (!partial_build)
	{
		int idxs[] = {0,1,(grid_size+1)+1,(grid_size+1)+1,(grid_size+1),0};
		for(int gx = 0;gx<grid_size;gx++){
			for(int gy = 0;gy<grid_size;gy++){
				if (mTypeMask & TOP_MASK){
					for(int i=5;i>=0;i--)mIndices.push_back(vtop+(gy*(grid_size+1))+gx+idxs[i]);
				}else{
					for(int i=0;i<6;i++)mIndices.push_back(vtop+(gy*(grid_size+1))+gx+idxs[i]);
				}
			}
		}
	}
		
	return TRUE;
}


BOOL LLVolumeFace::createCap(LLVolume* volume, BOOL partial_build)
{
	LLMemType m1(LLMemType::MTYPE_VOLUME);
	
	if (!(mTypeMask & HOLLOW_MASK) && 
		!(mTypeMask & OPEN_MASK) && 
		((volume->getParams().getPathParams().getBegin()==0.0f)&&
		(volume->getParams().getPathParams().getEnd()==1.0f))&&
		(volume->getParams().getProfileParams().getCurveType()==LL_PCODE_PROFILE_SQUARE &&
		 volume->getParams().getPathParams().getCurveType()==LL_PCODE_PATH_LINE)	
		){
		return createUnCutCubeCap(volume, partial_build);
	}

	S32 num_vertices = 0, num_indices = 0;

	const std::vector<LLVolume::Point>& mesh = volume->getMesh();
	const std::vector<LLVector3>& profile = volume->getProfile().mProfile;

	// All types of caps have the same number of vertices and indices
	num_vertices = profile.size();
	num_indices = (profile.size() - 2)*3;

	mVertices.resize(num_vertices);

	if (!partial_build)
	{
		mIndices.resize(num_indices);
	}

	S32 max_s = volume->getProfile().getTotal();
	S32 max_t = volume->getPath().mPath.size();

	mCenter.clearVec();

	S32 offset = 0;
	if (mTypeMask & TOP_MASK)
	{
		offset = (max_t-1) * max_s;
	}
	else
	{
		offset = mBeginS;
	}

	// Figure out the normal, assume all caps are flat faces.
	// Cross product to get normals.
	
	LLVector2 cuv;
	LLVector2 min_uv, max_uv;

	LLVector3& min = mExtents[0];
	LLVector3& max = mExtents[1];

	// Copy the vertices into the array
	for (S32 i = 0; i < num_vertices; i++)
	{
		if (mTypeMask & TOP_MASK)
		{
			mVertices[i].mTexCoord.mV[0] = profile[i].mV[0]+0.5f;
			mVertices[i].mTexCoord.mV[1] = profile[i].mV[1]+0.5f;
		}
		else
		{
			// Mirror for underside.
			mVertices[i].mTexCoord.mV[0] = profile[i].mV[0]+0.5f;
			mVertices[i].mTexCoord.mV[1] = 0.5f - profile[i].mV[1];
		}

		mVertices[i].mPosition = mesh[i + offset].mPos;
		
		if (i == 0)
		{
			min = max = mVertices[i].mPosition;
			min_uv = max_uv = mVertices[i].mTexCoord;
		}
		else
		{
			update_min_max(min,max, mVertices[i].mPosition);
			update_min_max(min_uv, max_uv, mVertices[i].mTexCoord);
		}
	}

	mCenter = (min+max)*0.5f;
	cuv = (min_uv + max_uv)*0.5f;

	LLVector3 binormal = calc_binormal_from_triangle( 
		mCenter, cuv,
		mVertices[0].mPosition, mVertices[0].mTexCoord,
		mVertices[1].mPosition, mVertices[1].mTexCoord);
	binormal.normVec();

	LLVector3 d0;
	LLVector3 d1;
	LLVector3 normal;

	d0 = mCenter-mVertices[0].mPosition;
	d1 = mCenter-mVertices[1].mPosition;

	normal = (mTypeMask & TOP_MASK) ? (d0%d1) : (d1%d0);
	normal.normVec();

	VertexData vd;
	vd.mPosition = mCenter;
	vd.mNormal = normal;
	vd.mBinormal = binormal;
	vd.mTexCoord = cuv;
	
	if (!(mTypeMask & HOLLOW_MASK) && !(mTypeMask & OPEN_MASK))
	{
		mVertices.push_back(vd);
		num_vertices++;
		if (!partial_build)
		{
			vector_append(mIndices, 3);
		}
	}
		
	
	for (S32 i = 0; i < num_vertices; i++)
	{
		mVertices[i].mBinormal = binormal;
		mVertices[i].mNormal = normal;
	}

	mHasBinormals = TRUE;

	if (partial_build)
	{
		return TRUE;
	}

	if (mTypeMask & HOLLOW_MASK)
	{
		if (mTypeMask & TOP_MASK)
		{
			// HOLLOW TOP
			// Does it matter if it's open or closed? - djs

			S32 pt1 = 0, pt2 = num_vertices - 1;
			S32 i = 0;
			while (pt2 - pt1 > 1)
			{
				// Use the profile points instead of the mesh, since you want
				// the un-transformed profile distances.
				LLVector3 p1 = profile[pt1];
				LLVector3 p2 = profile[pt2];
				LLVector3 pa = profile[pt1+1];
				LLVector3 pb = profile[pt2-1];

				p1.mV[VZ] = 0.f;
				p2.mV[VZ] = 0.f;
				pa.mV[VZ] = 0.f;
				pb.mV[VZ] = 0.f;

				// Use area of triangle to determine backfacing
				F32 area_1a2, area_1ba, area_21b, area_2ab;
				area_1a2 =  (p1.mV[0]*pa.mV[1] - pa.mV[0]*p1.mV[1]) +
							(pa.mV[0]*p2.mV[1] - p2.mV[0]*pa.mV[1]) +
							(p2.mV[0]*p1.mV[1] - p1.mV[0]*p2.mV[1]);

				area_1ba =  (p1.mV[0]*pb.mV[1] - pb.mV[0]*p1.mV[1]) +
							(pb.mV[0]*pa.mV[1] - pa.mV[0]*pb.mV[1]) +
							(pa.mV[0]*p1.mV[1] - p1.mV[0]*pa.mV[1]);

				area_21b =  (p2.mV[0]*p1.mV[1] - p1.mV[0]*p2.mV[1]) +
							(p1.mV[0]*pb.mV[1] - pb.mV[0]*p1.mV[1]) +
							(pb.mV[0]*p2.mV[1] - p2.mV[0]*pb.mV[1]);

				area_2ab =  (p2.mV[0]*pa.mV[1] - pa.mV[0]*p2.mV[1]) +
							(pa.mV[0]*pb.mV[1] - pb.mV[0]*pa.mV[1]) +
							(pb.mV[0]*p2.mV[1] - p2.mV[0]*pb.mV[1]);

				BOOL use_tri1a2 = TRUE;
				BOOL tri_1a2 = TRUE;
				BOOL tri_21b = TRUE;

				if (area_1a2 < 0)
				{
					tri_1a2 = FALSE;
				}
				if (area_2ab < 0)
				{
					// Can't use, because it contains point b
					tri_1a2 = FALSE;
				}
				if (area_21b < 0)
				{
					tri_21b = FALSE;
				}
				if (area_1ba < 0)
				{
					// Can't use, because it contains point b
					tri_21b = FALSE;
				}

				if (!tri_1a2)
				{
					use_tri1a2 = FALSE;
				}
				else if (!tri_21b)
				{
					use_tri1a2 = TRUE;
				}
				else
				{
					LLVector3 d1 = p1 - pa;
					LLVector3 d2 = p2 - pb;

					if (d1.magVecSquared() < d2.magVecSquared())
					{
						use_tri1a2 = TRUE;
					}
					else
					{
						use_tri1a2 = FALSE;
					}
				}

				if (use_tri1a2)
				{
					mIndices[i++] = pt1;
					mIndices[i++] = pt1 + 1;
					mIndices[i++] = pt2;
					pt1++;
				}
				else
				{
					mIndices[i++] = pt1;
					mIndices[i++] = pt2 - 1;
					mIndices[i++] = pt2;
					pt2--;
				}
			}
		}
		else
		{
			// HOLLOW BOTTOM
			// Does it matter if it's open or closed? - djs

			llassert(mTypeMask & BOTTOM_MASK);
			S32 pt1 = 0, pt2 = num_vertices - 1;

			S32 i = 0;
			while (pt2 - pt1 > 1)
			{
				// Use the profile points instead of the mesh, since you want
				// the un-transformed profile distances.
				LLVector3 p1 = profile[pt1];
				LLVector3 p2 = profile[pt2];
				LLVector3 pa = profile[pt1+1];
				LLVector3 pb = profile[pt2-1];

				p1.mV[VZ] = 0.f;
				p2.mV[VZ] = 0.f;
				pa.mV[VZ] = 0.f;
				pb.mV[VZ] = 0.f;

				// Use area of triangle to determine backfacing
				F32 area_1a2, area_1ba, area_21b, area_2ab;
				area_1a2 =  (p1.mV[0]*pa.mV[1] - pa.mV[0]*p1.mV[1]) +
							(pa.mV[0]*p2.mV[1] - p2.mV[0]*pa.mV[1]) +
							(p2.mV[0]*p1.mV[1] - p1.mV[0]*p2.mV[1]);

				area_1ba =  (p1.mV[0]*pb.mV[1] - pb.mV[0]*p1.mV[1]) +
							(pb.mV[0]*pa.mV[1] - pa.mV[0]*pb.mV[1]) +
							(pa.mV[0]*p1.mV[1] - p1.mV[0]*pa.mV[1]);

				area_21b =  (p2.mV[0]*p1.mV[1] - p1.mV[0]*p2.mV[1]) +
							(p1.mV[0]*pb.mV[1] - pb.mV[0]*p1.mV[1]) +
							(pb.mV[0]*p2.mV[1] - p2.mV[0]*pb.mV[1]);

				area_2ab =  (p2.mV[0]*pa.mV[1] - pa.mV[0]*p2.mV[1]) +
							(pa.mV[0]*pb.mV[1] - pb.mV[0]*pa.mV[1]) +
							(pb.mV[0]*p2.mV[1] - p2.mV[0]*pb.mV[1]);

				BOOL use_tri1a2 = TRUE;
				BOOL tri_1a2 = TRUE;
				BOOL tri_21b = TRUE;

				if (area_1a2 < 0)
				{
					tri_1a2 = FALSE;
				}
				if (area_2ab < 0)
				{
					// Can't use, because it contains point b
					tri_1a2 = FALSE;
				}
				if (area_21b < 0)
				{
					tri_21b = FALSE;
				}
				if (area_1ba < 0)
				{
					// Can't use, because it contains point b
					tri_21b = FALSE;
				}

				if (!tri_1a2)
				{
					use_tri1a2 = FALSE;
				}
				else if (!tri_21b)
				{
					use_tri1a2 = TRUE;
				}
				else
				{
					LLVector3 d1 = p1 - pa;
					LLVector3 d2 = p2 - pb;

					if (d1.magVecSquared() < d2.magVecSquared())
					{
						use_tri1a2 = TRUE;
					}
					else
					{
						use_tri1a2 = FALSE;
					}
				}

				// Flipped backfacing from top
				if (use_tri1a2)
				{
					mIndices[i++] = pt1;
					mIndices[i++] = pt2;
					mIndices[i++] = pt1 + 1;
					pt1++;
				}
				else
				{
					mIndices[i++] = pt1;
					mIndices[i++] = pt2;
					mIndices[i++] = pt2 - 1;
					pt2--;
				}
			}
		}
	}
	else
	{
		// Not hollow, generate the triangle fan.
		if (mTypeMask & TOP_MASK)
		{
			if (mTypeMask & OPEN_MASK)
			{
				// SOLID OPEN TOP
				// Generate indices
				// This is a tri-fan, so we reuse the same first point for all triangles.
				for (S32 i = 0; i < (num_vertices - 2); i++)
				{
					mIndices[3*i] = num_vertices - 1;
					mIndices[3*i+1] = i;
					mIndices[3*i+2] = i + 1;
				}
			}
			else
			{
				// SOLID CLOSED TOP
				for (S32 i = 0; i < (num_vertices - 2); i++)
				{				
					//MSMSM fix these caps but only for the un-cut case
					mIndices[3*i] = num_vertices - 1;
					mIndices[3*i+1] = i;
					mIndices[3*i+2] = i + 1;
				}
			}
		}
		else
		{
			if (mTypeMask & OPEN_MASK)
			{
				// SOLID OPEN BOTTOM
				// Generate indices
				// This is a tri-fan, so we reuse the same first point for all triangles.
				for (S32 i = 0; i < (num_vertices - 2); i++)
				{
					mIndices[3*i] = num_vertices - 1;
					mIndices[3*i+1] = i + 1;
					mIndices[3*i+2] = i;
				}
			}
			else
			{
				// SOLID CLOSED BOTTOM
				for (S32 i = 0; i < (num_vertices - 2); i++)
				{
					//MSMSM fix these caps but only for the un-cut case
					mIndices[3*i] = num_vertices - 1;
					mIndices[3*i+1] = i + 1;
					mIndices[3*i+2] = i;
				}
			}
		}
	}
	return TRUE;
}

void LLVolumeFace::createBinormals()
{
	LLMemType m1(LLMemType::MTYPE_VOLUME);
	
	if (!mHasBinormals)
	{
		//generate binormals
		for (U32 i = 0; i < mIndices.size()/3; i++) 
		{	//for each triangle
			const VertexData& v0 = mVertices[mIndices[i*3+0]];
			const VertexData& v1 = mVertices[mIndices[i*3+1]];
			const VertexData& v2 = mVertices[mIndices[i*3+2]];
						
			//calculate binormal
			LLVector3 binorm = calc_binormal_from_triangle(v0.mPosition, v0.mTexCoord,
															v1.mPosition, v1.mTexCoord,
															v2.mPosition, v2.mTexCoord);

			for (U32 j = 0; j < 3; j++) 
			{ //add triangle normal to vertices
				mVertices[mIndices[i*3+j]].mBinormal += binorm; // * (weight_sum - d[j])/weight_sum;
			}

			//even out quad contributions
			if (i % 2 == 0) 
			{
				mVertices[mIndices[i*3+2]].mBinormal += binorm;
			}
			else 
			{
				mVertices[mIndices[i*3+1]].mBinormal += binorm;
			}
		}

		//normalize binormals
		for (U32 i = 0; i < mVertices.size(); i++) 
		{
			mVertices[i].mBinormal.normVec();
			mVertices[i].mNormal.normVec();
		}

		mHasBinormals = TRUE;
	}
}

BOOL LLVolumeFace::createSide(LLVolume* volume, BOOL partial_build)
{
	LLMemType m1(LLMemType::MTYPE_VOLUME);
	
	BOOL flat = mTypeMask & FLAT_MASK;

	U8 sculpt_type = volume->getParams().getSculptType();
	U8 sculpt_stitching = sculpt_type & LL_SCULPT_TYPE_MASK;
	BOOL sculpt_invert = sculpt_type & LL_SCULPT_FLAG_INVERT;
	BOOL sculpt_mirror = sculpt_type & LL_SCULPT_FLAG_MIRROR;
	BOOL sculpt_reverse_horizontal = (sculpt_invert ? !sculpt_mirror : sculpt_mirror);  // XOR
	
	S32 num_vertices, num_indices;

	const std::vector<LLVolume::Point>& mesh = volume->getMesh();
	const std::vector<LLVector3>& profile = volume->getProfile().mProfile;
	const std::vector<LLPath::PathPt>& path_data = volume->getPath().mPath;

	S32 max_s = volume->getProfile().getTotal();

	S32 s, t, i;
	F32 ss, tt;

	num_vertices = mNumS*mNumT;
	num_indices = (mNumS-1)*(mNumT-1)*6;

	mVertices.resize(num_vertices);

	if (!partial_build)
	{
		mIndices.resize(num_indices);
		mEdge.resize(num_indices);
	}
	else
	{
		mHasBinormals = FALSE;
	}


	LLVector3& face_min = mExtents[0];
	LLVector3& face_max = mExtents[1];

	mCenter.clearVec();

	S32 begin_stex = llfloor( profile[mBeginS].mV[2] );
	S32 num_s = ((mTypeMask & INNER_MASK) && (mTypeMask & FLAT_MASK) && mNumS > 2) ? mNumS/2 : mNumS;

	S32 cur_vertex = 0;
	// Copy the vertices into the array
	for (t = mBeginT; t < mBeginT + mNumT; t++)
	{
		tt = path_data[t].mTexT;
		for (s = 0; s < num_s; s++)
		{
			if (mTypeMask & END_MASK)
			{
				if (s)
				{
					ss = 1.f;
				}
				else
				{
					ss = 0.f;
				}
			}
			else
			{
				// Get s value for tex-coord.
				if (!flat)
				{
					ss = profile[mBeginS + s].mV[2];
				}
				else
				{
					ss = profile[mBeginS + s].mV[2] - begin_stex;
				}
			}

			if (sculpt_reverse_horizontal)
			{
				ss = 1.f - ss;
			}
			
			// Check to see if this triangle wraps around the array.
			if (mBeginS + s >= max_s)
			{
				// We're wrapping
				i = mBeginS + s + max_s*(t-1);
			}
			else
			{
				i = mBeginS + s + max_s*t;
			}

			mVertices[cur_vertex].mPosition = mesh[i].mPos;
			mVertices[cur_vertex].mTexCoord = LLVector2(ss,tt);
		
			mVertices[cur_vertex].mNormal = LLVector3(0,0,0);
			mVertices[cur_vertex].mBinormal = LLVector3(0,0,0);
			
			if (cur_vertex == 0)
			{
				face_min = face_max = mesh[i].mPos;
			}
			else
			{
				update_min_max(face_min, face_max, mesh[i].mPos);
			}

			cur_vertex++;

			if ((mTypeMask & INNER_MASK) && (mTypeMask & FLAT_MASK) && mNumS > 2 && s > 0)
			{
				mVertices[cur_vertex].mPosition = mesh[i].mPos;
				mVertices[cur_vertex].mTexCoord = LLVector2(ss,tt);
			
				mVertices[cur_vertex].mNormal = LLVector3(0,0,0);
				mVertices[cur_vertex].mBinormal = LLVector3(0,0,0);
				cur_vertex++;
			}
		}
		
		if ((mTypeMask & INNER_MASK) && (mTypeMask & FLAT_MASK) && mNumS > 2)
		{
			if (mTypeMask & OPEN_MASK)
			{
				s = num_s-1;
			}
			else
			{
				s = 0;
			}

			i = mBeginS + s + max_s*t;
			ss = profile[mBeginS + s].mV[2] - begin_stex;
			mVertices[cur_vertex].mPosition = mesh[i].mPos;
			mVertices[cur_vertex].mTexCoord = LLVector2(ss,tt);
		
			mVertices[cur_vertex].mNormal = LLVector3(0,0,0);
			mVertices[cur_vertex].mBinormal = LLVector3(0,0,0);

			update_min_max(face_min,face_max,mesh[i].mPos);

			cur_vertex++;
		}
	}
	
	mCenter = (face_min + face_max) * 0.5f;

	S32 cur_index = 0;
	S32 cur_edge = 0;
	BOOL flat_face = mTypeMask & FLAT_MASK;

	if (!partial_build)
	{
		// Now we generate the indices.
		for (t = 0; t < (mNumT-1); t++)
		{
			for (s = 0; s < (mNumS-1); s++)
			{	
				mIndices[cur_index++] = s   + mNumS*t;			//bottom left
				mIndices[cur_index++] = s+1 + mNumS*(t+1);		//top right
				mIndices[cur_index++] = s   + mNumS*(t+1);		//top left
				mIndices[cur_index++] = s   + mNumS*t;			//bottom left
				mIndices[cur_index++] = s+1 + mNumS*t;			//bottom right
				mIndices[cur_index++] = s+1 + mNumS*(t+1);		//top right

				mEdge[cur_edge++] = (mNumS-1)*2*t+s*2+1;						//bottom left/top right neighbor face 
				if (t < mNumT-2) {												//top right/top left neighbor face 
					mEdge[cur_edge++] = (mNumS-1)*2*(t+1)+s*2+1;
				}
				else if (mNumT <= 3 || volume->getPath().isOpen() == TRUE) { //no neighbor
					mEdge[cur_edge++] = -1;
				}
				else { //wrap on T
					mEdge[cur_edge++] = s*2+1;
				}
				if (s > 0) {													//top left/bottom left neighbor face
					mEdge[cur_edge++] = (mNumS-1)*2*t+s*2-1;
				}
				else if (flat_face ||  volume->getProfile().isOpen() == TRUE) { //no neighbor
					mEdge[cur_edge++] = -1;
				}
				else {	//wrap on S
					mEdge[cur_edge++] = (mNumS-1)*2*t+(mNumS-2)*2+1;
				}
				
				if (t > 0) {													//bottom left/bottom right neighbor face
					mEdge[cur_edge++] = (mNumS-1)*2*(t-1)+s*2;
				}
				else if (mNumT <= 3 || volume->getPath().isOpen() == TRUE) { //no neighbor
					mEdge[cur_edge++] = -1;
				}
				else { //wrap on T
					mEdge[cur_edge++] = (mNumS-1)*2*(mNumT-2)+s*2;
				}
				if (s < mNumS-2) {												//bottom right/top right neighbor face
					mEdge[cur_edge++] = (mNumS-1)*2*t+(s+1)*2;
				}
				else if (flat_face || volume->getProfile().isOpen() == TRUE) { //no neighbor
					mEdge[cur_edge++] = -1;
				}
				else { //wrap on S
					mEdge[cur_edge++] = (mNumS-1)*2*t;
				}
				mEdge[cur_edge++] = (mNumS-1)*2*t+s*2;							//top right/bottom left neighbor face	
			}
		}
	}

	//generate normals 
	for (U32 i = 0; i < mIndices.size()/3; i++) //for each triangle
	{
		const S32 i0 = mIndices[i*3+0];
		const S32 i1 = mIndices[i*3+1];
		const S32 i2 = mIndices[i*3+2];
		const VertexData& v0 = mVertices[i0];
		const VertexData& v1 = mVertices[i1];
		const VertexData& v2 = mVertices[i2];
					
		//calculate triangle normal
		LLVector3 norm = (v0.mPosition-v1.mPosition) % (v0.mPosition-v2.mPosition);

		for (U32 j = 0; j < 3; j++) 
		{ //add triangle normal to vertices
			const S32 idx = mIndices[i*3+j];
			mVertices[idx].mNormal += norm; // * (weight_sum - d[j])/weight_sum;
		}

		//even out quad contributions
		if ((i & 1) == 0) 
		{
			mVertices[i2].mNormal += norm;
		}
		else 
		{
			mVertices[i1].mNormal += norm;
		}
	}
	
	// adjust normals based on wrapping and stitching
	
	BOOL s_bottom_converges = ((mVertices[0].mPosition - mVertices[mNumS*(mNumT-2)].mPosition).magVecSquared() < 0.000001f);
	BOOL s_top_converges = ((mVertices[mNumS-1].mPosition - mVertices[mNumS*(mNumT-2)+mNumS-1].mPosition).magVecSquared() < 0.000001f);
	if (sculpt_stitching == LL_SCULPT_TYPE_NONE)  // logic for non-sculpt volumes
	{
		if (volume->getPath().isOpen() == FALSE)
		{ //wrap normals on T
			for (S32 i = 0; i < mNumS; i++)
			{
				LLVector3 norm = mVertices[i].mNormal + mVertices[mNumS*(mNumT-1)+i].mNormal;
				mVertices[i].mNormal = norm;
				mVertices[mNumS*(mNumT-1)+i].mNormal = norm;
			}
		}

		if ((volume->getProfile().isOpen() == FALSE) && !(s_bottom_converges))
		{ //wrap normals on S
			for (S32 i = 0; i < mNumT; i++)
			{
				LLVector3 norm = mVertices[mNumS*i].mNormal + mVertices[mNumS*i+mNumS-1].mNormal;
				mVertices[mNumS * i].mNormal = norm;
				mVertices[mNumS * i+mNumS-1].mNormal = norm;
			}
		}
	
		if (volume->getPathType() == LL_PCODE_PATH_CIRCLE &&
			((volume->getProfileType() & LL_PCODE_PROFILE_MASK) == LL_PCODE_PROFILE_CIRCLE_HALF))
		{
			if (s_bottom_converges)
			{ //all lower S have same normal
				for (S32 i = 0; i < mNumT; i++)
				{
					mVertices[mNumS*i].mNormal = LLVector3(1,0,0);
				}
			}

			if (s_top_converges)
			{ //all upper S have same normal
				for (S32 i = 0; i < mNumT; i++)
				{
					mVertices[mNumS*i+mNumS-1].mNormal = LLVector3(-1,0,0);
				}
			}
		}
	}
	
	else  // logic for sculpt volumes
	{
		BOOL average_poles = FALSE;
		BOOL wrap_s = FALSE;
		BOOL wrap_t = FALSE;

		if (sculpt_stitching == LL_SCULPT_TYPE_SPHERE)
			average_poles = TRUE;

		if ((sculpt_stitching == LL_SCULPT_TYPE_SPHERE) ||
			(sculpt_stitching == LL_SCULPT_TYPE_TORUS) ||
			(sculpt_stitching == LL_SCULPT_TYPE_CYLINDER))
			wrap_s = TRUE;

		if (sculpt_stitching == LL_SCULPT_TYPE_TORUS)
			wrap_t = TRUE;
			
		
		if (average_poles)
		{
			// average normals for north pole
		
			LLVector3 average(0.0, 0.0, 0.0);
			for (S32 i = 0; i < mNumS; i++)
			{
				average += mVertices[i].mNormal;
			}

			// set average
			for (S32 i = 0; i < mNumS; i++)
			{
				mVertices[i].mNormal = average;
			}

			// average normals for south pole
		
			average = LLVector3(0.0, 0.0, 0.0);
			for (S32 i = 0; i < mNumS; i++)
			{
				average += mVertices[i + mNumS * (mNumT - 1)].mNormal;
			}

			// set average
			for (S32 i = 0; i < mNumS; i++)
			{
				mVertices[i + mNumS * (mNumT - 1)].mNormal = average;
			}

		}

		
		if (wrap_s)
		{
			for (S32 i = 0; i < mNumT; i++)
			{
				LLVector3 norm = mVertices[mNumS*i].mNormal + mVertices[mNumS*i+mNumS-1].mNormal;
				mVertices[mNumS * i].mNormal = norm;
				mVertices[mNumS * i+mNumS-1].mNormal = norm;
			}
		}


		
		if (wrap_t)
		{
			for (S32 i = 0; i < mNumS; i++)
			{
				LLVector3 norm = mVertices[i].mNormal + mVertices[mNumS*(mNumT-1)+i].mNormal;
				mVertices[i].mNormal = norm;
				mVertices[mNumS*(mNumT-1)+i].mNormal = norm;
			}
			
		}

	}

	return TRUE;
}

// Finds binormal based on three vertices with texture coordinates.
// Fills in dummy values if the triangle has degenerate texture coordinates.
LLVector3 calc_binormal_from_triangle( 
	const LLVector3& pos0,
	const LLVector2& tex0,
	const LLVector3& pos1,
	const LLVector2& tex1,
	const LLVector3& pos2,
	const LLVector2& tex2)
{
	LLVector3 rx0( pos0.mV[VX], tex0.mV[VX], tex0.mV[VY] );
	LLVector3 rx1( pos1.mV[VX], tex1.mV[VX], tex1.mV[VY] );
	LLVector3 rx2( pos2.mV[VX], tex2.mV[VX], tex2.mV[VY] );
	
	LLVector3 ry0( pos0.mV[VY], tex0.mV[VX], tex0.mV[VY] );
	LLVector3 ry1( pos1.mV[VY], tex1.mV[VX], tex1.mV[VY] );
	LLVector3 ry2( pos2.mV[VY], tex2.mV[VX], tex2.mV[VY] );

	LLVector3 rz0( pos0.mV[VZ], tex0.mV[VX], tex0.mV[VY] );
	LLVector3 rz1( pos1.mV[VZ], tex1.mV[VX], tex1.mV[VY] );
	LLVector3 rz2( pos2.mV[VZ], tex2.mV[VX], tex2.mV[VY] );
	
	LLVector3 r0 = (rx0 - rx1) % (rx0 - rx2);
	LLVector3 r1 = (ry0 - ry1) % (ry0 - ry2);
	LLVector3 r2 = (rz0 - rz1) % (rz0 - rz2);

	if( r0.mV[VX] && r1.mV[VX] && r2.mV[VX] )
	{
		LLVector3 binormal(
				-r0.mV[VZ] / r0.mV[VX],
				-r1.mV[VZ] / r1.mV[VX],
				-r2.mV[VZ] / r2.mV[VX]);
		// binormal.normVec();
		return binormal;
	}
	else
	{
		return LLVector3( 0, 1 , 0 );
	}
}
