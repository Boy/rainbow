/** 
 * @file lldrawpoolalpha.cpp
 * @brief LLDrawPoolAlpha class implementation
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

#include "lldrawpoolalpha.h"

#include "llglheaders.h"
#include "llviewercontrol.h"
#include "llcriticaldamp.h"
#include "llfasttimer.h"
#include "llrender.h"

#include "llcubemap.h"
#include "llsky.h"
#include "llagent.h"
#include "lldrawable.h"
#include "llface.h"
#include "llviewercamera.h"
#include "llviewerimagelist.h"	// For debugging
#include "llviewerobjectlist.h" // For debugging
#include "llviewerwindow.h"
#include "pipeline.h"
#include "llviewershadermgr.h"
#include "llviewerregion.h"
#include "lldrawpoolwater.h"
#include "llspatialpartition.h"

BOOL LLDrawPoolAlpha::sShowDebugAlpha = FALSE;



LLDrawPoolAlpha::LLDrawPoolAlpha(U32 type) :
		LLRenderPass(type), current_shader(NULL), target_shader(NULL),
		simple_shader(NULL), fullbright_shader(NULL)
{

}

LLDrawPoolAlpha::~LLDrawPoolAlpha()
{
}


void LLDrawPoolAlpha::prerender()
{
	mVertexShaderLevel = LLViewerShaderMgr::instance()->getVertexShaderLevel(LLViewerShaderMgr::SHADER_OBJECT);
}

void LLDrawPoolAlpha::beginRenderPass(S32 pass)
{
	LLFastTimer t(LLFastTimer::FTM_RENDER_ALPHA);
	
	if (LLPipeline::sUnderWaterRender)
	{
		simple_shader = &gObjectSimpleWaterProgram;
		fullbright_shader = &gObjectFullbrightWaterProgram;
	}
	else
	{
		simple_shader = &gObjectSimpleProgram;
		fullbright_shader = &gObjectFullbrightProgram;
	}

	if (mVertexShaderLevel > 0)
	{
		// Start out with no shaders.
		current_shader = target_shader = NULL;
		LLGLSLShader::bindNoShader();
	}
	gPipeline.enableLightsDynamic();
}

void LLDrawPoolAlpha::endRenderPass( S32 pass )
{
	LLFastTimer t(LLFastTimer::FTM_RENDER_ALPHA);
	LLRenderPass::endRenderPass(pass);

	if(gPipeline.canUseWindLightShaders()) 
	{
		LLGLSLShader::bindNoShader();
	}
}

void LLDrawPoolAlpha::render(S32 pass)
{
	LLFastTimer t(LLFastTimer::FTM_RENDER_ALPHA);

	LLGLDepthTest depth(GL_TRUE, LLDrawPoolWater::sSkipScreenCopy ? GL_TRUE : GL_FALSE);

	LLGLSPipelineAlpha gls_pipeline_alpha;
	
	renderAlpha(getVertexDataMask());

	if (sShowDebugAlpha)
	{
		if(gPipeline.canUseWindLightShaders()) 
		{
			LLGLSLShader::bindNoShader();
		}
		gPipeline.enableLightsFullbright(LLColor4(1,1,1,1));
		glColor4f(1,0,0,1);
		LLViewerImage::sSmokeImagep->addTextureStats(1024.f*1024.f);
		gGL.getTexUnit(0)->bind(LLViewerImage::sSmokeImagep.get());
		renderAlphaHighlight(LLVertexBuffer::MAP_VERTEX |
							LLVertexBuffer::MAP_TEXCOORD);
	}
}

void LLDrawPoolAlpha::renderAlpha(U32 mask)
{
	for (LLCullResult::sg_list_t::iterator i = gPipeline.beginAlphaGroups(); i != gPipeline.endAlphaGroups(); ++i)
	{
		LLSpatialGroup* group = *i;
		if (group->mSpatialPartition->mRenderByGroup &&
			!group->isDead())
		{
			renderGroupAlpha(group,LLRenderPass::PASS_ALPHA,mask,TRUE);
		}
	}
}

void LLDrawPoolAlpha::renderAlphaHighlight(U32 mask)
{
	for (LLCullResult::sg_list_t::iterator i = gPipeline.beginAlphaGroups(); i != gPipeline.endAlphaGroups(); ++i)
	{
		LLSpatialGroup* group = *i;
		if (group->mSpatialPartition->mRenderByGroup &&
			!group->isDead())
		{
			LLSpatialGroup::drawmap_elem_t& draw_info = group->mDrawMap[LLRenderPass::PASS_ALPHA];	

			for (LLSpatialGroup::drawmap_elem_t::iterator k = draw_info.begin(); k != draw_info.end(); ++k)	
			{
				LLDrawInfo& params = **k;
				
				if (params.mParticle)
				{
					continue;
				}

				LLRenderPass::applyModelMatrix(params);

				params.mVertexBuffer->setBuffer(mask);
				params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart, params.mEnd, params.mCount, params.mOffset);
				gPipeline.addTrianglesDrawn(params.mCount/3);
			}
		}
	}
}

void LLDrawPoolAlpha::renderGroupAlpha(LLSpatialGroup* group, U32 type, U32 mask, BOOL texture)
{
	BOOL initialized_lighting = FALSE;
	BOOL light_enabled = TRUE;
	BOOL is_particle = FALSE;
	BOOL use_shaders = (LLPipeline::sUnderWaterRender && gPipeline.canUseVertexShaders())
		|| gPipeline.canUseWindLightShadersOnObjects();
	F32 dist;

	// check to see if it's a particle and if it's "close"
	is_particle = !LLPipeline::sUnderWaterRender && (group->mSpatialPartition->mDrawableType == LLPipeline::RENDER_TYPE_PARTICLES);
	dist = group->mDistance;
	
	// don't use shader if debug setting is off and it's close or if it's a particle
	// and it's close
	if(is_particle && !gSavedSettings.getBOOL("RenderUseShaderNearParticles"))
	{
		if((dist < LLViewerCamera::getInstance()->getFar() * gSavedSettings.getF32("RenderShaderParticleThreshold")))
		{
			use_shaders = FALSE;
		}
	}

	LLSpatialGroup::drawmap_elem_t& draw_info = group->mDrawMap[type];

	if (group->mSpatialPartition->mDrawableType == LLPipeline::RENDER_TYPE_CLOUDS)
	{		
		if (!gSavedSettings.getBOOL("SkyUseClassicClouds"))
		{
			return;
		}
		gGL.setAlphaRejectSettings(LLRender::CF_DEFAULT);
	}
	else
	{
		if (LLPipeline::sImpostorRender)
		{
			gGL.setAlphaRejectSettings(LLRender::CF_GREATER, 0.5f);
		}
		else
		{
			gGL.setAlphaRejectSettings(LLRender::CF_DEFAULT);
		}
	}

	for (LLSpatialGroup::drawmap_elem_t::iterator k = draw_info.begin(); k != draw_info.end(); ++k)	
	{
		LLDrawInfo& params = **k;

		LLRenderPass::applyModelMatrix(params);

		if (texture && params.mTexture.notNull())
		{
			gGL.getTexUnit(0)->activate();
			gGL.getTexUnit(0)->bind(params.mTexture.get());
			params.mTexture->addTextureStats(params.mVSize);
			if (params.mTextureMatrix)
			{
				glMatrixMode(GL_TEXTURE);
				glLoadMatrixf((GLfloat*) params.mTextureMatrix->mMatrix);
				gPipeline.mTextureMatrixOps++;
			}
		}

		if (params.mFullbright)
		{
			// Turn off lighting if it hasn't already been so.
			if (light_enabled || !initialized_lighting)
			{
				initialized_lighting = TRUE;
				if (use_shaders) 
				{
					target_shader = fullbright_shader;
				}
				else
				{
					gPipeline.enableLightsFullbright(LLColor4(1,1,1,1));
				}
				light_enabled = FALSE;
			}
		}
		// Turn on lighting if it isn't already.
		else if (!light_enabled || !initialized_lighting)
		{
			initialized_lighting = TRUE;
			if (use_shaders) 
			{
				target_shader = simple_shader;
			}
			else
			{
				gPipeline.enableLightsDynamic();
			}
			light_enabled = TRUE;
		}

		// If we need shaders, and we're not ALREADY using the proper shader, then bind it
		// (this way we won't rebind shaders unnecessarily).
		if(use_shaders && (current_shader != target_shader))
		{
			llassert(target_shader != NULL);
			current_shader = target_shader;
			current_shader->bind();
		}
		else if (!use_shaders && current_shader != NULL)
		{
			LLGLSLShader::bindNoShader();
			current_shader = NULL;
		}

		params.mVertexBuffer->setBuffer(mask);
		params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart, params.mEnd, params.mCount, params.mOffset);
		gPipeline.addTrianglesDrawn(params.mCount/3);

		if (params.mTextureMatrix && texture && params.mTexture.notNull())
		{
			glLoadIdentity();
			glMatrixMode(GL_MODELVIEW);
		}
	}

	if (!light_enabled)
	{
		gPipeline.enableLightsDynamic();
	}
}
