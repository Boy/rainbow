/**
 * @file llwaterparammanager.cpp
 * @brief Implementation for the LLWaterParamManager class.
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "llwaterparammanager.h"

#include "llrender.h"

#include "pipeline.h"
#include "llsky.h"

#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "llcheckboxctrl.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "llviewercamera.h"
#include "llcombobox.h"
#include "lllineeditor.h"
#include "llsdserialize.h"

#include "v4math.h"
#include "llviewerdisplay.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "lldrawpoolwater.h"
#include "llagent.h"
#include "llviewerregion.h"

#include "llwlparammanager.h"
#include "llwaterparamset.h"
#include "llpostprocess.h"
#include "llfloaterwater.h"

#include "curl/curl.h"

LLWaterParamManager * LLWaterParamManager::sInstance = NULL;

LLWaterParamManager::LLWaterParamManager() :
	mFogColor(22.f/255.f, 43.f/255.f, 54.f/255.f, 0.0f, 0.0f, "waterFogColor", "WaterFogColor"),
	mFogDensity(4, "waterFogDensity", 2),
	mUnderWaterFogMod(0.25, "underWaterFogMod"),
	mNormalScale(2.f, 2.f, 2.f, "normScale"),
	mFresnelScale(0.5f, "fresnelScale"),
	mFresnelOffset(0.4f, "fresnelOffset"),
	mScaleAbove(0.025f, "scaleAbove"),
	mScaleBelow(0.2f, "scaleBelow"),
	mBlurMultiplier(0.1f, "blurMultiplier"),
	mWave1Dir(.5f, .5f, "wave1Dir"),
	mWave2Dir(.5f, .5f, "wave2Dir"),
	mDensitySliderValue(1.0f),
	mWaterFogKS(1.0f)
{
}

LLWaterParamManager::~LLWaterParamManager()
{
}

void LLWaterParamManager::loadAllPresets(const std::string& file_name)
{
	std::string path_name(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "windlight/water", ""));
	LL_INFOS2("AppInit", "Shaders") << "Loading Default water settings from " << path_name << LL_ENDL;
			
	bool found = true;			
	while(found) 
	{
		std::string name;
		found = gDirUtilp->getNextFileInDir(path_name, "*.xml", name, false);
		if(found)
		{

			name=name.erase(name.length()-4);

			// bugfix for SL-46920: preventing filenames that break stuff.
			char * curl_str = curl_unescape(name.c_str(), name.size());
			std::string unescaped_name(curl_str);
			curl_free(curl_str);
			curl_str = NULL;

			LL_DEBUGS2("AppInit", "Shaders") << "name: " << name << LL_ENDL;
			loadPreset(unescaped_name,FALSE);
		}
	}

	// And repeat for user presets, note the user presets will modify any system presets already loaded

	std::string path_name2(gDirUtilp->getExpandedFilename( LL_PATH_USER_SETTINGS , "windlight/water", ""));
	LL_INFOS2("AppInit", "Shaders") << "Loading User water settings from " << path_name2 << LL_ENDL;
			
	found = true;			
	while(found) 
	{
		std::string name;
		found = gDirUtilp->getNextFileInDir(path_name2, "*.xml", name, false);
		if(found)
		{
			name=name.erase(name.length()-4);

			// bugfix for SL-46920: preventing filenames that break stuff.
			char * curl_str = curl_unescape(name.c_str(), name.size());
			std::string unescaped_name(curl_str);
			curl_free(curl_str);
			curl_str = NULL;

			LL_DEBUGS2("AppInit", "Shaders") << "name: " << name << LL_ENDL;
			loadPreset(unescaped_name,FALSE);
		}
	}

}

void LLWaterParamManager::loadPreset(const std::string & name,bool propagate)
{
	// bugfix for SL-46920: preventing filenames that break stuff.
	char * curl_str = curl_escape(name.c_str(), name.size());
	std::string escaped_filename(curl_str);
	curl_free(curl_str);
	curl_str = NULL;

	escaped_filename += ".xml";

	std::string pathName(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "windlight/water", escaped_filename));
	llinfos << "Loading water settings from " << pathName << llendl;
	
	std::ifstream presetsXML;
	presetsXML.open(pathName.c_str());
	
	// That failed, try loading from the users area instead.
	if(!presetsXML)
	{
		pathName=gDirUtilp->getExpandedFilename( LL_PATH_USER_SETTINGS , "windlight/water", escaped_filename);
		llinfos << "Loading User water setting from " << pathName << llendl;
		presetsXML.open(pathName.c_str());
	}

	if (presetsXML)
	{
		LLSD paramsData(LLSD::emptyMap());

		LLPointer<LLSDParser> parser = new LLSDXMLParser();

		parser->parse(presetsXML, paramsData, LLSDSerialize::SIZE_UNLIMITED);

		std::map<std::string, LLWaterParamSet>::iterator mIt = mParamList.find(name);
		if(mIt == mParamList.end())
		{
			addParamSet(name, paramsData);
		}
		else 
		{
			setParamSet(name, paramsData);
		}
		presetsXML.close();
	} 
	else 
	{
		llwarns << "Can't find " << name << llendl;
		return;
	}

	if(propagate)
	{
		getParamSet(name, mCurParams);
		propagateParameters();
	}
}	

void LLWaterParamManager::savePreset(const std::string & name)
{
	// bugfix for SL-46920: preventing filenames that break stuff.
	char * curl_str = curl_escape(name.c_str(), name.size());
	std::string escaped_filename(curl_str);
	curl_free(curl_str);
	curl_str = NULL;

	escaped_filename += ".xml";

	// make an empty llsd
	LLSD paramsData(LLSD::emptyMap());
	std::string pathName(gDirUtilp->getExpandedFilename( LL_PATH_USER_SETTINGS , "windlight/water", escaped_filename));

	// fill it with LLSD windlight params
	paramsData = mParamList[name].getAll();

	// write to file
	llofstream presetsXML(pathName);
	LLPointer<LLSDFormatter> formatter = new LLSDXMLFormatter();
	formatter->format(paramsData, presetsXML, LLSDFormatter::OPTIONS_PRETTY);
	presetsXML.close();

	propagateParameters();
}


void LLWaterParamManager::propagateParameters(void)
{
	// bind the variables only if we're using shaders
	if(gPipeline.canUseVertexShaders())
	{
		LLViewerShaderMgr::shader_iter shaders_iter, end_shaders;
		end_shaders = LLViewerShaderMgr::instance()->endShaders();
		for(shaders_iter = LLViewerShaderMgr::instance()->beginShaders(); shaders_iter != end_shaders; ++shaders_iter)
		{
			if (shaders_iter->mProgramObject != 0
				&& shaders_iter->mShaderGroup == LLGLSLShader::SG_WATER)
			{
				shaders_iter->mUniformsDirty = TRUE;
			}
		}
	}

	bool err;
	F32 fog_density_slider = 
		log(mCurParams.getFloat(mFogDensity.mName, err)) / 
		log(mFogDensity.mBase);

	setDensitySliderValue(fog_density_slider);
}

void LLWaterParamManager::updateShaderUniforms(LLGLSLShader * shader)
{
	if (shader->mShaderGroup == LLGLSLShader::SG_WATER)
	{
		shader->uniform4fv(LLViewerShaderMgr::LIGHTNORM, 1, LLWLParamManager::instance()->getRotatedLightDir().mV);
		shader->uniform3fv("camPosLocal", 1, LLViewerCamera::getInstance()->getOrigin().mV);
		shader->uniform4fv("waterFogColor", 1, LLDrawPoolWater::sWaterFogColor.mV);
		shader->uniform4fv("waterPlane", 1, mWaterPlane.mV);
		shader->uniform1f("waterFogDensity", getFogDensity());
		shader->uniform1f("waterFogKS", mWaterFogKS);
		shader->uniform4f("distance_multiplier", 0, 0, 0, 0);
	}
}

void LLWaterParamManager::update(LLViewerCamera * cam)
{
	LLFastTimer ftm(LLFastTimer::FTM_UPDATE_WLPARAM);
	
	// update the shaders and the menu
	propagateParameters();
	
	// sync menus if they exist
	if(LLFloaterWater::isOpen()) 
	{
		LLFloaterWater::instance()->syncMenu();
	}

	stop_glerror();

	// only do this if we're dealing with shaders
	if(gPipeline.canUseVertexShaders()) 
	{
		//transform water plane to eye space
		glh::vec3f norm(0.f, 0.f, 1.f);
		glh::vec3f p(0.f, 0.f, gAgent.getRegion()->getWaterHeight()+0.1f);
		
		F32 modelView[16];
		for (U32 i = 0; i < 16; i++)
		{
			modelView[i] = (F32) gGLModelView[i];
		}

		glh::matrix4f mat(modelView);
		glh::matrix4f invtrans = mat.inverse().transpose();
		glh::vec3f enorm;
		glh::vec3f ep;
		invtrans.mult_matrix_vec(norm, enorm);
		enorm.normalize();
		mat.mult_matrix_vec(p, ep);

		mWaterPlane = LLVector4(enorm.v[0], enorm.v[1], enorm.v[2], -ep.dot(enorm));

		LLVector3 sunMoonDir;
		if (gSky.getSunDirection().mV[2] > NIGHTTIME_ELEVATION_COS) 	 
		{ 	 
			sunMoonDir = gSky.getSunDirection(); 	 
		} 	 
		else  	 
		{ 	 
			sunMoonDir = gSky.getMoonDirection(); 	 
		}
		sunMoonDir.normVec();
		mWaterFogKS = 1.f/llmax(sunMoonDir.mV[2], WATER_FOG_LIGHT_CLAMP);

		LLViewerShaderMgr::shader_iter shaders_iter, end_shaders;
		end_shaders = LLViewerShaderMgr::instance()->endShaders();
		for(shaders_iter = LLViewerShaderMgr::instance()->beginShaders(); shaders_iter != end_shaders; ++shaders_iter)
		{
			if (shaders_iter->mProgramObject != 0
				&& shaders_iter->mShaderGroup == LLGLSLShader::SG_WATER)
			{
				shaders_iter->mUniformsDirty = TRUE;
			}
		}
	}
}

// static
void LLWaterParamManager::initClass(void)
{
	instance();
}

// static
void LLWaterParamManager::cleanupClass(void)
{
	delete sInstance;
	sInstance = NULL;
}

bool LLWaterParamManager::addParamSet(const std::string& name, LLWaterParamSet& param)
{
	// add a new one if not one there already
	std::map<std::string, LLWaterParamSet>::iterator mIt = mParamList.find(name);
	if(mIt == mParamList.end()) 
	{	
		mParamList[name] = param;
		return true;
	}

	return false;
}

BOOL LLWaterParamManager::addParamSet(const std::string& name, LLSD const & param)
{
	// add a new one if not one there already
	std::map<std::string, LLWaterParamSet>::const_iterator finder = mParamList.find(name);
	if(finder == mParamList.end())
	{
		mParamList[name].setAll(param);
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

bool LLWaterParamManager::getParamSet(const std::string& name, LLWaterParamSet& param)
{
	// find it and set it
	std::map<std::string, LLWaterParamSet>::iterator mIt = mParamList.find(name);
	if(mIt != mParamList.end()) 
	{
		param = mParamList[name];
		param.mName = name;
		return true;
	}

	return false;
}

bool LLWaterParamManager::setParamSet(const std::string& name, LLWaterParamSet& param)
{
	mParamList[name] = param;

	return true;
}

bool LLWaterParamManager::setParamSet(const std::string& name, const LLSD & param)
{
	// quick, non robust (we won't be working with files, but assets) check
	if(!param.isMap()) 
	{
		return false;
	}
	
	mParamList[name].setAll(param);

	return true;
}

bool LLWaterParamManager::removeParamSet(const std::string& name, bool delete_from_disk)
{
	// remove from param list
	std::map<std::string, LLWaterParamSet>::iterator mIt = mParamList.find(name);
	if(mIt != mParamList.end()) 
	{
		mParamList.erase(mIt);
	}

	if(delete_from_disk)
	{

		std::string path_name(gDirUtilp->getExpandedFilename( LL_PATH_USER_SETTINGS , "windlight/water", ""));
		
		// use full curl escaped name
		char * curl_str = curl_escape(name.c_str(), name.size());
		std::string escaped_name(curl_str);
		curl_free(curl_str);
		curl_str = NULL;
		
		gDirUtilp->deleteFilesInDir(path_name, escaped_name + ".xml");
	}

	return true;
}

F32 LLWaterParamManager::getFogDensity(void)
{
	bool err;

	F32 fogDensity = mCurParams.getFloat("waterFogDensity", err);
	
	// modify if we're underwater
	const F32 water_height = gAgent.getRegion() ? gAgent.getRegion()->getWaterHeight() : 0.f;
	F32 camera_height = gAgent.getCameraPositionAgent().mV[2];
	if(camera_height <= water_height)
	{
		// raise it to the underwater fog density modifier
		fogDensity = pow(fogDensity, mCurParams.getFloat("underWaterFogMod", err));
	}

	return fogDensity;
}

// static
LLWaterParamManager * LLWaterParamManager::instance()
{
	if(NULL == sInstance)
	{
		sInstance = new LLWaterParamManager();

		sInstance->loadAllPresets(LLStringUtil::null);

		sInstance->getParamSet("Default", sInstance->mCurParams);
	}

	return sInstance;
}
