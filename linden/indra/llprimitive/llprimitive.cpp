/** 
 * @file llprimitive.cpp
 * @brief LLPrimitive base class
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
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
 * online at http://secondlifegrid.net/programs/open_source/licensing/flossexception
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

#include "material_codes.h"
#include "llmemtype.h"
#include "llerror.h"
#include "message.h"
#include "llprimitive.h"
#include "llvolume.h"
#include "legacy_object_types.h"
#include "v4coloru.h"
#include "llvolumemgr.h"
#include "llstring.h"
#include "lldatapacker.h"
#include "llsdutil.h"

/**
 * exported constants
 */

const F32 OBJECT_CUT_MIN = 0.f;
const F32 OBJECT_CUT_MAX = 1.f;
const F32 OBJECT_CUT_INC = 0.05f;
const F32 OBJECT_MIN_CUT_INC = 0.02f;
const F32 OBJECT_ROTATION_PRECISION = 0.05f;

const F32 OBJECT_TWIST_MIN = -360.f;
const F32 OBJECT_TWIST_MAX =  360.f;
const F32 OBJECT_TWIST_INC =   18.f;

// This is used for linear paths,
// since twist is used in a slightly different manner.
const F32 OBJECT_TWIST_LINEAR_MIN	= -180.f;
const F32 OBJECT_TWIST_LINEAR_MAX	=  180.f;
const F32 OBJECT_TWIST_LINEAR_INC	=    9.f;

const F32 OBJECT_MIN_HOLE_SIZE = 0.05f;
const F32 OBJECT_MAX_HOLE_SIZE_X = 1.0f;
const F32 OBJECT_MAX_HOLE_SIZE_Y = 0.5f;

// Revolutions parameters.
const F32 OBJECT_REV_MIN = 1.0f;
const F32 OBJECT_REV_MAX = 4.0f;
const F32 OBJECT_REV_INC = 0.1f;

// lights
const F32 LIGHT_MIN_RADIUS = 0.0f;
const F32 LIGHT_DEFAULT_RADIUS = 5.0f;
const F32 LIGHT_MAX_RADIUS = 20.0f;
const F32 LIGHT_MIN_FALLOFF = 0.0f;
const F32 LIGHT_DEFAULT_FALLOFF = 1.0f;
const F32 LIGHT_MAX_FALLOFF = 2.0f;
const F32 LIGHT_MIN_CUTOFF = 0.0f;
const F32 LIGHT_DEFAULT_CUTOFF = 0.0f;
const F32 LIGHT_MAX_CUTOFF = 180.f;

// "Tension" => [0,10], increments of 0.1
const F32 FLEXIBLE_OBJECT_MIN_TENSION = 0.0f;
const F32 FLEXIBLE_OBJECT_DEFAULT_TENSION = 1.0f;
const F32 FLEXIBLE_OBJECT_MAX_TENSION = 10.0f; 

// "Drag" => [0,10], increments of 0.1
const F32 FLEXIBLE_OBJECT_MIN_AIR_FRICTION = 0.0f;
const F32 FLEXIBLE_OBJECT_DEFAULT_AIR_FRICTION = 2.0f;
const F32 FLEXIBLE_OBJECT_MAX_AIR_FRICTION = 10.0f;

// "Gravity" = [-10,10], increments of 0.1
const F32 FLEXIBLE_OBJECT_MIN_GRAVITY = -10.0f;
const F32 FLEXIBLE_OBJECT_DEFAULT_GRAVITY = 0.3f;
const F32 FLEXIBLE_OBJECT_MAX_GRAVITY = 10.0f;

// "Wind" = [0,10], increments of 0.1
const F32 FLEXIBLE_OBJECT_MIN_WIND_SENSITIVITY = 0.0f;
const F32 FLEXIBLE_OBJECT_DEFAULT_WIND_SENSITIVITY = 0.0f;
const F32 FLEXIBLE_OBJECT_MAX_WIND_SENSITIVITY = 10.0f;

// I'll explain later...
const F32 FLEXIBLE_OBJECT_MAX_INTERNAL_TENSION_FORCE = 0.99f; 

const F32 FLEXIBLE_OBJECT_DEFAULT_LENGTH = 1.0f;
const BOOL FLEXIBLE_OBJECT_DEFAULT_USING_COLLISION_SPHERE = FALSE;
const BOOL FLEXIBLE_OBJECT_DEFAULT_RENDERING_COLLISION_SPHERE = FALSE;


const char *SCULPT_DEFAULT_TEXTURE = "be293869-d0d9-0a69-5989-ad27f1946fd4"; // old inverted texture: "7595d345-a24c-e7ef-f0bd-78793792133e";

//static 
// LEGACY: by default we use the LLVolumeMgr::gVolumeMgr global
// TODO -- eliminate this global from the codebase!
LLVolumeMgr* LLPrimitive::sVolumeManager = NULL;

// static
void LLPrimitive::setVolumeManager( LLVolumeMgr* volume_manager )
{
	if ( !volume_manager || sVolumeManager )
	{
		llerrs << "Unable to set LLPrimitive::sVolumeManager to NULL" << llendl;
	}
	sVolumeManager = volume_manager;
}

// static
bool LLPrimitive::cleanupVolumeManager()
{
	BOOL res = FALSE;
	if (sVolumeManager) 
	{
		res = sVolumeManager->cleanup();
		delete sVolumeManager;
		sVolumeManager = NULL;
	}
	return res;
}


//===============================================================
LLPrimitive::LLPrimitive()
:	mMiscFlags(0)
{
	mPrimitiveCode = 0;

	mMaterial = LL_MCODE_STONE;
	mVolumep  = NULL;

	mChanged  = UNCHANGED;

	mPosition.setVec(0.f,0.f,0.f);
	mVelocity.setVec(0.f,0.f,0.f);
	mAcceleration.setVec(0.f,0.f,0.f);

	mRotation.loadIdentity();
	mAngularVelocity.setVec(0.f,0.f,0.f);
	
	mScale.setVec(1.f,1.f,1.f);

	mNumTEs = 0;
	mTextureList = NULL;
}

//===============================================================
LLPrimitive::~LLPrimitive()
{
	if (mTextureList)
	{
		delete [] mTextureList;
		mTextureList = NULL;
	}

	// Cleanup handled by volume manager
	if (mVolumep)
	{
		sVolumeManager->unrefVolume(mVolumep);
	}
	mVolumep = NULL;
}

//===============================================================
// static
LLPrimitive *LLPrimitive::createPrimitive(LLPCode p_code)
{
	LLMemType m1(LLMemType::MTYPE_PRIMITIVE);
	LLPrimitive *retval = new LLPrimitive();
	
	if (retval)
	{
		retval->init_primitive(p_code);
	}
	else
	{
		llerrs << "primitive allocation failed" << llendl;
	}

	return retval;
}

//===============================================================
void LLPrimitive::init_primitive(LLPCode p_code)
{
	LLMemType m1(LLMemType::MTYPE_PRIMITIVE);
	if (mNumTEs)
	{
		if (mTextureList)
		{
			delete [] mTextureList;
		}
		mTextureList = new LLTextureEntry[mNumTEs];
	}

	mPrimitiveCode = p_code;
}

void LLPrimitive::setPCode(const U8 p_code)
{
	mPrimitiveCode = p_code;
}

//===============================================================
const LLTextureEntry * LLPrimitive::getTE(const U8 te_num) const
{
	// if we're asking for a non-existent face, return null
	if (mNumTEs && (te_num< mNumTEs))
	{
		return(&mTextureList[te_num]);
	}
	else
	{	
		return(NULL);
	}
}

