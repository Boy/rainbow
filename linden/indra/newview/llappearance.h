/** 
 * @file llappearance.h
 * @brief LLAppearance class definition
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

#ifndef LL_LLAPPEARANCE_H
#define LL_LLAPPEARANCE_H

#include "lluuid.h"

class LLAppearance
{
public:
	LLAppearance()										{}
	~LLAppearance()										{ mParamMap.clear(); } 

	void	addParam( S32 id, F32 value )				{ mParamMap[id] = value; }
	F32		getParam( S32 id, F32 defval )				{ return get_if_there(mParamMap, id, defval ); }

	void	addTexture( S32 te, const LLUUID& uuid )	{ if( te < LLVOAvatar::TEX_NUM_ENTRIES ) mTextures[te] = uuid; }
	const LLUUID& getTexture( S32 te )					{ return ( te < LLVOAvatar::TEX_NUM_ENTRIES ) ? mTextures[te] : LLUUID::null; }
	
	void	clear()										{ mParamMap.clear(); for( S32 i=0; i<LLVOAvatar::TEX_NUM_ENTRIES; i++ ) mTextures[i].setNull(); }

	typedef std::map<S32, F32> param_map_t;
	param_map_t mParamMap;
	LLUUID	mTextures[LLVOAvatar::TEX_NUM_ENTRIES];
};

#endif  // LL_LLAPPEARANCE_H
