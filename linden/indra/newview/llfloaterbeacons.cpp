/** 
 * @file llfloaterbeacons.cpp
 * @brief Front-end to LLPipeline controls for highlighting various kinds of objects.
 * @author Coco
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

#include "llviewerprecompiledheaders.h"
#include "llfloaterbeacons.h"
#include "llviewercontrol.h"
#include "lluictrlfactory.h"
#include "llcheckboxctrl.h"
#include "pipeline.h"

//MK
#include "llagent.h"

extern BOOL RRenabled;
//mk

LLFloaterBeacons::LLFloaterBeacons(const LLSD& seed)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_beacons.xml");

	// Initialize pipeline states from saved settings.
	// OK to do at floater constructor time because beacons do not display unless the floater is open
	// therefore it is OK to not initialize the pipeline state before needed.
	// Note also that we should replace those pipeline statics with direct lookup of the saved settings
	// eliminating the need to keep these states in sync.
	LLPipeline::setRenderScriptedTouchBeacons(gSavedSettings.getBOOL("scripttouchbeacon"));
	LLPipeline::setRenderScriptedBeacons(     gSavedSettings.getBOOL("scriptsbeacon"));
	LLPipeline::setRenderPhysicalBeacons(     gSavedSettings.getBOOL("physicalbeacon"));
	LLPipeline::setRenderSoundBeacons(        gSavedSettings.getBOOL("soundsbeacon"));
	LLPipeline::setRenderParticleBeacons(     gSavedSettings.getBOOL("particlesbeacon"));
	LLPipeline::setRenderHighlights(          gSavedSettings.getBOOL("renderhighlights"));
	LLPipeline::setRenderBeacons(             gSavedSettings.getBOOL("renderbeacons"));
}

BOOL LLFloaterBeacons::postBuild()
{
	childSetCommitCallback("touch_only",      onClickUICheck, this);
	childSetCommitCallback("scripted",        onClickUICheck, this);
	childSetCommitCallback("physical",        onClickUICheck, this);
	childSetCommitCallback("sounds",          onClickUICheck, this);
	childSetCommitCallback("particles",       onClickUICheck, this);
	childSetCommitCallback("highlights",      onClickUICheck, this);
	childSetCommitCallback("beacons",         onClickUICheck, this);
	return TRUE;
}

// Needed to make the floater visibility toggle the beacons.
// Too bad we can't just add control_name="BeaconAlwaysOn" to the XML.
void LLFloaterBeacons::open()
{
//MK
	if (RRenabled && gAgent.mRRInterface.mContainsEdit)
	{
		return;
	}
//mk
	LLFloater::open();
	gSavedSettings.setBOOL( "BeaconAlwaysOn", TRUE);
}
void LLFloaterBeacons::close(bool app_quitting)
{
	LLFloater::close(app_quitting);
	if(!app_quitting)
	{
		gSavedSettings.setBOOL( "BeaconAlwaysOn", FALSE);
	}
}

// Callback attached to each check box control to both affect their main purpose
// and to implement the couple screwy interdependency rules that some have.
//static 
void LLFloaterBeacons::onClickUICheck(LLUICtrl *ctrl, void* data)
{
	LLCheckBoxCtrl *check = (LLCheckBoxCtrl *)ctrl;
	std::string name = check->getName();
	LLFloaterBeacons* view = (LLFloaterBeacons*)data;
//MK
	if (RRenabled && gAgent.mRRInterface.mContainsEdit)
	{
		LLPipeline::setRenderScriptedBeacons(FALSE);
		LLPipeline::setRenderScriptedTouchBeacons(FALSE);
		view->getChild<LLCheckBoxCtrl>("scripted")->setControlValue(LLSD(FALSE));
		view->getChild<LLCheckBoxCtrl>("touch_only")->setControlValue(LLSD(FALSE));
		LLPipeline::setRenderPhysicalBeacons(FALSE);
		LLPipeline::setRenderSoundBeacons(FALSE);
		LLPipeline::setRenderParticleBeacons(FALSE);
		LLPipeline::setRenderBeacons(FALSE);
		LLPipeline::setRenderHighlights(FALSE);
		view->getChild<LLCheckBoxCtrl>("beacons")->setControlValue(LLSD(FALSE));
		view->getChild<LLCheckBoxCtrl>("highlights")->setControlValue(LLSD(FALSE));
		return;
	}
//mk
	if(     name == "touch_only")
	{
		LLPipeline::toggleRenderScriptedTouchBeacons(NULL);
		// Don't allow both to be ON at the same time. Toggle the other one off if both now on.
		if (
			LLPipeline::getRenderScriptedTouchBeacons(NULL) &&
			LLPipeline::getRenderScriptedBeacons(NULL) )
		{
			LLPipeline::setRenderScriptedBeacons(FALSE);
			view->getChild<LLCheckBoxCtrl>("scripted")->setControlValue(LLSD(FALSE));
			view->getChild<LLCheckBoxCtrl>("touch_only")->setControlValue(LLSD(TRUE)); // just to be sure it's in sync with llpipeline
		}
	}
	else if(name == "scripted")
	{
		LLPipeline::toggleRenderScriptedBeacons(NULL);
		// Don't allow both to be ON at the same time. Toggle the other one off if both now on.
		if (
			LLPipeline::getRenderScriptedTouchBeacons(NULL) &&
			LLPipeline::getRenderScriptedBeacons(NULL) )
		{
			LLPipeline::setRenderScriptedTouchBeacons(FALSE);
			view->getChild<LLCheckBoxCtrl>("touch_only")->setControlValue(LLSD(FALSE));
			view->getChild<LLCheckBoxCtrl>("scripted")->setControlValue(LLSD(TRUE)); // just to be sure it's in sync with llpipeline
		}
	}
	else if(name == "physical")       LLPipeline::setRenderPhysicalBeacons(check->get());
	else if(name == "sounds")         LLPipeline::setRenderSoundBeacons(check->get());
	else if(name == "particles")      LLPipeline::setRenderParticleBeacons(check->get());
	else if(name == "highlights")
	{
		LLPipeline::toggleRenderHighlights(NULL);
		// Don't allow both to be OFF at the same time. Toggle the other one on if both now off.
		if (
			!LLPipeline::getRenderBeacons(NULL) &&
			!LLPipeline::getRenderHighlights(NULL) )
		{
			LLPipeline::setRenderBeacons(TRUE);
			view->getChild<LLCheckBoxCtrl>("beacons")->setControlValue(LLSD(TRUE));
			view->getChild<LLCheckBoxCtrl>("highlights")->setControlValue(LLSD(FALSE)); // just to be sure it's in sync with llpipeline
		}
	}
	else if(name == "beacons")
	{
		LLPipeline::toggleRenderBeacons(NULL);
		// Don't allow both to be OFF at the same time. Toggle the other one on if both now off.
		if (
			!LLPipeline::getRenderBeacons(NULL) &&
			!LLPipeline::getRenderHighlights(NULL) )
		{
			LLPipeline::setRenderHighlights(TRUE);
			view->getChild<LLCheckBoxCtrl>("highlights")->setControlValue(LLSD(TRUE));
			view->getChild<LLCheckBoxCtrl>("beacons")->setControlValue(LLSD(FALSE)); // just to be sure it's in sync with llpipeline
		}
	}
}
