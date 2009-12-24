/** 
 * @file lldrawpoolterrain.cpp
 * @brief LLDrawPoolTerrain class implementation
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

#include "lldrawpoolterrain.h"

#include "llfasttimer.h"

#include "llagent.h"
#include "llviewercontrol.h"
#include "lldrawable.h"
#include "llface.h"
#include "llsky.h"
#include "llsurface.h"
#include "llsurfacepatch.h"
#include "llviewerregion.h"
#include "llvlcomposition.h"
#include "llviewerparcelmgr.h"		// for gRenderParcelOwnership
#include "llviewerparceloverlay.h"
#include "llvosurfacepatch.h"
#include "llviewercamera.h"
#include "llviewerimagelist.h" // To get alpha gradients
#include "llworld.h"
#include "pipeline.h"
#include "llviewershadermgr.h"
#include "llrender.h"

const F32 DETAIL_SCALE = 1.f/16.f;
int DebugDetailMap = 0;

S32 LLDrawPoolTerrain::sDetailMode = 1;
F32 LLDrawPoolTerrain::sDetailScale = DETAIL_SCALE;
static LLGLSLShader* sShader = NULL;

LLDrawPoolTerrain::LLDrawPoolTerrain(LLViewerImage *texturep) :
	LLFacePool(POOL_TERRAIN),
	mTexturep(texturep)
{
	// Hack!
	sDetailScale = 1.f/gSavedSettings.getF32("RenderTerrainScale");
	sDetailMode = gSavedSettings.getS32("RenderTerrainDetail");
	mAlphaRampImagep = gImageList.getImageFromFile("alpha_gradient.tga",
													TRUE, TRUE, GL_ALPHA8, GL_ALPHA,
													LLUUID("e97cf410-8e61-7005-ec06-629eba4cd1fb"));

	gGL.getTexUnit(0)->bind(mAlphaRampImagep.get());
	mAlphaRampImagep->setClamp(TRUE, TRUE);

	m2DAlphaRampImagep = gImageList.getImageFromFile("alpha_gradient_2d.j2c",
													TRUE, TRUE, GL_ALPHA8, GL_ALPHA,
													LLUUID("38b86f85-2575-52a9-a531-23108d8da837"));

	gGL.getTexUnit(0)->bind(m2DAlphaRampImagep.get());
	m2DAlphaRampImagep->setClamp(TRUE, TRUE);
	
	mTexturep->setBoostLevel(LLViewerImage::BOOST_TERRAIN);
	
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
}

LLDrawPoolTerrain::~LLDrawPoolTerrain()
{
	llassert( gPipeline.findPool( getType(), getTexture() ) == NULL );
}


LLDrawPool *LLDrawPoolTerrain::instancePool()
{
	return new LLDrawPoolTerrain(mTexturep);
}


void LLDrawPoolTerrain::prerender()
{
	mVertexShaderLevel = LLViewerShaderMgr::instance()->getVertexShaderLevel(LLViewerShaderMgr::SHADER_ENVIRONMENT);
	if (mVertexShaderLevel > 0)
	{
		sDetailMode = 1;
	}
	else
	{
		sDetailMode = gSavedSettings.getS32("RenderTerrainDetail");
	}
}

void LLDrawPoolTerrain::beginRenderPass( S32 pass )
{
	LLFastTimer t(LLFastTimer::FTM_RENDER_TERRAIN);
	LLFacePool::beginRenderPass(pass);

	sShader = LLPipeline::sUnderWaterRender ? 
					&gTerrainWaterProgram :
					&gTerrainProgram;

	if (mVertexShaderLevel > 1 && sShader->mShaderLevel > 0)
	{
		sShader->bind();
	}
}

void LLDrawPoolTerrain::endRenderPass( S32 pass )
{
	LLFastTimer t(LLFastTimer::FTM_RENDER_TERRAIN);
	LLFacePool::endRenderPass(pass);

	if (mVertexShaderLevel > 1 && sShader->mShaderLevel > 0) {
		sShader->unbind();
	}
}

//static
S32 LLDrawPoolTerrain::getDetailMode()
{
	return sDetailMode;
}

void LLDrawPoolTerrain::render(S32 pass)
{
	LLFastTimer t(LLFastTimer::FTM_RENDER_TERRAIN);
	
	if (mDrawFace.empty())
	{
		return;
	}

	// Hack! Get the region that this draw pool is rendering from!
	LLViewerRegion *regionp = mDrawFace[0]->getDrawable()->getVObj()->getRegion();
	LLVLComposition *compp = regionp->getComposition();
	for (S32 i = 0; i < 4; i++)
	{
		compp->mDetailTextures[i]->setBoostLevel(LLViewerImage::BOOST_TERRAIN);
		compp->mDetailTextures[i]->addTextureStats(1024.f*1024.f); // assume large pixel area
	}

	if (!gGLManager.mHasMultitexture)
	{
		// No multitexture, render simple land.
		renderSimple(); // Render without multitexture
		return;
	}
	// Render simplified land if video card can't do sufficient multitexturing
	if (!gGLManager.mHasARBEnvCombine || (gGLManager.mNumTextureUnits < 2))
	{
		renderSimple(); // Render without multitexture
		return;
	}

	LLGLSPipeline gls;
	LLOverrideFaceColor override(this, 1.f, 1.f, 1.f, 1.f);

	if (mVertexShaderLevel > 1 && sShader->mShaderLevel > 0)
	{
		gPipeline.enableLightsDynamic();
		renderFullShader();
	}
	else
	{
		gPipeline.enableLightsStatic();

		if (sDetailMode == 0){
			renderSimple();
		} else if (gGLManager.mNumTextureUnits < 4){
			renderFull2TU();
		} else {
			renderFull4TU();
		}
	}

	// Special-case for land ownership feedback
	if (gSavedSettings.getBOOL("ShowParcelOwners"))
	{
		if (mVertexShaderLevel > 1)
		{ //use fullbright shader for highlighting
			LLGLSLShader* old_shader = sShader;
			sShader->unbind();
			sShader = &gObjectFullbrightProgram;
			sShader->bind();
			renderOwnership();
			sShader = old_shader;
			sShader->bind();
		}
		else
		{
			gPipeline.disableLights();
			renderOwnership();
		}
	}
}


void LLDrawPoolTerrain::renderFullShader()
{
	// Hack! Get the region that this draw pool is rendering from!
	LLViewerRegion *regionp = mDrawFace[0]->getDrawable()->getVObj()->getRegion();
	LLVLComposition *compp = regionp->getComposition();
	LLViewerImage *detail_texture0p = compp->mDetailTextures[0];
	LLViewerImage *detail_texture1p = compp->mDetailTextures[1];
	LLViewerImage *detail_texture2p = compp->mDetailTextures[2];
	LLViewerImage *detail_texture3p = compp->mDetailTextures[3];

	LLVector3d region_origin_global = gAgent.getRegion()->getOriginGlobal();
	F32 offset_x = (F32)fmod(region_origin_global.mdV[VX], 1.0/(F64)sDetailScale)*sDetailScale;
	F32 offset_y = (F32)fmod(region_origin_global.mdV[VY], 1.0/(F64)sDetailScale)*sDetailScale;

	LLVector4 tp0, tp1;
	
	tp0.setVec(sDetailScale, 0.0f, 0.0f, offset_x);
	tp1.setVec(0.0f, sDetailScale, 0.0f, offset_y);

	//
	// detail texture 0
	//
	S32 detail0 = sShader->enableTexture(LLViewerShaderMgr::TERRAIN_DETAIL0);
	gGL.getTexUnit(detail0)->bind(detail_texture0p);
	gGL.getTexUnit(0)->activate();

	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);

	glTexGenfv(GL_S, GL_OBJECT_PLANE, tp0.mV);
	glTexGenfv(GL_T, GL_OBJECT_PLANE, tp1.mV);
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);

	//
	// detail texture 1
	//
	S32 detail1 = sShader->enableTexture(LLViewerShaderMgr::TERRAIN_DETAIL1); 
	gGL.getTexUnit(detail1)->bind(detail_texture1p);
	
	/// ALPHA TEXTURE COORDS 0:
	gGL.getTexUnit(1)->activate();
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	
	// detail texture 2
	//
	S32 detail2 = sShader->enableTexture(LLViewerShaderMgr::TERRAIN_DETAIL2);
	gGL.getTexUnit(detail2)->bind(detail_texture2p);

	gGL.getTexUnit(2)->activate();
	
	/// ALPHA TEXTURE COORDS 1:
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glTranslatef(-2.f, 0.f, 0.f);
	glMatrixMode(GL_MODELVIEW);

	//
	// detail texture 3
	//
	S32 detail3 = sShader->enableTexture(LLViewerShaderMgr::TERRAIN_DETAIL3);
	gGL.getTexUnit(detail3)->bind(detail_texture3p);
	
	/// ALPHA TEXTURE COORDS 2:
	gGL.getTexUnit(3)->activate();
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glTranslatef(-1.f, 0.f, 0.f);
	glMatrixMode(GL_MODELVIEW);

	//
	// Alpha Ramp 
	//
	S32 alpha_ramp = sShader->enableTexture(LLViewerShaderMgr::TERRAIN_ALPHARAMP);
	gGL.getTexUnit(alpha_ramp)->bind(m2DAlphaRampImagep.get());
		
	// GL_BLEND disabled by default
	drawLoop();

	// Disable multitexture
	sShader->disableTexture(LLViewerShaderMgr::TERRAIN_ALPHARAMP);
	sShader->disableTexture(LLViewerShaderMgr::TERRAIN_DETAIL0);
	sShader->disableTexture(LLViewerShaderMgr::TERRAIN_DETAIL1);
	sShader->disableTexture(LLViewerShaderMgr::TERRAIN_DETAIL2);
	sShader->disableTexture(LLViewerShaderMgr::TERRAIN_DETAIL3);

	gGL.getTexUnit(alpha_ramp)->unbind(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(4)->disable();
	gGL.getTexUnit(4)->activate();
	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);

	gGL.getTexUnit(detail3)->unbind(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(3)->disable();
	gGL.getTexUnit(3)->activate();
	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);

	gGL.getTexUnit(detail2)->unbind(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(2)->disable();
	gGL.getTexUnit(2)->activate();
	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);

	gGL.getTexUnit(detail1)->unbind(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(1)->disable();
	gGL.getTexUnit(1)->activate();
	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	
	//----------------------------------------------------------------------------
	// Restore Texture Unit 0 defaults
	
	gGL.getTexUnit(detail0)->unbind(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(0)->enable(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(0)->activate();
	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
}

void LLDrawPoolTerrain::renderFull4TU()
{
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);

	// Hack! Get the region that this draw pool is rendering from!
	LLViewerRegion *regionp = mDrawFace[0]->getDrawable()->getVObj()->getRegion();
	LLVLComposition *compp = regionp->getComposition();
	LLViewerImage *detail_texture0p = compp->mDetailTextures[0];
	LLViewerImage *detail_texture1p = compp->mDetailTextures[1];
	LLViewerImage *detail_texture2p = compp->mDetailTextures[2];
	LLViewerImage *detail_texture3p = compp->mDetailTextures[3];

	LLVector3d region_origin_global = gAgent.getRegion()->getOriginGlobal();
	F32 offset_x = (F32)fmod(region_origin_global.mdV[VX], 1.0/(F64)sDetailScale)*sDetailScale;
	F32 offset_y = (F32)fmod(region_origin_global.mdV[VY], 1.0/(F64)sDetailScale)*sDetailScale;

	LLVector4 tp0, tp1;
	
	tp0.setVec(sDetailScale, 0.0f, 0.0f, offset_x);
	tp1.setVec(0.0f, sDetailScale, 0.0f, offset_y);

	gGL.blendFunc(LLRender::BF_ONE_MINUS_SOURCE_ALPHA, LLRender::BF_SOURCE_ALPHA);
	
	//----------------------------------------------------------------------------
	// Pass 1/1

	//
	// Stage 0: detail texture 0
	//
	gGL.getTexUnit(0)->activate();
	gGL.getTexUnit(0)->bind(detail_texture0p);
	glClientActiveTextureARB(GL_TEXTURE0_ARB);
	
	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);

	glTexGenfv(GL_S, GL_OBJECT_PLANE, tp0.mV);
	glTexGenfv(GL_T, GL_OBJECT_PLANE, tp1.mV);

	gGL.getTexUnit(0)->setTextureColorBlend(LLTexUnit::TBO_REPLACE, LLTexUnit::TBS_TEX_COLOR);

	//
	// Stage 1: Generate alpha ramp for detail0/detail1 transition
	//

	gGL.getTexUnit(1)->bind(m2DAlphaRampImagep.get());
	gGL.getTexUnit(1)->enable(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(1)->activate();
	glClientActiveTextureARB(GL_TEXTURE1_ARB);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	// Care about alpha only
	gGL.getTexUnit(1)->setTextureColorBlend(LLTexUnit::TBO_REPLACE, LLTexUnit::TBS_PREV_COLOR);
	gGL.getTexUnit(1)->setTextureAlphaBlend(LLTexUnit::TBO_REPLACE, LLTexUnit::TBS_TEX_ALPHA);

	//
	// Stage 2: Interpolate detail1 with existing based on ramp
	//
	gGL.getTexUnit(2)->bind(detail_texture1p);
	gGL.getTexUnit(2)->enable(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(2)->activate();

	glClientActiveTextureARB(GL_TEXTURE2_ARB);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGenfv(GL_S, GL_OBJECT_PLANE, tp0.mV);
	glTexGenfv(GL_T, GL_OBJECT_PLANE, tp1.mV);

	gGL.getTexUnit(2)->setTextureColorBlend(LLTexUnit::TBO_LERP_PREV_ALPHA, LLTexUnit::TBS_PREV_COLOR, LLTexUnit::TBS_TEX_COLOR);

	//
	// Stage 3: Modulate with primary (vertex) color for lighting
	//
	gGL.getTexUnit(3)->bind(detail_texture1p);
	gGL.getTexUnit(3)->enable(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(3)->activate();
	glClientActiveTextureARB(GL_TEXTURE3_ARB);

	// Set alpha texture and do lighting modulation
	gGL.getTexUnit(3)->setTextureColorBlend(LLTexUnit::TBO_MULT, LLTexUnit::TBS_PREV_COLOR, LLTexUnit::TBS_VERT_COLOR);

	gGL.getTexUnit(0)->activate();
	glClientActiveTextureARB(GL_TEXTURE0_ARB);

	// GL_BLEND disabled by default
	drawLoop();

	//----------------------------------------------------------------------------
	// Second pass

	// Stage 0: Write detail3 into base
	//
	gGL.getTexUnit(0)->activate();
	gGL.getTexUnit(0)->bind(detail_texture3p);
	glClientActiveTextureARB(GL_TEXTURE0_ARB);

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGenfv(GL_S, GL_OBJECT_PLANE, tp0.mV);
	glTexGenfv(GL_T, GL_OBJECT_PLANE, tp1.mV);

	gGL.getTexUnit(0)->setTextureColorBlend(LLTexUnit::TBO_REPLACE, LLTexUnit::TBS_TEX_COLOR);

	//
	// Stage 1: Generate alpha ramp for detail2/detail3 transition
	//
	gGL.getTexUnit(1)->bind(m2DAlphaRampImagep.get());
	gGL.getTexUnit(1)->enable(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(1)->activate();
	glClientActiveTextureARB(GL_TEXTURE1_ARB);

	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	// Set the texture matrix
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glTranslatef(-2.f, 0.f, 0.f);

	// Care about alpha only
	gGL.getTexUnit(1)->setTextureColorBlend(LLTexUnit::TBO_REPLACE, LLTexUnit::TBS_PREV_COLOR);
	gGL.getTexUnit(1)->setTextureAlphaBlend(LLTexUnit::TBO_REPLACE, LLTexUnit::TBS_TEX_ALPHA);


	//
	// Stage 2: Interpolate detail2 with existing based on ramp
	//
	gGL.getTexUnit(2)->bind(detail_texture2p);
	gGL.getTexUnit(2)->enable(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(2)->activate();

	glClientActiveTextureARB(GL_TEXTURE2_ARB);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGenfv(GL_S, GL_OBJECT_PLANE, tp0.mV);
	glTexGenfv(GL_T, GL_OBJECT_PLANE, tp1.mV);

	gGL.getTexUnit(2)->setTextureColorBlend(LLTexUnit::TBO_LERP_PREV_ALPHA, LLTexUnit::TBS_TEX_COLOR, LLTexUnit::TBS_PREV_COLOR);	

	//
	// Stage 3: Generate alpha ramp for detail1/detail2 transition
	//
	gGL.getTexUnit(3)->bind(m2DAlphaRampImagep.get());
	gGL.getTexUnit(3)->enable(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(3)->activate();

	glClientActiveTextureARB(GL_TEXTURE3_ARB);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	// Set the texture matrix
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glTranslatef(-1.f, 0.f, 0.f);
  
	// Set alpha texture and do lighting modulation
	gGL.getTexUnit(3)->setTextureColorBlend(LLTexUnit::TBO_MULT, LLTexUnit::TBS_PREV_COLOR, LLTexUnit::TBS_VERT_COLOR);
	gGL.getTexUnit(3)->setTextureAlphaBlend(LLTexUnit::TBO_REPLACE, LLTexUnit::TBS_TEX_ALPHA);

	gGL.getTexUnit(0)->activate();
	glClientActiveTextureARB(GL_TEXTURE0_ARB);
	{
		LLGLEnable blend(GL_BLEND);
		drawLoop();
	}

	LLVertexBuffer::unbind();
	// Disable multitexture
	gGL.getTexUnit(3)->unbind(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(3)->disable();
	gGL.getTexUnit(3)->activate();
	glClientActiveTextureARB(GL_TEXTURE3_ARB);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);

	gGL.getTexUnit(2)->unbind(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(2)->disable();
	gGL.getTexUnit(2)->activate();
	glClientActiveTextureARB(GL_TEXTURE2_ARB);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);

	gGL.getTexUnit(1)->unbind(LLTexUnit::TT_TEXTURE);	
	gGL.getTexUnit(1)->disable();
	gGL.getTexUnit(1)->activate();
 	glClientActiveTextureARB(GL_TEXTURE1_ARB);
 	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);

	// Restore blend state
	gGL.setSceneBlendType(LLRender::BT_ALPHA);
	
	//----------------------------------------------------------------------------
	// Restore Texture Unit 0 defaults
	
	gGL.getTexUnit(0)->activate();
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	glClientActiveTextureARB(GL_TEXTURE0_ARB);
	glDisableClientState(GL_NORMAL_ARRAY);

	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);

	gGL.getTexUnit(0)->setTextureBlendType(LLTexUnit::TB_MULT);
}

void LLDrawPoolTerrain::renderFull2TU()
{
	// Hack! Get the region that this draw pool is rendering from!
	LLViewerRegion *regionp = mDrawFace[0]->getDrawable()->getVObj()->getRegion();
	LLVLComposition *compp = regionp->getComposition();
	LLViewerImage *detail_texture0p = compp->mDetailTextures[0];
	LLViewerImage *detail_texture1p = compp->mDetailTextures[1];
	LLViewerImage *detail_texture2p = compp->mDetailTextures[2];
	LLViewerImage *detail_texture3p = compp->mDetailTextures[3];

	LLVector3d region_origin_global = gAgent.getRegion()->getOriginGlobal();
	F32 offset_x = (F32)fmod(region_origin_global.mdV[VX], 1.0/(F64)sDetailScale)*sDetailScale;
	F32 offset_y = (F32)fmod(region_origin_global.mdV[VY], 1.0/(F64)sDetailScale)*sDetailScale;

	LLVector4 tp0, tp1;
	
	tp0.setVec(sDetailScale, 0.0f, 0.0f, offset_x);
	tp1.setVec(0.0f, sDetailScale, 0.0f, offset_y);

	gGL.blendFunc(LLRender::BF_ONE_MINUS_SOURCE_ALPHA, LLRender::BF_SOURCE_ALPHA);
	
	//----------------------------------------------------------------------------
	// Pass 1/4

	//
	// Stage 0: Render detail 0 into base
	//
	gGL.getTexUnit(0)->bind(detail_texture0p);
	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);

	glTexGenfv(GL_S, GL_OBJECT_PLANE, tp0.mV);
	glTexGenfv(GL_T, GL_OBJECT_PLANE, tp1.mV);

	gGL.getTexUnit(0)->setTextureColorBlend(LLTexUnit::TBO_MULT, LLTexUnit::TBS_TEX_COLOR, LLTexUnit::TBS_VERT_COLOR);

	drawLoop();

	//----------------------------------------------------------------------------
	// Pass 2/4
	
	//
	// Stage 0: Generate alpha ramp for detail0/detail1 transition
	//
	gGL.getTexUnit(0)->bind(m2DAlphaRampImagep.get());
	
	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);
	
	// Care about alpha only
	gGL.getTexUnit(0)->setTextureColorBlend(LLTexUnit::TBO_REPLACE, LLTexUnit::TBS_PREV_COLOR);
	gGL.getTexUnit(0)->setTextureAlphaBlend(LLTexUnit::TBO_REPLACE, LLTexUnit::TBS_TEX_ALPHA);


	//
	// Stage 1: Write detail1
	//
	gGL.getTexUnit(1)->bind(detail_texture1p);
	gGL.getTexUnit(1)->enable(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(1)->activate();

	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGenfv(GL_S, GL_OBJECT_PLANE, tp0.mV);
	glTexGenfv(GL_T, GL_OBJECT_PLANE, tp1.mV);

	gGL.getTexUnit(1)->setTextureColorBlend(LLTexUnit::TBO_MULT, LLTexUnit::TBS_TEX_COLOR, LLTexUnit::TBS_VERT_COLOR);
	gGL.getTexUnit(1)->setTextureAlphaBlend(LLTexUnit::TBO_REPLACE, LLTexUnit::TBS_PREV_ALPHA);

	gGL.getTexUnit(0)->activate();
	{
		LLGLEnable blend(GL_BLEND);
		drawLoop();
	}
	//----------------------------------------------------------------------------
	// Pass 3/4
	
	//
	// Stage 0: Generate alpha ramp for detail1/detail2 transition
	//
	gGL.getTexUnit(0)->bind(m2DAlphaRampImagep.get());

	// Set the texture matrix
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glTranslatef(-1.f, 0.f, 0.f);

	// Care about alpha only
	gGL.getTexUnit(0)->setTextureColorBlend(LLTexUnit::TBO_REPLACE, LLTexUnit::TBS_PREV_COLOR);
	gGL.getTexUnit(0)->setTextureAlphaBlend(LLTexUnit::TBO_REPLACE, LLTexUnit::TBS_TEX_ALPHA);

	//
	// Stage 1: Write detail2
	//
	gGL.getTexUnit(1)->bind(detail_texture2p);
	gGL.getTexUnit(1)->enable(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(1)->activate();
	
	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGenfv(GL_S, GL_OBJECT_PLANE, tp0.mV);
	glTexGenfv(GL_T, GL_OBJECT_PLANE, tp1.mV);

	gGL.getTexUnit(1)->setTextureColorBlend(LLTexUnit::TBO_MULT, LLTexUnit::TBS_TEX_COLOR, LLTexUnit::TBS_VERT_COLOR);
	gGL.getTexUnit(1)->setTextureAlphaBlend(LLTexUnit::TBO_REPLACE, LLTexUnit::TBS_PREV_ALPHA);

	{
		LLGLEnable blend(GL_BLEND);
		drawLoop();
	}
	
	//----------------------------------------------------------------------------
	// Pass 4/4
	
	//
	// Stage 0: Generate alpha ramp for detail2/detail3 transition
	//
	gGL.getTexUnit(0)->activate();
	gGL.getTexUnit(0)->bind(m2DAlphaRampImagep.get());
	// Set the texture matrix
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glTranslatef(-2.f, 0.f, 0.f);

	// Care about alpha only
	gGL.getTexUnit(0)->setTextureColorBlend(LLTexUnit::TBO_REPLACE, LLTexUnit::TBS_PREV_COLOR);
	gGL.getTexUnit(0)->setTextureAlphaBlend(LLTexUnit::TBO_REPLACE, LLTexUnit::TBS_TEX_ALPHA);

	// Stage 1: Write detail3
	gGL.getTexUnit(1)->bind(detail_texture3p);
	gGL.getTexUnit(1)->enable(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(1)->activate();

	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGenfv(GL_S, GL_OBJECT_PLANE, tp0.mV);
	glTexGenfv(GL_T, GL_OBJECT_PLANE, tp1.mV);

	gGL.getTexUnit(1)->setTextureColorBlend(LLTexUnit::TBO_MULT, LLTexUnit::TBS_TEX_COLOR, LLTexUnit::TBS_VERT_COLOR);
	gGL.getTexUnit(1)->setTextureAlphaBlend(LLTexUnit::TBO_REPLACE, LLTexUnit::TBS_PREV_ALPHA);

	gGL.getTexUnit(0)->activate();
	{
		LLGLEnable blend(GL_BLEND);
		drawLoop();
	}
	
	// Restore blend state
	gGL.setSceneBlendType(LLRender::BT_ALPHA);
	
	// Disable multitexture
	
	gGL.getTexUnit(1)->unbind(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(1)->disable();
	gGL.getTexUnit(1)->activate();

	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);

	//----------------------------------------------------------------------------
	// Restore Texture Unit 0 defaults
	
	gGL.getTexUnit(0)->activate();
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	gGL.getTexUnit(0)->setTextureBlendType(LLTexUnit::TB_MULT);
}


void LLDrawPoolTerrain::renderSimple()
{
	LLVector4 tp0, tp1;

	//----------------------------------------------------------------------------
	// Pass 1/1

	// Stage 0: Base terrain texture pass
	mTexturep->addTextureStats(1024.f*1024.f);

	gGL.getTexUnit(0)->activate();
	gGL.getTexUnit(0)->enable(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(0)->bind(mTexturep.get());
	
	LLVector3 origin_agent = mDrawFace[0]->getDrawable()->getVObj()->getRegion()->getOriginAgent();
	F32 tscale = 1.f/256.f;
	tp0.setVec(tscale, 0.f, 0.0f, -1.f*(origin_agent.mV[0]/256.f));
	tp1.setVec(0.f, tscale, 0.0f, -1.f*(origin_agent.mV[1]/256.f));
	
	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGenfv(GL_S, GL_OBJECT_PLANE, tp0.mV);
	glTexGenfv(GL_T, GL_OBJECT_PLANE, tp1.mV);
	
	gGL.getTexUnit(0)->setTextureColorBlend(LLTexUnit::TBO_MULT, LLTexUnit::TBS_TEX_COLOR, LLTexUnit::TBS_VERT_COLOR);

	drawLoop();

	//----------------------------------------------------------------------------
	// Restore Texture Unit 0 defaults
	
	gGL.getTexUnit(0)->activate();
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	gGL.getTexUnit(0)->setTextureBlendType(LLTexUnit::TB_MULT);
}

//============================================================================

void LLDrawPoolTerrain::renderOwnership()
{
	LLGLSPipelineAlpha gls_pipeline_alpha;

	llassert(!mDrawFace.empty());

	// Each terrain pool is associated with a single region.
	// We need to peek back into the viewer's data to find out
	// which ownership overlay texture to use.
	LLFace					*facep				= mDrawFace[0];
	LLDrawable				*drawablep			= facep->getDrawable();
	const LLViewerObject	*objectp				= drawablep->getVObj();
	const LLVOSurfacePatch	*vo_surface_patchp	= (LLVOSurfacePatch *)objectp;
	LLSurfacePatch			*surface_patchp		= vo_surface_patchp->getPatch();
	LLSurface				*surfacep			= surface_patchp->getSurface();
	LLViewerRegion			*regionp			= surfacep->getRegion();
	LLViewerParcelOverlay	*overlayp			= regionp->getParcelOverlay();
	LLImageGL				*texturep			= overlayp->getTexture();

	gGL.getTexUnit(0)->bind(texturep);

	// *NOTE: Because the region is 256 meters wide, but has 257 pixels, the 
	// texture coordinates for pixel 256x256 is not 1,1. This makes the
	// ownership map not line up with the selection. We address this with
	// a texture matrix multiply.
	glMatrixMode(GL_TEXTURE);
	glPushMatrix();

	const F32 TEXTURE_FUDGE = 257.f / 256.f;
	glScalef( TEXTURE_FUDGE, TEXTURE_FUDGE, 1.f );
	for (std::vector<LLFace*>::iterator iter = mDrawFace.begin();
		 iter != mDrawFace.end(); iter++)
	{
		LLFace *facep = *iter;
		facep->renderIndexed(LLVertexBuffer::MAP_VERTEX |
							LLVertexBuffer::MAP_TEXCOORD);
	}

	glMatrixMode(GL_TEXTURE);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}


void LLDrawPoolTerrain::renderForSelect()
{
	if (mDrawFace.empty())
	{
		return;
	}

	
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	for (std::vector<LLFace*>::iterator iter = mDrawFace.begin();
		 iter != mDrawFace.end(); iter++)
	{
		LLFace *facep = *iter;
		if (!facep->getDrawable()->isDead() && (facep->getDrawable()->getVObj()->mGLName))
		{
			facep->renderForSelect(LLVertexBuffer::MAP_VERTEX);
		}
	}
}

void LLDrawPoolTerrain::dirtyTextures(const std::set<LLViewerImage*>& textures)
{
	if (textures.find(mTexturep) != textures.end())
	{
		for (std::vector<LLFace*>::iterator iter = mReferences.begin();
			 iter != mReferences.end(); iter++)
		{
			LLFace *facep = *iter;
			gPipeline.markTextured(facep->getDrawable());
		}
	}
}

LLViewerImage *LLDrawPoolTerrain::getTexture()
{
	return mTexturep;
}

LLViewerImage *LLDrawPoolTerrain::getDebugTexture()
{
	return mTexturep;
}


LLColor3 LLDrawPoolTerrain::getDebugColor() const
{
	return LLColor3(0.f, 0.f, 1.f);
}