//===============================================================
void LLPrimitive::setNumTEs(const U8 num_tes)
{
	if (num_tes == mNumTEs)
	{
		return;
	}
	
	// Right now, we don't try and preserve entries when the number of faces
	// changes.

	LLMemType m1(LLMemType::MTYPE_PRIMITIVE);
	if (num_tes)
	{
		LLTextureEntry *new_tes;
		new_tes = new LLTextureEntry[num_tes];
		U32 i;
		for (i = 0; i < num_tes; i++)
		{
			if (i < mNumTEs)
			{
				new_tes[i] = mTextureList[i];
			}
			else if (mNumTEs)
			{
				new_tes[i] = mTextureList[mNumTEs - 1];
			}
			else
			{
				new_tes[i] = LLTextureEntry();
			}
		}
		delete[] mTextureList;
		mTextureList = new_tes;
	}
	else
	{
		delete[] mTextureList;
		mTextureList = NULL;
	}


	mNumTEs = num_tes;
}

//===============================================================
void  LLPrimitive::setAllTETextures(const LLUUID &tex_id)
{
	U8 i;

	for (i = 0; i < mNumTEs; i++)
	{
		mTextureList[i].setID(tex_id);
	}
}

//===============================================================
void LLPrimitive::setTE(const U8 index, const LLTextureEntry &te)
{
	mTextureList[index] = te;
}

S32  LLPrimitive::setTETexture(const U8 te, const LLUUID &tex_id)
{
    // if we're asking for a non-existent face, return null
	if (te >= mNumTEs)
	{
		llwarns << "setting non-existent te " << te << llendl
		return 0;
	}

	return mTextureList[te].setID(tex_id);
}

S32  LLPrimitive::setTEColor(const U8 te, const LLColor4 &color)
{
    // if we're asking for a non-existent face, return null
	if (te >= mNumTEs)
	{
		llwarns << "setting non-existent te " << te << llendl
		return 0;
	}

	return mTextureList[te].setColor(color);
}

S32  LLPrimitive::setTEColor(const U8 te, const LLColor3 &color)
{
    // if we're asking for a non-existent face, return null
	if (te >= mNumTEs)
	{
		llwarns << "setting non-existent te " << te << llendl
		return 0;
	}

	return mTextureList[te].setColor(color);
}

S32  LLPrimitive::setTEAlpha(const U8 te, const F32 alpha)
{
    // if we're asking for a non-existent face, return null
	if (te >= mNumTEs)
	{
		llwarns << "setting non-existent te " << te << llendl
		return 0;
	}

	return mTextureList[te].setAlpha(alpha);
}

//===============================================================
S32  LLPrimitive::setTEScale(const U8 te, const F32 s, const F32 t)
{
    // if we're asking for a non-existent face, return null
	if (te >= mNumTEs)
	{
		llwarns << "Setting nonexistent face" << llendl;
		return 0;
	}

	return mTextureList[te].setScale(s,t);
}


// BUG: slow - done this way because texture entries have some
// voodoo related to texture coords
S32 LLPrimitive::setTEScaleS(const U8 te, const F32 s)
{
	if (te >= mNumTEs)
	{
		llwarns << "Setting nonexistent face" << llendl;
		return 0;
	}

	F32 ignore, t;
	mTextureList[te].getScale(&ignore, &t);
	return mTextureList[te].setScale(s,t);
}


// BUG: slow - done this way because texture entries have some
// voodoo related to texture coords
S32 LLPrimitive::setTEScaleT(const U8 te, const F32 t)
{
	if (te >= mNumTEs)
	{
		llwarns << "Setting nonexistent face" << llendl;
		return 0;
	}

	F32 s, ignore;
	mTextureList[te].getScale(&s, &ignore);
	return mTextureList[te].setScale(s,t);
}


//===============================================================
S32  LLPrimitive::setTEOffset(const U8 te, const F32 s, const F32 t)
{
    // if we're asking for a non-existent face, return null
	if (te >= mNumTEs)
	{
		llwarns << "Setting nonexistent face" << llendl;
		return 0;
	}

	return mTextureList[te].setOffset(s,t);
}


// BUG: slow - done this way because texture entries have some
// voodoo related to texture coords
S32 LLPrimitive::setTEOffsetS(const U8 te, const F32 s)
{
	if (te >= mNumTEs)
	{
		llwarns << "Setting nonexistent face" << llendl;
		return 0;
	}

	F32 ignore, t;
	mTextureList[te].getOffset(&ignore, &t);
	return mTextureList[te].setOffset(s,t);
}


// BUG: slow - done this way because texture entries have some
// voodoo related to texture coords
S32 LLPrimitive::setTEOffsetT(const U8 te, const F32 t)
{
	if (te >= mNumTEs)
	{
		llwarns << "Setting nonexistent face" << llendl;
		return 0;
	}

	F32 s, ignore;
	mTextureList[te].getOffset(&s, &ignore);
	return mTextureList[te].setOffset(s,t);
}


//===============================================================
S32  LLPrimitive::setTERotation(const U8 te, const F32 r)
{
     // if we're asking for a non-existent face, return null
	if (te >= mNumTEs)
	{
		llwarns << "Setting nonexistent face" << llendl;
		return 0;
	}

	return mTextureList[te].setRotation(r);
}


//===============================================================
S32  LLPrimitive::setTEBumpShinyFullbright(const U8 te, const U8 bump)
{
    // if we're asking for a non-existent face, return null
	if (te >= mNumTEs)
	{
		llwarns << "setting non-existent te " << te << llendl
		return 0;
	}

	return mTextureList[te].setBumpShinyFullbright( bump );
}

S32  LLPrimitive::setTEMediaTexGen(const U8 te, const U8 media)
{
    // if we're asking for a non-existent face, return null
	if (te >= mNumTEs)
	{
		llwarns << "setting non-existent te " << te << llendl
		return 0;
	}

	return mTextureList[te].setMediaTexGen( media );
}

S32  LLPrimitive::setTEBumpmap(const U8 te, const U8 bump)
{
    // if we're asking for a non-existent face, return null
	if (te >= mNumTEs)
	{
		llwarns << "setting non-existent te " << te << llendl
		return 0;
	}

	return mTextureList[te].setBumpmap( bump );
}

S32  LLPrimitive::setTEBumpShiny(const U8 te, const U8 bump_shiny)
{
    // if we're asking for a non-existent face, return null
	if (te >= mNumTEs)
	{
		llwarns << "setting non-existent te " << te << llendl
		return 0;
	}

	return mTextureList[te].setBumpShiny( bump_shiny );
}

S32  LLPrimitive::setTETexGen(const U8 te, const U8 texgen)
{
    // if we're asking for a non-existent face, return null
	if (te >= mNumTEs)
	{
		llwarns << "setting non-existent te " << te << llendl
		return 0;
	}

	return mTextureList[te].setTexGen( texgen );
}

S32  LLPrimitive::setTEShiny(const U8 te, const U8 shiny)
{
    // if we're asking for a non-existent face, return null
	if (te >= mNumTEs)
	{
		llwarns << "setting non-existent te " << te << llendl
		return 0;
	}

	return mTextureList[te].setShiny( shiny );
}

S32  LLPrimitive::setTEFullbright(const U8 te, const U8 fullbright)
{
    // if we're asking for a non-existent face, return null
	if (te >= mNumTEs)
	{
		llwarns << "setting non-existent te " << te << llendl
		return 0;
	}

	return mTextureList[te].setFullbright( fullbright );
}

