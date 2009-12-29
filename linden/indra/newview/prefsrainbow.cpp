/** 
 * @file prefsrainbow.cpp
 * @author Henri Beauchamp
 * @brief Rainbow Viewer preferences panel, modified from Cool SL Viewer
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 * 
 * Copyright (c) 2008, Henri Beauchamp.
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

#include "prefsrainbow.h"

#include "llstartup.h"
#include "llviewercontrol.h"
#include "lluictrlfactory.h"
#include "llcombobox.h"

class LLPrefsRainbowImpl : public LLPanel
{
public:
	LLPrefsRainbowImpl();
	/*virtual*/ ~LLPrefsRainbowImpl() { };

	virtual void refresh();

	void apply();
	void cancel();

protected:
	static void onCommitCheckBox(LLUICtrl* ctrl, void* user_data);
	BOOL mShowGrids;
	BOOL mSaveScriptsAsMono;
	BOOL mDoubleClickTeleport;
	BOOL mHideNotificationsInChat;
	BOOL mUseOldTrackingDots;
	BOOL mAllowMUpose;
	BOOL mAutoCloseOOC;
	BOOL mPlayTypingSound;
	BOOL mPrivateLookAt;
	BOOL mFetchInventoryOnLogin;
	BOOL mRestrainedLife;
	BOOL mSecondsInChatAndIMs;
	U32 mTimeFormat;
	U32 mDateFormat;
};


LLPrefsRainbowImpl::LLPrefsRainbowImpl()
 : LLPanel("Rainbow Prefs Panel")
{
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_preferences_rainbow.xml");
	refresh();
}

void LLPrefsRainbowImpl::refresh()
{
	mShowGrids					= gSavedSettings.getBOOL("ForceShowGrid");
	mSaveScriptsAsMono			= gSavedSettings.getBOOL("SaveScriptsAsMono");
	mDoubleClickTeleport			= gSavedSettings.getBOOL("DoubleClickTeleport");
	mHideNotificationsInChat		= gSavedSettings.getBOOL("HideNotificationsInChat");
	mUseOldTrackingDots			= gSavedSettings.getBOOL("UseOldTrackingDots");
	mAllowMUpose				= gSavedSettings.getBOOL("AllowMUpose");
	mAutoCloseOOC				= gSavedSettings.getBOOL("AutoCloseOOC");
	mPlayTypingSound			= gSavedSettings.getBOOL("PlayTypingSound");
	mPrivateLookAt				= gSavedSettings.getBOOL("PrivateLookAt");
	mSecondsInChatAndIMs			= gSavedSettings.getBOOL("SecondsInChatAndIMs");
	mFetchInventoryOnLogin			= gSavedSettings.getBOOL("FetchInventoryOnLogin");
}

{
	std::string format = gSavedSettings.getString("ShortTimeFormat");
	if (format.find("%p") == -1)
	{
		mTimeFormat = 0;
	}
	else
	{
		mTimeFormat = 1;
	}

	format = gSavedSettings.getString("ShortDateFormat");
	if (format.find("%m/%d/%") != -1)
	{
		mDateFormat = 2;
	}
	else if (format.find("%d/%m/%") != -1)
	{
		mDateFormat = 1;
	}
	else
	{
		mDateFormat = 0;
	}

	// time format combobox
	LLComboBox* combo = getChild<LLComboBox>("time_format_combobox");
	if (combo)
	{
		combo->setCurrentByIndex(mTimeFormat);
	}

	// date format combobox
	combo = getChild<LLComboBox>("date_format_combobox");
	if (combo)
	{
		combo->setCurrentByIndex(mDateFormat);
	}
}

void LLPrefsRainbowImpl::cancel()
{
	gSavedSettings.setBOOL("ForceShowGrid",				mShowGrids);
	gSavedSettings.setBOOL("SaveScriptsAsMono",			mSaveScriptsAsMono);
	gSavedSettings.setBOOL("DoubleClickTeleport",		mDoubleClickTeleport);
	gSavedSettings.setBOOL("HideNotificationsInChat",	mHideNotificationsInChat);
	gSavedSettings.setBOOL("UseOldTrackingDots",		mUseOldTrackingDots);
	gSavedSettings.setBOOL("AllowMUpose",				mAllowMUpose);
	gSavedSettings.setBOOL("AutoCloseOOC",				mAutoCloseOOC);
	gSavedSettings.setBOOL("PlayTypingSound",			mPlayTypingSound);
	gSavedSettings.setBOOL("PrivateLookAt",				mPrivateLookAt);
	gSavedSettings.setBOOL("FetchInventoryOnLogin",		mFetchInventoryOnLogin);
	gSavedSettings.setBOOL("RestrainedLife",			mRestrainedLife);
	gSavedSettings.setBOOL("SecondsInChatAndIMs",		mSecondsInChatAndIMs);
}

void LLPrefsRainbowImpl::apply()
{
	std::string short_date, long_date, short_time, long_time, timestamp;	

	LLComboBox* combo = getChild<LLComboBox>("time_format_combobox");
	if (combo) {
		mTimeFormat = combo->getCurrentIndex();
	}

	combo = getChild<LLComboBox>("date_format_combobox");
	if (combo)
	{
		mDateFormat = combo->getCurrentIndex();
	}

	if (mTimeFormat == 0)
	{
		short_time = "%H:%M";
		long_time  = "%H:%M:%S";
		timestamp  = " %H:%M:%S";
	}
	else
	{
		short_time = "%I:%M %p";
		long_time  = "%I:%M:%S %p";
		timestamp  = " %I:%M %p";
	}

	if (mDateFormat == 0)
	{
		short_date = "%Y-%m-%d";
		long_date = "%A %d %B %Y";
		timestamp = "%a %d %b %Y" + timestamp;
	}
	else if (mDateFormat == 1)
	{
		short_date = "%d/%m/%Y";
		long_date = "%A %d %B %Y";
		timestamp = "%a %d %b %Y" + timestamp;
	}
	else
	{
		short_date = "%m/%d/%Y";
		long_date = "%A, %B %d %Y";
		timestamp = "%a %b %d %Y" + timestamp;
	}

	gSavedSettings.setString("ShortDateFormat",	short_date);
	gSavedSettings.setString("LongDateFormat",	long_date);
	gSavedSettings.setString("ShortTimeFormat",	short_time);
	gSavedSettings.setString("LongTimeFormat",	long_time);
	gSavedSettings.setString("TimestampFormat",	timestamp);
}

//---------------------------------------------------------------------------

LLPrefsRainbow::LLPrefsRainbow()
:	impl(* new LLPrefsRainbowImpl())
{
}

LLPrefsRainbow::~LLPrefsRainbow()
{
	delete &impl;
}

void LLPrefsRainbow::apply()
{
	impl.apply();
}

void LLPrefsRainbow::cancel()
{
	impl.cancel();
}

LLPanel* LLPrefsRainbow::getPanel()
{
	return &impl;
}
