/** 
 * @file llviewershadermgr.cpp
 * @brief Viewer shader manager implementation.
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 * 
 * Copyright (c) 2005-2009, Linden Research, Inc.
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
#include "llviewershadermgr.h"

#include "llfile.h"
#include "llviewerwindow.h"
#include "llviewercontrol.h"
#include "pipeline.h"
#include "llworld.h"
#include "llwlparammanager.h"
#include "llwaterparammanager.h"
#include "llsky.h"
#include "llvosky.h"
#include "llrender.h"

#if LL_DARWIN
#include "OpenGL/OpenGL.h"
#endif

#ifdef LL_RELEASE_FOR_DOWNLOAD
#define UNIFORM_ERRS LL_WARNS_ONCE("Shader")
#else
#define UNIFORM_ERRS LL_ERRS("Shader")
#endif

// Lots of STL stuff in here, using namespace std to keep things more readable
using std::vector;
using std::pair;
using std::make_pair;
using std::string;

LLVector4			gShinyOrigin;

//object shaders
LLGLSLShader		gObjectSimpleProgram;
LLGLSLShader		gObjectSimpleWaterProgram;
LLGLSLShader		gObjectFullbrightProgram;
LLGLSLShader		gObjectFullbrightWaterProgram;

LLGLSLShader		gObjectFullbrightShinyProgram;
LLGLSLShader		gObjectShinyProgram;
LLGLSLShader		gObjectShinyWaterProgram;

//environment shaders
LLGLSLShader		gTerrainProgram;
LLGLSLShader		gTerrainWaterProgram;
LLGLSLShader		gWaterProgram;
LLGLSLShader		gUnderWaterProgram;

//interface shaders
LLGLSLShader		gHighlightProgram;

//avatar shader handles
LLGLSLShader		gAvatarProgram;
LLGLSLShader		gAvatarWaterProgram;
LLGLSLShader		gAvatarEyeballProgram;
LLGLSLShader		gAvatarPickProgram;

// WindLight shader handles
LLGLSLShader			gWLSkyProgram;
LLGLSLShader			gWLCloudProgram;

// Effects Shaders
LLGLSLShader			gGlowProgram;
LLGLSLShader			gGlowExtractProgram;
LLGLSLShader			gPostColorFilterProgram;
LLGLSLShader			gPostNightVisionProgram;

// Deferred rendering shaders
LLGLSLShader			gDeferredDiffuseProgram;

//current avatar shader parameter pointer
GLint				gAvatarMatrixParam;

LLViewerShaderMgr::LLViewerShaderMgr() :
	mVertexShaderLevel(SHADER_COUNT, 0)
{	
	/// Make sure WL Sky is the first program
	mShaderList.push_back(&gWLSkyProgram);
	mShaderList.push_back(&gWLCloudProgram);
	mShaderList.push_back(&gAvatarProgram);
	mShaderList.push_back(&gObjectShinyProgram);
	mShaderList.push_back(&gWaterProgram);
	mShaderList.push_back(&gAvatarEyeballProgram); 
	mShaderList.push_back(&gObjectSimpleProgram);
	mShaderList.push_back(&gObjectFullbrightProgram);
	mShaderList.push_back(&gObjectFullbrightShinyProgram);
	mShaderList.push_back(&gTerrainProgram);
	mShaderList.push_back(&gTerrainWaterProgram);
	mShaderList.push_back(&gObjectSimpleWaterProgram);
	mShaderList.push_back(&gObjectFullbrightWaterProgram);
	mShaderList.push_back(&gAvatarWaterProgram);
	mShaderList.push_back(&gObjectShinyWaterProgram);
	mShaderList.push_back(&gUnderWaterProgram);
}

LLViewerShaderMgr::~LLViewerShaderMgr()
{
	mVertexShaderLevel.clear();
	mShaderList.clear();
}

// static
LLViewerShaderMgr * LLViewerShaderMgr::instance()
{
	if(NULL == sInstance)
	{
		sInstance = new LLViewerShaderMgr();
	}

	return static_cast<LLViewerShaderMgr*>(sInstance);
}

void LLViewerShaderMgr::initAttribsAndUniforms(void)
{
	if (mReservedAttribs.empty())
	{
		mReservedAttribs.push_back("materialColor");
		mReservedAttribs.push_back("specularColor");
		mReservedAttribs.push_back("binormal");

		mAvatarAttribs.reserve(5);
		mAvatarAttribs.push_back("weight");
		mAvatarAttribs.push_back("clothing");
		mAvatarAttribs.push_back("gWindDir");
		mAvatarAttribs.push_back("gSinWaveParams");
		mAvatarAttribs.push_back("gGravity");

		mAvatarUniforms.push_back("matrixPalette");

		mReservedUniforms.reserve(24);
		mReservedUniforms.push_back("diffuseMap");
		mReservedUniforms.push_back("specularMap");
		mReservedUniforms.push_back("bumpMap");
		mReservedUniforms.push_back("environmentMap");
		mReservedUniforms.push_back("cloude_noise_texture");
		mReservedUniforms.push_back("fullbright");
		mReservedUniforms.push_back("lightnorm");
		mReservedUniforms.push_back("sunlight_color");
		mReservedUniforms.push_back("ambient");
		mReservedUniforms.push_back("blue_horizon");
		mReservedUniforms.push_back("blue_density");
		mReservedUniforms.push_back("haze_horizon");
		mReservedUniforms.push_back("haze_density");
		mReservedUniforms.push_back("cloud_shadow");
		mReservedUniforms.push_back("density_multiplier");
		mReservedUniforms.push_back("distance_multiplier");
		mReservedUniforms.push_back("max_y");
		mReservedUniforms.push_back("glow");
		mReservedUniforms.push_back("cloud_color");
		mReservedUniforms.push_back("cloud_pos_density1");
		mReservedUniforms.push_back("cloud_pos_density2");
		mReservedUniforms.push_back("cloud_scale");
		mReservedUniforms.push_back("gamma");
		mReservedUniforms.push_back("scene_light_strength");

		mWLUniforms.push_back("camPosLocal");

		mTerrainUniforms.reserve(5);
		mTerrainUniforms.push_back("detail_0");
		mTerrainUniforms.push_back("detail_1");
		mTerrainUniforms.push_back("detail_2");
		mTerrainUniforms.push_back("detail_3");
		mTerrainUniforms.push_back("alpha_ramp");

		mGlowUniforms.push_back("glowDelta");
		mGlowUniforms.push_back("glowStrength");

		mGlowExtractUniforms.push_back("minLuminance");
		mGlowExtractUniforms.push_back("maxExtractAlpha");
		mGlowExtractUniforms.push_back("lumWeights");
		mGlowExtractUniforms.push_back("warmthWeights");
		mGlowExtractUniforms.push_back("warmthAmount");

		mShinyUniforms.push_back("origin");

		mWaterUniforms.reserve(12);
		mWaterUniforms.push_back("screenTex");
		mWaterUniforms.push_back("screenDepth");
		mWaterUniforms.push_back("refTex");
		mWaterUniforms.push_back("eyeVec");
		mWaterUniforms.push_back("time");
		mWaterUniforms.push_back("d1");
		mWaterUniforms.push_back("d2");
		mWaterUniforms.push_back("lightDir");
		mWaterUniforms.push_back("specular");
		mWaterUniforms.push_back("lightExp");
		mWaterUniforms.push_back("fogCol");
		mWaterUniforms.push_back("kd");
		mWaterUniforms.push_back("refScale");
		mWaterUniforms.push_back("waterHeight");
	}	
}
	

//============================================================================
// Set Levels

S32 LLViewerShaderMgr::getVertexShaderLevel(S32 type)
{
	return LLPipeline::sDisableShaders ? 0 : mVertexShaderLevel[type];
}

//============================================================================
// Shader Management

void LLViewerShaderMgr::setShaders()
{
	if (!gPipeline.mInitialized)
	{
		return;
	}
	// Make sure the compiled shader map is cleared before we recompile shaders.
	mShaderObjects.clear();
	
	initAttribsAndUniforms();
	gPipeline.releaseGLBuffers();

	if (gSavedSettings.getBOOL("VertexShaderEnable"))
	{
		LLPipeline::sWaterReflections = gGLManager.mHasCubeMap;
		LLPipeline::sRenderGlow = gSavedSettings.getBOOL("RenderGlow"); 
	}
	else
	{
			LLPipeline::sRenderGlow = 
			LLPipeline::sWaterReflections = FALSE;
	}
	
	//hack to reset buffers that change behavior with shaders
	gPipeline.resetVertexBuffers();

	if (gViewerWindow)
	{
		gViewerWindow->setCursor(UI_CURSOR_WAIT);
	}

	// Lighting
	gPipeline.setLightingDetail(-1);

	// Shaders
	LL_INFOS("ShaderLoading") << "\n~~~~~~~~~~~~~~~~~~\n Loading Shaders:\n~~~~~~~~~~~~~~~~~~" << LL_ENDL;
	for (S32 i = 0; i < SHADER_COUNT; i++)
	{
		mVertexShaderLevel[i] = 0;
	}
	mMaxAvatarShaderLevel = 0;

	if (LLFeatureManager::getInstance()->isFeatureAvailable("VertexShaderEnable") 
		&& gSavedSettings.getBOOL("VertexShaderEnable"))
	{
		S32 light_class = 2;
		S32 env_class = 2;
		S32 obj_class = 2;
		S32 effect_class = 2;
		S32 wl_class = 2;
		S32 water_class = 2;
		S32 deferred_class = 0;
		if (!gSavedSettings.getBOOL("WindLightUseAtmosShaders"))
		{
			// user has disabled WindLight in their settings, downgrade
			// windlight shaders to stub versions.
			wl_class = 1;

			// if class one or less, turn off more shaders
			// since higher end cards won't see any real gain
			// from turning off most of the shaders,
			// but class one would
			// TODO: Make water on class one cards color things
			// beneath it properly
			if(LLFeatureManager::getInstance()->getGPUClass() < GPU_CLASS_2)
			{
				// use lesser water and other stuff
				light_class = 2;
				env_class = 0;
				obj_class = 0;
				effect_class = 1;
				water_class = 1;
			}
		}

		if (gSavedSettings.getBOOL("RenderDeferred"))
		{
			light_class = 1;
			env_class = 0;
			obj_class = 0;
			water_class = 1;
			effect_class = 1;
			deferred_class = 1;
		}

		if(!gSavedSettings.getBOOL("EnableRippleWater"))
		{
			water_class = 0;
		}

		// Trigger a full rebuild of the fallback skybox / cubemap if we've toggled windlight shaders
		if (mVertexShaderLevel[SHADER_WINDLIGHT] != wl_class && gSky.mVOSkyp.notNull())
		{
			gSky.mVOSkyp->forceSkyUpdate();
		}

		// Load lighting shaders
		mVertexShaderLevel[SHADER_LIGHTING] = light_class;
		mVertexShaderLevel[SHADER_INTERFACE] = light_class;
		mVertexShaderLevel[SHADER_ENVIRONMENT] = env_class;
		mVertexShaderLevel[SHADER_WATER] = water_class;
		mVertexShaderLevel[SHADER_OBJECT] = obj_class;
		mVertexShaderLevel[SHADER_EFFECT] = effect_class;
		mVertexShaderLevel[SHADER_WINDLIGHT] = wl_class;
		mVertexShaderLevel[SHADER_DEFERRED] = deferred_class;

		BOOL loaded = loadBasicShaders();

		if (loaded)
		{
			gPipeline.mVertexShadersEnabled = TRUE;
			gPipeline.mVertexShadersLoaded = 1;

			// Load all shaders to set max levels
			loadShadersEnvironment();
			loadShadersWater();
			loadShadersObject();
			loadShadersWindLight();
			loadShadersEffects();
			loadShadersInterface();
			loadShadersDeferred();

			// Load max avatar shaders to set the max level
			mVertexShaderLevel[SHADER_AVATAR] = 3;
			mMaxAvatarShaderLevel = 3;
			loadShadersAvatar();
			
#if 0 && LL_DARWIN // force avatar shaders off for mac
			mVertexShaderLevel[SHADER_AVATAR] = 0;
			sMaxAvatarShaderLevel = 0;
#else
			if (gSavedSettings.getBOOL("RenderAvatarVP"))
			{
				BOOL avatar_cloth = gSavedSettings.getBOOL("RenderAvatarCloth");
				S32 avatar_class = 1;
				
				// cloth is a class3 shader
				if(avatar_cloth)
				{
					avatar_class = 3;
				}

				// Set the actual level
				mVertexShaderLevel[SHADER_AVATAR] = avatar_class;
				loadShadersAvatar();
				if (mVertexShaderLevel[SHADER_AVATAR] != avatar_class)
				{
					if (mVertexShaderLevel[SHADER_AVATAR] == 0)
					{
						gSavedSettings.setBOOL("RenderAvatarVP", FALSE);
					}
					if(llmax(mVertexShaderLevel[SHADER_AVATAR]-1,0) >= 3)
					{
						avatar_cloth = true;
					}
					else
					{
						avatar_cloth = false;
					}
					gSavedSettings.setBOOL("RenderAvatarCloth", avatar_cloth);
				}
			}
			else
			{
				mVertexShaderLevel[SHADER_AVATAR] = 0;
				gSavedSettings.setBOOL("RenderAvatarCloth", FALSE);
				loadShadersAvatar(); // unloads
			}
#endif
		}
		else
		{
			gPipeline.mVertexShadersEnabled = FALSE;
			gPipeline.mVertexShadersLoaded = 0;
			mVertexShaderLevel[SHADER_LIGHTING] = 0;
			mVertexShaderLevel[SHADER_INTERFACE] = 0;
			mVertexShaderLevel[SHADER_ENVIRONMENT] = 0;
			mVertexShaderLevel[SHADER_WATER] = 0;
			mVertexShaderLevel[SHADER_OBJECT] = 0;
			mVertexShaderLevel[SHADER_EFFECT] = 0;
			mVertexShaderLevel[SHADER_WINDLIGHT] = 0;
			mVertexShaderLevel[SHADER_AVATAR] = 0;
		}
	}
	else
	{
		gPipeline.mVertexShadersEnabled = FALSE;
		gPipeline.mVertexShadersLoaded = 0;
		mVertexShaderLevel[SHADER_LIGHTING] = 0;
		mVertexShaderLevel[SHADER_INTERFACE] = 0;
		mVertexShaderLevel[SHADER_ENVIRONMENT] = 0;
		mVertexShaderLevel[SHADER_WATER] = 0;
		mVertexShaderLevel[SHADER_OBJECT] = 0;
		mVertexShaderLevel[SHADER_EFFECT] = 0;
		mVertexShaderLevel[SHADER_WINDLIGHT] = 0;
		mVertexShaderLevel[SHADER_AVATAR] = 0;
	}
	
	if (gViewerWindow)
	{
		gViewerWindow->setCursor(UI_CURSOR_ARROW);
	}
	gPipeline.createGLBuffers();
}

void LLViewerShaderMgr::unloadShaders()
{
	gObjectSimpleProgram.unload();
	gObjectSimpleWaterProgram.unload();
	gObjectFullbrightProgram.unload();
	gObjectFullbrightWaterProgram.unload();

	gObjectShinyProgram.unload();
	gObjectFullbrightShinyProgram.unload();
	gObjectShinyWaterProgram.unload();
	gWaterProgram.unload();
	gUnderWaterProgram.unload();
	gTerrainProgram.unload();
	gTerrainWaterProgram.unload();
	gGlowProgram.unload();
	gGlowExtractProgram.unload();
	gAvatarProgram.unload();
	gAvatarWaterProgram.unload();
	gAvatarEyeballProgram.unload();
	gAvatarPickProgram.unload();
	gHighlightProgram.unload();

	gWLSkyProgram.unload();
	gWLCloudProgram.unload();

	gPostColorFilterProgram.unload();
	gPostNightVisionProgram.unload();

	gDeferredDiffuseProgram.unload();

	mVertexShaderLevel[SHADER_LIGHTING] = 0;
	mVertexShaderLevel[SHADER_OBJECT] = 0;
	mVertexShaderLevel[SHADER_AVATAR] = 0;
	mVertexShaderLevel[SHADER_ENVIRONMENT] = 0;
	mVertexShaderLevel[SHADER_WATER] = 0;
	mVertexShaderLevel[SHADER_INTERFACE] = 0;
	mVertexShaderLevel[SHADER_EFFECT] = 0;
	mVertexShaderLevel[SHADER_WINDLIGHT] = 0;

	gPipeline.mVertexShadersLoaded = 0;
}

BOOL LLViewerShaderMgr::loadBasicShaders()
{
	// Load basic dependency shaders first
	// All of these have to load for any shaders to function
	
#if LL_DARWIN // Mac can't currently handle all 8 lights, 
	S32 sum_lights_class = 2;
#else 
	S32 sum_lights_class = 3;

	// class one cards will get the lower sum lights
	// class zero we're not going to think about
	// since a class zero card COULD be a ridiculous new card
	// and old cards should have the features masked
	if(LLFeatureManager::getInstance()->getGPUClass() == GPU_CLASS_1)
	{
		sum_lights_class = 2;
	}
#endif

	// If we have sun and moon only checked, then only sum those lights.
	if (gPipeline.getLightingDetail() == 0)
	{
		sum_lights_class = 1;
	}

	// Load the Basic Vertex Shaders at the appropriate level. 
	// (in order of shader function call depth for reference purposes, deepest level first)

	vector< pair<string, S32> > shaders;
	shaders.reserve(10);
	shaders.push_back( make_pair( "windlight/atmosphericsVarsV.glsl",		mVertexShaderLevel[SHADER_WINDLIGHT] ) );
	shaders.push_back( make_pair( "windlight/atmosphericsHelpersV.glsl",	mVertexShaderLevel[SHADER_WINDLIGHT] ) );
	shaders.push_back( make_pair( "lighting/lightFuncV.glsl",				mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( make_pair( "lighting/sumLightsV.glsl",				sum_lights_class ) );
	shaders.push_back( make_pair( "lighting/lightV.glsl",					mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( make_pair( "lighting/lightFuncSpecularV.glsl",		mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( make_pair( "lighting/sumLightsSpecularV.glsl",		sum_lights_class ) );
	shaders.push_back( make_pair( "lighting/lightSpecularV.glsl",			mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( make_pair( "windlight/atmosphericsV.glsl",			mVertexShaderLevel[SHADER_WINDLIGHT] ) );
	shaders.push_back( make_pair( "avatar/avatarSkinV.glsl",				1 ) );

	// We no longer have to bind the shaders to global glhandles, they are automatically added to a map now.
	for (U32 i = 0; i < shaders.size(); i++)
	{
		// Note usage of GL_VERTEX_SHADER_ARB
		if (loadShaderFile(shaders[i].first, shaders[i].second, GL_VERTEX_SHADER_ARB) == 0)
		{
			return FALSE;
		}
	}

	// Load the Basic Fragment Shaders at the appropriate level. 
	// (in order of shader function call depth for reference purposes, deepest level first)

	shaders.clear();
	shaders.reserve(12);
	shaders.push_back( make_pair( "windlight/atmosphericsVarsF.glsl",		mVertexShaderLevel[SHADER_WINDLIGHT] ) );
	shaders.push_back( make_pair( "windlight/gammaF.glsl",					mVertexShaderLevel[SHADER_WINDLIGHT]) );
	shaders.push_back( make_pair( "windlight/atmosphericsF.glsl",			mVertexShaderLevel[SHADER_WINDLIGHT] ) );
	shaders.push_back( make_pair( "windlight/transportF.glsl",				mVertexShaderLevel[SHADER_WINDLIGHT] ) );	
	shaders.push_back( make_pair( "environment/waterFogF.glsl",				mVertexShaderLevel[SHADER_WATER] ) );
	shaders.push_back( make_pair( "lighting/lightF.glsl",					mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( make_pair( "lighting/lightFullbrightF.glsl",			mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( make_pair( "lighting/lightWaterF.glsl",				mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( make_pair( "lighting/lightFullbrightWaterF.glsl",	mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( make_pair( "lighting/lightShinyF.glsl",				mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( make_pair( "lighting/lightFullbrightShinyF.glsl",	mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( make_pair( "lighting/lightShinyWaterF.glsl",			mVertexShaderLevel[SHADER_LIGHTING] ) );
	
	for (U32 i = 0; i < shaders.size(); i++)
	{
		// Note usage of GL_FRAGMENT_SHADER_ARB
		if (loadShaderFile(shaders[i].first, shaders[i].second, GL_FRAGMENT_SHADER_ARB) == 0)
		{
			return FALSE;
		}
	}

	return TRUE;
}

BOOL LLViewerShaderMgr::loadShadersEnvironment()
{
	BOOL success = TRUE;

	if (mVertexShaderLevel[SHADER_ENVIRONMENT] == 0)
	{
		gTerrainProgram.unload();
		return FALSE;
	}

	if (success)
	{
		gTerrainProgram.mName = "Terrain Shader";
		gTerrainProgram.mFeatures.calculatesLighting = true;
		gTerrainProgram.mFeatures.calculatesAtmospherics = true;
		gTerrainProgram.mFeatures.hasAtmospherics = true;
		gTerrainProgram.mFeatures.hasGamma = true;
		gTerrainProgram.mShaderFiles.clear();
		gTerrainProgram.mShaderFiles.push_back(make_pair("environment/terrainV.glsl", GL_VERTEX_SHADER_ARB));
		gTerrainProgram.mShaderFiles.push_back(make_pair("environment/terrainF.glsl", GL_FRAGMENT_SHADER_ARB));
		gTerrainProgram.mShaderLevel = mVertexShaderLevel[SHADER_ENVIRONMENT];
		success = gTerrainProgram.createShader(NULL, &mTerrainUniforms);
	}

	if (!success)
	{
		mVertexShaderLevel[SHADER_ENVIRONMENT] = 0;
		return FALSE;
	}
	
	LLWorld::getInstance()->updateWaterObjects();
	
	return TRUE;
}

BOOL LLViewerShaderMgr::loadShadersWater()
{
	BOOL success = TRUE;
	BOOL terrainWaterSuccess = TRUE;

	if (mVertexShaderLevel[SHADER_WATER] == 0)
	{
		gWaterProgram.unload();
		gUnderWaterProgram.unload();
		gTerrainWaterProgram.unload();
		return FALSE;
	}

	if (success)
	{
		// load water shader
		gWaterProgram.mName = "Water Shader";
		gWaterProgram.mFeatures.calculatesAtmospherics = true;
		gWaterProgram.mFeatures.hasGamma = true;
		gWaterProgram.mFeatures.hasTransport = true;
		gWaterProgram.mShaderFiles.clear();
		gWaterProgram.mShaderFiles.push_back(make_pair("environment/waterV.glsl", GL_VERTEX_SHADER_ARB));
		gWaterProgram.mShaderFiles.push_back(make_pair("environment/waterF.glsl", GL_FRAGMENT_SHADER_ARB));
		gWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_WATER];
		success = gWaterProgram.createShader(NULL, &mWaterUniforms);
	}

	if (success)
	{
		//load under water vertex shader
		gUnderWaterProgram.mName = "Underwater Shader";
		gUnderWaterProgram.mFeatures.calculatesAtmospherics = true;
		gUnderWaterProgram.mShaderFiles.clear();
		gUnderWaterProgram.mShaderFiles.push_back(make_pair("environment/waterV.glsl", GL_VERTEX_SHADER_ARB));
		gUnderWaterProgram.mShaderFiles.push_back(make_pair("environment/underWaterF.glsl", GL_FRAGMENT_SHADER_ARB));
		gUnderWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_WATER];
		gUnderWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		
		success = gUnderWaterProgram.createShader(NULL, &mWaterUniforms);
	}

	if (success)
	{
		//load terrain water shader
		gTerrainWaterProgram.mName = "Terrain Water Shader";
		gTerrainWaterProgram.mFeatures.calculatesLighting = true;
		gTerrainWaterProgram.mFeatures.calculatesAtmospherics = true;
		gTerrainWaterProgram.mFeatures.hasAtmospherics = true;
		gTerrainWaterProgram.mFeatures.hasWaterFog = true;
		gTerrainWaterProgram.mShaderFiles.clear();
		gTerrainWaterProgram.mShaderFiles.push_back(make_pair("environment/terrainV.glsl", GL_VERTEX_SHADER_ARB));
		gTerrainWaterProgram.mShaderFiles.push_back(make_pair("environment/terrainWaterF.glsl", GL_FRAGMENT_SHADER_ARB));
		gTerrainWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_ENVIRONMENT];
		gTerrainWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		terrainWaterSuccess = gTerrainWaterProgram.createShader(NULL, &mTerrainUniforms);
	}	

	/// Keep track of water shader levels
	if (gWaterProgram.mShaderLevel != mVertexShaderLevel[SHADER_WATER]
		|| gUnderWaterProgram.mShaderLevel != mVertexShaderLevel[SHADER_WATER])
	{
		mVertexShaderLevel[SHADER_WATER] = llmin(gWaterProgram.mShaderLevel, gUnderWaterProgram.mShaderLevel);
	}

	if (!success)
	{
		mVertexShaderLevel[SHADER_WATER] = 0;
		return FALSE;
	}

	// if we failed to load the terrain water shaders and we need them (using class2 water),
	// then drop down to class1 water.
	if (mVertexShaderLevel[SHADER_WATER] > 1 && !terrainWaterSuccess)
	{
		mVertexShaderLevel[SHADER_WATER]--;
		return loadShadersWater();
	}
	
	LLWorld::getInstance()->updateWaterObjects();

	return TRUE;
}

BOOL LLViewerShaderMgr::loadShadersEffects()
{
	BOOL success = TRUE;

	if (mVertexShaderLevel[SHADER_EFFECT] == 0)
	{
		gGlowProgram.unload();
		gGlowExtractProgram.unload();
		gPostColorFilterProgram.unload();	
		gPostNightVisionProgram.unload();
		return FALSE;
	}

	if (success)
	{
		gGlowProgram.mName = "Glow Shader (Post)";
		gGlowProgram.mShaderFiles.clear();
		gGlowProgram.mShaderFiles.push_back(make_pair("effects/glowV.glsl", GL_VERTEX_SHADER_ARB));
		gGlowProgram.mShaderFiles.push_back(make_pair("effects/glowF.glsl", GL_FRAGMENT_SHADER_ARB));
		gGlowProgram.mShaderLevel = mVertexShaderLevel[SHADER_EFFECT];
		success = gGlowProgram.createShader(NULL, &mGlowUniforms);
		if (!success)
		{
			LLPipeline::sRenderGlow = FALSE;
		}
	}
	
	if (success)
	{
		gGlowExtractProgram.mName = "Glow Extract Shader (Post)";
		gGlowExtractProgram.mShaderFiles.clear();
		gGlowExtractProgram.mShaderFiles.push_back(make_pair("effects/glowExtractV.glsl", GL_VERTEX_SHADER_ARB));
		gGlowExtractProgram.mShaderFiles.push_back(make_pair("effects/glowExtractF.glsl", GL_FRAGMENT_SHADER_ARB));
		gGlowExtractProgram.mShaderLevel = mVertexShaderLevel[SHADER_EFFECT];
		success = gGlowExtractProgram.createShader(NULL, &mGlowExtractUniforms);
		if (!success)
		{
			LLPipeline::sRenderGlow = FALSE;
		}
	}
	
#if 0
	// disabling loading of postprocess shaders until we fix
	// ATI sampler2DRect compatibility.
	
	//load Color Filter Shader
	if (success)
	{
		vector<string> shaderUniforms;
		shaderUniforms.reserve(7);
		shaderUniforms.push_back("RenderTexture");
		shaderUniforms.push_back("gamma");
		shaderUniforms.push_back("brightness");
		shaderUniforms.push_back("contrast");
		shaderUniforms.push_back("contrastBase");
		shaderUniforms.push_back("saturation");
		shaderUniforms.push_back("lumWeights");

		gPostColorFilterProgram.mName = "Color Filter Shader (Post)";
		gPostColorFilterProgram.mShaderFiles.clear();
		gPostColorFilterProgram.mShaderFiles.push_back(make_pair("effects/colorFilterF.glsl", GL_FRAGMENT_SHADER_ARB));
		gPostColorFilterProgram.mShaderFiles.push_back(make_pair("effects/drawQuadV.glsl", GL_VERTEX_SHADER_ARB));
		gPostColorFilterProgram.mShaderLevel = mVertexShaderLevel[SHADER_EFFECT];
		success = gPostColorFilterProgram.createShader(NULL, &shaderUniforms);
	}

	//load Night Vision Shader
	if (success)
	{
		vector<string> shaderUniforms;
		shaderUniforms.reserve(5);
		shaderUniforms.push_back("RenderTexture");
		shaderUniforms.push_back("NoiseTexture");
		shaderUniforms.push_back("brightMult");
		shaderUniforms.push_back("noiseStrength");
		shaderUniforms.push_back("lumWeights");

		gPostNightVisionProgram.mName = "Night Vision Shader (Post)";
		gPostNightVisionProgram.mShaderFiles.clear();
		gPostNightVisionProgram.mShaderFiles.push_back(make_pair("effects/nightVisionF.glsl", GL_FRAGMENT_SHADER_ARB));
		gPostNightVisionProgram.mShaderFiles.push_back(make_pair("effects/drawQuadV.glsl", GL_VERTEX_SHADER_ARB));
		gPostNightVisionProgram.mShaderLevel = mVertexShaderLevel[SHADER_EFFECT];
		success = gPostNightVisionProgram.createShader(NULL, &shaderUniforms);
	}
	#endif

	return success;

}

BOOL LLViewerShaderMgr::loadShadersDeferred()
{
	if (mVertexShaderLevel[SHADER_DEFERRED] == 0)
	{
		gDeferredDiffuseProgram.unload();
		return FALSE;
	}

	BOOL success = TRUE;

	if (success)
	{
		gDeferredDiffuseProgram.mName = "Deffered Diffuse Shader";
		gDeferredDiffuseProgram.mShaderFiles.clear();
		gDeferredDiffuseProgram.mShaderFiles.push_back(make_pair("deferred/diffuseV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredDiffuseProgram.mShaderFiles.push_back(make_pair("deferred/diffuseF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredDiffuseProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredDiffuseProgram.createShader(NULL, NULL);
	}

	return success;
}

BOOL LLViewerShaderMgr::loadShadersObject()
{
	BOOL success = TRUE;

	if (mVertexShaderLevel[SHADER_OBJECT] == 0)
	{
		gObjectShinyProgram.unload();
		gObjectFullbrightShinyProgram.unload();
		gObjectShinyWaterProgram.unload();
		gObjectSimpleProgram.unload();
		gObjectSimpleWaterProgram.unload();
		gObjectFullbrightProgram.unload();
		gObjectFullbrightWaterProgram.unload();
		return FALSE;
	}

	if (success)
	{
		gObjectSimpleProgram.mName = "Simple Shader";
		gObjectSimpleProgram.mFeatures.calculatesLighting = true;
		gObjectSimpleProgram.mFeatures.calculatesAtmospherics = true;
		gObjectSimpleProgram.mFeatures.hasGamma = true;
		gObjectSimpleProgram.mFeatures.hasAtmospherics = true;
		gObjectSimpleProgram.mFeatures.hasLighting = true;
		gObjectSimpleProgram.mShaderFiles.clear();
		gObjectSimpleProgram.mShaderFiles.push_back(make_pair("objects/simpleV.glsl", GL_VERTEX_SHADER_ARB));
		gObjectSimpleProgram.mShaderFiles.push_back(make_pair("objects/simpleF.glsl", GL_FRAGMENT_SHADER_ARB));
		gObjectSimpleProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectSimpleProgram.createShader(NULL, NULL);
	}
	
	if (success)
	{
		gObjectSimpleWaterProgram.mName = "Simple Water Shader";
		gObjectSimpleWaterProgram.mFeatures.calculatesLighting = true;
		gObjectSimpleWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectSimpleWaterProgram.mFeatures.hasWaterFog = true;
		gObjectSimpleWaterProgram.mFeatures.hasAtmospherics = true;
		gObjectSimpleWaterProgram.mFeatures.hasLighting = true;
		gObjectSimpleWaterProgram.mShaderFiles.clear();
		gObjectSimpleWaterProgram.mShaderFiles.push_back(make_pair("objects/simpleV.glsl", GL_VERTEX_SHADER_ARB));
		gObjectSimpleWaterProgram.mShaderFiles.push_back(make_pair("objects/simpleWaterF.glsl", GL_FRAGMENT_SHADER_ARB));
		gObjectSimpleWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		gObjectSimpleWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectSimpleWaterProgram.createShader(NULL, NULL);
	}
	
	if (success)
	{
		gObjectFullbrightProgram.mName = "Fullbright Shader";
		gObjectFullbrightProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightProgram.mFeatures.hasGamma = true;
		gObjectFullbrightProgram.mFeatures.hasTransport = true;
		gObjectFullbrightProgram.mFeatures.isFullbright = true;
		gObjectFullbrightProgram.mShaderFiles.clear();
		gObjectFullbrightProgram.mShaderFiles.push_back(make_pair("objects/fullbrightV.glsl", GL_VERTEX_SHADER_ARB));
		gObjectFullbrightProgram.mShaderFiles.push_back(make_pair("objects/fullbrightF.glsl", GL_FRAGMENT_SHADER_ARB));
		gObjectFullbrightProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectFullbrightProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gObjectFullbrightWaterProgram.mName = "Fullbright Water Shader";
		gObjectFullbrightWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightWaterProgram.mFeatures.isFullbright = true;
		gObjectFullbrightWaterProgram.mFeatures.hasWaterFog = true;		
		gObjectFullbrightWaterProgram.mFeatures.hasTransport = true;
		gObjectFullbrightWaterProgram.mShaderFiles.clear();
		gObjectFullbrightWaterProgram.mShaderFiles.push_back(make_pair("objects/fullbrightV.glsl", GL_VERTEX_SHADER_ARB));
		gObjectFullbrightWaterProgram.mShaderFiles.push_back(make_pair("objects/fullbrightWaterF.glsl", GL_FRAGMENT_SHADER_ARB));
		gObjectFullbrightWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		gObjectFullbrightWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectFullbrightWaterProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gObjectShinyProgram.mName = "Shiny Shader";
		gObjectShinyProgram.mFeatures.calculatesAtmospherics = true;
		gObjectShinyProgram.mFeatures.calculatesLighting = true;
		gObjectShinyProgram.mFeatures.hasGamma = true;
		gObjectShinyProgram.mFeatures.hasAtmospherics = true;
		gObjectShinyProgram.mFeatures.isShiny = true;
		gObjectShinyProgram.mShaderFiles.clear();
		gObjectShinyProgram.mShaderFiles.push_back(make_pair("objects/shinyV.glsl", GL_VERTEX_SHADER_ARB));
		gObjectShinyProgram.mShaderFiles.push_back(make_pair("objects/shinyF.glsl", GL_FRAGMENT_SHADER_ARB));		
		gObjectShinyProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectShinyProgram.createShader(NULL, &mShinyUniforms);
	}

	if (success)
	{
		gObjectShinyWaterProgram.mName = "Shiny Water Shader";
		gObjectShinyWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectShinyWaterProgram.mFeatures.calculatesLighting = true;
		gObjectShinyWaterProgram.mFeatures.isShiny = true;
		gObjectShinyWaterProgram.mFeatures.hasWaterFog = true;
		gObjectShinyWaterProgram.mFeatures.hasAtmospherics = true;
		gObjectShinyWaterProgram.mShaderFiles.clear();
		gObjectShinyWaterProgram.mShaderFiles.push_back(make_pair("objects/shinyWaterF.glsl", GL_FRAGMENT_SHADER_ARB));
		gObjectShinyWaterProgram.mShaderFiles.push_back(make_pair("objects/shinyV.glsl", GL_VERTEX_SHADER_ARB));
		gObjectShinyWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		gObjectShinyWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectShinyWaterProgram.createShader(NULL, &mShinyUniforms);
	}
	
	if (success)
	{
		gObjectFullbrightShinyProgram.mName = "Fullbright Shiny Shader";
		gObjectFullbrightShinyProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightShinyProgram.mFeatures.isFullbright = true;
		gObjectFullbrightShinyProgram.mFeatures.isShiny = true;
		gObjectFullbrightShinyProgram.mFeatures.hasGamma = true;
		gObjectFullbrightShinyProgram.mFeatures.hasTransport = true;
		gObjectFullbrightShinyProgram.mShaderFiles.clear();
		gObjectFullbrightShinyProgram.mShaderFiles.push_back(make_pair("objects/fullbrightShinyV.glsl", GL_VERTEX_SHADER_ARB));
		gObjectFullbrightShinyProgram.mShaderFiles.push_back(make_pair("objects/fullbrightShinyF.glsl", GL_FRAGMENT_SHADER_ARB));
		gObjectFullbrightShinyProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectFullbrightShinyProgram.createShader(NULL, &mShinyUniforms);
	}


	if( !success )
	{
		mVertexShaderLevel[SHADER_OBJECT] = 0;
		return FALSE;
	}
	
	return TRUE;
}

BOOL LLViewerShaderMgr::loadShadersAvatar()
{
	BOOL success = TRUE;

	if (mVertexShaderLevel[SHADER_AVATAR] == 0)
	{
		gAvatarProgram.unload();
		gAvatarWaterProgram.unload();
		gAvatarEyeballProgram.unload();
		gAvatarPickProgram.unload();
		return FALSE;
	}

	if (success)
	{
		gAvatarProgram.mName = "Avatar Shader";
		gAvatarProgram.mFeatures.hasSkinning = true;
		gAvatarProgram.mFeatures.calculatesAtmospherics = true;
		gAvatarProgram.mFeatures.calculatesLighting = true;
		gAvatarProgram.mFeatures.hasGamma = true;
		gAvatarProgram.mFeatures.hasAtmospherics = true;
		gAvatarProgram.mFeatures.hasLighting = true;
		gAvatarProgram.mShaderFiles.clear();
		gAvatarProgram.mShaderFiles.push_back(make_pair("avatar/avatarV.glsl", GL_VERTEX_SHADER_ARB));
		gAvatarProgram.mShaderFiles.push_back(make_pair("avatar/avatarF.glsl", GL_FRAGMENT_SHADER_ARB));
		gAvatarProgram.mShaderLevel = mVertexShaderLevel[SHADER_AVATAR];
		success = gAvatarProgram.createShader(&mAvatarAttribs, &mAvatarUniforms);
			
		if (success)
		{
			gAvatarWaterProgram.mName = "Avatar Water Shader";
			gAvatarWaterProgram.mFeatures.hasSkinning = true;
			gAvatarWaterProgram.mFeatures.calculatesAtmospherics = true;
			gAvatarWaterProgram.mFeatures.calculatesLighting = true;
			gAvatarWaterProgram.mFeatures.hasWaterFog = true;
			gAvatarWaterProgram.mFeatures.hasAtmospherics = true;
			gAvatarWaterProgram.mFeatures.hasLighting = true;
			gAvatarWaterProgram.mShaderFiles.clear();
			gAvatarWaterProgram.mShaderFiles.push_back(make_pair("avatar/avatarV.glsl", GL_VERTEX_SHADER_ARB));
			gAvatarWaterProgram.mShaderFiles.push_back(make_pair("objects/simpleWaterF.glsl", GL_FRAGMENT_SHADER_ARB));
			// Note: no cloth under water:
			gAvatarWaterProgram.mShaderLevel = llmin(mVertexShaderLevel[SHADER_AVATAR], 1);	
			gAvatarWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;				
			success = gAvatarWaterProgram.createShader(&mAvatarAttribs, &mAvatarUniforms);
		}

		/// Keep track of avatar levels
		if (gAvatarProgram.mShaderLevel != mVertexShaderLevel[SHADER_AVATAR])
		{
			mMaxAvatarShaderLevel = mVertexShaderLevel[SHADER_AVATAR] = gAvatarProgram.mShaderLevel;
		}
	}

	if (success)
	{
		gAvatarPickProgram.mName = "Avatar Pick Shader";
		gAvatarPickProgram.mFeatures.hasSkinning = true;
		gAvatarPickProgram.mShaderFiles.clear();
		gAvatarPickProgram.mShaderFiles.push_back(make_pair("avatar/pickAvatarV.glsl", GL_VERTEX_SHADER_ARB));
		gAvatarPickProgram.mShaderFiles.push_back(make_pair("avatar/pickAvatarF.glsl", GL_FRAGMENT_SHADER_ARB));
		gAvatarPickProgram.mShaderLevel = mVertexShaderLevel[SHADER_AVATAR];
		success = gAvatarPickProgram.createShader(&mAvatarAttribs, &mAvatarUniforms);
	}

	if (success)
	{
		gAvatarEyeballProgram.mName = "Avatar Eyeball Program";
		gAvatarEyeballProgram.mFeatures.calculatesLighting = true;
		gAvatarEyeballProgram.mFeatures.isSpecular = true;
		gAvatarEyeballProgram.mFeatures.calculatesAtmospherics = true;
		gAvatarEyeballProgram.mFeatures.hasGamma = true;
		gAvatarEyeballProgram.mFeatures.hasAtmospherics = true;
		gAvatarEyeballProgram.mFeatures.hasLighting = true;
		gAvatarEyeballProgram.mShaderFiles.clear();
		gAvatarEyeballProgram.mShaderFiles.push_back(make_pair("avatar/eyeballV.glsl", GL_VERTEX_SHADER_ARB));
		gAvatarEyeballProgram.mShaderFiles.push_back(make_pair("avatar/eyeballF.glsl", GL_FRAGMENT_SHADER_ARB));
		gAvatarEyeballProgram.mShaderLevel = mVertexShaderLevel[SHADER_AVATAR];
		success = gAvatarEyeballProgram.createShader(NULL, NULL);
	}

	if( !success )
	{
		mVertexShaderLevel[SHADER_AVATAR] = 0;
		mMaxAvatarShaderLevel = 0;
		return FALSE;
	}
	
	return TRUE;
}

BOOL LLViewerShaderMgr::loadShadersInterface()
{
	BOOL success = TRUE;

	if (mVertexShaderLevel[SHADER_INTERFACE] == 0)
	{
		gHighlightProgram.unload();
		return FALSE;
	}
	
	if (success)
	{
		gHighlightProgram.mName = "Highlight Shader";
		gHighlightProgram.mShaderFiles.clear();
		gHighlightProgram.mShaderFiles.push_back(make_pair("interface/highlightV.glsl", GL_VERTEX_SHADER_ARB));
		gHighlightProgram.mShaderFiles.push_back(make_pair("interface/highlightF.glsl", GL_FRAGMENT_SHADER_ARB));
		gHighlightProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];		
		success = gHighlightProgram.createShader(NULL, NULL);
	}

	if( !success )
	{
		mVertexShaderLevel[SHADER_INTERFACE] = 0;
		return FALSE;
	}
	
	return TRUE;
}

BOOL LLViewerShaderMgr::loadShadersWindLight()
{	
	BOOL success = TRUE;

	if (mVertexShaderLevel[SHADER_WINDLIGHT] < 2)
	{
		gWLSkyProgram.unload();
		gWLCloudProgram.unload();
		return FALSE;
	}

	if (success)
	{
		gWLSkyProgram.mName = "Windlight Sky Shader";
		//gWLSkyProgram.mFeatures.hasGamma = true;
		gWLSkyProgram.mShaderFiles.clear();
		gWLSkyProgram.mShaderFiles.push_back(make_pair("windlight/skyV.glsl", GL_VERTEX_SHADER_ARB));
		gWLSkyProgram.mShaderFiles.push_back(make_pair("windlight/skyF.glsl", GL_FRAGMENT_SHADER_ARB));
		gWLSkyProgram.mShaderLevel = mVertexShaderLevel[SHADER_WINDLIGHT];
		gWLSkyProgram.mShaderGroup = LLGLSLShader::SG_SKY;
		success = gWLSkyProgram.createShader(NULL, &mWLUniforms);
	}

	if (success)
	{
		gWLCloudProgram.mName = "Windlight Cloud Program";
		//gWLCloudProgram.mFeatures.hasGamma = true;
		gWLCloudProgram.mShaderFiles.clear();
		gWLCloudProgram.mShaderFiles.push_back(make_pair("windlight/cloudsV.glsl", GL_VERTEX_SHADER_ARB));
		gWLCloudProgram.mShaderFiles.push_back(make_pair("windlight/cloudsF.glsl", GL_FRAGMENT_SHADER_ARB));
		gWLCloudProgram.mShaderLevel = mVertexShaderLevel[SHADER_WINDLIGHT];
		gWLCloudProgram.mShaderGroup = LLGLSLShader::SG_SKY;
		success = gWLCloudProgram.createShader(NULL, &mWLUniforms);
	}

	return success;
}

std::string LLViewerShaderMgr::getShaderDirPrefix(void)
{
	return gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "shaders/class");
}

void LLViewerShaderMgr::updateShaderUniforms(LLGLSLShader * shader)
{
	LLWLParamManager::instance()->updateShaderUniforms(shader);
	LLWaterParamManager::instance()->updateShaderUniforms(shader);
}