S32  LLPrimitive::setTEMediaFlags(const U8 te, const U8 media_flags)
{
    // if we're asking for a non-existent face, return null
	if (te >= mNumTEs)
	{
		llwarns << "setting non-existent te " << te << llendl
		return 0;
	}

	return mTextureList[te].setMediaFlags( media_flags );
}

S32 LLPrimitive::setTEGlow(const U8 te, const F32 glow)
{
	// if we're asking for a non-existent face, return null
	if (te >= mNumTEs)
	{
		llwarns << "setting non-existent te " << te << llendl
			return 0;
	}

	return mTextureList[te].setGlow( glow );
}


LLPCode LLPrimitive::legacyToPCode(const U8 legacy)
{
	// TODO: Should this default to something valid?
	// Maybe volume?
	LLPCode pcode = 0;

	switch (legacy)
	{
		/*
	case BOX:
		pcode = LL_PCODE_CUBE;
		break;
	case CYLINDER:
		pcode = LL_PCODE_CYLINDER;
		break;
	case CONE:
		pcode = LL_PCODE_CONE;
		break;
	case HALF_CONE:
		pcode = LL_PCODE_CONE_HEMI;
		break;
	case HALF_CYLINDER:
		pcode = LL_PCODE_CYLINDER_HEMI;
		break;
	case HALF_SPHERE:
		pcode = LL_PCODE_SPHERE_HEMI;
		break;
	case PRISM:
		pcode = LL_PCODE_PRISM;
		break;
	case PYRAMID:
		pcode = LL_PCODE_PYRAMID;
		break;
	case SPHERE:
		pcode = LL_PCODE_SPHERE;
		break;
	case TETRAHEDRON:
		pcode = LL_PCODE_TETRAHEDRON;
		break;
	case DEMON:
		pcode = LL_PCODE_LEGACY_DEMON;
		break;
	case LSL_TEST:
		pcode = LL_PCODE_LEGACY_LSL_TEST;
		break;
	case ORACLE:
		pcode = LL_PCODE_LEGACY_ORACLE;
		break;
	case TEXTBUBBLE:
		pcode = LL_PCODE_LEGACY_TEXT_BUBBLE;
		break;
	case ATOR:
		pcode = LL_PCODE_LEGACY_ATOR;
		break;
	case BASIC_SHOT:
		pcode = LL_PCODE_LEGACY_SHOT;
		break;
	case BIG_SHOT:
		pcode = LL_PCODE_LEGACY_SHOT_BIG;
		break;
	case BIRD:
		pcode = LL_PCODE_LEGACY_BIRD;
		break;
	case ROCK:
		pcode = LL_PCODE_LEGACY_ROCK;
		break;
	case SMOKE:
		pcode = LL_PCODE_LEGACY_SMOKE;
		break;
	case SPARK:
		pcode = LL_PCODE_LEGACY_SPARK;
		break;
		*/
	case PRIMITIVE_VOLUME:
		pcode = LL_PCODE_VOLUME;
		break;
	case GRASS:
		pcode = LL_PCODE_LEGACY_GRASS;
		break;
	case PART_SYS:
		pcode = LL_PCODE_LEGACY_PART_SYS;
		break;
	case PLAYER:
		pcode = LL_PCODE_LEGACY_AVATAR;
		break;
	case TREE:
		pcode = LL_PCODE_LEGACY_TREE;
		break;
	case TREE_NEW:
		pcode = LL_PCODE_TREE_NEW;
		break;
	default:
		llwarns << "Unknown legacy code " << legacy << " [" << (S32)legacy << "]!" << llendl;
	}

	return pcode;
}

U8 LLPrimitive::pCodeToLegacy(const LLPCode pcode)
{
	U8 legacy;
	switch (pcode)
	{
/*
	case LL_PCODE_CUBE:
		legacy = BOX;
		break;
	case LL_PCODE_CYLINDER:
		legacy = CYLINDER;
		break;
	case LL_PCODE_CONE:
		legacy = CONE;
		break;
	case LL_PCODE_CONE_HEMI:
		legacy = HALF_CONE;
		break;
	case LL_PCODE_CYLINDER_HEMI:
		legacy = HALF_CYLINDER;
		break;
	case LL_PCODE_SPHERE_HEMI:
		legacy = HALF_SPHERE;
		break;
	case LL_PCODE_PRISM:
		legacy = PRISM;
		break;
	case LL_PCODE_PYRAMID:
		legacy = PYRAMID;
		break;
	case LL_PCODE_SPHERE:
		legacy = SPHERE;
		break;
	case LL_PCODE_TETRAHEDRON:
		legacy = TETRAHEDRON;
		break;
	case LL_PCODE_LEGACY_ATOR:
		legacy = ATOR;
		break;
	case LL_PCODE_LEGACY_SHOT:
		legacy = BASIC_SHOT;
		break;
	case LL_PCODE_LEGACY_SHOT_BIG:
		legacy = BIG_SHOT;
		break;
	case LL_PCODE_LEGACY_BIRD:
		legacy = BIRD;
		break;		
	case LL_PCODE_LEGACY_DEMON:
		legacy = DEMON;
		break;
	case LL_PCODE_LEGACY_LSL_TEST:
		legacy = LSL_TEST;
		break;
	case LL_PCODE_LEGACY_ORACLE:
		legacy = ORACLE;
		break;
	case LL_PCODE_LEGACY_ROCK:
		legacy = ROCK;
		break;
	case LL_PCODE_LEGACY_TEXT_BUBBLE:
		legacy = TEXTBUBBLE;
		break;
	case LL_PCODE_LEGACY_SMOKE:
		legacy = SMOKE;
		break;
	case LL_PCODE_LEGACY_SPARK:
		legacy = SPARK;
		break;
*/
	case LL_PCODE_VOLUME:
		legacy = PRIMITIVE_VOLUME;
		break;
	case LL_PCODE_LEGACY_GRASS:
		legacy = GRASS;
		break;
	case LL_PCODE_LEGACY_PART_SYS:
		legacy = PART_SYS;
		break;
	case LL_PCODE_LEGACY_AVATAR:
		legacy = PLAYER;
		break;
	case LL_PCODE_LEGACY_TREE:
		legacy = TREE;
		break;
	case LL_PCODE_TREE_NEW:
		legacy = TREE_NEW;
		break;
	default:
		llwarns << "Unknown pcode " << (S32)pcode << ":" << pcode << "!" << llendl;
		return 0;
	}
	return legacy;
}


