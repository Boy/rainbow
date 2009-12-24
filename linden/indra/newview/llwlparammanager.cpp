/**
 * @file llwlparammanager.cpp
 * @brief Implementation for the LLWLParamManager class.
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

#include "llwlparammanager.h"

#include "pipeline.h"
#include "llsky.h"

#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "llcheckboxctrl.h"
#include "lluictrlfactory.h"
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

#include "llwlparamset.h"
#include "llpostprocess.h"
#include "llfloaterwindlight.h"
#include "llfloaterdaycycle.h"
#include "llfloaterenvsettings.h"

#include "curl/curl.h"

//MK
extern BOOL RRenabled;
//mk

LLWLParamManager * LLWLParamManager::sInstance = NULL;

LLWLParamManager::LLWLParamManager() :

	//set the defaults for the controls
	// index is from sWLUniforms in pipeline.cpp line 979

	/// Sun Delta Terrain tweak variables.
	mSunDeltaYaw(180.0f),
	mSceneLightStrength(2.0f),
	mWLGamma(1.0f, "gamma"),

	mBlueHorizon(0.25f, 0.25f, 1.0f, 1.0f, "blue_horizon", "WLBlueHorizon"),
	mHazeDensity(1.0f, 1.0f, 1.0f, 0.5f, "haze_density"),
	mBlueDensity(0.25f, 0.25f, 0.25f, 1.0f, "blue_density", "WLBlueDensity"),
	mDensityMult(1.0f, "density_multiplier", 1000),
	mHazeHorizon(1.0f, 1.0f, 1.0f, 0.5f, "haze_horizon"),
	mMaxAlt(4000.0f, "max_y"),

	// Lighting
	mLightnorm(0.f, 0.707f, -0.707f, 1.f, "lightnorm"),
	mSunlight(0.5f, 0.5f, 0.5f, 1.0f, "sunlight_color", "WLSunlight"),
	mAmbient(0.5f, 0.75f, 1.0f, 1.19f, "ambient", "WLAmbient"),
	mGlow(18.0f, 0.0f, -0.01f, 1.0f, "glow"),

	// Clouds
	mCloudColor(0.5f, 0.5f, 0.5f, 1.0f, "cloud_color", "WLCloudColor"),
	mCloudMain(0.5f, 0.5f, 0.125f, 1.0f, "cloud_pos_density1"),
	mCloudCoverage(0.0f, "cloud_shadow"),
	mCloudDetail(0.0f, 0.0f, 0.0f, 1.0f, "cloud_pos_density2"),
	mDistanceMult(1.0f, "distance_multiplier"),
	mCloudScale(0.42f, "cloud_scale"),

	// sky dome
	mDomeOffset(0.96f),
	mDomeRadius(15000.f)
{
}

LLWLParamManager::~LLWLParamManager()
{
}

void LLWLParamManager::loadPresets(const std::string& file_name)
{
	std::string path_name(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "windlight/skies", ""));
	LL_INFOS2("AppInit", "Shaders") << "Loading Default WindLight settings from " << path_name << LL_ENDL;
			
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

	std::string path_name2(gDirUtilp->getExpandedFilename( LL_PATH_USER_SETTINGS , "windlight/skies", ""));
	LL_INFOS2("AppInit", "Shaders") << "Loading User WindLight settings from " << path_name2 << LL_ENDL;
			
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

void LLWLParamManager::savePresets(const std::string & fileName)
{
	//Nobody currently calls me, but if they did, then its reasonable to write the data out to the user's folder
	//and not over the RO system wide version.

	LLSD paramsData(LLSD::emptyMap());
	
	std::string pathName(gDirUtilp->getExpandedFilename( LL_PATH_USER_SETTINGS , "windlight", fileName));

	for(std::map<std::string, LLWLParamSet>::iterator mIt = mParamList.begin();
		mIt != mParamList.end();
		++mIt) 
	{
		paramsData[mIt->first] = mIt->second.getAll();
	}

	llofstream presetsXML(pathName);

	LLPointer<LLSDFormatter> formatter = new LLSDXMLFormatter();

	formatter->format(paramsData, presetsXML, LLSDFormatter::OPTIONS_PRETTY);

	presetsXML.close();
}

void LLWLParamManager::loadPreset(const std::string & name,bool propagate)
{
	
	// bugfix for SL-46920: preventing filenames that break stuff.
	char * curl_str = curl_escape(name.c_str(), name.size());
	std::string escaped_filename(curl_str);
	curl_free(curl_str);
	curl_str = NULL;

	escaped_filename += ".xml";

	std::string pathName(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "windlight/skies", escaped_filename));
	llinfos << "Loading WindLight sky setting from " << pathName << llendl;

	llifstream presetsXML;
	presetsXML.open(pathName.c_str());

	// That failed, try loading from the users area instead.
	if(!presetsXML)
	{
		pathName=gDirUtilp->getExpandedFilename( LL_PATH_USER_SETTINGS , "windlight/skies", escaped_filename);
		llinfos << "Loading User WindLight sky setting from " << pathName << llendl;
		presetsXML.open(pathName.c_str());
	}

	if (presetsXML)
	{
		LLSD paramsData(LLSD::emptyMap());

		LLPointer<LLSDParser> parser = new LLSDXMLParser();

		parser->parse(presetsXML, paramsData, LLSDSerialize::SIZE_UNLIMITED);

		std::map<std::string, LLWLParamSet>::iterator mIt = mParamList.find(name);
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

//MK
	if (RRenabled) 
	{
		gAgent.mRRInterface.setLastLoadedPreset (name);
	}
//mk
	
	if(propagate)
	{
		getParamSet(name, mCurParams);
		propagateParameters();
	}
}	

void LLWLParamManager::savePreset(const std::string & name)
{
	// bugfix for SL-46920: preventing filenames that break stuff.
	char * curl_str = curl_escape(name.c_str(), name.size());
	std::string escaped_filename(curl_str);
	curl_free(curl_str);
	curl_str = NULL;

	escaped_filename += ".xml";

	// make an empty llsd
	LLSD paramsData(LLSD::emptyMap());
	std::string pathName(gDirUtilp->getExpandedFilename( LL_PATH_USER_SETTINGS , "windlight/skies", escaped_filename));

	// fill it with LLSD windlight params
	paramsData = mParamList[name].getAll();

	// write to file
	llofstream presetsXML(pathName);
	LLPointer<LLSDFormatter> formatter = new LLSDXMLFormatter();
	formatter->format(paramsData, presetsXML, LLSDFormatter::OPTIONS_PRETTY);
	presetsXML.close();

	propagateParameters();
}

void LLWLParamManager::updateShaderUniforms(LLGLSLShader * shader)
{
	if (gPipeline.canUseWindLightShaders())
	{
		mCurParams.update(shader);
	}

	if (shader->mShaderGroup == LLGLSLShader::SG_DEFAULT)
	{
		shader->uniform4fv(LLViewerShaderMgr::LIGHTNORM, 1, mRotatedLightDir.mV);
		shader->uniform3fv("camPosLocal", 1, LLViewerCamera::getInstance()->getOrigin().mV);
	} 

	else if (shader->mShaderGroup == LLGLSLShader::SG_SKY)
	{
		shader->uniform4fv(LLViewerShaderMgr::LIGHTNORM, 1, mClampedLightDir.mV);
	}

	shader->uniform1f("scene_light_strength", mSceneLightStrength);
	
}

void LLWLParamManager::propagateParameters(void)
{
	LLFastTimer ftm(LLFastTimer::FTM_UPDATE_WLPARAM);
	
	LLVector4 sunDir;
	LLVector4 moonDir;

	// set the sun direction from SunAngle and EastAngle
	F32 sinTheta = sin(mCurParams.getEastAngle());
	F32 cosTheta = cos(mCurParams.getEastAngle());

	F32 sinPhi = sin(mCurParams.getSunAngle());
	F32 cosPhi = cos(mCurParams.getSunAngle());

	sunDir.mV[0] = -sinTheta * cosPhi;
	sunDir.mV[1] = sinPhi;
	sunDir.mV[2] = cosTheta * cosPhi;
	sunDir.mV[3] = 0;

	moonDir = -sunDir;

	// is the normal from the sun or the moon
	if(sunDir.mV[1] >= 0)
	{
		mLightDir = sunDir;
	}
	else if(sunDir.mV[1] < 0 && sunDir.mV[1] > NIGHTTIME_ELEVATION_COS)
	{
		// clamp v1 to 0 so sun never points up and causes weirdness on some machines
		LLVector3 vec(sunDir.mV[0], sunDir.mV[1], sunDir.mV[2]);
		vec.mV[1] = 0;
		vec.normVec();
		mLightDir = LLVector4(vec, 0.f);
	}
	else
	{
		mLightDir = moonDir;
	}

	// calculate the clamp lightnorm for sky (to prevent ugly banding in sky
	// when haze goes below the horizon
	mClampedLightDir = sunDir;

	if (mClampedLightDir.mV[1] < -0.1f)
	{
		mClampedLightDir.mV[1] = -0.1f;
	}

	mCurParams.set("lightnorm", mLightDir);

	// bind the variables for all shaders only if we're using WindLight
	LLViewerShaderMgr::shader_iter shaders_iter, end_shaders;
	end_shaders = LLViewerShaderMgr::instance()->endShaders();
	for(shaders_iter = LLViewerShaderMgr::instance()->beginShaders(); shaders_iter != end_shaders; ++shaders_iter) 
	{
		if (shaders_iter->mProgramObject != 0
			&& (gPipeline.canUseWindLightShaders()
				|| shaders_iter->mShaderGroup == LLGLSLShader::SG_WATER))
		{
			shaders_iter->mUniformsDirty = TRUE;
		}
	}

	// get the cfr version of the sun's direction
	LLVector3 cfrSunDir(sunDir.mV[2], sunDir.mV[0], sunDir.mV[1]);

	// set direction and don't allow overriding
	gSky.setSunDirection(cfrSunDir, LLVector3(0,0,0));
	gSky.setOverrideSun(TRUE);
}

void LLWLParamManager::update(LLViewerCamera * cam)
{
	LLFastTimer ftm(LLFastTimer::FTM_UPDATE_WLPARAM);
	
	// update clouds, sun, and general
	mCurParams.updateCloudScrolling();
	
	// update only if running
	if(mAnimator.mIsRunning) 
	{
		mAnimator.update(mCurParams);
	}

	// update the shaders and the menu
	propagateParameters();
	
	// sync menus if they exist
	if(LLFloaterWindLight::isOpen()) 
	{
		LLFloaterWindLight::instance()->syncMenu();
	}
	if(LLFloaterDayCycle::isOpen()) 
	{
		LLFloaterDayCycle::instance()->syncMenu();
	}
	if(LLFloaterEnvSettings::isOpen()) 
	{
		LLFloaterEnvSettings::instance()->syncMenu();
	}

	F32 camYaw = cam->getYaw();

	stop_glerror();

	// *TODO: potential optimization - this block may only need to be
	// executed some of the time.  For example for water shaders only.
	{
		F32 camYawDelta = mSunDeltaYaw * DEG_TO_RAD;
		
		LLVector3 lightNorm3(mLightDir);	
		lightNorm3 *= LLQuaternion(-(camYaw + camYawDelta), LLVector3(0.f, 1.f, 0.f));
		mRotatedLightDir = LLVector4(lightNorm3, 0.f);

		LLViewerShaderMgr::shader_iter shaders_iter, end_shaders;
		end_shaders = LLViewerShaderMgr::instance()->endShaders();
		for(shaders_iter = LLViewerShaderMgr::instance()->beginShaders(); shaders_iter != end_shaders; ++shaders_iter)
		{
			if (shaders_iter->mProgramObject != 0
				&& (gPipeline.canUseWindLightShaders()
				|| shaders_iter->mShaderGroup == LLGLSLShader::SG_WATER))
			{
				shaders_iter->mUniformsDirty = TRUE;
			}
		}
	}
}

// static
void LLWLParamManager::initClass(void)
{
	instance();
}

// static
void LLWLParamManager::cleanupClass()
{
	delete sInstance;
	sInstance = NULL;
}

void LLWLParamManager::resetAnimator(F32 curTime, bool run)
{
	mAnimator.setTrack(mDay.mTimeMap, mDay.mDayRate, 
		curTime, run);

	return;
}
bool LLWLParamManager::addParamSet(const std::string& name, LLWLParamSet& param)
{
	// add a new one if not one there already
	std::map<std::string, LLWLParamSet>::iterator mIt = mParamList.find(name);
	if(mIt == mParamList.end()) 
	{	
		mParamList[name] = param;
		return true;
	}

	return false;
}

BOOL LLWLParamManager::addParamSet(const std::string& name, LLSD const & param)
{
	// add a new one if not one there already
	std::map<std::string, LLWLParamSet>::const_iterator finder = mParamList.find(name);
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

bool LLWLParamManager::getParamSet(const std::string& name, LLWLParamSet& param)
{
	// find it and set it
	std::map<std::string, LLWLParamSet>::iterator mIt = mParamList.find(name);
	if(mIt != mParamList.end()) 
	{
		param = mParamList[name];
		param.mName = name;
		return true;
	}

	return false;
}

bool LLWLParamManager::setParamSet(const std::string& name, LLWLParamSet& param)
{
	mParamList[name] = param;

	return true;
}

bool LLWLParamManager::setParamSet(const std::string& name, const LLSD & param)
{
	// quick, non robust (we won't be working with files, but assets) check
	if(!param.isMap()) 
	{
		return false;
	}
	
	mParamList[name].setAll(param);

	return true;
}

bool LLWLParamManager::removeParamSet(const std::string& name, bool delete_from_disk)
{
	// remove from param list
	std::map<std::string, LLWLParamSet>::iterator mIt = mParamList.find(name);
	if(mIt != mParamList.end()) 
	{
		mParamList.erase(mIt);
	}

	F32 key;

	// remove all references
	bool stat = true;
	do 
	{
		// get it
		stat = mDay.getKey(name, key);
		if(stat == false) 
		{
			break;
		}

		// and remove
		stat = mDay.removeKey(key);

	} while(stat == true);
	
	if(delete_from_disk)
	{
		std::string path_name(gDirUtilp->getExpandedFilename( LL_PATH_USER_SETTINGS , "windlight/skies", ""));
		
		// use full curl escaped name
		char * curl_str = curl_escape(name.c_str(), name.size());
		std::string escaped_name(curl_str);
		curl_free(curl_str);
		curl_str = NULL;
		
		gDirUtilp->deleteFilesInDir(path_name, escaped_name + ".xml");
	}

	return true;
}


// static
LLWLParamManager * LLWLParamManager::instance()
{
	if(NULL == sInstance)
	{
		sInstance = new LLWLParamManager();

		sInstance->loadPresets(LLStringUtil::null);

		// load the day
		sInstance->mDay.loadDayCycle(std::string("Default.xml"));

		// *HACK - sets cloud scrolling to what we want... fix this better in the future
		sInstance->getParamSet("Default", sInstance->mCurParams);

		// set it to noon
		sInstance->resetAnimator(0.5, true);

		// but use linden time sets it to what the estate is
		sInstance->mAnimator.mUseLindenTime = true;
	}

	return sInstance;
}
