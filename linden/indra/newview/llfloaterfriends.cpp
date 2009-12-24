/** 
 * @file llfloaterfriends.cpp
 * @author Phoenix
 * @date 2005-01-13
 * @brief Implementation of the friends floater
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

#include "llfloaterfriends.h"

#include <sstream>

#include "lldir.h"

#include "llagent.h"
#include "llappviewer.h"	// for gLastVersionChannel
#include "llfloateravatarpicker.h"
#include "llviewerwindow.h"
#include "llbutton.h"
#include "llfloateravatarinfo.h"
#include "llinventorymodel.h"
#include "llnamelistctrl.h"
#include "llnotify.h"
#include "llresmgr.h"
#include "llimview.h"
#include "lluictrlfactory.h"
#include "llmenucommands.h"
#include "llviewercontrol.h"
#include "llviewermessage.h"
#include "lltimer.h"
#include "lltextbox.h"
#include "llvoiceclient.h"

//Maximum number of people you can select to do an operation on at once.
#define MAX_FRIEND_SELECT 20
#define DEFAULT_PERIOD 5.0
#define RIGHTS_CHANGE_TIMEOUT 5.0
#define OBSERVER_TIMEOUT 0.5

#define ONLINE_SIP_ICON_NAME "slim_icon_16_viewer.tga"

// simple class to observe the calling cards.
class LLLocalFriendsObserver : public LLFriendObserver, public LLEventTimer
{
public: 
	LLLocalFriendsObserver(LLPanelFriends* floater) : mFloater(floater), LLEventTimer(OBSERVER_TIMEOUT)
	{
		mEventTimer.stop();
	}
	virtual ~LLLocalFriendsObserver()
	{
		mFloater = NULL;
	}
	virtual void changed(U32 mask)
	{
		// events can arrive quickly in bulk - we need not process EVERY one of them -
		// so we wait a short while to let others pile-in, and process them in aggregate.
		mEventTimer.start();

		// save-up all the mask-bits which have come-in
		mMask |= mask;
	}
	virtual BOOL tick()
	{
		mFloater->updateFriends(mMask);

		mEventTimer.stop();
		mMask = 0;

		return FALSE;
	}
	
protected:
	LLPanelFriends* mFloater;
	U32 mMask;
};

LLPanelFriends::LLPanelFriends() :
	LLPanel(),
	LLEventTimer(DEFAULT_PERIOD),
	mObserver(NULL),
	mShowMaxSelectWarning(TRUE),
	mAllowRightsChange(TRUE),
	mNumRightsChanged(0)
{
	mEventTimer.stop();
	mObserver = new LLLocalFriendsObserver(this);
	LLAvatarTracker::instance().addObserver(mObserver);
	// For notification when SIP online status changes.
	LLVoiceClient::getInstance()->addObserver(mObserver);
}

LLPanelFriends::~LLPanelFriends()
{
	// For notification when SIP online status changes.
	LLVoiceClient::getInstance()->removeObserver(mObserver);
	LLAvatarTracker::instance().removeObserver(mObserver);
	delete mObserver;
}

BOOL LLPanelFriends::tick()
{
	mEventTimer.stop();
	mPeriod = DEFAULT_PERIOD;
	mAllowRightsChange = TRUE;
	updateFriends(LLFriendObserver::ADD);
	return FALSE;
}

void LLPanelFriends::updateFriends(U32 changed_mask)
{
	LLUUID selected_id;
	LLCtrlListInterface *friends_list = childGetListInterface("friend_list");
	if (!friends_list) return;
	LLCtrlScrollInterface *friends_scroll = childGetScrollInterface("friend_list");
	if (!friends_scroll) return;
	
	// We kill the selection warning, otherwise we'll spam with warning popups
	// if the maximum amount of friends are selected
	mShowMaxSelectWarning = false;

	LLDynamicArray<LLUUID> selected_friends = getSelectedIDs();
	if(changed_mask & (LLFriendObserver::ADD | LLFriendObserver::REMOVE | LLFriendObserver::ONLINE))
	{
		refreshNames(changed_mask);
	}
	else if(changed_mask & LLFriendObserver::POWERS)
	{
		--mNumRightsChanged;
		if(mNumRightsChanged > 0)
		{
			mPeriod = RIGHTS_CHANGE_TIMEOUT;	
			mEventTimer.start();
			mAllowRightsChange = FALSE;
		}
		else
		{
			tick();
		}
	}
	if(selected_friends.size() > 0)
	{
		// only non-null if friends was already found. This may fail,
		// but we don't really care here, because refreshUI() will
		// clean up the interface.
		friends_list->setCurrentByID(selected_id);
		for(LLDynamicArray<LLUUID>::iterator itr = selected_friends.begin(); itr != selected_friends.end(); ++itr)
		{
			friends_list->setSelectedByValue(*itr, true);
		}
	}

	refreshUI();
	mShowMaxSelectWarning = true;
}

// virtual
BOOL LLPanelFriends::postBuild()
{
	mFriendsList = getChild<LLScrollListCtrl>("friend_list");
	mFriendsList->setMaxSelectable(MAX_FRIEND_SELECT);
	mFriendsList->setMaximumSelectCallback(onMaximumSelect);
	mFriendsList->setCommitOnSelectionChange(TRUE);
	childSetCommitCallback("friend_list", onSelectName, this);
	childSetDoubleClickCallback("friend_list", onClickIM);

	U32 changed_mask = LLFriendObserver::ADD | LLFriendObserver::REMOVE | LLFriendObserver::ONLINE;
	refreshNames(changed_mask);

	childSetAction("im_btn", onClickIM, this);
	childSetAction("profile_btn", onClickProfile, this);
	childSetAction("offer_teleport_btn", onClickOfferTeleport, this);
	childSetAction("pay_btn", onClickPay, this);
	childSetAction("add_btn", onClickAddFriend, this);
	childSetAction("remove_btn", onClickRemove, this);

	setDefaultBtn("im_btn");

	updateFriends(LLFriendObserver::ADD);
	refreshUI();

	// primary sort = online status, secondary sort = name
	mFriendsList->sortByColumn(std::string("friend_name"), TRUE);
	mFriendsList->sortByColumn(std::string("icon_online_status"), FALSE);

	return TRUE;
}

BOOL LLPanelFriends::addFriend(const LLUUID& agent_id)
{
	LLAvatarTracker& at = LLAvatarTracker::instance();
	const LLRelationship* relationInfo = at.getBuddyInfo(agent_id);
	if(!relationInfo) return FALSE;

	bool isOnlineSIP = LLVoiceClient::getInstance()->isOnlineSIP(agent_id);
	bool isOnline = relationInfo->isOnline();

	std::string fullname;
	BOOL have_name = gCacheName->getFullName(agent_id, fullname);
	
	LLSD element;
	element["id"] = agent_id;
	LLSD& friend_column = element["columns"][LIST_FRIEND_NAME];
	friend_column["column"] = "friend_name";
	friend_column["value"] = fullname;
	friend_column["font"] = "SANSSERIF";
	friend_column["font-style"] = "NORMAL";	

	LLSD& online_status_column = element["columns"][LIST_ONLINE_STATUS];
	online_status_column["column"] = "icon_online_status";
	online_status_column["type"] = "icon";
	
	if (isOnline)
	{
		friend_column["font-style"] = "BOLD";	
		online_status_column["value"] = "icon_avatar_online.tga";
	}
	else if(isOnlineSIP)
	{
		friend_column["font-style"] = "BOLD";	
		online_status_column["value"] = ONLINE_SIP_ICON_NAME;
	}

	LLSD& online_column = element["columns"][LIST_VISIBLE_ONLINE];
	online_column["column"] = "icon_visible_online";
	online_column["type"] = "checkbox";
	online_column["value"] = relationInfo->isRightGrantedTo(LLRelationship::GRANT_ONLINE_STATUS);

	LLSD& visible_map_column = element["columns"][LIST_VISIBLE_MAP];
	visible_map_column["column"] = "icon_visible_map";
	visible_map_column["type"] = "checkbox";
	visible_map_column["value"] = relationInfo->isRightGrantedTo(LLRelationship::GRANT_MAP_LOCATION);

	LLSD& edit_my_object_column = element["columns"][LIST_EDIT_MINE];
	edit_my_object_column["column"] = "icon_edit_mine";
	edit_my_object_column["type"] = "checkbox";
	edit_my_object_column["value"] = relationInfo->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS);

	LLSD& edit_their_object_column = element["columns"][LIST_EDIT_THEIRS];
	edit_their_object_column["column"] = "icon_edit_theirs";
	edit_their_object_column["type"] = "checkbox";
	edit_their_object_column["enabled"] = "";
	edit_their_object_column["value"] = relationInfo->isRightGrantedFrom(LLRelationship::GRANT_MODIFY_OBJECTS);

	LLSD& update_gen_column = element["columns"][LIST_FRIEND_UPDATE_GEN];
	update_gen_column["column"] = "friend_last_update_generation";
	update_gen_column["value"] = have_name ? relationInfo->getChangeSerialNum() : -1;

	mFriendsList->addElement(element, ADD_BOTTOM);
	return have_name;
}

// propagate actual relationship to UI.
// Does not resort the UI list because it can be called frequently. JC
BOOL LLPanelFriends::updateFriendItem(const LLUUID& agent_id, const LLRelationship* info)
{
	if (!info) return FALSE;
	LLScrollListItem* itemp = mFriendsList->getItem(agent_id);
	if (!itemp) return FALSE;
	
	bool isOnlineSIP = LLVoiceClient::getInstance()->isOnlineSIP(itemp->getUUID());
	bool isOnline = info->isOnline();

	std::string fullname;
	BOOL have_name = gCacheName->getFullName(agent_id, fullname);
	
	// Name of the status icon to use
	std::string statusIcon;
	
	if(isOnline)
	{
		statusIcon = "icon_avatar_online.tga";
	}
	else if(isOnlineSIP)
	{
		statusIcon = ONLINE_SIP_ICON_NAME;
	}

	itemp->getColumn(LIST_ONLINE_STATUS)->setValue(statusIcon);
	
	itemp->getColumn(LIST_FRIEND_NAME)->setValue(fullname);
	// render name of online friends in bold text
	((LLScrollListText*)itemp->getColumn(LIST_FRIEND_NAME))->setFontStyle((isOnline || isOnlineSIP) ? LLFontGL::BOLD : LLFontGL::NORMAL);	
	itemp->getColumn(LIST_VISIBLE_ONLINE)->setValue(info->isRightGrantedTo(LLRelationship::GRANT_ONLINE_STATUS));
	itemp->getColumn(LIST_VISIBLE_MAP)->setValue(info->isRightGrantedTo(LLRelationship::GRANT_MAP_LOCATION));
	itemp->getColumn(LIST_EDIT_MINE)->setValue(info->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS));
	S32 change_generation = have_name ? info->getChangeSerialNum() : -1;
	itemp->getColumn(LIST_FRIEND_UPDATE_GEN)->setValue(change_generation);

	// enable this item, in case it was disabled after user input
	itemp->setEnabled(TRUE);

	// Do not resort, this function can be called frequently.
	return have_name;
}

void LLPanelFriends::refreshRightsChangeList()
{
	LLDynamicArray<LLUUID> friends = getSelectedIDs();
	S32 num_selected = friends.size();

	bool can_offer_teleport = num_selected >= 1;
	bool selected_friends_online = true;

	LLTextBox* processing_label = getChild<LLTextBox>("process_rights_label");

	if(!mAllowRightsChange)
	{
		if(processing_label)
		{
			processing_label->setVisible(true);
			// ignore selection for now
			friends.clear();
			num_selected = 0;
		}
	}
	else if(processing_label)
	{
		processing_label->setVisible(false);
	}

	const LLRelationship* friend_status = NULL;
	for(LLDynamicArray<LLUUID>::iterator itr = friends.begin(); itr != friends.end(); ++itr)
	{
		friend_status = LLAvatarTracker::instance().getBuddyInfo(*itr);
		if (friend_status)
		{
			if(!friend_status->isOnline())
			{
				can_offer_teleport = false;
				selected_friends_online = false;
			}
		}
		else // missing buddy info, don't allow any operations
		{
			can_offer_teleport = false;
		}
	}
	
	if (num_selected == 0)  // nothing selected
	{
		childSetEnabled("im_btn", FALSE);
		childSetEnabled("offer_teleport_btn", FALSE);
	}
	else // we have at least one friend selected...
	{
		// only allow IMs to groups when everyone in the group is online
		// to be consistent with context menus in inventory and because otherwise
		// offline friends would be silently dropped from the session
		childSetEnabled("im_btn", selected_friends_online || num_selected == 1);
		childSetEnabled("offer_teleport_btn", can_offer_teleport);
	}
}

struct SortFriendsByID
{
	bool operator() (const LLScrollListItem* const a, const LLScrollListItem* const b) const
	{
		return a->getValue().asUUID() < b->getValue().asUUID();
	}
};

void LLPanelFriends::refreshNames(U32 changed_mask)
{
	LLDynamicArray<LLUUID> selected_ids = getSelectedIDs();	
	S32 pos = mFriendsList->getScrollPos();	
	
	// get all buddies we know about
	LLAvatarTracker::buddy_map_t all_buddies;
	LLAvatarTracker::instance().copyBuddyList(all_buddies);

	BOOL have_names = TRUE;

	if(changed_mask & (LLFriendObserver::ADD | LLFriendObserver::REMOVE))
	{
		have_names &= refreshNamesSync(all_buddies);
	}

	if(changed_mask & LLFriendObserver::ONLINE)
	{
		have_names &= refreshNamesPresence(all_buddies);
	}

	if (!have_names)
	{
		mEventTimer.start();
	}
	// Changed item in place, need to request sort and update columns
	// because we might have changed data in a column on which the user
	// has already sorted. JC
	mFriendsList->sortItems();

	// re-select items
	mFriendsList->selectMultiple(selected_ids);
	mFriendsList->setScrollPos(pos);
}

BOOL LLPanelFriends::refreshNamesSync(const LLAvatarTracker::buddy_map_t & all_buddies)
{
	mFriendsList->deleteAllItems();

	BOOL have_names = TRUE;
	LLAvatarTracker::buddy_map_t::const_iterator buddy_it = all_buddies.begin();

	for(; buddy_it != all_buddies.end(); ++buddy_it)
	{
		have_names &= addFriend(buddy_it->first);
	}

	return have_names;
}

BOOL LLPanelFriends::refreshNamesPresence(const LLAvatarTracker::buddy_map_t & all_buddies)
{
	std::vector<LLScrollListItem*> items = mFriendsList->getAllData();
	std::sort(items.begin(), items.end(), SortFriendsByID());

	LLAvatarTracker::buddy_map_t::const_iterator buddy_it  = all_buddies.begin();
	std::vector<LLScrollListItem*>::const_iterator item_it = items.begin();
	BOOL have_names = TRUE;

	while(true)
	{
		if(item_it == items.end() || buddy_it == all_buddies.end())
		{
			break;
		}

		const LLUUID & buddy_uuid = buddy_it->first;
		const LLUUID & item_uuid  = (*item_it)->getValue().asUUID();
		if(item_uuid == buddy_uuid)
		{
			const LLRelationship* info = buddy_it->second;
			if (!info) 
			{	
				++item_it;
				continue;
			}
			
			S32 last_change_generation = (*item_it)->getColumn(LIST_FRIEND_UPDATE_GEN)->getValue().asInteger();
			if (last_change_generation < info->getChangeSerialNum())
			{
				// update existing item in UI
				have_names &= updateFriendItem(buddy_it->first, info);
			}

			++buddy_it;
			++item_it;
		}
		else if(item_uuid < buddy_uuid)
		{
			++item_it;
		}
		else //if(item_uuid > buddy_uuid)
		{
			++buddy_it;
		}
	}

	return have_names;
}

void LLPanelFriends::refreshUI()
{	
	BOOL single_selected = FALSE;
	BOOL multiple_selected = FALSE;
	int num_selected = mFriendsList->getAllSelected().size();
	if(num_selected > 0)
	{
		single_selected = TRUE;
		if(num_selected > 1)
		{
			childSetText("friend_name_label", getString("Multiple"));
			multiple_selected = TRUE;		
		}
		else
		{			
			childSetText("friend_name_label", mFriendsList->getFirstSelected()->getColumn(LIST_FRIEND_NAME)->getValue().asString() + "...");
		}
	}
	else
	{
		childSetText("friend_name_label", LLStringUtil::null);
	}


	//Options that can only be performed with one friend selected
	childSetEnabled("profile_btn", single_selected && !multiple_selected);
	childSetEnabled("pay_btn", single_selected && !multiple_selected);

	//Options that can be performed with up to MAX_FRIEND_SELECT friends selected
	//(single_selected will always be true in this situations)
	childSetEnabled("remove_btn", single_selected);
	childSetEnabled("im_btn", single_selected);
	childSetEnabled("friend_rights", single_selected);

	refreshRightsChangeList();
}

LLDynamicArray<LLUUID> LLPanelFriends::getSelectedIDs()
{
	LLUUID selected_id;
	LLDynamicArray<LLUUID> friend_ids;
	std::vector<LLScrollListItem*> selected = mFriendsList->getAllSelected();
	for(std::vector<LLScrollListItem*>::iterator itr = selected.begin(); itr != selected.end(); ++itr)
	{
		friend_ids.push_back((*itr)->getUUID());
	}
	return friend_ids;
}

// static
void LLPanelFriends::onSelectName(LLUICtrl* ctrl, void* user_data)
{
	LLPanelFriends* panelp = (LLPanelFriends*)user_data;

	if(panelp)
	{
		panelp->refreshUI();
		// check to see if rights have changed
		panelp->applyRightsToFriends();
	}
}

//static
void LLPanelFriends::onMaximumSelect(void* user_data)
{
	LLStringUtil::format_map_t args;
	args["[MAX_SELECT]"] = llformat("%d", MAX_FRIEND_SELECT);
	LLNotifyBox::showXml("MaxListSelectMessage", args);
};

// static
void LLPanelFriends::onClickProfile(void* user_data)
{
	LLPanelFriends* panelp = (LLPanelFriends*)user_data;

	//llinfos << "LLPanelFriends::onClickProfile()" << llendl;
	LLDynamicArray<LLUUID> ids = panelp->getSelectedIDs();
	if(ids.size() > 0)
	{
		LLUUID agent_id = ids[0];
		BOOL online;
		online = LLAvatarTracker::instance().isBuddyOnline(agent_id);
		LLFloaterAvatarInfo::showFromFriend(agent_id, online);
	}
}

// static
void LLPanelFriends::onClickIM(void* user_data)
{
	LLPanelFriends* panelp = (LLPanelFriends*)user_data;

	//llinfos << "LLPanelFriends::onClickIM()" << llendl;
	LLDynamicArray<LLUUID> ids = panelp->getSelectedIDs();
	if(ids.size() > 0)
	{
		if(ids.size() == 1)
		{
			LLUUID agent_id = ids[0];
			const LLRelationship* info = LLAvatarTracker::instance().getBuddyInfo(agent_id);
			std::string fullname;
			if(info && gCacheName->getFullName(agent_id, fullname))
			{
				gIMMgr->setFloaterOpen(TRUE);
				gIMMgr->addSession(fullname, IM_NOTHING_SPECIAL, agent_id);
			}		
		}
		else
		{
			gIMMgr->setFloaterOpen(TRUE);
			gIMMgr->addSession("Friends Conference", IM_SESSION_CONFERENCE_START, ids[0], ids);
		}
		make_ui_sound("UISndStartIM");
	}
}

// static
void LLPanelFriends::requestFriendship(const LLUUID& target_id, const std::string& target_name, const std::string& message)
{
	LLUUID calling_card_folder_id = gInventory.findCategoryUUIDForType(LLAssetType::AT_CALLINGCARD);
	send_improved_im(target_id,
					 target_name,
					 message,
					 IM_ONLINE,
					 IM_FRIENDSHIP_OFFERED,
					 calling_card_folder_id);
}

struct LLAddFriendData
{
	LLUUID mID;
	std::string mName;
};

// static
void LLPanelFriends::callbackAddFriendWithMessage(S32 option, const std::string& text, void* data)
{
	LLAddFriendData* add = (LLAddFriendData*)data;
	if (option == 0)
	{
		requestFriendship(add->mID, add->mName, text);
	}
	delete add;
}

// static
void LLPanelFriends::callbackAddFriend(S32 option, void* data)
{
	LLAddFriendData* add = (LLAddFriendData*)data;
	if (option == 0)
	{
		// Servers older than 1.25 require the text of the message to be the
		// calling card folder ID for the offering user. JC
		LLUUID calling_card_folder_id = 
			gInventory.findCategoryUUIDForType(LLAssetType::AT_CALLINGCARD);
		std::string message = calling_card_folder_id.asString();
		requestFriendship(add->mID, add->mName, message);
	}
	delete add;
}

// static
void LLPanelFriends::onPickAvatar(const std::vector<std::string>& names,
									const std::vector<LLUUID>& ids,
									void* )
{
	if (names.empty()) return;
	if (ids.empty()) return;
	requestFriendshipDialog(ids[0], names[0]);
}

// static
void LLPanelFriends::requestFriendshipDialog(const LLUUID& id,
											   const std::string& name)
{
	if(id == gAgentID)
	{
		LLNotifyBox::showXml("AddSelfFriend");
		return;
	}

	LLAddFriendData* data = new LLAddFriendData();
	data->mID = id;
	data->mName = name;
	
	LLStringUtil::format_map_t args;
	args["[NAME]"] = name;

	// Look for server versions like: Second Life Server 1.24.4.95600
	if (gLastVersionChannel.find(" 1.24.") != std::string::npos)
	{
		// Old and busted server version, doesn't support friend
		// requests with messages.
		gViewerWindow->alertXml("AddFriend", args, callbackAddFriend, data);
	}
	else
	{
		gViewerWindow->alertXmlEditText("AddFriendWithMessage", args, NULL, NULL, callbackAddFriendWithMessage, data);
	}
}

// static
void LLPanelFriends::onClickAddFriend(void* user_data)
{
	LLPanelFriends* panelp = (LLPanelFriends*)user_data;
	LLFloater* root_floater = gFloaterView->getParentFloater(panelp);
	LLFloaterAvatarPicker* picker = LLFloaterAvatarPicker::show(onPickAvatar, user_data, FALSE, TRUE);
	if (root_floater)
	{
		root_floater->addDependentFloater(picker);
	}
}

// static
void LLPanelFriends::onClickRemove(void* user_data)
{
	LLPanelFriends* panelp = (LLPanelFriends*)user_data;

	//llinfos << "LLPanelFriends::onClickRemove()" << llendl;
	LLDynamicArray<LLUUID> ids = panelp->getSelectedIDs();
	LLStringUtil::format_map_t args;
	if(ids.size() > 0)
	{
		std::string msgType = "RemoveFromFriends";
		if(ids.size() == 1)
		{
			LLUUID agent_id = ids[0];
			std::string first, last;
			if(gCacheName->getName(agent_id, first, last))
			{
				args["[FIRST_NAME]"] = first;
				args["[LAST_NAME]"] = last;	
			}
		}
		else
		{
			msgType = "RemoveMultipleFromFriends";
		}
		gViewerWindow->alertXml(msgType,
			args,
			&handleRemove,
			(void*)new LLDynamicArray<LLUUID>(ids));
	}
}

// static
void LLPanelFriends::onClickOfferTeleport(void* user_data)
{
	LLPanelFriends* panelp = (LLPanelFriends*)user_data;

	LLDynamicArray<LLUUID> ids = panelp->getSelectedIDs();
	if(ids.size() > 0)
	{	
		handle_lure(ids);
	}
}

// static
void LLPanelFriends::onClickPay(void* user_data)
{
	LLPanelFriends* panelp = (LLPanelFriends*)user_data;

	LLDynamicArray<LLUUID> ids = panelp->getSelectedIDs();
	if(ids.size() == 1)
	{	
		handle_pay_by_id(ids[0]);
	}
}

void LLPanelFriends::confirmModifyRights(rights_map_t& ids, EGrantRevoke command)
{
	if (ids.empty()) return;
	
	LLStringUtil::format_map_t args;
	if(ids.size() > 0)
	{
		// copy map of ids onto heap
		rights_map_t* rights = new rights_map_t(ids); 
		// package with panel pointer
		std::pair<LLPanelFriends*, rights_map_t*>* user_data = new std::pair<LLPanelFriends*, rights_map_t*>(this, rights);

		// for single friend, show their name
		if(ids.size() == 1)
		{
			LLUUID agent_id = ids.begin()->first;
			std::string first, last;
			if(gCacheName->getName(agent_id, first, last))
			{
				args["[FIRST_NAME]"] = first;
				args["[LAST_NAME]"] = last;	
			}
			if (command == GRANT)
			{
				gViewerWindow->alertXml("GrantModifyRights", args, modifyRightsConfirmation, user_data);
			}
			else
			{
				gViewerWindow->alertXml("RevokeModifyRights", args, modifyRightsConfirmation, user_data);
			}
		}
		else
		{
			if (command == GRANT)
			{
				gViewerWindow->alertXml("GrantModifyRightsMultiple", args, modifyRightsConfirmation, user_data);
			}
			else
			{
				gViewerWindow->alertXml("RevokeModifyRightsMultiple", args, modifyRightsConfirmation, user_data);
			}
		}
	}
}

// static
void LLPanelFriends::modifyRightsConfirmation(S32 option, void* user_data)
{
	std::pair<LLPanelFriends*, rights_map_t*>* data = (std::pair<LLPanelFriends*, rights_map_t*>*)user_data;
	LLPanelFriends* panelp = data->first;

	if(panelp)
	{
		if(0 == option)
		{
			panelp->sendRightsGrant(*(data->second));
		}
		else
		{
			// need to resync view with model, since user cancelled operation
			rights_map_t* rights = data->second;
			rights_map_t::iterator rights_it;
			for (rights_it = rights->begin(); rights_it != rights->end(); ++rights_it)
			{
				const LLRelationship* info = LLAvatarTracker::instance().getBuddyInfo(rights_it->first);
				panelp->updateFriendItem(rights_it->first, info);
				// Might have changed the column the user is sorted on.
				panelp->mFriendsList->sortItems();
			}
		}
		panelp->refreshUI();
	}

	delete data->second;
	delete data;
}

void LLPanelFriends::applyRightsToFriends()
{
	BOOL rights_changed = FALSE;

	// store modify rights separately for confirmation
	rights_map_t rights_updates;

	BOOL need_confirmation = FALSE;
	EGrantRevoke confirmation_type = GRANT;

	// this assumes that changes only happened to selected items
	std::vector<LLScrollListItem*> selected = mFriendsList->getAllSelected();
	for(std::vector<LLScrollListItem*>::iterator itr = selected.begin(); itr != selected.end(); ++itr)
	{
		LLUUID id = (*itr)->getValue();
		const LLRelationship* buddy_relationship = LLAvatarTracker::instance().getBuddyInfo(id);
		if (buddy_relationship == NULL) continue;

		bool show_online_staus = (*itr)->getColumn(LIST_VISIBLE_ONLINE)->getValue().asBoolean();
		bool show_map_location = (*itr)->getColumn(LIST_VISIBLE_MAP)->getValue().asBoolean();
		bool allow_modify_objects = (*itr)->getColumn(LIST_EDIT_MINE)->getValue().asBoolean();

		S32 rights = buddy_relationship->getRightsGrantedTo();
		if(buddy_relationship->isRightGrantedTo(LLRelationship::GRANT_ONLINE_STATUS) != show_online_staus)
		{
			rights_changed = TRUE;
			if(show_online_staus) 
			{
				rights |= LLRelationship::GRANT_ONLINE_STATUS;
			}
			else 
			{
				// ONLINE_STATUS necessary for MAP_LOCATION
				rights &= ~LLRelationship::GRANT_ONLINE_STATUS;
				rights &= ~LLRelationship::GRANT_MAP_LOCATION;
				// propagate rights constraint to UI
				(*itr)->getColumn(LIST_VISIBLE_MAP)->setValue(FALSE);
			}
		}
		if(buddy_relationship->isRightGrantedTo(LLRelationship::GRANT_MAP_LOCATION) != show_map_location)
		{
			rights_changed = TRUE;
			if(show_map_location) 
			{
				// ONLINE_STATUS necessary for MAP_LOCATION
				rights |= LLRelationship::GRANT_MAP_LOCATION;
				rights |= LLRelationship::GRANT_ONLINE_STATUS;
				(*itr)->getColumn(LIST_VISIBLE_ONLINE)->setValue(TRUE);
			}
			else 
			{
				rights &= ~LLRelationship::GRANT_MAP_LOCATION;
			}
		}
		
		// now check for change in modify object rights, which requires confirmation
		if(buddy_relationship->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS) != allow_modify_objects)
		{
			rights_changed = TRUE;
			need_confirmation = TRUE;

			if(allow_modify_objects)
			{
				rights |= LLRelationship::GRANT_MODIFY_OBJECTS;
				confirmation_type = GRANT;
			}
			else
			{
				rights &= ~LLRelationship::GRANT_MODIFY_OBJECTS;
				confirmation_type = REVOKE;
			}
		}

		if (rights_changed)
		{
			rights_updates.insert(std::make_pair(id, rights));
			// disable these ui elements until response from server
			// to avoid race conditions
			(*itr)->setEnabled(FALSE);
		}
	}

	// separately confirm grant and revoke of modify rights
	if (need_confirmation)
	{
		confirmModifyRights(rights_updates, confirmation_type);
	}
	else
	{
		sendRightsGrant(rights_updates);
	}
}

void LLPanelFriends::sendRightsGrant(rights_map_t& ids)
{
	if (ids.empty()) return;

	LLMessageSystem* msg = gMessageSystem;

	// setup message header
	msg->newMessageFast(_PREHASH_GrantUserRights);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUID(_PREHASH_AgentID, gAgent.getID());
	msg->addUUID(_PREHASH_SessionID, gAgent.getSessionID());

	rights_map_t::iterator id_it;
	rights_map_t::iterator end_it = ids.end();
	for(id_it = ids.begin(); id_it != end_it; ++id_it)
	{
		msg->nextBlockFast(_PREHASH_Rights);
		msg->addUUID(_PREHASH_AgentRelated, id_it->first);
		msg->addS32(_PREHASH_RelatedRights, id_it->second);
	}

	mNumRightsChanged = ids.size();
	gAgent.sendReliableMessage();
}



// static
void LLPanelFriends::handleRemove(S32 option, void* user_data)
{
	LLDynamicArray<LLUUID>* ids = static_cast<LLDynamicArray<LLUUID>*>(user_data);
	for(LLDynamicArray<LLUUID>::iterator itr = ids->begin(); itr != ids->end(); ++itr)
	{
		LLUUID id = (*itr);
		const LLRelationship* ip = LLAvatarTracker::instance().getBuddyInfo(id);
		if(ip)
		{			
			switch(option)
			{
			case 0: // YES
				if( ip->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS))
				{
					LLAvatarTracker::instance().empower(id, FALSE);
					LLAvatarTracker::instance().notifyObservers();
				}
				LLAvatarTracker::instance().terminateBuddy(id);
				LLAvatarTracker::instance().notifyObservers();
				gInventory.addChangedMask(LLInventoryObserver::LABEL | LLInventoryObserver::CALLING_CARD, LLUUID::null);
				gInventory.notifyObservers();
				break;

			case 1: // NO
			default:
				llinfos << "No removal performed." << llendl;
				break;
			}
		}
		
	}
	delete ids;
}