// static
// Don't crash or llerrs here!  This function is used for debug strings.
std::string LLPrimitive::pCodeToString(const LLPCode pcode)
{
	std::string pcode_string;

	U8 base_code = pcode & LL_PCODE_BASE_MASK;
	if (!pcode)
	{
		pcode_string = "null";
	}
	else if ((base_code) == LL_PCODE_LEGACY)
	{
		// It's a legacy object
		switch (pcode)
		{
		case LL_PCODE_LEGACY_GRASS:
		  pcode_string = "grass";
			break;
		case LL_PCODE_LEGACY_PART_SYS:
		  pcode_string = "particle system";
			break;
		case LL_PCODE_LEGACY_AVATAR:
		  pcode_string = "avatar";
			break;
		case LL_PCODE_LEGACY_TEXT_BUBBLE:
		  pcode_string = "text bubble";
			break;
		case LL_PCODE_LEGACY_TREE:
		  pcode_string = "tree";
			break;
		case LL_PCODE_TREE_NEW:
		  pcode_string = "tree_new";
			break;
		default:
		  pcode_string = llformat( "unknown legacy pcode %i",(U32)pcode);
		}
	}
	else
	{
		std::string shape;
		std::string mask;
		if (base_code == LL_PCODE_CUBE)
		{
			shape = "cube";
		}
		else if (base_code == LL_PCODE_CYLINDER)
		{
			shape = "cylinder";
		}
		else if (base_code == LL_PCODE_CONE)
		{
			shape = "cone";
		}
		else if (base_code == LL_PCODE_PRISM)
		{
			shape = "prism";
		}
		else if (base_code == LL_PCODE_PYRAMID)
		{
			shape = "pyramid";
		}
		else if (base_code == LL_PCODE_SPHERE)
		{
			shape = "sphere";
		}
		else if (base_code == LL_PCODE_TETRAHEDRON)
		{
			shape = "tetrahedron";
		}
		else if (base_code == LL_PCODE_VOLUME)
		{
			shape = "volume";
		}
		else if (base_code == LL_PCODE_APP)
		{
			shape = "app";
		}
		else
		{
			llwarns << "Unknown base mask for pcode: " << base_code << llendl;
		}

		U8 mask_code = pcode & (~LL_PCODE_BASE_MASK);
		if (base_code == LL_PCODE_APP)
		{
			mask = llformat( "%x", mask_code);
		}
		else if (mask_code & LL_PCODE_HEMI_MASK)
		{
			mask = "hemi";
		}
		else 
		{
			mask = llformat( "%x", mask_code);
		}

		if (mask[0])
		{
			pcode_string = llformat( "%s-%s", shape.c_str(), mask.c_str());
		}
		else
		{
			pcode_string = llformat( "%s", shape.c_str());
		}
	}

	return pcode_string;
}


void LLPrimitive::copyTEs(const LLPrimitive *primitivep)
{
	U32 i;
	if (primitivep->getNumTEs() != getNumTEs())
	{
		llwarns << "Primitives don't have same number of TE's" << llendl;
	}
	U32 num_tes = llmin(primitivep->getNumTEs(), getNumTEs());
	for (i = 0; i < num_tes; i++)
	{
		const LLTextureEntry *tep = primitivep->getTE(i);
		F32 s, t;
		setTETexture(i, tep->getID());
		setTEColor(i, tep->getColor());
		tep->getScale(&s, &t);
		setTEScale(i, s, t);
		tep->getOffset(&s, &t);
		setTEOffset(i, s, t);
		setTERotation(i, tep->getRotation());
		setTEBumpShinyFullbright(i, tep->getBumpShinyFullbright());
		setTEMediaTexGen(i, tep->getMediaTexGen());
		setTEGlow(i, tep->getGlow());
	}
}

S32	face_index_from_id(LLFaceID face_ID, const std::vector<LLProfile::Face>& faceArray)
{
	S32 i;
	for (i = 0; i < (S32)faceArray.size(); i++)
	{
		if (faceArray[i].mFaceID == face_ID)
		{
			return i;
		}
	}
	return -1;
}

BOOL LLPrimitive::setVolume(const LLVolumeParams &volume_params, const S32 detail, bool unique_volume)
{
	LLMemType m1(LLMemType::MTYPE_VOLUME);
	LLVolume *volumep;
	if (unique_volume)
	{
		F32 volume_detail = LLVolumeLODGroup::getVolumeScaleFromDetail(detail);
		if (mVolumep.notNull() && volume_params == mVolumep->getParams() && (volume_detail == mVolumep->getDetail()))
		{
			return FALSE;
		}
		volumep = new LLVolume(volume_params, volume_detail, FALSE, TRUE);
	}
	else
	{
		if (mVolumep.notNull())
		{
			F32 volume_detail = LLVolumeLODGroup::getVolumeScaleFromDetail(detail);
			if (volume_params == mVolumep->getParams() && (volume_detail == mVolumep->getDetail()))
			{
				return FALSE;
			}
		}

		volumep = sVolumeManager->refVolume(volume_params, detail);
		if (volumep == mVolumep)
		{
			sVolumeManager->unrefVolume( volumep );  // LLVolumeMgr::refVolume() creates a reference, but we don't need a second one.
			return TRUE;
		}
	}

	setChanged(GEOMETRY);

	
	if (!mVolumep)
	{
		mVolumep = volumep;
		//mFaceMask = mVolumep->generateFaceMask();
		setNumTEs(mVolumep->getNumFaces());
		return TRUE;
	}

	U32 old_face_mask = mVolumep->mFaceMask;

	// build the new object
	sVolumeManager->unrefVolume(mVolumep);
	mVolumep = volumep;
	
	U32 new_face_mask = mVolumep->mFaceMask;	
	if (old_face_mask != new_face_mask) 
	{
		setNumTEs(mVolumep->getNumFaces());
	}	
	
	return TRUE;
}

