/**
 * @file hbfloatergrouptitles.h
 * @brief HBFloaterGroupTitles class implementation
 *
 * This class implements a floater where all available group titles are
 * listed, allowing the user to activate any via simple double-click.
 *
 * $LicenseInfo:firstyear=2010&license=viewergpl$
 * 
 * Copyright (c) 2010, Henri Beauchamp.
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

#include "message.h"
#include "roles_constants.h"

#include "hbfloatergrouptitles.h"

#include "llagent.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"

// static variable
HBFloaterGroupTitles* HBFloaterGroupTitles::sInstance = NULL;

// HBFloaterGroupTitlesObserver class

HBFloaterGroupTitlesObserver::HBFloaterGroupTitlesObserver(HBFloaterGroupTitles* instance, const LLUUID& group_id)
:	LLGroupMgrObserver(group_id),
	mFloaterInstance(instance)
{
	LLGroupMgr::getInstance()->addObserver(this);
}

// virtual
HBFloaterGroupTitlesObserver::~HBFloaterGroupTitlesObserver()
{
	LLGroupMgr::getInstance()->removeObserver(this);
}

// virtual
void HBFloaterGroupTitlesObserver::changed(LLGroupChange gc)
{
	if (gc != GC_PROPERTIES)
	{
		mFloaterInstance->setDirty();
	}
}


// HBFloaterGroupTitles class

HBFloaterGroupTitles::HBFloaterGroupTitles()
:	LLFloater(std::string("group titles")),
	mIsDirty(true)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_group_titles.xml", NULL);
	gAgent.addListener(this, "new group");
	sInstance = this;
}

// virtual
HBFloaterGroupTitles::~HBFloaterGroupTitles()
{
	gAgent.removeListener(this);
	std::map<LLUUID, HBFloaterGroupTitlesObserver*>::iterator it;
	while (!mObservers.empty())
	{
		it = mObservers.begin();
		HBFloaterGroupTitlesObserver* observer = (*it).second;
		delete observer;
		mObservers.erase(it);
	}
	sInstance = NULL;
}

// static
void HBFloaterGroupTitles::show()
{
	if (!sInstance)
	{
		sInstance = new HBFloaterGroupTitles();
	}
	sInstance->setFocus(TRUE);
	sInstance->open();
}

// virtual
BOOL HBFloaterGroupTitles::postBuild()
{
	mTitlesList = getChild<LLScrollListCtrl>("titles_list");
	if (!mTitlesList)
	{
		return FALSE;
	}
	mTitlesList->setCallbackUserData(this);
	mTitlesList->setDoubleClickCallback(onActivate);
	childSetAction("close", onCloseButtonPressed, this);
	childSetAction("refresh", onRefreshButtonPressed, this);
	childSetAction("activate", onActivate, this);
	return TRUE;
}

// virtual
void HBFloaterGroupTitles::draw()
{
	if (mIsDirty && mTitlesList)
	{
		S32 i;
		S32 count = gAgent.mGroups.count();
		LLUUID id;
		LLUUID highlight_id = LLUUID::null;
		LLUUID current_group_id = gAgent.getGroupID();
		std::vector<LLGroupTitle>::const_iterator citer;
		std::string style;
		LLGroupData* group_datap;
		LLGroupMgrGroupData* gmgr_datap;
		LLSD element;

		LLCtrlListInterface *title_list = mTitlesList->getListInterface();
		S32 scrollpos = mTitlesList->getScrollPos();
		mTitlesList->deleteAllItems();

		for (i = 0; i < count; ++i)
		{
			group_datap = &gAgent.mGroups.get(i);
			id = group_datap->mID;
			// Add an observer for this group if there's none so far.
			if (mObservers.find(id) == mObservers.end())
			{
				HBFloaterGroupTitlesObserver* observer = new HBFloaterGroupTitlesObserver(this, id);
				mObservers.insert(std::pair<LLUUID, HBFloaterGroupTitlesObserver*>(id, observer));
			}
			gmgr_datap = LLGroupMgr::getInstance()->getGroupData(id);
			if (!gmgr_datap)
			{
				LLGroupMgr::getInstance()->sendGroupTitlesRequest(id);
				continue;
			}
			for (citer = gmgr_datap->mTitles.begin(); citer != gmgr_datap->mTitles.end(); citer++)
			{
				style = "NORMAL";
				if (current_group_id == id && citer->mSelected)
				{
					style = "BOLD";
					highlight_id = citer->mRoleID;
				}
				element["id"] = citer->mRoleID;
				element["columns"][LIST_TITLE]["column"] = "title";
				element["columns"][LIST_TITLE]["value"] = citer->mTitle;
				element["columns"][LIST_TITLE]["font-style"] = style;
				element["columns"][LIST_GROUP_NAME]["column"] = "group_name";
				element["columns"][LIST_GROUP_NAME]["value"] = group_datap->mName;
				element["columns"][LIST_GROUP_NAME]["font-style"] = style;
				element["columns"][LIST_GROUP_ID]["column"] = "group_id";
				element["columns"][LIST_GROUP_ID]["value"] = id;
				title_list->addElement(element, ADD_SORTED);
			}
		}

		// add "none" to list at top
		style = "NORMAL";
		if (current_group_id.isNull())
		{
			style = "BOLD";
		}
		element["id"] = LLUUID::null;
		element["columns"][LIST_TITLE]["column"] = "title";
		element["columns"][LIST_TITLE]["value"] = "none";
		element["columns"][LIST_TITLE]["font-style"] = style;
		element["columns"][LIST_GROUP_NAME]["column"] = "group_name";
		element["columns"][LIST_GROUP_NAME]["value"] = "none";
		element["columns"][LIST_GROUP_NAME]["font-style"] = style;
		element["columns"][LIST_GROUP_ID]["column"] = "group_id";
		element["columns"][LIST_GROUP_ID]["value"] = LLUUID::null;
		title_list->addElement(element, ADD_TOP);

		mTitlesList->setScrollPos(scrollpos);
		title_list->selectByValue(highlight_id);
		mIsDirty = false;
	}

	LLFloater::draw();
}

// static
void HBFloaterGroupTitles::onCloseButtonPressed(void* userdata)
{
	HBFloaterGroupTitles* self = (HBFloaterGroupTitles*) userdata;
	if (self)
	{
		self->close();
	}
}

// static
void HBFloaterGroupTitles::onRefreshButtonPressed(void* userdata)
{
	HBFloaterGroupTitles* self = (HBFloaterGroupTitles*) userdata;
	if (!self) return;
	std::map<LLUUID, HBFloaterGroupTitlesObserver*>::iterator it;
	while (!self->mObservers.empty())
	{
		it = self->mObservers.begin();
		HBFloaterGroupTitlesObserver* observer = (*it).second;
		delete observer;
		self->mObservers.erase(it);
	}
	sInstance = NULL;
	LLGroupMgr::debugClearAllGroups(NULL);
	self->setDirty();
}

// static
void HBFloaterGroupTitles::onActivate(void* userdata)
{
	HBFloaterGroupTitles* self = (HBFloaterGroupTitles*) userdata;
	if (!self || !self->mTitlesList) return;
	LLScrollListItem *item = self->mTitlesList->getFirstSelected();
	if (!item) return;

	// Get the group id
	LLUUID group_id = item->getColumn(LIST_GROUP_ID)->getValue().asUUID();

	// Set the title for this group
	LLGroupMgr::getInstance()->sendGroupTitleUpdate(group_id, item->getUUID());

	// Set the group if needed.
	LLUUID old_group_id = gAgent.getGroupID();
//MK
	if (group_id != old_group_id && (!gRRenabled || !gAgent.mRRInterface.contains("setgroup")))
//mk
	{
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_ActivateGroup);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->addUUIDFast(_PREHASH_GroupID, group_id);
		gAgent.sendReliableMessage();
	}
	else
	{
		// Force a refresh via the observer
		if (group_id == LLUUID::null)
		{
			group_id = old_group_id;
		}
		LLGroupMgr::getInstance()->sendGroupTitlesRequest(group_id);
	}
}

// LLSimpleListener
//virtual
bool HBFloaterGroupTitles::handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
{
	if (event->desc() == "new group")
	{
		setDirty();
		return true;
	}
	return false;
}
