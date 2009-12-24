/** 
 * @file lldrawpoolterrain.h
 * @brief LLDrawPoolTerrain class definition
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

#ifndef LL_LLDRAWPOOLTERRAIN_H
#define LL_LLDRAWPOOLTERRAIN_H

#include "lldrawpool.h"

class LLDrawPoolTerrain : public LLFacePool
{
	LLPointer<LLViewerImage> mTexturep;
public:
	enum
	{
		VERTEX_DATA_MASK =	LLVertexBuffer::MAP_VERTEX |
							LLVertexBuffer::MAP_NORMAL |
							LLVertexBuffer::MAP_TEXCOORD |
							LLVertexBuffer::MAP_TEXCOORD2 |
							LLVertexBuffer::MAP_COLOR
	};

	virtual U32 getVertexDataMask() { return VERTEX_DATA_MASK; }
	static S32 getDetailMode();

	LLDrawPoolTerrain(LLViewerImage *texturep);
	virtual ~LLDrawPoolTerrain();

	/*virtual*/ LLDrawPool *instancePool();


	/*virtual*/ void render(S32 pass = 0);
	/*virtual*/ void prerender();
	/*virtual*/ void beginRenderPass( S32 pass );
	/*virtual*/ void endRenderPass( S32 pass );
	/*virtual*/ void renderForSelect();
	/*virtual*/ void dirtyTextures(const std::set<LLViewerImage*>& textures);
	/*virtual*/ LLViewerImage *getTexture();
	/*virtual*/ LLViewerImage *getDebugTexture();
	/*virtual*/ LLColor3 getDebugColor() const; // For AGP debug display

	LLPointer<LLViewerImage> mAlphaRampImagep;
	LLPointer<LLViewerImage> m2DAlphaRampImagep;
	LLPointer<LLViewerImage> mAlphaNoiseImagep;

	static S32 sDetailMode;
	static F32 sDetailScale; // meters per texture
protected:
	void renderSimple();
	void renderOwnership();

	void renderFull2TU();
	void renderFull4TU();
	void renderFullShader();
};

#endif // LL_LLDRAWPOOLSIMPLE_H