BOOL LLPrimitive::setMaterial(U8 material)
{
	if (material != mMaterial)
	{
		mMaterial = material;
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

void LLPrimitive::setTEArrays(const U8 size,
							  const LLUUID* image_ids,
							  const F32* scale_s,
							  const F32* scale_t)
{
	S32 cur_size = size;
	if (cur_size > getNumTEs())
	{
		llwarns << "Trying to set more TEs than exist!" << llendl;
		cur_size = getNumTEs();
	}

	S32 i;
	// Copy over image information
	for (i = 0; i < cur_size; i++)
	{
		// This is very BAD!!!!!!
		if (image_ids != NULL)
		{
			setTETexture(i,image_ids[i]);
		}
		if (scale_s && scale_t)
		{
			setTEScale(i, scale_s[i], scale_t[i]);
		}
 	}

	if (i < getNumTEs())
	{
		cur_size--;
		for (i=i; i < getNumTEs(); i++)		// the i=i removes a gcc warning
		{
			if (image_ids != NULL)
			{
				setTETexture(i, image_ids[cur_size]);
			}
			if (scale_s && scale_t)
			{
				setTEScale(i, scale_s[cur_size], scale_t[cur_size]);
			}
		}
	}
}

const F32 LL_MAX_SCALE_S = 100.0f;
const F32 LL_MAX_SCALE_T = 100.0f;
S32 LLPrimitive::packTEField(U8 *cur_ptr, U8 *data_ptr, U8 data_size, U8 last_face_index, EMsgVariableType type) const
{
	S32 face_index;
	S32 i;
	U64 exception_faces;
	U8 *start_loc = cur_ptr;

	htonmemcpy(cur_ptr,data_ptr + (last_face_index * data_size), type, data_size);
	cur_ptr += data_size;
	
	for (face_index = last_face_index-1; face_index >= 0; face_index--)
	{
		BOOL already_sent = FALSE;
		for (i = face_index+1; i <= last_face_index; i++)
		{ 
			if (!memcmp(data_ptr+(data_size *face_index), data_ptr+(data_size *i), data_size))
			{
				already_sent = TRUE;
				break;
			}
		}

		if (!already_sent)
		{
			exception_faces = 0;
			for (i = face_index; i >= 0; i--)
			{ 
				if (!memcmp(data_ptr+(data_size *face_index), data_ptr+(data_size *i), data_size))
				{
					exception_faces |= ((U64)1 << i); 
				}
			}
			
			//assign exception faces to cur_ptr
			if (exception_faces >= (0x1 << 7))
			{
				if (exception_faces >= (0x1 << 14))
				{
					if (exception_faces >= (0x1 << 21))
					{
						if (exception_faces >= (0x1 << 28))
						{
							*cur_ptr++ = (U8)(((exception_faces >> 28) & 0x7F) | 0x80);
						}
						*cur_ptr++ = (U8)(((exception_faces >> 21) & 0x7F) | 0x80);
					}
					*cur_ptr++ = (U8)(((exception_faces >> 14) & 0x7F) | 0x80);
				}
				*cur_ptr++ = (U8)(((exception_faces >> 7) & 0x7F) | 0x80);
			}
			
			*cur_ptr++ = (U8)(exception_faces & 0x7F);
			
			htonmemcpy(cur_ptr,data_ptr + (face_index * data_size), type, data_size);
			cur_ptr += data_size;
   		}
	}
	return (S32)(cur_ptr - start_loc);
}

S32 LLPrimitive::unpackTEField(U8 *cur_ptr, U8 *buffer_end, U8 *data_ptr, U8 data_size, U8 face_count, EMsgVariableType type)
{
	U8 *start_loc = cur_ptr;
	U64 i;
	htonmemcpy(data_ptr,cur_ptr, type,data_size);
	cur_ptr += data_size;

	for (i = 1; i < face_count; i++)
	{
		// Already unswizzled, don't need to unswizzle it again!
		memcpy(data_ptr+(i*data_size),data_ptr,data_size);	/* Flawfinder: ignore */ 
	}
	
	while ((cur_ptr < buffer_end) && (*cur_ptr != 0))
	{
//		llinfos << "TE exception" << llendl;
		i = 0;
		while (*cur_ptr & 0x80)
		{
			i |= ((*cur_ptr++) & 0x7F);
			i = i << 7;
		}

		i |= *cur_ptr++;

		for (S32 j = 0; j < face_count; j++)
		{
			if (i & 0x01)
			{
				htonmemcpy(data_ptr+(j*data_size),cur_ptr,type,data_size);
//				char foo[64];
//				sprintf(foo,"%x %x",*(data_ptr+(j*data_size)), *(data_ptr+(j*data_size)+1));
//				llinfos << "Assigning " << foo << " to face " << j << llendl;			
			}
			i = i >> 1;
		}
		cur_ptr += data_size;		
	}
	return (S32)(cur_ptr - start_loc);
}


// Pack information about all texture entries into container:
// { TextureEntry Variable 2 }
// Includes information about image ID, color, scale S,T, offset S,T and rotation
BOOL LLPrimitive::packTEMessage(LLMessageSystem *mesgsys) const
{
	const U32 MAX_TES = 32;

	U8     image_ids[MAX_TES*16];
	U8     colors[MAX_TES*4];
	F32    scale_s[MAX_TES];
	F32    scale_t[MAX_TES];
	S16    offset_s[MAX_TES];
	S16    offset_t[MAX_TES];
	S16    image_rot[MAX_TES];
	U8	   bump[MAX_TES];
	U8	   media_flags[MAX_TES];
    U8     glow[MAX_TES];
	
	const U32 MAX_TE_BUFFER = 4096;
	U8 packed_buffer[MAX_TE_BUFFER];
	U8 *cur_ptr = packed_buffer;
	
	S32 last_face_index = getNumTEs() - 1;
	
	if (last_face_index > -1)
	{
		// ...if we hit the front, send one image id
		S8 face_index;
		LLColor4U coloru;
		for (face_index = 0; face_index <= last_face_index; face_index++)
		{
			// Directly sending image_ids is not safe!
			memcpy(&image_ids[face_index*16],getTE(face_index)->getID().mData,16);	/* Flawfinder: ignore */ 

			// Cast LLColor4 to LLColor4U
			coloru.setVec( getTE(face_index)->getColor() );

			// Note:  This is an optimization to send common colors (1.f, 1.f, 1.f, 1.f)
			// as all zeros.  However, the subtraction and addition must be done in unsigned
			// byte space, not in float space, otherwise off-by-one errors occur. JC
			colors[4*face_index]     = 255 - coloru.mV[0];
			colors[4*face_index + 1] = 255 - coloru.mV[1];
			colors[4*face_index + 2] = 255 - coloru.mV[2];
			colors[4*face_index + 3] = 255 - coloru.mV[3];

			const LLTextureEntry* te = getTE(face_index);
			scale_s[face_index] = (F32) te->mScaleS;
			scale_t[face_index] = (F32) te->mScaleT;
			offset_s[face_index] = (S16) llround((llclamp(te->mOffsetS,-1.0f,1.0f) * (F32)0x7FFF)) ;
			offset_t[face_index] = (S16) llround((llclamp(te->mOffsetT,-1.0f,1.0f) * (F32)0x7FFF)) ;
			image_rot[face_index] = (S16) llround(((fmod(te->mRotation, F_TWO_PI)/F_TWO_PI) * (F32)0x7FFF));
			bump[face_index] = te->getBumpShinyFullbright();
			media_flags[face_index] = te->getMediaTexGen();
			glow[face_index] = (U8) llround((llclamp(te->getGlow(), 0.0f, 1.0f) * (F32)0xFF));
		}

		cur_ptr += packTEField(cur_ptr, (U8 *)image_ids, sizeof(LLUUID),last_face_index, MVT_LLUUID);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8 *)colors, 4 ,last_face_index, MVT_U8);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8 *)scale_s, 4 ,last_face_index, MVT_F32);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8 *)scale_t, 4 ,last_face_index, MVT_F32);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8 *)offset_s, 2 ,last_face_index, MVT_S16Array);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8 *)offset_t, 2 ,last_face_index, MVT_S16Array);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8 *)image_rot, 2 ,last_face_index, MVT_S16Array);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8 *)bump, 1 ,last_face_index, MVT_U8);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8 *)media_flags, 1 ,last_face_index, MVT_U8);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8 *)glow, 1 ,last_face_index, MVT_U8);
	}
   	mesgsys->addBinaryDataFast(_PREHASH_TextureEntry, packed_buffer, (S32)(cur_ptr - packed_buffer));

	return FALSE;
}


BOOL LLPrimitive::packTEMessage(LLDataPacker &dp) const
{
	const U32 MAX_TES = 32;

	U8     image_ids[MAX_TES*16];
	U8     colors[MAX_TES*4];
	F32    scale_s[MAX_TES];
	F32    scale_t[MAX_TES];
	S16    offset_s[MAX_TES];
	S16    offset_t[MAX_TES];
	S16    image_rot[MAX_TES];
	U8	   bump[MAX_TES];
	U8	   media_flags[MAX_TES];
    U8     glow[MAX_TES];
	
	const U32 MAX_TE_BUFFER = 4096;
	U8 packed_buffer[MAX_TE_BUFFER];
	U8 *cur_ptr = packed_buffer;
	
	S32 last_face_index = getNumTEs() - 1;
	
	if (last_face_index > -1)
	{
		// ...if we hit the front, send one image id
		S8 face_index;
		LLColor4U coloru;
		for (face_index = 0; face_index <= last_face_index; face_index++)
		{
			// Directly sending image_ids is not safe!
			memcpy(&image_ids[face_index*16],getTE(face_index)->getID().mData,16);	/* Flawfinder: ignore */ 

			// Cast LLColor4 to LLColor4U
			coloru.setVec( getTE(face_index)->getColor() );

			// Note:  This is an optimization to send common colors (1.f, 1.f, 1.f, 1.f)
			// as all zeros.  However, the subtraction and addition must be done in unsigned
			// byte space, not in float space, otherwise off-by-one errors occur. JC
			colors[4*face_index]     = 255 - coloru.mV[0];
			colors[4*face_index + 1] = 255 - coloru.mV[1];
			colors[4*face_index + 2] = 255 - coloru.mV[2];
			colors[4*face_index + 3] = 255 - coloru.mV[3];

			const LLTextureEntry* te = getTE(face_index);
			scale_s[face_index] = (F32) te->mScaleS;
			scale_t[face_index] = (F32) te->mScaleT;
			offset_s[face_index] = (S16) llround((llclamp(te->mOffsetS,-1.0f,1.0f) * (F32)0x7FFF)) ;
			offset_t[face_index] = (S16) llround((llclamp(te->mOffsetT,-1.0f,1.0f) * (F32)0x7FFF)) ;
			image_rot[face_index] = (S16) llround(((fmod(te->mRotation, F_TWO_PI)/F_TWO_PI) * (F32)0x7FFF));
			bump[face_index] = te->getBumpShinyFullbright();
			media_flags[face_index] = te->getMediaTexGen();
            glow[face_index] = (U8) llround((llclamp(te->getGlow(), 0.0f, 1.0f) * (F32)0xFF));
		}

		cur_ptr += packTEField(cur_ptr, (U8 *)image_ids, sizeof(LLUUID),last_face_index, MVT_LLUUID);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8 *)colors, 4 ,last_face_index, MVT_U8);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8 *)scale_s, 4 ,last_face_index, MVT_F32);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8 *)scale_t, 4 ,last_face_index, MVT_F32);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8 *)offset_s, 2 ,last_face_index, MVT_S16Array);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8 *)offset_t, 2 ,last_face_index, MVT_S16Array);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8 *)image_rot, 2 ,last_face_index, MVT_S16Array);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8 *)bump, 1 ,last_face_index, MVT_U8);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8 *)media_flags, 1 ,last_face_index, MVT_U8);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8 *)glow, 1 ,last_face_index, MVT_U8);
	}

	dp.packBinaryData(packed_buffer, (S32)(cur_ptr - packed_buffer), "TextureEntry");
	return FALSE;
}

