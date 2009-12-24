/** 
 * @file llfloaterjoystick.cpp
 * @brief Joystick preferences panel
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

// file include
#include "llfloaterjoystick.h"

// linden library includes
#include "llerror.h"
#include "llrect.h"
#include "llstring.h"

// project includes
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "llappviewer.h"
#include "llviewerjoystick.h"

LLFloaterJoystick::LLFloaterJoystick(const LLSD& data)
	: LLFloater("floater_joystick")
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_joystick.xml");
	center();
}

void LLFloaterJoystick::draw()
{
	bool joystick_inited = LLViewerJoystick::getInstance()->isJoystickInitialized();
	childSetEnabled("enable_joystick", joystick_inited);
	childSetEnabled("joystick_type", joystick_inited);
	std::string desc = LLViewerJoystick::getInstance()->getDescription();
	if (desc.empty()) desc = getString("NoDevice");
	childSetText("joystick_type", desc);

	LLViewerJoystick* joystick(LLViewerJoystick::getInstance());
	for (U32 i = 0; i < 6; i++)
	{
		F32 value = joystick->getJoystickAxis(i);
		mAxisStats[i]->addValue(value * gFrameIntervalSeconds);
		
		if (mAxisStatsBar[i]->mMinBar > value)
		{
			mAxisStatsBar[i]->mMinBar = value;
		}
		if (mAxisStatsBar[i]->mMaxBar < value)
		{
			mAxisStatsBar[i]->mMaxBar = value;
		}
	}

	LLFloater::draw();
}

BOOL LLFloaterJoystick::postBuild()
{		
	F32 range = gSavedSettings.getBOOL("Cursor3D") ? 1024.f : 2.f;
	LLUIString axis = getString("Axis");
	LLUIString joystick = getString("JoystickMonitor");

	// use this child to get relative positioning info; we'll place the
	// joystick monitor on its right, vertically aligned to it.
	LLView* child = getChild<LLView>("FlycamAxisScale1");
	LLRect rect;

	if (child)
	{
		LLRect r = child->getRect();
		LLRect f = getRect();
		rect = LLRect(350, r.mTop, r.mRight + 200, 0);
	}

	mAxisStatsView = new LLStatView("axis values", joystick, "", rect);
	mAxisStatsView->setDisplayChildren(TRUE);

	for (U32 i = 0; i < 6; i++)
	{
		axis.setArg("[NUM]", llformat("%d", i));
		mAxisStats[i] = new LLStat(4);
		mAxisStatsBar[i] = mAxisStatsView->addStat(axis, mAxisStats[i]);
		mAxisStatsBar[i]->mMinBar = -range;
		mAxisStatsBar[i]->mMaxBar = range;
		mAxisStatsBar[i]->mLabelSpacing = range * 0.5f;
		mAxisStatsBar[i]->mTickSpacing = range * 0.25f;			
	}

	addChild(mAxisStatsView);
	
	childSetAction("SpaceNavigatorDefaults", onClickRestoreSNDefaults, this);
	refresh();
	return TRUE;
}

LLFloaterJoystick::~LLFloaterJoystick()
{
	// Children all cleaned up by default view destructor.
}


void LLFloaterJoystick::apply()
{
}

void LLFloaterJoystick::refresh()
{
	LLFloater::refresh();

	mJoystickAxis[0] = gSavedSettings.getS32("JoystickAxis0");
	mJoystickAxis[1] = gSavedSettings.getS32("JoystickAxis1");
	mJoystickAxis[2] = gSavedSettings.getS32("JoystickAxis2");
	mJoystickAxis[3] = gSavedSettings.getS32("JoystickAxis3");
	mJoystickAxis[4] = gSavedSettings.getS32("JoystickAxis4");
	mJoystickAxis[5] = gSavedSettings.getS32("JoystickAxis5");
	mJoystickAxis[6] = gSavedSettings.getS32("JoystickAxis6");
	
	m3DCursor = gSavedSettings.getBOOL("Cursor3D");
	mAutoLeveling = gSavedSettings.getBOOL("AutoLeveling");
	mZoomDirect  = gSavedSettings.getBOOL("ZoomDirect");

	mAvatarAxisScale[0] = gSavedSettings.getF32("AvatarAxisScale0");
	mAvatarAxisScale[1] = gSavedSettings.getF32("AvatarAxisScale1");
	mAvatarAxisScale[2] = gSavedSettings.getF32("AvatarAxisScale2");
	mAvatarAxisScale[3] = gSavedSettings.getF32("AvatarAxisScale3");
	mAvatarAxisScale[4] = gSavedSettings.getF32("AvatarAxisScale4");
	mAvatarAxisScale[5] = gSavedSettings.getF32("AvatarAxisScale5");

	mBuildAxisScale[0] = gSavedSettings.getF32("BuildAxisScale0");
	mBuildAxisScale[1] = gSavedSettings.getF32("BuildAxisScale1");
	mBuildAxisScale[2] = gSavedSettings.getF32("BuildAxisScale2");
	mBuildAxisScale[3] = gSavedSettings.getF32("BuildAxisScale3");
	mBuildAxisScale[4] = gSavedSettings.getF32("BuildAxisScale4");
	mBuildAxisScale[5] = gSavedSettings.getF32("BuildAxisScale5");

	mFlycamAxisScale[0] = gSavedSettings.getF32("FlycamAxisScale0");
	mFlycamAxisScale[1] = gSavedSettings.getF32("FlycamAxisScale1");
	mFlycamAxisScale[2] = gSavedSettings.getF32("FlycamAxisScale2");
	mFlycamAxisScale[3] = gSavedSettings.getF32("FlycamAxisScale3");
	mFlycamAxisScale[4] = gSavedSettings.getF32("FlycamAxisScale4");
	mFlycamAxisScale[5] = gSavedSettings.getF32("FlycamAxisScale5");
	mFlycamAxisScale[6] = gSavedSettings.getF32("FlycamAxisScale6");

	mAvatarAxisDeadZone[0] = gSavedSettings.getF32("AvatarAxisDeadZone0");
	mAvatarAxisDeadZone[1] = gSavedSettings.getF32("AvatarAxisDeadZone1");
	mAvatarAxisDeadZone[2] = gSavedSettings.getF32("AvatarAxisDeadZone2");
	mAvatarAxisDeadZone[3] = gSavedSettings.getF32("AvatarAxisDeadZone3");
	mAvatarAxisDeadZone[4] = gSavedSettings.getF32("AvatarAxisDeadZone4");
	mAvatarAxisDeadZone[5] = gSavedSettings.getF32("AvatarAxisDeadZone5");

	mBuildAxisDeadZone[0] = gSavedSettings.getF32("BuildAxisDeadZone0");
	mBuildAxisDeadZone[1] = gSavedSettings.getF32("BuildAxisDeadZone1");
	mBuildAxisDeadZone[2] = gSavedSettings.getF32("BuildAxisDeadZone2");
	mBuildAxisDeadZone[3] = gSavedSettings.getF32("BuildAxisDeadZone3");
	mBuildAxisDeadZone[4] = gSavedSettings.getF32("BuildAxisDeadZone4");
	mBuildAxisDeadZone[5] = gSavedSettings.getF32("BuildAxisDeadZone5");

	mFlycamAxisDeadZone[0] = gSavedSettings.getF32("FlycamAxisDeadZone0");
	mFlycamAxisDeadZone[1] = gSavedSettings.getF32("FlycamAxisDeadZone1");
	mFlycamAxisDeadZone[2] = gSavedSettings.getF32("FlycamAxisDeadZone2");
	mFlycamAxisDeadZone[3] = gSavedSettings.getF32("FlycamAxisDeadZone3");
	mFlycamAxisDeadZone[4] = gSavedSettings.getF32("FlycamAxisDeadZone4");
	mFlycamAxisDeadZone[5] = gSavedSettings.getF32("FlycamAxisDeadZone5");
	mFlycamAxisDeadZone[6] = gSavedSettings.getF32("FlycamAxisDeadZone6");

	mAvatarFeathering = gSavedSettings.getF32("AvatarFeathering");
	mBuildFeathering = gSavedSettings.getF32("BuildFeathering");
	mFlycamFeathering = gSavedSettings.getF32("FlycamFeathering");
}

void LLFloaterJoystick::cancel()
{
	llinfos << "reading from gSavedSettings->Cursor3D=" 
		<< gSavedSettings.getBOOL("Cursor3D") << "; m3DCursor=" 
		<< m3DCursor << llendl;

	gSavedSettings.setS32("JoystickAxis0", mJoystickAxis[0]);
	gSavedSettings.setS32("JoystickAxis1", mJoystickAxis[1]);
	gSavedSettings.setS32("JoystickAxis2", mJoystickAxis[2]);
	gSavedSettings.setS32("JoystickAxis3", mJoystickAxis[3]);
	gSavedSettings.setS32("JoystickAxis4", mJoystickAxis[4]);
	gSavedSettings.setS32("JoystickAxis5", mJoystickAxis[5]);
	gSavedSettings.setS32("JoystickAxis6", mJoystickAxis[6]);

	gSavedSettings.setBOOL("Cursor3D", m3DCursor);
	gSavedSettings.setBOOL("AutoLeveling", mAutoLeveling);
	gSavedSettings.setBOOL("ZoomDirect", mZoomDirect );

	gSavedSettings.setF32("AvatarAxisScale0", mAvatarAxisScale[0]);
	gSavedSettings.setF32("AvatarAxisScale1", mAvatarAxisScale[1]);
	gSavedSettings.setF32("AvatarAxisScale2", mAvatarAxisScale[2]);
	gSavedSettings.setF32("AvatarAxisScale3", mAvatarAxisScale[3]);
	gSavedSettings.setF32("AvatarAxisScale4", mAvatarAxisScale[4]);
	gSavedSettings.setF32("AvatarAxisScale5", mAvatarAxisScale[5]);

	gSavedSettings.setF32("BuildAxisScale0", mBuildAxisScale[0]);
	gSavedSettings.setF32("BuildAxisScale1", mBuildAxisScale[1]);
	gSavedSettings.setF32("BuildAxisScale2", mBuildAxisScale[2]);
	gSavedSettings.setF32("BuildAxisScale3", mBuildAxisScale[3]);
	gSavedSettings.setF32("BuildAxisScale4", mBuildAxisScale[4]);
	gSavedSettings.setF32("BuildAxisScale5", mBuildAxisScale[5]);

	gSavedSettings.setF32("FlycamAxisScale0", mFlycamAxisScale[0]);
	gSavedSettings.setF32("FlycamAxisScale1", mFlycamAxisScale[1]);
	gSavedSettings.setF32("FlycamAxisScale2", mFlycamAxisScale[2]);
	gSavedSettings.setF32("FlycamAxisScale3", mFlycamAxisScale[3]);
	gSavedSettings.setF32("FlycamAxisScale4", mFlycamAxisScale[4]);
	gSavedSettings.setF32("FlycamAxisScale5", mFlycamAxisScale[5]);
	gSavedSettings.setF32("FlycamAxisScale6", mFlycamAxisScale[6]);

	gSavedSettings.setF32("AvatarAxisDeadZone0", mAvatarAxisDeadZone[0]);
	gSavedSettings.setF32("AvatarAxisDeadZone1", mAvatarAxisDeadZone[1]);
	gSavedSettings.setF32("AvatarAxisDeadZone2", mAvatarAxisDeadZone[2]);
	gSavedSettings.setF32("AvatarAxisDeadZone3", mAvatarAxisDeadZone[3]);
	gSavedSettings.setF32("AvatarAxisDeadZone4", mAvatarAxisDeadZone[4]);
	gSavedSettings.setF32("AvatarAxisDeadZone5", mAvatarAxisDeadZone[5]);

	gSavedSettings.setF32("BuildAxisDeadZone0", mBuildAxisDeadZone[0]);
	gSavedSettings.setF32("BuildAxisDeadZone1", mBuildAxisDeadZone[1]);
	gSavedSettings.setF32("BuildAxisDeadZone2", mBuildAxisDeadZone[2]);
	gSavedSettings.setF32("BuildAxisDeadZone3", mBuildAxisDeadZone[3]);
	gSavedSettings.setF32("BuildAxisDeadZone4", mBuildAxisDeadZone[4]);
	gSavedSettings.setF32("BuildAxisDeadZone5", mBuildAxisDeadZone[5]);

	gSavedSettings.setF32("FlycamAxisDeadZone0", mFlycamAxisDeadZone[0]);
	gSavedSettings.setF32("FlycamAxisDeadZone1", mFlycamAxisDeadZone[1]);
	gSavedSettings.setF32("FlycamAxisDeadZone2", mFlycamAxisDeadZone[2]);
	gSavedSettings.setF32("FlycamAxisDeadZone3", mFlycamAxisDeadZone[3]);
	gSavedSettings.setF32("FlycamAxisDeadZone4", mFlycamAxisDeadZone[4]);
	gSavedSettings.setF32("FlycamAxisDeadZone5", mFlycamAxisDeadZone[5]);
	gSavedSettings.setF32("FlycamAxisDeadZone6", mFlycamAxisDeadZone[6]);

	gSavedSettings.setF32("AvatarFeathering", mAvatarFeathering);
	gSavedSettings.setF32("BuildFeathering", mBuildFeathering);
	gSavedSettings.setF32("FlycamFeathering", mFlycamFeathering);
}

void LLFloaterJoystick::onClickRestoreSNDefaults(void *joy_panel)
{
	setSNDefaults();
}

void LLFloaterJoystick::setSNDefaults()
{
	LLViewerJoystick::getInstance()->setSNDefaults();
}
