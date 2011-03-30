/** 
 * @file llfloaterdisplayname.cpp
 * @author Leyla Farazha
 * @brief Implementation of the LLFloaterDisplayName class.
 *
 * backported for viewer 1.22 code base by Boy Lane
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */


#include "llviewerprecompiledheaders.h"
#include "llfloater.h"

#include "llviewerdisplayname.h"
#include "llfloaterdisplayname.h"
#include "llavatarnamecache.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"

#include "llagent.h"


LLFloaterDisplayName* LLFloaterDisplayName::sInstance = NULL;

LLFloaterDisplayName::LLFloaterDisplayName() : LLFloater(std::string("Display Name"))
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_display_name.xml");
	LLFloaterDisplayName::sInstance = this;
}

LLFloaterDisplayName::~LLFloaterDisplayName()
{
	LLFloaterDisplayName::sInstance = NULL;
}

void LLFloaterDisplayName::show()
{
	if (!LLFloaterDisplayName::sInstance)
	{
		LLFloaterDisplayName::sInstance = new LLFloaterDisplayName();
	}
	LLFloaterDisplayName::sInstance->open();
}

void LLFloaterDisplayName::onOpen()
{
	getChild<LLUICtrl>("display_name_editor")->clear();
	getChild<LLUICtrl>("display_name_confirm")->clear();

	LLAvatarName av_name;
	LLAvatarNameCache::get(gAgent.getID(), &av_name);

	F64 now_secs = LLDate::now().secondsSinceEpoch();

	if (now_secs < av_name.mNextUpdate)
	{
		// ...can't update until some time in the future
		F64 next_update_local_secs = av_name.mNextUpdate;
		std::string next_update_string;
#ifdef LOCALIZED_TIME
		timeToFormattedString((time_t)next_update_local_secs,
							  gSavedSettings.getString("TimestampFormat"),
							  next_update_string);
#else
		LLDate next_update_local(next_update_local_secs);
		next_update_string = next_update_local.asString();
#endif
		getChild<LLUICtrl>("lockout_text")->setTextArg("[TIME]", next_update_string);
		getChild<LLUICtrl>("lockout_text")->setVisible(true);
		getChild<LLUICtrl>("now_ok_text")->setVisible(false);
		getChild<LLUICtrl>("save_btn")->setEnabled(false);
		getChild<LLUICtrl>("display_name_editor")->setEnabled(false);
		getChild<LLUICtrl>("display_name_confirm")->setEnabled(false);
		getChild<LLUICtrl>("cancel_btn")->setFocus(TRUE);
		
	}
	else
	{
		getChild<LLUICtrl>("lockout_text")->setVisible(false);
		getChild<LLUICtrl>("now_ok_text")->setVisible(true);
		getChild<LLUICtrl>("save_btn")->setEnabled(true);
		getChild<LLUICtrl>("display_name_editor")->setEnabled(true);
		getChild<LLUICtrl>("display_name_confirm")->setEnabled(true);

	}
}

BOOL LLFloaterDisplayName::postBuild()
{
	childSetAction("reset_btn", onReset, this);
	childSetAction("cancel_btn", onCancel, this);
	childSetAction("save_btn", onSave, this);

	center();

	return TRUE;
}

void LLFloaterDisplayName::onCacheSetName(bool success,
										  const std::string& reason,
										  const LLSD& content)
{
	if (success)
	{
		// Inform the user that the change took place, but will take a while
		// to percolate.
		LLStringUtil::format_map_t args;
		args["DISPLAY_NAME"] = content["display_name"].asString();
		gViewerWindow->alertXml("SetDisplayNameSuccess", args);

		// Re-fetch my name, as it may have been sanitized by the service
		//LLAvatarNameCache::get(getAvatarId(),
		//	boost::bind(&LLPanelMyProfileEdit::onNameCache, this, _1, _2));
		return;
	}

	// Request failed, notify the user
	gViewerWindow->alertXml("SetDisplayNameFailedGeneric");
}

void LLFloaterDisplayName::onCancel(void* data)
{
	LLFloaterDisplayName* self = (LLFloaterDisplayName*)data;
	self->setVisible(false);
}

void LLFloaterDisplayName::onReset(void* data)
{
	LLFloaterDisplayName* self = (LLFloaterDisplayName*)data;

	if (LLAvatarNameCache::useDisplayNames())
	{
		LLViewerDisplayName::set("",
			boost::bind(&LLFloaterDisplayName::onCacheSetName, self, _1, _2, _3));
	}	
	else
	{
		gViewerWindow->alertXml("SetDisplayNameFailedGeneric");
	}

	self->setVisible(false);
}


void LLFloaterDisplayName::onSave(void* data)
{
	LLFloaterDisplayName* self = (LLFloaterDisplayName*)data;

	std::string display_name_utf8 = self->getChild<LLUICtrl>("display_name_editor")->getValue().asString();
	std::string display_name_confirm = self->getChild<LLUICtrl>("display_name_confirm")->getValue().asString();

	if (display_name_utf8.compare(display_name_confirm))
	{
		gViewerWindow->alertXml("SetDisplayNameMismatch");
		return;
	}

	const U32 DISPLAY_NAME_MAX_LENGTH = 31; // characters, not bytes
	LLWString display_name_wstr = utf8string_to_wstring(display_name_utf8);
	if (display_name_wstr.size() > DISPLAY_NAME_MAX_LENGTH)
	{
		LLStringUtil::format_map_t args;
		args["LENGTH"] = llformat("%d", DISPLAY_NAME_MAX_LENGTH);
		gViewerWindow->alertXml("SetDisplayNameFailedLength", args);
		return;
	}

	if (LLAvatarNameCache::useDisplayNames())
	{
		LLViewerDisplayName::set(display_name_utf8,
			boost::bind(&LLFloaterDisplayName::onCacheSetName, self, _1, _2, _3));	
	}
	else
	{
		gViewerWindow->alertXml("SetDisplayNameFailedGeneric");
	}

	self->setVisible(false);
}