S32 LLPrimitive::unpackTEMessage(LLMessageSystem *mesgsys, char *block_name)
{
	return(unpackTEMessage(mesgsys,block_name,-1));
}

S32 LLPrimitive::unpackTEMessage(LLMessageSystem *mesgsys, char *block_name, const S32 block_num)
{
	// use a negative block_num to indicate a single-block read (a non-variable block)
	S32 retval = 0;
	const U32 MAX_TES = 32;

	// Avoid construction of 32 UUIDs per call. JC

	U8     image_data[MAX_TES*16];
	U8	  colors[MAX_TES*4];
	F32    scale_s[MAX_TES];
	F32    scale_t[MAX_TES];
	S16    offset_s[MAX_TES];
	S16    offset_t[MAX_TES];
	S16    image_rot[MAX_TES];
	U8	   bump[MAX_TES];
	U8	   media_flags[MAX_TES];
    U8     glow[MAX_TES];
	
	const U32 MAX_TE_BUFFER = 4096;
	U8 packed_buffer[MAX_TE_BUFFER];
	U8 *cur_ptr = packed_buffer;

	U32 size;
	U32 face_count = 0;

	if (block_num < 0)
	{
		size = mesgsys->getSizeFast(block_name, _PREHASH_TextureEntry);
	}
	else
	{
		size = mesgsys->getSizeFast(block_name, block_num, _PREHASH_TextureEntry);
	}

	if (size == 0)
	{
		return retval;
	}

	if (block_num < 0)
	{
		mesgsys->getBinaryDataFast(block_name, _PREHASH_TextureEntry, packed_buffer, 0, 0, MAX_TE_BUFFER);
	}
	else
	{
		mesgsys->getBinaryDataFast(block_name, _PREHASH_TextureEntry, packed_buffer, 0, block_num, MAX_TE_BUFFER);
	}

	face_count = getNumTEs();

	cur_ptr += unpackTEField(cur_ptr, packed_buffer+size, (U8 *)image_data, 16, face_count, MVT_LLUUID);
	cur_ptr++;
	cur_ptr += unpackTEField(cur_ptr, packed_buffer+size, (U8 *)colors, 4, face_count, MVT_U8);
	cur_ptr++;
	cur_ptr += unpackTEField(cur_ptr, packed_buffer+size, (U8 *)scale_s, 4, face_count, MVT_F32);
	cur_ptr++;
	cur_ptr += unpackTEField(cur_ptr, packed_buffer+size, (U8 *)scale_t, 4, face_count, MVT_F32);
	cur_ptr++;
	cur_ptr += unpackTEField(cur_ptr, packed_buffer+size, (U8 *)offset_s, 2, face_count, MVT_S16Array);
	cur_ptr++;
	cur_ptr += unpackTEField(cur_ptr, packed_buffer+size, (U8 *)offset_t, 2, face_count, MVT_S16Array);
	cur_ptr++;
	cur_ptr += unpackTEField(cur_ptr, packed_buffer+size, (U8 *)image_rot, 2, face_count, MVT_S16Array);
	cur_ptr++;
	cur_ptr += unpackTEField(cur_ptr, packed_buffer+size, (U8 *)bump, 1, face_count, MVT_U8);
	cur_ptr++;
	cur_ptr += unpackTEField(cur_ptr, packed_buffer+size, (U8 *)media_flags, 1, face_count, MVT_U8);
	cur_ptr++;
	cur_ptr += unpackTEField(cur_ptr, packed_buffer+size, (U8 *)glow, 1, face_count, MVT_U8);
	
	LLColor4 color;
	LLColor4U coloru;
	for (U32 i = 0; i < face_count; i++)
	{
		retval |= setTETexture(i, ((LLUUID*)image_data)[i]);
		retval |= setTEScale(i, scale_s[i], scale_t[i]);
		retval |= setTEOffset(i, (F32)offset_s[i] / (F32)0x7FFF, (F32) offset_t[i] / (F32) 0x7FFF);
		retval |= setTERotation(i, ((F32)image_rot[i]/ (F32)0x7FFF) * F_TWO_PI);
		retval |= setTEBumpShinyFullbright(i, bump[i]);
		retval |= setTEMediaTexGen(i, media_flags[i]);
		retval |= setTEGlow(i, (F32)glow[i] / (F32)0xFF);
		coloru = LLColor4U(colors + 4*i);

		// Note:  This is an optimization to send common colors (1.f, 1.f, 1.f, 1.f)
		// as all zeros.  However, the subtraction and addition must be done in unsigned
		// byte space, not in float space, otherwise off-by-one errors occur. JC
		color.mV[VRED]		= F32(255 - coloru.mV[VRED])   / 255.f;
		color.mV[VGREEN]	= F32(255 - coloru.mV[VGREEN]) / 255.f;
		color.mV[VBLUE]		= F32(255 - coloru.mV[VBLUE])  / 255.f;
		color.mV[VALPHA]	= F32(255 - coloru.mV[VALPHA]) / 255.f;

		retval |= setTEColor(i, color);
	}

	return retval;
}

