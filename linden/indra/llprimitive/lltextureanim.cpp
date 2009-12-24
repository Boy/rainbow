/** 
 * @file lltextureanim.cpp
 * @brief LLTextureAnim base class
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

#include "lltextureanim.h"
#include "message.h"
#include "lldatapacker.h"

const S32 TA_BLOCK_SIZE = 16;

LLTextureAnim::LLTextureAnim()
{
	reset();
}


LLTextureAnim::~LLTextureAnim()
{
}


void LLTextureAnim::reset()
{
	mMode = 0;
	mFace = -1;
	mSizeX = 4;
	mSizeY = 4;
	mStart = 0.f;
	mLength = 0.f;
	mRate = 1.f;
}

BOOL LLTextureAnim::equals(const LLTextureAnim &other) const
{
	if (mMode != other.mMode)
	{
		return FALSE;
	}
	if (mFace != other.mFace)
	{
		return FALSE;
	}
	if (mSizeX != other.mSizeX)
	{
		return FALSE;
	}
	if (mSizeY != other.mSizeY)
	{
		return FALSE;
	}
	if (mStart != other.mStart)
	{
		return FALSE;
	}
	if (mLength != other.mLength)
	{
		return FALSE;
	}
	if (mRate != other.mRate)
	{
		return FALSE;
	}

	return TRUE;
}
void LLTextureAnim::packTAMessage(LLMessageSystem *mesgsys) const
{
	U8 data[TA_BLOCK_SIZE];
	data[0] = mMode;
	data[1] = mFace;
	data[2] = mSizeX;
	data[3] = mSizeY;
	htonmemcpy(data + 4, &mStart, MVT_F32, sizeof(F32));
	htonmemcpy(data + 8, &mLength, MVT_F32, sizeof(F32));
	htonmemcpy(data + 12, &mRate, MVT_F32, sizeof(F32));

	mesgsys->addBinaryDataFast(_PREHASH_TextureAnim, data, TA_BLOCK_SIZE);
}


void LLTextureAnim::packTAMessage(LLDataPacker &dp) const
{
	U8 data[TA_BLOCK_SIZE];
	data[0] = mMode;
	data[1] = mFace;
	data[2] = mSizeX;
	data[3] = mSizeY;
	htonmemcpy(data + 4, &mStart, MVT_F32, sizeof(F32));
	htonmemcpy(data + 8, &mLength, MVT_F32, sizeof(F32));
	htonmemcpy(data + 12, &mRate, MVT_F32, sizeof(F32));

	dp.packBinaryData(data, TA_BLOCK_SIZE, "TextureAnimation");
}


void LLTextureAnim::unpackTAMessage(LLMessageSystem *mesgsys, const S32 block_num)
{
	S32 size = mesgsys->getSizeFast(_PREHASH_ObjectData, block_num, _PREHASH_TextureAnim);

	if (size != TA_BLOCK_SIZE)
	{
		if (size)
		{
			llwarns << "Bad size " << size << " for TA block, ignoring." << llendl;
		}
		mMode = 0;
		return;
	}

	U8 data[TA_BLOCK_SIZE];
	mesgsys->getBinaryDataFast(_PREHASH_ObjectData, _PREHASH_TextureAnim, data, TA_BLOCK_SIZE, block_num);

	mMode = data[0];
	mFace = data[1];
	if (mMode & LLTextureAnim::SMOOTH)
	{
		mSizeX = llmax((U8)0, data[2]);
		mSizeY = llmax((U8)0, data[3]);
	}
	else
	{
		mSizeX = llmax((U8)1, data[2]);
		mSizeY = llmax((U8)1, data[3]);
	}
	htonmemcpy(&mStart, data + 4, MVT_F32, sizeof(F32));
	htonmemcpy(&mLength, data + 8, MVT_F32, sizeof(F32));
	htonmemcpy(&mRate, data + 12, MVT_F32, sizeof(F32));
}

void LLTextureAnim::unpackTAMessage(LLDataPacker &dp)
{
	S32 size;
	U8 data[TA_BLOCK_SIZE];
	dp.unpackBinaryData(data, size, "TextureAnimation");
	if (size != TA_BLOCK_SIZE)
	{
		if (size)
		{
			llwarns << "Bad size " << size << " for TA block, ignoring." << llendl;
		}
		mMode = 0;
		return;
	}

	mMode = data[0];
	mFace = data[1];
	mSizeX = llmax((U8)1, data[2]);
	mSizeY = llmax((U8)1, data[3]);
	htonmemcpy(&mStart, data + 4, MVT_F32, sizeof(F32));
	htonmemcpy(&mLength, data + 8, MVT_F32, sizeof(F32));
	htonmemcpy(&mRate, data + 12, MVT_F32, sizeof(F32));
}

LLSD LLTextureAnim::asLLSD() const
{
	LLSD sd;
	sd["mode"] = mMode;
	sd["face"] = mFace;
	sd["sizeX"] = mSizeX;
	sd["sizeY"] = mSizeY;
	sd["start"] = mStart;
	sd["length"] = mLength;
	sd["rate"] = mRate;
	return sd;
}

bool LLTextureAnim::fromLLSD(LLSD& sd)
{
	const char *w;
	w = "mode";
	if (sd.has(w))
	{
		mMode =	(U8)sd[w].asInteger();
	} else goto fail;

	w = "face";
	if (sd.has(w))
	{
		mFace =	(S8)sd[w].asInteger();
	} else goto fail;

	w = "sizeX";
	if (sd.has(w))
	{
		mSizeX = (U8)sd[w].asInteger();
	} else goto fail;

	w = "sizeY";
	if (sd.has(w))
	{
		mSizeY = (U8)sd[w].asInteger();
	} else goto fail;

	w = "start";
	if (sd.has(w))
	{
		mStart = (F32)sd[w].asReal();
	} else goto fail;

	w = "length";
	if (sd.has(w))
	{
		mLength = (F32)sd[w].asReal();
	} else goto fail;

	w = "rate";
	if (sd.has(w))
	{
		mRate =	(F32)sd[w].asReal();
	} else goto fail;

	return true;
fail:
	return false;
}
