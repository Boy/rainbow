/** 
 * @file llcolorscheme.h
 * @brief Definition of colors used for map
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 * 
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#ifndef LL_LLCOLORSCHEME_H
#define LL_LLCOLORSCHEME_H

#include "v4color.h"

extern LLColor4 gTrackColor;
extern LLColor4 gSelfMapColor;
extern LLColor4 gAvatarMapColor;
extern LLColor4 gFriendMapColor;
extern LLColor4 gLandmarkMapColor;
extern LLColor4 gLocationMapColor;
extern LLColor4 gTelehubMapColor;
extern LLColor4 gFrustumMapColor;
extern LLColor4 gRotatingFrustumMapColor;
extern LLColor4 gEventColor;
extern LLColor4 gPopularColor;
extern LLColor4 gPickColor;
extern LLColor4 gHomeColor;
extern LLColor4 gDisabledTrackColor;

void init_colors();

#endif