S32 LLPrimitive::unpackTEMessage(LLDataPacker &dp)
{
	// use a negative block_num to indicate a single-block read (a non-variable block)
	S32 retval = 0;
	const U32 MAX_TES = 32;

	// Avoid construction of 32 UUIDs per call
	static LLUUID image_ids[MAX_TES];

	U8     image_data[MAX_TES*16];
	U8	   colors[MAX_TES*4];
	F32    scale_s[MAX_TES];
	F32    scale_t[MAX_TES];
	S16    offset_s[MAX_TES];
	S16    offset_t[MAX_TES];
	S16    image_rot[MAX_TES];
	U8	   bump[MAX_TES];
	U8	   media_flags[MAX_TES];
    U8     glow[MAX_TES];

	const U32 MAX_TE_BUFFER = 4096;
	U8 packed_buffer[MAX_TE_BUFFER];
	U8 *cur_ptr = packed_buffer;

	S32 size;
	U32 face_count = 0;

	if (!dp.unpackBinaryData(packed_buffer, size, "TextureEntry"))
	{
		retval = TEM_INVALID;
		llwarns << "Bad texture entry block!  Abort!" << llendl;
		return retval;
	}

	if (size == 0)
	{
		return retval;
	}

	face_count = getNumTEs();
	U32 i;

	cur_ptr += unpackTEField(cur_ptr, packed_buffer+size, (U8 *)image_data, 16, face_count, MVT_LLUUID);
	cur_ptr++;
	cur_ptr += unpackTEField(cur_ptr, packed_buffer+size, (U8 *)colors, 4, face_count, MVT_U8);
	cur_ptr++;
	cur_ptr += unpackTEField(cur_ptr, packed_buffer+size, (U8 *)scale_s, 4, face_count, MVT_F32);
	cur_ptr++;
	cur_ptr += unpackTEField(cur_ptr, packed_buffer+size, (U8 *)scale_t, 4, face_count, MVT_F32);
	cur_ptr++;
	cur_ptr += unpackTEField(cur_ptr, packed_buffer+size, (U8 *)offset_s, 2, face_count, MVT_S16Array);
	cur_ptr++;
	cur_ptr += unpackTEField(cur_ptr, packed_buffer+size, (U8 *)offset_t, 2, face_count, MVT_S16Array);
	cur_ptr++;
	cur_ptr += unpackTEField(cur_ptr, packed_buffer+size, (U8 *)image_rot, 2, face_count, MVT_S16Array);
	cur_ptr++;
	cur_ptr += unpackTEField(cur_ptr, packed_buffer+size, (U8 *)bump, 1, face_count, MVT_U8);
	cur_ptr++;
	cur_ptr += unpackTEField(cur_ptr, packed_buffer+size, (U8 *)media_flags, 1, face_count, MVT_U8);
	cur_ptr++;
	cur_ptr += unpackTEField(cur_ptr, packed_buffer+size, (U8 *)glow, 1, face_count, MVT_U8);

	for (i = 0; i < face_count; i++)
	{
		memcpy(image_ids[i].mData,&image_data[i*16],16);	/* Flawfinder: ignore */ 	
	}
	
	LLColor4 color;
	LLColor4U coloru;
	for (i = 0; i < face_count; i++)
	{
		retval |= setTETexture(i, image_ids[i]);
		retval |= setTEScale(i, scale_s[i], scale_t[i]);
		retval |= setTEOffset(i, (F32)offset_s[i] / (F32)0x7FFF, (F32) offset_t[i] / (F32) 0x7FFF);
		retval |= setTERotation(i, ((F32)image_rot[i]/ (F32)0x7FFF) * F_TWO_PI);
		retval |= setTEBumpShinyFullbright(i, bump[i]);
		retval |= setTEMediaTexGen(i, media_flags[i]);
		retval |= setTEGlow(i, (F32)glow[i] / (F32)0xFF);
		coloru = LLColor4U(colors + 4*i);

		// Note:  This is an optimization to send common colors (1.f, 1.f, 1.f, 1.f)
		// as all zeros.  However, the subtraction and addition must be done in unsigned
		// byte space, not in float space, otherwise off-by-one errors occur. JC
		color.mV[VRED]		= F32(255 - coloru.mV[VRED])   / 255.f;
		color.mV[VGREEN]	= F32(255 - coloru.mV[VGREEN]) / 255.f;
		color.mV[VBLUE]		= F32(255 - coloru.mV[VBLUE])  / 255.f;
		color.mV[VALPHA]	= F32(255 - coloru.mV[VALPHA]) / 255.f;

		retval |= setTEColor(i, color);
	}

	return retval;
}

void LLPrimitive::setTextureList(LLTextureEntry *listp)
{
	LLTextureEntry* old_texture_list = mTextureList;
	mTextureList = listp;
 	delete[] old_texture_list;
}

//============================================================================

// Moved from llselectmgr.cpp
// BUG: Only works for boxes.
// Face numbering for flex boxes as of 1.14.2

// static
bool LLPrimitive::getTESTAxes(const U8 face, U32* s_axis, U32* t_axis)
{
	if (face == 0)
	{
		*s_axis = VX; *t_axis = VY;
		return true;
	}
	else if (face == 1)
	{
		*s_axis = VX; *t_axis = VZ;
		return true;
	}
	else if (face == 2)
	{
		*s_axis = VY; *t_axis = VZ;
		return true;
	}
	else if (face == 3)
	{
		*s_axis = VX; *t_axis = VZ;
		return true;
	}
	else if (face == 4)
	{
		*s_axis = VY; *t_axis = VZ;
		return true;
	}
	else if (face == 5)
	{
		*s_axis = VX; *t_axis = VY;
		return true;
	}
	else
	{
		// unknown face
		return false;
	}
}

//============================================================================

//static 
BOOL LLNetworkData::isValid(U16 param_type, U32 size)
{
	// ew - better mechanism needed
	
	switch(param_type)
	{
	case PARAMS_FLEXIBLE:
		return (size == 16);
	case PARAMS_LIGHT:
		return (size == 16);
	case PARAMS_SCULPT:
		return (size == 17);
	}
	
	return FALSE;
}

//============================================================================

LLLightParams::LLLightParams()
{
	mColor.setToWhite();
	mRadius = 10.f;
	mCutoff = 0.0f;
	mFalloff = 0.75f;

	mType = PARAMS_LIGHT;
}

BOOL LLLightParams::pack(LLDataPacker &dp) const
{
	LLColor4U color4u(mColor);
	dp.packColor4U(color4u, "color");
	dp.packF32(mRadius, "radius");
	dp.packF32(mCutoff, "cutoff");
	dp.packF32(mFalloff, "falloff");
	return TRUE;
}

BOOL LLLightParams::unpack(LLDataPacker &dp)
{
	LLColor4U color;
	dp.unpackColor4U(color, "color");
	setColor(LLColor4(color));

	F32 radius;
	dp.unpackF32(radius, "radius");
	setRadius(radius);

	F32 cutoff;
	dp.unpackF32(cutoff, "cutoff");
	setCutoff(cutoff);

	F32 falloff;
	dp.unpackF32(falloff, "falloff");
	setFalloff(falloff);
	
	return TRUE;
}

bool LLLightParams::operator==(const LLNetworkData& data) const
{
	if (data.mType != PARAMS_LIGHT)
	{
		return false;
	}
	const LLLightParams *param = (const LLLightParams*)&data;
	if (param->mColor != mColor ||
		param->mRadius != mRadius ||
		param->mCutoff != mCutoff ||
		param->mFalloff != mFalloff)
	{
		return false;
	}
	return true;
}

void LLLightParams::copy(const LLNetworkData& data)
{
	const LLLightParams *param = (LLLightParams*)&data;
	mType = param->mType;
	mColor = param->mColor;
	mRadius = param->mRadius;
	mCutoff = param->mCutoff;
	mFalloff = param->mFalloff;
}

LLSD LLLightParams::asLLSD() const
{
	LLSD sd;
	
	sd["color"] = ll_sd_from_color4(getColor());
	sd["radius"] = getRadius();
	sd["falloff"] = getFalloff();
	sd["cutoff"] = getCutoff();
		
	return sd;
}

bool LLLightParams::fromLLSD(LLSD& sd)
{
	const char *w;
	w = "color";
	if (sd.has(w))
	{
		setColor( ll_color4_from_sd(sd["color"]) );
	} else goto fail;
	w = "radius";
	if (sd.has(w))
	{
		setRadius( (F32)sd[w].asReal() );
	} else goto fail;
	w = "falloff";
	if (sd.has(w))
	{
		setFalloff( (F32)sd[w].asReal() );
	} else goto fail;
	w = "cutoff";
	if (sd.has(w))
	{
		setCutoff( (F32)sd[w].asReal() );
	} else goto fail;
	
	return true;
 fail:
	return false;
}

