/** 
 * @file llfloaterinspect.cpp
 * @brief Floater for object inspection tool
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 * 
 * Copyright (c) 2006-2009, Linden Research, Inc.
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
#include "llfloateravatarinfo.h"
#include "llfloaterinspect.h"
#include "llfloatertools.h"
#include "llcachename.h"
#include "llscrolllistctrl.h"
#include "llselectmgr.h"
#include "lltoolcomp.h"
#include "lltoolmgr.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"
#include "lluictrlfactory.h"

LLFloaterInspect* LLFloaterInspect::sInstance = NULL;

LLFloaterInspect::LLFloaterInspect(void) :
	LLFloater(std::string("Inspect Object")),
	mDirty(FALSE),
	mQueuedInventoryRequest(LLUUID::null)
{
	sInstance = this;
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_inspect.xml");
}

LLFloaterInspect::~LLFloaterInspect(void)
{
	if(!gFloaterTools->getVisible())
	{
		if(LLToolMgr::getInstance()->getBaseTool() == LLToolCompInspect::getInstance())
		{
			LLToolMgr::getInstance()->clearTransientTool();
		}
		// Switch back to basic toolset
		LLToolMgr::getInstance()->setCurrentToolset(gBasicToolset);	
	}
	else
	{
		gFloaterTools->setFocus(TRUE);
	}
	sInstance = NULL;
}

BOOL LLFloaterInspect::isVisible()
{
	return (!!sInstance);
}

void LLFloaterInspect::show(void* ignored)
{
	// setForceSelection ensures that the pie menu does not deselect things when it 
	// looses the focus (this can happen with "select own objects only" enabled
	// VWR-1471
	BOOL forcesel = LLSelectMgr::getInstance()->setForceSelection(TRUE);

	if (!sInstance)	// first use
	{
		sInstance = new LLFloaterInspect;
	}

	sInstance->open();
	LLToolMgr::getInstance()->setTransientTool(LLToolCompInspect::getInstance());
	LLSelectMgr::getInstance()->setForceSelection(forcesel);	// restore previouis value

	sInstance->mObjectSelection = LLSelectMgr::getInstance()->getSelection();
	sInstance->refresh();
}

void LLFloaterInspect::onClickCreatorProfile(void* ctrl)
{
	if(sInstance->mObjectList->getAllSelected().size() == 0)
	{
		return;
	}
	LLScrollListItem* first_selected =
		sInstance->mObjectList->getFirstSelected();

	if (first_selected)
	{
		struct f : public LLSelectedNodeFunctor
		{
			LLUUID obj_id;
			f(const LLUUID& id) : obj_id(id) {}
			virtual bool apply(LLSelectNode* node)
			{
				return (obj_id == node->getObject()->getID());
			}
		} func(first_selected->getUUID());
		LLSelectNode* node = sInstance->mObjectSelection->getFirstNode(&func);
		if(node)
		{
			LLFloaterAvatarInfo::showFromDirectory(node->mPermissions->getCreator());
		}
	}
}

void LLFloaterInspect::onClickOwnerProfile(void* ctrl)
{
	if(sInstance->mObjectList->getAllSelected().size() == 0) return;
	LLScrollListItem* first_selected =
		sInstance->mObjectList->getFirstSelected();

	if (first_selected)
	{
		LLUUID selected_id = first_selected->getUUID();
		struct f : public LLSelectedNodeFunctor
		{
			LLUUID obj_id;
			f(const LLUUID& id) : obj_id(id) {}
			virtual bool apply(LLSelectNode* node)
			{
				return (obj_id == node->getObject()->getID());
			}
		} func(selected_id);
		LLSelectNode* node = sInstance->mObjectSelection->getFirstNode(&func);
		if(node)
		{
			const LLUUID& owner_id = node->mPermissions->getOwner();
			LLFloaterAvatarInfo::showFromDirectory(owner_id);
		}
	}
}

void LLFloaterInspect::onClickRefresh(void* data)
{
	if (sInstance)
	{
		sInstance->mInventoryNums.clear();
		dirty();
	}
}

void LLFloaterInspect::onClickClose(void* data)
{
	LLFloaterInspect* self = (LLFloaterInspect*)data;
	if (self)
	{
		self->close();
	}
}

BOOL LLFloaterInspect::postBuild()
{
	mObjectList = getChild<LLScrollListCtrl>("object_list");
	childSetAction("button owner", onClickOwnerProfile, this);
	childSetAction("button creator", onClickCreatorProfile, this);
	childSetAction("refresh", onClickRefresh, this);
	childSetAction("close", onClickClose, this);
	childSetCommitCallback("object_list", onSelectObject);
	return TRUE;
}

void LLFloaterInspect::onSelectObject(LLUICtrl* ctrl, void* user_data)
{
	if(LLFloaterInspect::getSelectedUUID() != LLUUID::null)
	{
		sInstance->childSetEnabled("button owner", true);
		sInstance->childSetEnabled("button creator", true);
	}
}

LLUUID LLFloaterInspect::getSelectedUUID()
{
	if(sInstance)
	{
		if(sInstance->mObjectList->getAllSelected().size() > 0)
		{
			LLScrollListItem* first_selected =
				sInstance->mObjectList->getFirstSelected();
			if (first_selected)
			{
				return first_selected->getUUID();
			}
		}
	}
	return LLUUID::null;
}

void LLFloaterInspect::refresh()
{
	LLUUID creator_id;
	std::string creator_name;
	S32 pos = mObjectList->getScrollPos();
	childSetEnabled("button owner", false);
	childSetEnabled("button creator", false);
	LLUUID selected_uuid;
	S32 selected_index = mObjectList->getFirstSelectedIndex();
	if(selected_index > -1)
	{
		LLScrollListItem* first_selected =
			mObjectList->getFirstSelected();
		if (first_selected)
		{
			selected_uuid = first_selected->getUUID();
		}
	}
	mObjectList->deleteAllItems();

	//List all transient objects, then all linked objects
	for (LLObjectSelection::valid_iterator iter = mObjectSelection->valid_begin();
		 iter != mObjectSelection->valid_end(); iter++)
	{
		LLSelectNode* obj = *iter;
		if (!obj || obj->mCreationDate == 0)
		{	// Don't have valid information from the server, so skip this one
			continue;
		}
		time_t timestamp = (time_t) (obj->mCreationDate / 1000000);
#ifdef LOCALIZED_TIME
		std::string time;
		timeToFormattedString(timestamp, gSavedSettings.getString("TimestampFormat"), time);
#else
		char time[MAX_STRING];
		LLStringUtil::copy(time, ctime(&timestamp), MAX_STRING);
		time[24] = '\0';
#endif
		std::string owner_name, creator_name, last_owner_name;
		gCacheName->getFullName(obj->mPermissions->getOwner(), owner_name);
		gCacheName->getFullName(obj->mPermissions->getCreator(), creator_name);
		gCacheName->getFullName(obj->mPermissions->getLastOwner(), last_owner_name);
		LLViewerObject* vobj = obj->getObject();
		LLUUID id = vobj->getID();
		S32 scripts, total;
		std::map<LLUUID, std::pair<S32,S32> >::iterator itr = mInventoryNums.find(id);
		if (itr != mInventoryNums.end())
		{
			scripts = itr->second.first;
			total = itr->second.second;
		}
		else
		{
			scripts = total = -1;
			if (mQueuedInventoryRequest.isNull())
			{
				mQueuedInventoryRequest = id;
				registerVOInventoryListener(vobj, NULL);
				requestVOInventory();
				total = -2;
			}
		}
		LLSD row;
		row["id"] = id;
		row["columns"][0]["column"] = "object_name";
		row["columns"][0]["type"] = "text";
		// make sure we're either at the top of the link chain
		// or top of the editable chain, for attachments
		if (vobj->isRoot() || vobj->isRootEdit())
		{
			row["columns"][0]["value"] = obj->mName;
			row["columns"][0]["font-style"] = "BOLD";
		}
		else
		{
			row["columns"][0]["value"] = std::string("   ") + obj->mName;
		}
		row["columns"][1]["column"] = "owner_name";
		row["columns"][1]["type"] = "text";
		row["columns"][1]["value"] = owner_name;
		row["columns"][2]["column"] = "last_owner_name";
		row["columns"][2]["type"] = "text";
		row["columns"][2]["value"] = last_owner_name;
		row["columns"][3]["column"] = "creator_name";
		row["columns"][3]["type"] = "text";
		row["columns"][3]["value"] = creator_name;
		row["columns"][4]["column"] = "creation_date";
		row["columns"][4]["type"] = "text";
		row["columns"][4]["value"] = time;
		row["columns"][5]["column"] = "inventory";
		row["columns"][5]["type"] = "text";
		row["columns"][5]["value"] = (total < 0 ? (total == -2 ? "loading..." : "?")
												: llformat("%d/%d", scripts, total));
		mObjectList->addElement(row, ADD_TOP);
	}
	if(selected_index > -1 && mObjectList->getItemIndex(selected_uuid) == selected_index)
	{
		mObjectList->selectNthItem(selected_index);
	}
	else
	{
		mObjectList->selectNthItem(0);
	}
	onSelectObject(this, NULL);
	mObjectList->setScrollPos(pos);
}

void LLFloaterInspect::inventoryChanged(LLViewerObject* viewer_object,
										InventoryObjectList* inv, S32, void*)
{
	if (!viewer_object || !inv)
	{
		return;
	}
	const LLUUID id = viewer_object->getID();
	if (id == mQueuedInventoryRequest)
	{
		S32 scripts = 0, total = 0;
		LLAssetType::EType type;
		InventoryObjectList::const_iterator it;
		for (it = inv->begin(); it != inv->end(); it++)
		{
			type = (*it)->getType();
			if (type == LLAssetType::AT_LSL_TEXT ||
			type == LLAssetType::AT_SCRIPT)		// Legacy scripts
			{
				scripts++;
			}
			if (type != LLAssetType::AT_LSL_BYTECODE && 	// Do not count the bytecode associated with AT_LSL_TEXT
				type != LLAssetType::AT_ROOT_CATEGORY &&	// Do not count the root folder
				type != LLAssetType::AT_CATEGORY &&			// Do not count folders
				type != LLAssetType::AT_NONE)				// There's one such unknown item per prim...
			{
				total++;
			}
		}
		mQueuedInventoryRequest = LLUUID::null;
		removeVOInventoryListener();
		mInventoryNums[id] = std::make_pair(scripts, total);
		mDirty = TRUE;
	}
}

void LLFloaterInspect::onFocusReceived()
{
	LLToolMgr::getInstance()->setTransientTool(LLToolCompInspect::getInstance());
	LLFloater::onFocusReceived();
}

void LLFloaterInspect::dirty()
{
	if (sInstance)
	{
		if (!sInstance->mQueuedInventoryRequest.isNull())
		{
			sInstance->removeVOInventoryListener();
			sInstance->mQueuedInventoryRequest = LLUUID::null;
		}
		sInstance->setDirty();
	}
}

void LLFloaterInspect::draw()
{
	if (mDirty)
	{
		mDirty = FALSE;
		refresh();
	}

	LLFloater::draw();
}
