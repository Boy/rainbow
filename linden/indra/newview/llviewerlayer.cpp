/** 
 * @file llviewerlayer.cpp
 * @brief LLViewerLayer class implementation
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

#include "llviewerprecompiledheaders.h"

#include "llviewerlayer.h"
#include "llerror.h"
#include "llmath.h"

LLViewerLayer::LLViewerLayer(const S32 width, const F32 scale)
{
	mWidth = width;
	mScale = scale;
	mScaleInv = 1.f/scale;
	mDatap = new F32[width*width];

	for (S32 i = 0; i < width*width; i++)
	{
		*(mDatap + i) = 0.f;
	}
}

LLViewerLayer::~LLViewerLayer()
{
	delete[] mDatap;
	mDatap = NULL;
}

F32 LLViewerLayer::getValue(const S32 x, const S32 y) const
{
//	llassert(x >= 0);
//	llassert(x < mWidth);
//	llassert(y >= 0);
//	llassert(y < mWidth);

	return *(mDatap + x + y*mWidth);
}

F32 LLViewerLayer::getValueScaled(const F32 x, const F32 y) const
{
	S32 x1, x2, y1, y2;
	F32 x_frac, y_frac;

	x_frac = x*mScaleInv;
	x1 = llfloor(x_frac);
	x2 = x1 + 1;
	x_frac -= x1;

	y_frac = y*mScaleInv;
	y1 = llfloor(y_frac);
	y2 = y1 + 1;
	y_frac -= y1;

	x1 = llmin((S32)mWidth-1, x1);
	x1 = llmax(0, x1);
	x2 = llmin((S32)mWidth-1, x2);
	x2 = llmax(0, x2);
	y1 = llmin((S32)mWidth-1, y1);
	y1 = llmax(0, y1);
	y2 = llmin((S32)mWidth-1, y2);
	y2 = llmax(0, y2);

	// Take weighted average of all four points (bilinear interpolation)
	S32 row1 = y1 * mWidth;
	S32 row2 = y2 * mWidth;
	
	// Access in squential order in memory, and don't use immediately.
	F32 row1_left  = mDatap[ row1 + x1 ];
	F32 row1_right = mDatap[ row1 + x2 ];
	F32 row2_left  = mDatap[ row2 + x1 ];
	F32 row2_right = mDatap[ row2 + x2 ];
	
	F32 row1_interp = row1_left - x_frac * (row1_left - row1_right);
	F32 row2_interp = row2_left - x_frac * (row2_left - row2_right);
	
	return row1_interp - y_frac * (row1_interp - row2_interp);
}