//============================================================================

LLFlexibleObjectData::LLFlexibleObjectData()
{
	mSimulateLOD				= FLEXIBLE_OBJECT_DEFAULT_NUM_SECTIONS;
	mGravity					= FLEXIBLE_OBJECT_DEFAULT_GRAVITY;
	mAirFriction				= FLEXIBLE_OBJECT_DEFAULT_AIR_FRICTION;
	mWindSensitivity			= FLEXIBLE_OBJECT_DEFAULT_WIND_SENSITIVITY;
	mTension					= FLEXIBLE_OBJECT_DEFAULT_TENSION;
	//mUsingCollisionSphere		= FLEXIBLE_OBJECT_DEFAULT_USING_COLLISION_SPHERE;
	//mRenderingCollisionSphere	= FLEXIBLE_OBJECT_DEFAULT_RENDERING_COLLISION_SPHERE;
	mUserForce					= LLVector3(0.f, 0.f, 0.f);

	mType = PARAMS_FLEXIBLE;
}

BOOL LLFlexibleObjectData::pack(LLDataPacker &dp) const
{
	// Custom, uber-svelte pack "softness" in upper bits of tension & drag
	U8 bit1 = (mSimulateLOD & 2) << 6;
	U8 bit2 = (mSimulateLOD & 1) << 7;
	dp.packU8((U8)(mTension*10.01f) + bit1, "tension");
	dp.packU8((U8)(mAirFriction*10.01f) + bit2, "drag");
	dp.packU8((U8)((mGravity+10.f)*10.01f), "gravity");
	dp.packU8((U8)(mWindSensitivity*10.01f), "wind");
	dp.packVector3(mUserForce, "userforce");
	return TRUE;
}

BOOL LLFlexibleObjectData::unpack(LLDataPacker &dp)
{
	U8 tension, friction, gravity, wind;
	U8 bit1, bit2;
	dp.unpackU8(tension, "tension");	bit1 = (tension >> 6) & 2;
										mTension = ((F32)(tension&0x7f))/10.f;
	dp.unpackU8(friction, "drag");		bit2 = (friction >> 7) & 1;
										mAirFriction = ((F32)(friction&0x7f))/10.f;
										mSimulateLOD = bit1 | bit2;
	dp.unpackU8(gravity, "gravity");	mGravity = ((F32)gravity)/10.f - 10.f;
	dp.unpackU8(wind, "wind");			mWindSensitivity = ((F32)wind)/10.f;
	if (dp.hasNext())
	{
		dp.unpackVector3(mUserForce, "userforce");
	}
	else
	{
		mUserForce.setVec(0.f, 0.f, 0.f);
	}
	return TRUE;
}

bool LLFlexibleObjectData::operator==(const LLNetworkData& data) const
{
	if (data.mType != PARAMS_FLEXIBLE)
	{
		return false;
	}
	LLFlexibleObjectData *flex_data = (LLFlexibleObjectData*)&data;
	return (mSimulateLOD == flex_data->mSimulateLOD &&
			mGravity == flex_data->mGravity &&
			mAirFriction == flex_data->mAirFriction &&
			mWindSensitivity == flex_data->mWindSensitivity &&
			mTension == flex_data->mTension &&
			mUserForce == flex_data->mUserForce);
			//mUsingCollisionSphere == flex_data->mUsingCollisionSphere &&
			//mRenderingCollisionSphere == flex_data->mRenderingCollisionSphere
}

void LLFlexibleObjectData::copy(const LLNetworkData& data)
{
	const LLFlexibleObjectData *flex_data = (LLFlexibleObjectData*)&data;
	mSimulateLOD = flex_data->mSimulateLOD;
	mGravity = flex_data->mGravity;
	mAirFriction = flex_data->mAirFriction;
	mWindSensitivity = flex_data->mWindSensitivity;
	mTension = flex_data->mTension;
	mUserForce = flex_data->mUserForce;
	//mUsingCollisionSphere = flex_data->mUsingCollisionSphere;
	//mRenderingCollisionSphere = flex_data->mRenderingCollisionSphere;
}

LLSD LLFlexibleObjectData::asLLSD() const
{
	LLSD sd;

	sd["air_friction"] = getAirFriction();
	sd["gravity"] = getGravity();
	sd["simulate_lod"] = getSimulateLOD();
	sd["tension"] = getTension();
	sd["user_force"] = getUserForce().getValue();
	sd["wind_sensitivity"] = getWindSensitivity();
	
	return sd;
}

bool LLFlexibleObjectData::fromLLSD(LLSD& sd)
{
	const char *w;
	w = "air_friction";
	if (sd.has(w))
	{
		setAirFriction( (F32)sd[w].asReal() );
	} else goto fail;
	w = "gravity";
	if (sd.has(w))
	{
		setGravity( (F32)sd[w].asReal() );
	} else goto fail;
	w = "simulate_lod";
	if (sd.has(w))
	{
		setSimulateLOD( sd[w].asInteger() );
	} else goto fail;
	w = "tension";
	if (sd.has(w))
	{
		setTension( (F32)sd[w].asReal() );
	} else goto fail;
	w = "user_force";
	if (sd.has(w))
	{
		LLVector3 user_force = ll_vector3_from_sd(sd[w], 0);
		setUserForce( user_force );
	} else goto fail;
	w = "wind_sensitivity";
	if (sd.has(w))
	{
		setWindSensitivity( (F32)sd[w].asReal() );
	} else goto fail;
	
	return true;
 fail:
	return false;
}

//============================================================================

LLSculptParams::LLSculptParams()
{
	mType = PARAMS_SCULPT;
	mSculptTexture.set(SCULPT_DEFAULT_TEXTURE);
	mSculptType = LL_SCULPT_TYPE_SPHERE;
}

BOOL LLSculptParams::pack(LLDataPacker &dp) const
{
	dp.packUUID(mSculptTexture, "texture");
	dp.packU8(mSculptType, "type");
	
	return TRUE;
}

BOOL LLSculptParams::unpack(LLDataPacker &dp)
{
	dp.unpackUUID(mSculptTexture, "texture");
	dp.unpackU8(mSculptType, "type");
	
	return TRUE;
}

bool LLSculptParams::operator==(const LLNetworkData& data) const
{
	if (data.mType != PARAMS_SCULPT)
	{
		return false;
	}
	
	const LLSculptParams *param = (const LLSculptParams*)&data;
	if ( (param->mSculptTexture != mSculptTexture) ||
		 (param->mSculptType != mSculptType) )
		 
	{
		return false;
	}
	
	return true;
}

void LLSculptParams::copy(const LLNetworkData& data)
{
	const LLSculptParams *param = (LLSculptParams*)&data;
	mSculptTexture = param->mSculptTexture;
	mSculptType = param->mSculptType;
}



LLSD LLSculptParams::asLLSD() const
{
	LLSD sd;
	
	sd["texture"] = mSculptTexture;
	sd["type"] = mSculptType;
		
	return sd;
}

bool LLSculptParams::fromLLSD(LLSD& sd)
{
	const char *w;
	w = "texture";
	if (sd.has(w))
	{
		setSculptTexture( sd[w] );
	} else goto fail;
	w = "type";
	if (sd.has(w))
	{
		setSculptType( (U8)sd[w].asInteger() );
	} else goto fail;
	
	return true;
 fail:
	return false;
}

