/** 
 * @file lldrawpoolwater.cpp
 * @brief LLDrawPoolWater class implementation
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
#include "llfeaturemanager.h"
#include "lldrawpoolwater.h"

#include "llviewercontrol.h"
#include "lldir.h"
#include "llerror.h"
#include "m3math.h"
#include "llrender.h"

#include "llagent.h"		// for gAgent for getRegion for getWaterHeight
#include "llcubemap.h"
#include "lldrawable.h"
#include "llface.h"
#include "llsky.h"
#include "llviewercamera.h" // to get OGL_TO_CFR_ROTATION
#include "llviewerimagelist.h"
#include "llviewerregion.h"
#include "llvosky.h"
#include "llvowater.h"
#include "llworld.h"
#include "pipeline.h"
#include "llviewershadermgr.h"
#include "llwaterparammanager.h"

const LLUUID WATER_TEST("2bfd3884-7e27-69b9-ba3a-3e673f680004");

static float sTime;

BOOL LLDrawPoolWater::sSkipScreenCopy = FALSE;
BOOL LLDrawPoolWater::sNeedsReflectionUpdate = TRUE;
BOOL LLDrawPoolWater::sNeedsDistortionUpdate = TRUE;
LLColor4 LLDrawPoolWater::sWaterFogColor = LLColor4(0.2f, 0.5f, 0.5f, 0.f);
LLVector3 LLDrawPoolWater::sLightDir;

LLDrawPoolWater::LLDrawPoolWater() :
	LLFacePool(POOL_WATER)
{
	mHBTex[0] = gImageList.getImage(gSunTextureID, TRUE, TRUE);
	gGL.getTexUnit(0)->bind(mHBTex[0].get());
	mHBTex[0]->setClamp(TRUE, TRUE);

	mHBTex[1] = gImageList.getImage(gMoonTextureID, TRUE, TRUE);
	gGL.getTexUnit(0)->bind(mHBTex[1].get());
	mHBTex[1]->setClamp(TRUE, TRUE);

	mWaterImagep = gImageList.getImage(WATER_TEST);
	mWaterNormp = gImageList.getImage(DEFAULT_WATER_NORMAL);

	restoreGL();
}

LLDrawPoolWater::~LLDrawPoolWater()
{
}

//static
void LLDrawPoolWater::restoreGL()
{
	
}

LLDrawPool *LLDrawPoolWater::instancePool()
{
	llerrs << "Should never be calling instancePool on a water pool!" << llendl;
	return NULL;
}


void LLDrawPoolWater::prerender()
{
	mVertexShaderLevel = (gGLManager.mHasCubeMap && LLCubeMap::sUseCubeMaps) ?
		LLViewerShaderMgr::instance()->getVertexShaderLevel(LLViewerShaderMgr::SHADER_WATER) : 0;

	// got rid of modulation by light color since it got a little too
	// green at sunset and sl-57047 (underwater turns black at 8:00)
	sWaterFogColor = LLWaterParamManager::instance()->getFogColor();
	sWaterFogColor.mV[3] = 0;

}

S32 LLDrawPoolWater::getNumPasses()
{
	if (LLViewerCamera::getInstance()->getOrigin().mV[2] < 1024.f)
	{
		return 1;
	}

	return 0;
}

void LLDrawPoolWater::render(S32 pass)
{
	LLFastTimer ftm(LLFastTimer::FTM_RENDER_WATER);
	if (mDrawFace.empty() || LLDrawable::getCurrentFrame() <= 1)
	{
		return;
	}

	//do a quick 'n dirty depth sort
	for (std::vector<LLFace*>::iterator iter = mDrawFace.begin();
			 iter != mDrawFace.end(); iter++)
	{
		LLFace* facep = *iter;
		facep->mDistance = -facep->mCenterLocal.mV[2];
	}

	std::sort(mDrawFace.begin(), mDrawFace.end(), LLFace::CompareDistanceGreater());

	LLGLEnable blend(GL_BLEND);

	if ((mVertexShaderLevel > 0) && !sSkipScreenCopy)
	{
		shade();
		return;
	}

	LLVOSky *voskyp = gSky.mVOSkyp;

	stop_glerror();

	if (!gGLManager.mHasMultitexture)
	{
		// Ack!  No multitexture!  Bail!
		return;
	}

	LLFace* refl_face = voskyp->getReflFace();

	gPipeline.disableLights();
	
	LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);

	LLGLDisable cullFace(GL_CULL_FACE);
	
	// Set up second pass first
	mWaterImagep->addTextureStats(1024.f*1024.f);
	gGL.getTexUnit(1)->activate();
	gGL.getTexUnit(1)->enable(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(1)->bind(mWaterImagep.get());

	LLVector3 camera_up = LLViewerCamera::getInstance()->getUpAxis();
	F32 up_dot = camera_up * LLVector3::z_axis;

	LLColor4 water_color;
	if (LLViewerCamera::getInstance()->cameraUnderWater())
	{
		water_color.setVec(1.f, 1.f, 1.f, 0.4f);
	}
	else
	{
		water_color.setVec(1.f, 1.f, 1.f, 0.5f*(1.f + up_dot));
	}

	glColor4fv(water_color.mV);

	// Automatically generate texture coords for detail map
	glEnable(GL_TEXTURE_GEN_S); //texture unit 1
	glEnable(GL_TEXTURE_GEN_T); //texture unit 1
	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);

	// Slowly move over time.
	F32 offset = fmod(gFrameTimeSeconds*2.f, 100.f);
	F32 tp0[4] = {16.f/256.f, 0.0f, 0.0f, offset*0.01f};
	F32 tp1[4] = {0.0f, 16.f/256.f, 0.0f, offset*0.01f};
	glTexGenfv(GL_S, GL_OBJECT_PLANE, tp0);
	glTexGenfv(GL_T, GL_OBJECT_PLANE, tp1);

	gGL.getTexUnit(1)->setTextureColorBlend(LLTexUnit::TBO_MULT, LLTexUnit::TBS_TEX_COLOR, LLTexUnit::TBS_PREV_COLOR);
	gGL.getTexUnit(1)->setTextureAlphaBlend(LLTexUnit::TBO_REPLACE, LLTexUnit::TBS_PREV_ALPHA);

	gGL.getTexUnit(0)->activate();
	
	glClearStencil(1);
	glClear(GL_STENCIL_BUFFER_BIT);
	LLGLEnable gls_stencil(GL_STENCIL_TEST);
	glStencilOp(GL_KEEP, GL_REPLACE, GL_KEEP);
	glStencilFunc(GL_ALWAYS, 0, 0xFFFFFFFF);

	for (std::vector<LLFace*>::iterator iter = mDrawFace.begin();
		 iter != mDrawFace.end(); iter++)
	{
		LLFace *face = *iter;
		if (voskyp->isReflFace(face))
		{
			continue;
		}
		gGL.getTexUnit(0)->bind(face->getTexture());
		face->renderIndexed();
	}

	// Now, disable texture coord generation on texture state 1
	gGL.getTexUnit(1)->activate();
	gGL.getTexUnit(1)->unbind(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(1)->disable();
	glDisable(GL_TEXTURE_GEN_S); //texture unit 1
	glDisable(GL_TEXTURE_GEN_T); //texture unit 1

	// Disable texture coordinate and color arrays
	gGL.getTexUnit(0)->activate();
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	stop_glerror();
	
	if (gSky.mVOSkyp->getCubeMap())
	{
		gSky.mVOSkyp->getCubeMap()->enable(0);
		gSky.mVOSkyp->getCubeMap()->bind();

		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
		LLMatrix4 camera_mat = LLViewerCamera::getInstance()->getModelview();
		LLMatrix4 camera_rot(camera_mat.getMat3());
		camera_rot.invert();

		glLoadMatrixf((F32 *)camera_rot.mMatrix);

		glMatrixMode(GL_MODELVIEW);
		LLOverrideFaceColor overrid(this, 1.f, 1.f, 1.f,  0.5f*up_dot);

		gGL.getTexUnit(0)->setTextureBlendType(LLTexUnit::TB_MULT);

		for (std::vector<LLFace*>::iterator iter = mDrawFace.begin();
			 iter != mDrawFace.end(); iter++)
		{
			LLFace *face = *iter;
			if (voskyp->isReflFace(face))
			{
				//refl_face = face;
				continue;
			}

			if (face->getGeomCount() > 0)
			{					
				face->renderIndexed();
			}
		}

		gGL.getTexUnit(0)->setTextureBlendType(LLTexUnit::TB_MULT);

		if (gSky.mVOSkyp->getCubeMap())
		{
			gSky.mVOSkyp->getCubeMap()->disable();
		}
		
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		gGL.getTexUnit(0)->enable(LLTexUnit::TT_TEXTURE);
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);
		
	}

	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    if (refl_face)
	{
		glStencilFunc(GL_NOTEQUAL, 0, 0xFFFFFFFF);
		renderReflection(refl_face);
	}

	gGL.getTexUnit(0)->setTextureBlendType(LLTexUnit::TB_MULT);
}

void LLDrawPoolWater::renderReflection(LLFace* face)
{
	LLVOSky *voskyp = gSky.mVOSkyp;

	if (!voskyp)
	{
		return;
	}

	if (!face->getGeomCount())
	{
		return;
	}
	
	S8 dr = voskyp->getDrawRefl();
	if (dr < 0)
	{
		return;
	}

	LLGLSNoFog noFog;

	gGL.getTexUnit(0)->bind(mHBTex[dr].get());

	LLOverrideFaceColor override(this, face->getFaceColor().mV);
	face->renderIndexed();
}

void LLDrawPoolWater::shade()
{
	gGL.setColorMask(true, true);

	LLVOSky *voskyp = gSky.mVOSkyp;

	if(voskyp == NULL) 
	{
		return;
	}

	LLGLDisable blend(GL_BLEND);

	LLColor3 light_diffuse(0,0,0);
	F32 light_exp = 0.0f;
	LLVector3 light_dir;
	LLColor3 light_color;

	if (gSky.getSunDirection().mV[2] > NIGHTTIME_ELEVATION_COS) 	 
    { 	 
        light_dir  = gSky.getSunDirection(); 	 
        light_dir.normVec(); 	
		light_color = gSky.getSunDiffuseColor();
		if(gSky.mVOSkyp) {
	        light_diffuse = gSky.mVOSkyp->getSun().getColorCached(); 	 
			light_diffuse.normVec(); 	 
		}
        light_exp = light_dir * LLVector3(light_dir.mV[0], light_dir.mV[1], 0); 	 
        light_diffuse *= light_exp + 0.25f; 	 
    } 	 
    else  	 
    { 	 
        light_dir       = gSky.getMoonDirection(); 	 
        light_dir.normVec(); 	 
		light_color = gSky.getMoonDiffuseColor();
        light_diffuse   = gSky.mVOSkyp->getMoon().getColorCached(); 	 
        light_diffuse.normVec(); 	 
        light_diffuse *= 0.5f; 	 
        light_exp = light_dir * LLVector3(light_dir.mV[0], light_dir.mV[1], 0); 	 
    }

	light_exp *= light_exp;
	light_exp *= light_exp;
	light_exp *= light_exp;
	light_exp *= light_exp;
	light_exp *= 256.f;
	light_exp = light_exp > 32.f ? light_exp : 32.f;

	LLGLSLShader* shader;

	F32 eyedepth = LLViewerCamera::getInstance()->getOrigin().mV[2] - gAgent.getRegion()->getWaterHeight();
	
	if (eyedepth < 0.f && LLPipeline::sWaterReflections)
	{
		shader = &gUnderWaterProgram;
	}
	else
	{
		shader = &gWaterProgram;
	}

	sTime = (F32)LLFrameTimer::getElapsedSeconds()*0.5f;
	
	S32 reftex = shader->enableTexture(LLViewerShaderMgr::WATER_REFTEX);
		
	if (reftex > -1)
	{
		gGL.getTexUnit(reftex)->activate();
		gGL.getTexUnit(reftex)->bind(&gPipeline.mWaterRef);
		gGL.getTexUnit(0)->activate();
	}	

	//bind normal map
	S32 bumpTex = shader->enableTexture(LLViewerShaderMgr::BUMP_MAP);

	LLWaterParamManager * param_mgr = LLWaterParamManager::instance();

	// change mWaterNormp if needed
	if (mWaterNormp->getID() != param_mgr->getNormalMapID())
	{
		mWaterNormp = gImageList.getImage(param_mgr->getNormalMapID());
	}

	mWaterNormp->addTextureStats(1024.f*1024.f);
	gGL.getTexUnit(bumpTex)->bind(mWaterNormp.get());
	mWaterNormp->setMipFilterNearest (mWaterNormp->getMipFilterNearest(),
									  !gSavedSettings.getBOOL("RenderWaterMipNormal"));
	
	S32 screentex = shader->enableTexture(LLViewerShaderMgr::WATER_SCREENTEX);	
	stop_glerror();
	
	shader->bind();

	if (screentex > -1)
	{
		shader->uniform4fv(LLViewerShaderMgr::WATER_FOGCOLOR, 1, sWaterFogColor.mV);
		shader->uniform1f(LLViewerShaderMgr::WATER_FOGDENSITY, 
			param_mgr->getFogDensity());
	}

	gGL.getTexUnit(screentex)->bind(&gPipeline.mWaterDis);	

	if (mVertexShaderLevel == 1)
	{
		sWaterFogColor.mV[3] = param_mgr->mDensitySliderValue;
		shader->uniform4fv(LLViewerShaderMgr::WATER_FOGCOLOR, 1, sWaterFogColor.mV);
	}

	F32 screenRes[] = 
	{
		1.f/gGLViewport[2],
		1.f/gGLViewport[3]
	};
	shader->uniform2fv("screenRes", 1, screenRes);
	stop_glerror();
	
	S32 diffTex = shader->enableTexture(LLViewerShaderMgr::DIFFUSE_MAP);
	stop_glerror();
	
	light_dir.normVec();
	sLightDir = light_dir;
	
	light_diffuse *= 6.f;

	//shader->uniformMatrix4fv("inverse_ref", 1, GL_FALSE, (GLfloat*) gGLObliqueProjectionInverse.mMatrix);
	shader->uniform1f(LLViewerShaderMgr::WATER_WATERHEIGHT, eyedepth);
	shader->uniform1f(LLViewerShaderMgr::WATER_TIME, sTime);
	shader->uniform3fv(LLViewerShaderMgr::WATER_EYEVEC, 1, LLViewerCamera::getInstance()->getOrigin().mV);
	shader->uniform3fv(LLViewerShaderMgr::WATER_SPECULAR, 1, light_diffuse.mV);
	shader->uniform1f(LLViewerShaderMgr::WATER_SPECULAR_EXP, light_exp);
	shader->uniform2fv(LLViewerShaderMgr::WATER_WAVE_DIR1, 1, param_mgr->getWave1Dir().mV);
	shader->uniform2fv(LLViewerShaderMgr::WATER_WAVE_DIR2, 1, param_mgr->getWave2Dir().mV);
	shader->uniform3fv(LLViewerShaderMgr::WATER_LIGHT_DIR, 1, light_dir.mV);

	shader->uniform3fv("normScale", 1, param_mgr->getNormalScale().mV);
	shader->uniform1f("fresnelScale", param_mgr->getFresnelScale());
	shader->uniform1f("fresnelOffset", param_mgr->getFresnelOffset());
	shader->uniform1f("blurMultiplier", param_mgr->getBlurMultiplier());

	F32 sunAngle = llmax(0.f, light_dir.mV[2]);
	F32 scaledAngle = 1.f - sunAngle;

	shader->uniform1f("sunAngle", sunAngle);
	shader->uniform1f("scaledAngle", scaledAngle);
	shader->uniform1f("sunAngle2", 0.1f + 0.2f*sunAngle);

	LLColor4 water_color;
	LLVector3 camera_up = LLViewerCamera::getInstance()->getUpAxis();
	F32 up_dot = camera_up * LLVector3::z_axis;
	if (LLViewerCamera::getInstance()->cameraUnderWater())
	{
		water_color.setVec(1.f, 1.f, 1.f, 0.4f);
		shader->uniform1f(LLViewerShaderMgr::WATER_REFSCALE, param_mgr->getScaleBelow());
	}
	else
	{
		water_color.setVec(1.f, 1.f, 1.f, 0.5f*(1.f + up_dot));
		shader->uniform1f(LLViewerShaderMgr::WATER_REFSCALE, param_mgr->getScaleAbove());
	}

	if (water_color.mV[3] > 0.9f)
	{
		water_color.mV[3] = 0.9f;
	}

	glColor4fv(water_color.mV);

	{
		LLGLDisable cullface(GL_CULL_FACE);
		for (std::vector<LLFace*>::iterator iter = mDrawFace.begin();
			iter != mDrawFace.end(); iter++)
		{
			LLFace *face = *iter;

			if (voskyp->isReflFace(face))
			{
				continue;
			}

			LLVOWater* water = (LLVOWater*) face->getViewerObject();
			gGL.getTexUnit(diffTex)->bind(face->getTexture());

			sNeedsReflectionUpdate = TRUE;
			
			if (water->getUseTexture())
			{
				sNeedsDistortionUpdate = TRUE;
				face->renderIndexed();
			}
			else
			{ //smash background faces to far clip plane
				if (water->getIsEdgePatch())
				{
					LLGLClampToFarClip far_clip(glh_get_current_projection());
					face->renderIndexed();
				}
				else
				{
					sNeedsDistortionUpdate = TRUE;
					face->renderIndexed();
				}
			}
		}
	}
	
	shader->disableTexture(LLViewerShaderMgr::ENVIRONMENT_MAP, LLTexUnit::TT_CUBE_MAP);
	shader->disableTexture(LLViewerShaderMgr::WATER_SCREENTEX);	
	shader->disableTexture(LLViewerShaderMgr::BUMP_MAP);
	shader->disableTexture(LLViewerShaderMgr::DIFFUSE_MAP);
	shader->disableTexture(LLViewerShaderMgr::WATER_REFTEX);
	shader->disableTexture(LLViewerShaderMgr::WATER_SCREENDEPTH);
	shader->unbind();

	gGL.getTexUnit(0)->activate();
	gGL.getTexUnit(0)->enable(LLTexUnit::TT_TEXTURE);
	gGL.setColorMask(true, false);

}

void LLDrawPoolWater::renderForSelect()
{
	// Can't select water!
	return;
}


void LLDrawPoolWater::renderFaceSelected(LLFace *facep, 
									LLImageGL *image, 
									const LLColor4 &color,
									const S32 index_offset, const S32 index_count)
{
	// Can't select water
	return;
}


LLViewerImage *LLDrawPoolWater::getDebugTexture()
{
	return LLViewerImage::sSmokeImagep;
}

LLColor3 LLDrawPoolWater::getDebugColor() const
{
	return LLColor3(0.f, 1.f, 1.f);
}
