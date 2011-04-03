/** 
 * @file llfloateravatarlist.cpp
 * @brief Avatar list/radar floater
 *
 * @author Dale Glass <dale@daleglass.net>, (C) 2007
 */

/**
 * Rewritten by jcool410
 * Removed usage of globals
 * Removed TrustNET
 * Added utilization of "minimap" data
 * Heavily modified by Henri Beauchamp (the laggy spying tool becomes a true,
 * low lag radar)
 */

#include "llviewerprecompiledheaders.h"

#include "llavatarconstants.h"
#include "llfloateravatarlist.h"

#include "llcachename.h"
#include "lluictrlfactory.h"
#include "llviewerwindow.h"
#include "llscrolllistctrl.h"
#include "llradiogroup.h"
#include "llviewercontrol.h"

#include "llvoavatar.h"
#include "llimview.h"
#include "llfloateravatarinfo.h"
#include "llregionflags.h"
#include "llfloaterreporter.h"
#include "llagent.h"
#include "llviewerregion.h"
#include "lltracker.h"
#include "llviewerstats.h"
#include "llerror.h"
#include "llchat.h"
#include "llfloaterchat.h"
#include "llviewermessage.h"
#include "llweb.h"
#include "llviewerobjectlist.h"
#include "llmutelist.h"
#include "llcallbacklist.h"

#include <time.h>
#include <string.h>

#include <map>


#include "llworld.h"

#include "llsdutil.h"

/**
 * @brief How long to keep people who are gone in the list and in memory.
 */
const F32 DEAD_KEEP_TIME = 10.0f;

extern U32 gFrameCount;

typedef enum e_radar_alert_type
{
	ALERT_TYPE_SIM = 0,
	ALERT_TYPE_DRAW = 1,
	ALERT_TYPE_SHOUTRANGE = 2,
	ALERT_TYPE_CHATRANGE = 3
} ERadarAlertType;

void announce(std::string msg)
{
	//llinfos << "Radar broadcasting key: " << key.asString() << " - on channel " << gSavedSettings.getS32("RadarChatKeysChannel") << llendl;
	gMessageSystem->newMessage("ScriptDialogReply");
	gMessageSystem->nextBlock("AgentData");
	gMessageSystem->addUUID("AgentID", gAgent.getID());
	gMessageSystem->addUUID("SessionID", gAgent.getSessionID());
	gMessageSystem->nextBlock("Data");
	gMessageSystem->addUUID("ObjectID", gAgent.getID());
	gMessageSystem->addS32("ChatChannel", gSavedSettings.getS32("RadarChatKeysChannel"));
	gMessageSystem->addS32("ButtonIndex", 1);
	gMessageSystem->addString("ButtonLabel", msg);
	gAgent.sendReliableMessage();
}

void chat_avatar_status(std::string name, LLUUID key, ERadarAlertType type, bool entering)
{
	if (gSavedSettings.getBOOL("RadarChatAlerts"))
	{
		LLChat chat;
		switch(type)
		{
		case ALERT_TYPE_SIM:
			if (gSavedSettings.getBOOL("RadarAlertSim"))
			{
				chat.mText = name+" has "+(entering ? "entered" : "left")+" the sim.";
			}
			break;
		case ALERT_TYPE_DRAW:
			if (gSavedSettings.getBOOL("RadarAlertDraw"))
			{
				chat.mText = name+" has "+(entering ? "entered" : "left")+" draw distance.";
			}
			break;
		case ALERT_TYPE_SHOUTRANGE:
			if (gSavedSettings.getBOOL("RadarAlertShoutRange"))
			{
				chat.mText = name+" has "+(entering ? "entered" : "left")+" shout range.";
			}
			break;
		case ALERT_TYPE_CHATRANGE:
			if (gSavedSettings.getBOOL("RadarAlertChatRange"))
			{
				chat.mText = name+" has "+(entering ? "entered" : "left")+" chat range.";
			}
			break;
		}
		if (chat.mText != "")
		{
			chat.mSourceType = CHAT_SOURCE_SYSTEM;
			LLFloaterChat::addChat(chat);
		}
	}
	if (type == ALERT_TYPE_SIM && entering && gSavedSettings.getBOOL("RadarChatKeys"))
	{
		announce(key.asString());
	}
}

LLAvatarListEntry::LLAvatarListEntry(const LLUUID& id, const std::string &name, const LLVector3d &position) :
		mID(id), mName(name), mDisplayName(name), mPosition(position), mDrawPosition(), mMarked(FALSE),
		mFocused(FALSE), mUpdateTimer(), mFrame(gFrameCount), mInSimFrame(U32_MAX), mInDrawFrame(U32_MAX),
		mInChatFrame(U32_MAX), mInShoutFrame(U32_MAX)
{
}

void LLAvatarListEntry::setPosition(LLVector3d position, bool this_sim, bool drawn, bool chatrange, bool shoutrange)
{
	if (drawn)
	{
		mDrawPosition = position;
	}
	else if (mInDrawFrame == U32_MAX)
	{
		mDrawPosition.setZero();
	}

	mPosition = position;

	mFrame = gFrameCount;
	if (this_sim)
	{
		if (mInSimFrame == U32_MAX)
		{
			chat_avatar_status(mName, mID, ALERT_TYPE_SIM, true);
		}
		mInSimFrame = mFrame;
	}
	if (drawn)
	{
		if (mInDrawFrame == U32_MAX)
		{
			chat_avatar_status(mName, mID, ALERT_TYPE_DRAW, true);
		}
		mInDrawFrame = mFrame;
	}
	if (shoutrange)
	{
		if (mInShoutFrame == U32_MAX)
		{
			chat_avatar_status(mName, mID, ALERT_TYPE_SHOUTRANGE, true);
		}
		mInShoutFrame = mFrame;
	}
	if (chatrange)
	{
		if (mInChatFrame == U32_MAX)
		{
			chat_avatar_status(mName, mID, ALERT_TYPE_CHATRANGE, true);
		}
		mInChatFrame = mFrame;
	}

	mUpdateTimer.start();
}

bool LLAvatarListEntry::getAlive()
{
	U32 current = gFrameCount;
	if (mInSimFrame != U32_MAX && (current - mInSimFrame) >= 2)
	{
		mInSimFrame = U32_MAX;
		chat_avatar_status(mName, mID, ALERT_TYPE_SIM, false);
	}
	if (mInDrawFrame != U32_MAX && (current - mInDrawFrame) >= 2)
	{
		mInDrawFrame = U32_MAX;
		chat_avatar_status(mName, mID, ALERT_TYPE_DRAW, false);
	}
	if (mInShoutFrame != U32_MAX && (current - mInShoutFrame) >= 2)
	{
		mInShoutFrame = U32_MAX;
		chat_avatar_status(mName, mID, ALERT_TYPE_SHOUTRANGE, false);
	}
	if (mInChatFrame != U32_MAX && (current - mInChatFrame) >= 2)
	{
		mInChatFrame = U32_MAX;
		chat_avatar_status(mName, mID, ALERT_TYPE_CHATRANGE, false);
	}
	return ((current - mFrame) <= 2);
}

F32 LLAvatarListEntry::getEntryAgeSeconds()
{
	return mUpdateTimer.getElapsedTimeF32();
}

BOOL LLAvatarListEntry::isDead()
{
	return getEntryAgeSeconds() > DEAD_KEEP_TIME;
}

LLFloaterAvatarList* LLFloaterAvatarList::sInstance = NULL;

LLFloaterAvatarList::LLFloaterAvatarList() :  LLFloater(std::string("radar"))
{
	llassert_always(sInstance == NULL);
	sInstance = this;
	mUpdateRate = gSavedSettings.getU32("RadarUpdateRate") * 3 + 3;
}

LLFloaterAvatarList::~LLFloaterAvatarList()
{
	gIdleCallbacks.deleteFunction(LLFloaterAvatarList::callbackIdle);
	sInstance = NULL;
}

//static
void LLFloaterAvatarList::toggle(void*)
{
//MK
	if (gRRenabled && gAgent.mRRInterface.mContainsShownames)
	{
		if (sInstance && sInstance->getVisible())
		{	
			sInstance->close(false);
		}
	}
//mk
	if (sInstance)
	{
		if (sInstance->getVisible())
		{
			sInstance->close(false);
		}
		else
		{
			sInstance->open();
		}
	}
	else
	{
		showInstance();
	}
}

//static
void LLFloaterAvatarList::showInstance()
{
//MK
	if (gRRenabled && gAgent.mRRInterface.mContainsShownames)
	{
		return;
	}
//mk
	if (sInstance)
	{
		if (!sInstance->getVisible())
		{
			sInstance->open();
		}
	}
	else
	{
		sInstance = new LLFloaterAvatarList();
		LLUICtrlFactory::getInstance()->buildFloater(sInstance, "floater_radar.xml");
	}
}

void LLFloaterAvatarList::draw()
{
	LLFloater::draw();
}

void LLFloaterAvatarList::onOpen()
{
	gSavedSettings.setBOOL("ShowRadar", TRUE);
	sInstance->setVisible(TRUE);
}

void LLFloaterAvatarList::onClose(bool app_quitting)
{
	sInstance->setVisible(FALSE);
	if (!app_quitting)
	{
		gSavedSettings.setBOOL("ShowRadar", FALSE);
	}
	if (!gSavedSettings.getBOOL("RadarKeepOpen") || app_quitting)
	{
		destroy();
	}
}

BOOL LLFloaterAvatarList::postBuild()
{
	// Default values
	mTracking = FALSE;
	mUpdate = TRUE;

	// Hide them until some other way is found
	// Users may not expect to find a Ban feature on the Eject button
	childSetVisible("estate_ban_btn", false);

	// Set callbacks
	childSetAction("profile_btn", onClickProfile, this);
	childSetAction("im_btn", onClickIM, this);
	childSetAction("offer_btn", onClickTeleportOffer, this);
	childSetAction("track_btn", onClickTrack, this);
	childSetAction("mark_btn", onClickMark, this);
	childSetAction("focus_btn", onClickFocus, this);
	childSetAction("prev_in_list_btn", onClickPrevInList, this);
	childSetAction("next_in_list_btn", onClickNextInList, this);
	childSetAction("prev_marked_btn", onClickPrevMarked, this);
	childSetAction("next_marked_btn", onClickNextMarked, this);
	
	childSetAction("get_key_btn", onClickGetKey, this);

	childSetAction("freeze_btn", onClickFreeze, this);
	childSetAction("eject_btn", onClickEject, this);
	childSetAction("mute_btn", onClickMute, this);
	childSetAction("ar_btn", onClickAR, this);
	childSetAction("teleport_btn", onClickTeleport, this);
	childSetAction("estate_eject_btn", onClickEjectFromEstate, this);

	childSetAction("send_keys_btn", onClickSendKeys, this);

	getChild<LLRadioGroup>("update_rate")->setSelectedIndex(gSavedSettings.getU32("RadarUpdateRate"));
	childSetCommitCallback("update_rate", onCommitUpdateRate, this);

	// Get a pointer to the scroll list from the interface
	mAvatarList = getChild<LLScrollListCtrl>("avatar_list");
	mAvatarList->sortByColumn("distance", TRUE);
	mAvatarList->setCommitOnSelectionChange(TRUE);
	childSetCommitCallback("avatar_list", onSelectName, this);
	refreshAvatarList();

	gIdleCallbacks.addFunction(LLFloaterAvatarList::callbackIdle);

	return TRUE;
}

void LLFloaterAvatarList::updateAvatarList()
{
	if (sInstance != this) return;

	if (gRRenabled && gAgent.mRRInterface.mContainsShownames)
	{
		close();
	}
	//llinfos << "radar refresh: updating map" << llendl;

	// Check whether updates are enabled
	LLCheckboxCtrl* check = getChild<LLCheckboxCtrl>("update_enabled_cb");
	if (check && !check->getValue())
	{
		mUpdate = FALSE;
		refreshTracker();
		return;
	}
	else
	{
		mUpdate = TRUE;
	}

	LLVector3d mypos = gAgent.getPositionGlobal();

	{
		std::vector<LLUUID> avatar_ids;
		std::vector<LLUUID> sorted_avatar_ids;
		std::vector<LLVector3d> positions;

		LLWorld::getInstance()->getAvatars(&avatar_ids, &positions, mypos, F32_MAX);

		sorted_avatar_ids = avatar_ids;
		std::sort(sorted_avatar_ids.begin(), sorted_avatar_ids.end());

		for (std::vector<LLCharacter*>::const_iterator iter = LLCharacter::sInstances.begin(); iter != LLCharacter::sInstances.end(); ++iter)
		{
			LLUUID avid = (*iter)->getID();

			if (!std::binary_search(sorted_avatar_ids.begin(), sorted_avatar_ids.end(), avid))
			{
				avatar_ids.push_back(avid);
			}
		}

		size_t i;
		size_t count = avatar_ids.size();

		for (i = 0; i < count; ++i)
		{
			std::string name, first, last;
			const LLUUID &avid = avatar_ids[i];

			LLVector3d position;
			LLViewerObject *obj = gObjectList.findObject(avid);

			if (obj)
			{
				LLVOAvatar* avatarp = dynamic_cast<LLVOAvatar*>(obj);

				if (avatarp == NULL)
				{
					continue;
				}

				// Skip if avatar is dead(what's that?)
				// or if the avatar is ourselves.
				if (avatarp->isDead() || avatarp->isSelf())
				{
					continue;
				}

				// Get avatar data
				position = gAgent.getPosGlobalFromAgent(avatarp->getCharacterPosition());
				name = avatarp->getFullname();

				// Apparently, sometimes the name comes out empty, with a " " name. This is because
				// getFullname concatenates first and last name with a " " in the middle.
				// This code will avoid adding a nameless entry to the list until it acquires a name.

				//duped for lower section
				if (name.empty() || (name.compare(" ") == 0))// || (name.compare(gCacheName->getDefaultName()) == 0))
				{
					if (gCacheName->getName(avid, first, last))
					{
						name = first + " " + last;
					}
					else
					{
						continue;
					}
				}

				std::string display_name = name;
				if (LLAvatarName::sOmitResidentAsLastName)
				{
					LLStringUtil::replaceString(display_name, " Resident", "");
				}
				if (LLAvatarNameCache::useDisplayNames())
				{
					LLAvatarName avatar_name;
					if (LLAvatarNameCache::get(avid, &avatar_name))
					{
						if (LLAvatarNameCache::useDisplayNames() == 2)
						{
							display_name = avatar_name.mDisplayName;
						}
						else
						{
							display_name = avatar_name.getNames();
						}
					}
				}

				if (avid.isNull())
				{
					//llinfos << "Key empty for avatar " << name << llendl;
					continue;
				}

				if (mAvatars.count(avid) > 0)
				{
					// Avatar already in list, update position
					F32 dist = (F32)(position - mypos).magVec();
					mAvatars[avid].setPosition(position, (avatarp->getRegion() == gAgent.getRegion()), true, dist < 20.0, dist < 100.0);
				}
				else
				{
					// Avatar not there yet, add it
					LLAvatarListEntry entry(avid, name, position);
					mAvatars[avid] = entry;
				}
				// update avatar display name.
				mAvatars[avid].setDisplayName(display_name);
			}
			else
			{
				if (i < positions.size())
				{
					position = positions[i];
				}
				else
				{
					continue;
				}

				if (gCacheName->getName(avid, first, last))
				{
					name = first + " " + last;
				}
				else
				{
					//name = gCacheName->getDefaultName();
					continue; //prevent (Loading...)
				}

				std::string display_name = name;
				if (LLAvatarName::sOmitResidentAsLastName)
				{
					LLStringUtil::replaceString(display_name, " Resident", "");
				}
				if (LLAvatarNameCache::useDisplayNames())
				{
					LLAvatarName avatar_name;
					if (LLAvatarNameCache::get(avid, &avatar_name))
					{
						if (LLAvatarNameCache::useDisplayNames() == 2)
						{
							display_name = avatar_name.mDisplayName;
						}
						else
						{
							display_name = avatar_name.getNames();
						}
					}
				}

				if (mAvatars.count(avid) > 0)
				{
					// Avatar already in list, update position
					F32 dist = (F32)(position - mypos).magVec();
					mAvatars[avid].setPosition(position, gAgent.getRegion()->pointInRegionGlobal(position), false, dist < 20.0, dist < 100.0);
				}
				else
				{
					// Avatar not there yet, add it
					LLAvatarListEntry entry(avid, name, position);
					mAvatars[avid] = entry;
				}
				// update avatar display name.
				mAvatars[avid].setDisplayName(display_name);
			}
		}
	}
	
//	llinfos << "radar refresh: done" << llendl;

	expireAvatarList();
	refreshAvatarList();
	refreshTracker();
}

void LLFloaterAvatarList::expireAvatarList()
{
//	llinfos << "radar: expiring" << llendl;
	std::map<LLUUID, LLAvatarListEntry>::iterator iter;
	std::queue<LLUUID> delete_queue;

	for (iter = mAvatars.begin(); iter != mAvatars.end(); iter++)
	{
		LLAvatarListEntry *entry = &iter->second;
		
		if (!entry->getAlive() && entry->isDead())
		{
			//llinfos << "radar: expiring avatar " << entry->getDisplayName() << llendl;
			LLUUID av_id = entry->getID();
			delete_queue.push(av_id);
			if (av_id == mTrackedAvatar)
			{
				stopTracker();
			}
		}
	}

	while (!delete_queue.empty())
	{
		mAvatars.erase(delete_queue.front());
		delete_queue.pop();
	}
}

/**
 * Redraws the avatar list
 * Only does anything if the avatar list is visible.
 * @author Dale Glass
 */
void LLFloaterAvatarList::refreshAvatarList() 
{
	// Don't update list when interface is hidden
	if (!sInstance->getVisible()) return;

	// We rebuild the list fully each time it's refreshed
	// The assumption is that it's faster to refill it and sort than
	// to rebuild the whole list.
	LLDynamicArray<LLUUID> selected = mAvatarList->getSelectedIDs();
	S32 scrollpos = mAvatarList->getScrollPos();

	mAvatarList->deleteAllItems();

	LLVector3d mypos = gAgent.getPositionGlobal();
	LLVector3d posagent;
	posagent.setVec(gAgent.getPositionAgent());
	LLVector3d simpos = mypos - posagent;

	std::map<LLUUID, LLAvatarListEntry>::iterator iter;
	for (iter = mAvatars.begin(); iter != mAvatars.end(); iter++)
	{
		LLSD element;
		LLUUID av_id;

		LLAvatarListEntry *entry = &iter->second;

		// Skip if avatar hasn't been around
		if (entry->isDead())
		{
			continue;
		}

		av_id = entry->getID();

		LLVector3d position = entry->getPosition();
		BOOL UnknownAltitude = false;

		LLVector3d delta = position - mypos;
		F32 distance = (F32)delta.magVec();
		if (position.mdV[VZ] == 0.0)
		{
			UnknownAltitude = true;
			distance = 9000.0;
		}
		delta.mdV[2] = 0.0f;
		F32 side_distance = (F32)delta.magVec();

		// HACK: Workaround for an apparent bug:
		// sometimes avatar entries get stuck, and are registered
		// by the client as perpetually moving in the same direction.
		// this makes sure they get removed from the visible list eventually

		//jcool410 -- this fucks up seeing dueds thru minimap data > 1024m away, so, lets just say > 2048m to the side is bad
		//aka 8 sims
		if (side_distance > 2048.0f)
		{
			continue;
		}

		if (av_id.isNull())
		{
			//llwarns << "Avatar with null key somehow got into the list!" << llendl;
			continue;
		}

		element["id"] = av_id;

		element["columns"][LIST_MARK]["column"] = "marked";
		element["columns"][LIST_MARK]["type"] = "text";
		if (entry->isMarked())
		{
			element["columns"][LIST_MARK]["value"] = "X";
			element["columns"][LIST_MARK]["color"] = LLColor4::blue.getValue();
			element["columns"][LIST_MARK]["font-style"] = "BOLD";
		}
		else
		{
			element["columns"][LIST_MARK]["value"] = "";
		}

		element["columns"][LIST_AVATAR_NAME]["column"] = "avatar_name";
		element["columns"][LIST_AVATAR_NAME]["type"] = "text";
		element["columns"][LIST_AVATAR_NAME]["value"] = entry->getDisplayName().c_str();
		if (entry->getEntryAgeSeconds() > 1.0f)
		{
			element["columns"][LIST_AVATAR_NAME]["font-style"] = "ITALIC";
		}
		else if (entry->isFocused())
		{
			element["columns"][LIST_AVATAR_NAME]["font-style"] = "BOLD";
		}
		if (LLMuteList::getInstance()->isMuted(av_id))
		{
			element["columns"][LIST_AVATAR_NAME]["color"] = LLColor4::red2.getValue();
		}
		else if (is_agent_friend(av_id))
		{
			element["columns"][LIST_AVATAR_NAME]["color"] = LLColor4::green3.getValue();
		}

		char temp[32];
		LLColor4 color = LLColor4::black;
		element["columns"][LIST_DISTANCE]["column"] = "distance";
		element["columns"][LIST_DISTANCE]["type"] = "text";
		if (UnknownAltitude)
		{
			strcpy(temp, "?");
			if (entry->isDrawn())
			{
				color = LLColor4::red;
			}
		}
		else
		{
			if (distance < 100.0)
			{
				snprintf(temp, sizeof(temp), "%.1f", distance);
				if (distance > 20.0f)
				{
					color = LLColor4::yellow1;
				}
				else
				{
					color = LLColor4::green2;
				}
			}
			else
			{
				if (entry->isDrawn())
				{
					color = LLColor4::red;
				}
				snprintf(temp, sizeof(temp), "%d", (S32)distance);
			}
		}
		element["columns"][LIST_DISTANCE]["value"] = temp;
		element["columns"][LIST_DISTANCE]["color"] = color.getValue();

		position = position - simpos;

		S32 x = (S32)position.mdV[VX];
		S32 y = (S32)position.mdV[VY];
		if (x >= 0 && x <= 256 && y >= 0 && y <= 256)
		{
			snprintf(temp, sizeof(temp), "%d, %d", x, y);
		}
		else
		{
			temp[0] = '\0';
			if (y < 0)
			{
				strcat(temp, "S");
			}
			else if (y > 256)
			{
				strcat(temp, "N");
			}
			if (x < 0)
			{
				strcat(temp, "W");
			}
			else if (x > 256)
			{
				strcat(temp, "E");
			}
		}
		element["columns"][LIST_POSITION]["column"] = "position";
		element["columns"][LIST_POSITION]["type"] = "text";
		element["columns"][LIST_POSITION]["value"] = temp;

		element["columns"][LIST_ALTITUDE]["column"] = "altitude";
		element["columns"][LIST_ALTITUDE]["type"] = "text";
		if (UnknownAltitude)
		{
			strcpy(temp, "?");
		}
		else
		{
			snprintf(temp, sizeof(temp), "%d", (S32)position.mdV[VZ]);
		}
		element["columns"][LIST_ALTITUDE]["value"] = temp;

		// Add to list
		mAvatarList->addElement(element, ADD_BOTTOM);
	}

	// finish
	mAvatarList->sortItems();
	mAvatarList->selectMultiple(selected);
	mAvatarList->setScrollPos(scrollpos);

//	llinfos << "radar refresh: done" << llendl;

}

// static
void LLFloaterAvatarList::onClickIM(void* userdata)
{
	//llinfos << "LLFloaterFriends::onClickIM()" << llendl;
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;

	LLDynamicArray<LLUUID> ids = self->mAvatarList->getSelectedIDs();
	if (ids.size() > 0)
	{
		if (ids.size() == 1)
		{
			// Single avatar
			LLUUID agent_id = ids[0];

			char buffer[MAX_STRING];
			snprintf(buffer, MAX_STRING, "%s", self->mAvatars[agent_id].getName().c_str());
			gIMMgr->setFloaterOpen(TRUE);
			gIMMgr->addSession(buffer, IM_NOTHING_SPECIAL, agent_id);
		}
		else
		{
			// Group IM
			LLUUID session_id;
			session_id.generate();
			gIMMgr->setFloaterOpen(TRUE);
			gIMMgr->addSession("Avatars Conference", IM_SESSION_CONFERENCE_START, ids[0], ids);
		}
	}
}

void LLFloaterAvatarList::onClickTeleportOffer(void *userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;

	LLDynamicArray<LLUUID> ids = self->mAvatarList->getSelectedIDs();
	if (ids.size() > 0)
	{
		handle_lure(ids);
	}
}

void LLFloaterAvatarList::onClickTrack(void *userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
	
 	LLScrollListItem *item =   self->mAvatarList->getFirstSelected();
	if (!item) return;

	LLUUID agent_id = item->getUUID();

	if (self->mTracking && self->mTrackedAvatar == agent_id) {
		self->stopTracker();
	}
	else
	{
		self->mTracking = TRUE;
		self->mTrackedAvatar = agent_id;
//		trackAvatar only works for friends allowing you to see them on map...
//		LLTracker::trackAvatar(agent_id, self->mAvatars[agent_id].getDisplayName());
		std::string name = self->mAvatars[agent_id].getDisplayName();
		if (!self->mUpdate)
		{
			name += "\n(last known position)";
		}
		LLTracker::trackLocation(self->mAvatars[agent_id].getPosition(), name, "");
	}
}

void LLFloaterAvatarList::stopTracker()
{
	LLTracker::stopTracking(NULL);
	mTracking = FALSE;
}

void LLFloaterAvatarList::refreshTracker()
{
	if (!mTracking) return;

	if (LLTracker::isTracking(NULL))
	{
		LLVector3d pos;
		if (mUpdate)
		{
			pos = mAvatars[mTrackedAvatar].getPosition();
		}
		else
		{
			LLViewerObject *obj = gObjectList.findObject(mTrackedAvatar);
			if (!obj)
			{
				stopTracker();
				return;
			}
			LLVOAvatar* avatarp = dynamic_cast<LLVOAvatar*>(obj);
			if (!avatarp)
			{
				stopTracker();
				return;
			}
			pos = gAgent.getPosGlobalFromAgent(avatarp->getCharacterPosition());
		}
		if (pos != LLTracker::getTrackedPositionGlobal())
		{
			std::string name = mAvatars[mTrackedAvatar].getDisplayName();
			LLTracker::trackLocation(pos, name, "");
		}
	}
	else
	{
		stopTracker();
	}
}

LLAvatarListEntry * LLFloaterAvatarList::getAvatarEntry(LLUUID avatar)
{
	if (avatar.isNull())
	{
		return NULL;
	}

	std::map<LLUUID, LLAvatarListEntry>::iterator iter;

	iter = mAvatars.find(avatar);
	if (iter == mAvatars.end())
	{
		return NULL;
	}

	return &iter->second;	
}

//static
void LLFloaterAvatarList::onClickMark(void *userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
	LLDynamicArray<LLUUID> ids = self->mAvatarList->getSelectedIDs();

	for (LLDynamicArray<LLUUID>::iterator itr = ids.begin(); itr != ids.end(); ++itr)
	{
		LLUUID avid = *itr;
		LLAvatarListEntry *entry = self->getAvatarEntry(avid);
		if (entry != NULL)
		{
			entry->toggleMark();
		}
	}
}

void LLFloaterAvatarList::onClickFocus(void *userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
	
 	LLScrollListItem *item = self->mAvatarList->getFirstSelected();
	if (item)
	{
		self->mFocusedAvatar = item->getUUID();
		self->focusOnCurrent();
	}
}

void LLFloaterAvatarList::removeFocusFromAll()
{
	std::map<LLUUID, LLAvatarListEntry>::iterator iter;

	for (iter = mAvatars.begin(); iter != mAvatars.end(); iter++)
	{
		LLAvatarListEntry *entry = &iter->second;
		entry->setFocus(FALSE);
	}
}

void LLFloaterAvatarList::focusOnCurrent()
{
	std::map<LLUUID, LLAvatarListEntry>::iterator iter;
	LLAvatarListEntry *entry;

	if (mAvatars.size() == 0)
	{
		return;
	}

	for (iter = mAvatars.begin(); iter != mAvatars.end(); iter++)
	{
		entry = &iter->second;

		if (entry->isDead())
			continue;

		if (entry->getID() == mFocusedAvatar)
		{
			removeFocusFromAll();
			entry->setFocus(TRUE);
			gAgent.lookAtObject(mFocusedAvatar, CAMERA_POSITION_OBJECT);
			return;
		}
	}
}

void LLFloaterAvatarList::focusOnPrev(BOOL marked_only)
{
	std::map<LLUUID, LLAvatarListEntry>::iterator iter;
	LLAvatarListEntry *prev = NULL;
	LLAvatarListEntry *entry;

	if (mAvatars.size() == 0)
	{
		return;
	}

	for (iter = mAvatars.begin(); iter != mAvatars.end(); iter++)
	{
		entry = &iter->second;

		if (entry->isDead())
			continue;

		if (prev != NULL && entry->getID() == mFocusedAvatar)
		{
			break;
		}

		if ((!marked_only && entry->isDrawn()) || entry->isMarked())
		{
			prev = entry;
		}
	}

	if (prev != NULL)
	{
		removeFocusFromAll();
		prev->setFocus(TRUE);
		mFocusedAvatar = prev->getID();
		gAgent.lookAtObject(mFocusedAvatar, CAMERA_POSITION_OBJECT);
	}
}

void LLFloaterAvatarList::focusOnNext(BOOL marked_only)
{
	std::map<LLUUID, LLAvatarListEntry>::iterator iter;
	BOOL found = FALSE;
	LLAvatarListEntry *next = NULL;
	LLAvatarListEntry *entry;

	if (mAvatars.size() == 0)
	{
		return;
	}

	for (iter = mAvatars.begin(); iter != mAvatars.end(); iter++)
	{
		entry = &iter->second;

		if (entry->isDead())
			continue;

		if (next == NULL && ((!marked_only && entry->isDrawn()) || entry->isMarked()))
		{
			next = entry;
		}

		if (found && ((!marked_only && entry->isDrawn()) || entry->isMarked()))
		{
			next = entry;
			break;
		}

		if (entry->getID() == mFocusedAvatar)
		{
			found = TRUE;
		} 
	}

	if (next != NULL)
	{
		removeFocusFromAll();
		next->setFocus(TRUE);
		mFocusedAvatar = next->getID();
		gAgent.lookAtObject(mFocusedAvatar, CAMERA_POSITION_OBJECT);
	}
}

//static
void LLFloaterAvatarList::onClickPrevInList(void *userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
	self->focusOnPrev(FALSE);
}

//static
void LLFloaterAvatarList::onClickNextInList(void *userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
	self->focusOnNext(FALSE);
}

//static
void LLFloaterAvatarList::onClickPrevMarked(void *userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
	self->focusOnPrev(TRUE);
}

//static
void LLFloaterAvatarList::onClickNextMarked(void *userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
	self->focusOnNext(TRUE);
}

//static
void LLFloaterAvatarList::onClickGetKey(void *userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
 	LLScrollListItem *item = self->mAvatarList->getFirstSelected();

	if (NULL == item) return;

	LLUUID agent_id = item->getUUID();

	char buffer[UUID_STR_LENGTH];		/*Flawfinder: ignore*/
	agent_id.toString(buffer);

	gViewerWindow->mWindow->copyTextToClipboard(utf8str_to_wstring(buffer));
}

void LLFloaterAvatarList::onClickSendKeys(void *userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
	std::map<LLUUID, LLAvatarListEntry>::iterator iter;
	LLAvatarListEntry *entry;

	if (self->mAvatars.size() == 0)
		return;

	for (iter = self->mAvatars.begin(); iter != self->mAvatars.end(); iter++)
	{
		entry = &iter->second;

		if (!entry->isDead() && entry->isInSim())
			announce(entry->getID().asString());
	}
}

static void send_freeze(const LLUUID& avatar_id, bool freeze)
{
	U32 flags = 0x0;
	if (!freeze)
	{
		// unfreeze
		flags |= 0x1;
	}

	LLMessageSystem* msg = gMessageSystem;
	LLViewerObject* avatar = gObjectList.findObject(avatar_id);

	if (avatar)
	{
		msg->newMessage("FreezeUser");
		msg->nextBlock("AgentData");
		msg->addUUID("AgentID", gAgent.getID());
		msg->addUUID("SessionID", gAgent.getSessionID());
		msg->nextBlock("Data");
		msg->addUUID("TargetID", avatar_id);
		msg->addU32("Flags", flags);
		msg->sendReliable( avatar->getRegion()->getHost());
	}
}

static void send_eject(const LLUUID& avatar_id, bool ban)
{	
	LLMessageSystem* msg = gMessageSystem;
	LLViewerObject* avatar = gObjectList.findObject(avatar_id);

	if (avatar)
	{
		U32 flags = 0x0;
		if (ban)
		{
			// eject and add to ban list
			flags |= 0x1;
		}

		msg->newMessage("EjectUser");
		msg->nextBlock("AgentData");
		msg->addUUID("AgentID", gAgent.getID());
		msg->addUUID("SessionID", gAgent.getSessionID());
		msg->nextBlock("Data");
		msg->addUUID("TargetID", avatar_id);
		msg->addU32("Flags", flags);
		msg->sendReliable( avatar->getRegion()->getHost());
	}
}

static void send_estate_message(
	const char* request,
	const LLUUID &target)
{

	LLMessageSystem* msg = gMessageSystem;
	LLUUID invoice;

	// This seems to provide an ID so that the sim can say which request it's
	// replying to. I think this can be ignored for now.
	invoice.generate();

	llinfos << "Sending estate request '" << request << "'" << llendl;
	msg->newMessage("EstateOwnerMessage");
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null); //not used
	msg->nextBlock("MethodData");
	msg->addString("Method", request);
	msg->addUUID("Invoice", invoice);

	// Agent id
	msg->nextBlock("ParamList");
	msg->addString("Parameter", gAgent.getID().asString().c_str());

	// Target
	msg->nextBlock("ParamList");
	msg->addString("Parameter", target.asString().c_str());

	msg->sendReliable(gAgent.getRegion()->getHost());
}

static void cmd_freeze(const LLUUID& avatar, const std::string &name)      { send_freeze(avatar, true); }
static void cmd_unfreeze(const LLUUID& avatar, const std::string &name)    { send_freeze(avatar, false); }
static void cmd_eject(const LLUUID& avatar, const std::string &name)       { send_eject(avatar, false); }
static void cmd_ban(const LLUUID& avatar, const std::string &name)         { send_eject(avatar, true); }
static void cmd_profile(const LLUUID& avatar, const std::string &name)     { LLFloaterAvatarInfo::showFromDirectory(avatar); }
static void cmd_estate_eject(const LLUUID &avatar, const std::string &name){ send_estate_message("teleporthomeuser", avatar); }

void LLFloaterAvatarList::doCommand(void (*func)(const LLUUID &avatar, const std::string &name))
{
	LLDynamicArray<LLUUID> ids = mAvatarList->getSelectedIDs();

	for (LLDynamicArray<LLUUID>::iterator itr = ids.begin(); itr != ids.end(); ++itr)
	{
		LLUUID avid = *itr;
		LLAvatarListEntry *entry = getAvatarEntry(avid);
		if (entry != NULL)
		{
			llinfos << "Executing command on " << entry->getDisplayName() << llendl;
			func(avid, entry->getName());
		}
	}
}

std::string LLFloaterAvatarList::getSelectedNames(const std::string& separator)
{
	std::string ret = "";
	
	LLDynamicArray<LLUUID> ids = mAvatarList->getSelectedIDs();
	for (LLDynamicArray<LLUUID>::iterator itr = ids.begin(); itr != ids.end(); ++itr)
	{
		LLUUID avid = *itr;
		LLAvatarListEntry *entry = getAvatarEntry(avid);
		if (entry != NULL)
		{
			if (!ret.empty()) ret += separator;
			ret += entry->getName();
		}
	}

	return ret;
}

std::string LLFloaterAvatarList::getSelectedName()
{
	LLUUID id = getSelectedID();
	LLAvatarListEntry *entry = getAvatarEntry(id);
	if (entry)
	{
		return entry->getName();
	}
	return "";
}

LLUUID LLFloaterAvatarList::getSelectedID()
{
	LLScrollListItem *item = mAvatarList->getFirstSelected();
	if (item) return item->getUUID();
	return LLUUID::null;
}

//static 
void LLFloaterAvatarList::callbackFreeze(S32 option, void* data)
{
	LLFloaterAvatarList *self = LLFloaterAvatarList::sInstance;

	if (option == 0)
	{
		self->doCommand(cmd_freeze);
	}
	else if (option == 1)
	{
		self->doCommand(cmd_unfreeze);
	}
}

//static 
void LLFloaterAvatarList::callbackEject(S32 option, void* data)
{
	LLFloaterAvatarList *self = LLFloaterAvatarList::sInstance;
 
	if (option == 0)
	{
		self->doCommand(cmd_eject);
	}
	else if (option == 1)
	{
		self->doCommand(cmd_ban);
	}
}

//static 
void LLFloaterAvatarList::callbackEjectFromEstate(S32 option, void* data)
{
	LLFloaterAvatarList *self = LLFloaterAvatarList::sInstance;

	if (option == 0)
	{
		self->doCommand(cmd_estate_eject);
	}
}

//static
void LLFloaterAvatarList::callbackIdle(void *userdata) {
	if (LLFloaterAvatarList::sInstance != NULL)
	{
		// Do not update at every frame: this would be insane !
		if (gFrameCount % LLFloaterAvatarList::sInstance->mUpdateRate == 0)
		{
			LLFloaterAvatarList::sInstance->updateAvatarList();
		}
	}
}

void LLFloaterAvatarList::onClickFreeze(void *userdata)
{
	LLStringUtilBase<char>::format_map_t args;
	args["[NAMES]"] = ((LLFloaterAvatarList*)userdata)->getSelectedNames();
	gViewerWindow->alertXml("FreezeAvatar", args, callbackFreeze, userdata);
}

//static
void LLFloaterAvatarList::onClickEject(void *userdata)
{
	LLStringUtilBase<char>::format_map_t args;
	args["[NAMES]"] = ((LLFloaterAvatarList*)userdata)->getSelectedNames();
	gViewerWindow->alertXml("EjectAvatar", args, callbackEject, userdata);
}

//static
void LLFloaterAvatarList::onClickMute(void *userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;

	LLDynamicArray<LLUUID> ids = self->mAvatarList->getSelectedIDs();
	if (ids.size() > 0)
	{
		for (LLDynamicArray<LLUUID>::iterator itr = ids.begin(); itr != ids.end(); ++itr)
		{
			LLUUID agent_id = *itr;
		
			std::string agent_name;
			if (gCacheName->getFullName(agent_id, agent_name))
			{
				if (LLMuteList::getInstance()->isMuted(agent_id))
				{
					LLMute mute(agent_id, agent_name, LLMute::AGENT);
					LLMuteList::getInstance()->remove(mute);	
				}
				else
				{
					LLMute mute(agent_id, agent_name, LLMute::AGENT);
					LLMuteList::getInstance()->add(mute);
				}
			}
		}
	}
}

//static
void LLFloaterAvatarList::onClickEjectFromEstate(void *userdata)
{
	LLStringUtilBase<char>::format_map_t args;
	args["[NAMES]"] = ((LLFloaterAvatarList*)userdata)->getSelectedNames();
	gViewerWindow->alertXml("EjectAvatarEstate", args, callbackEjectFromEstate, userdata);
}

//static
void LLFloaterAvatarList::onClickAR(void *userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
 	LLScrollListItem *item = self->mAvatarList->getFirstSelected();
	if (item)
	{
		LLUUID agent_id = item->getUUID();
		LLAvatarListEntry *entry = self->getAvatarEntry(agent_id);
		if (entry)
		{
			LLFloaterReporter::showFromObject(entry->getID());
		}
	}
}

// static
void LLFloaterAvatarList::onClickProfile(void* userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
	self->doCommand(cmd_profile);
}

//static
void LLFloaterAvatarList::onClickTeleport(void* userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
 	LLScrollListItem *item = self->mAvatarList->getFirstSelected();
	if (item)
	{
		LLUUID agent_id = item->getUUID();
		LLAvatarListEntry *entry = self->getAvatarEntry(agent_id);
		if (entry)
		{
//			llinfos << "Trying to teleport to " << entry->getName() << " at " << entry->getPosition() << llendl;
			gAgent.teleportViaLocation(entry->getPosition());
		}
	}
}

void LLFloaterAvatarList::onSelectName(LLUICtrl*, void* userdata)
{
	LLFloaterAvatarList* self = (LLFloaterAvatarList*)userdata;

 	LLScrollListItem *item = self->mAvatarList->getFirstSelected();
	if (item)
	{
		LLUUID agent_id = item->getUUID();
		LLAvatarListEntry *entry = self->getAvatarEntry(agent_id);
		if (entry)
		{
			BOOL enabled = entry->isDrawn();
			self->childSetEnabled("focus_btn", enabled);
			self->childSetEnabled("prev_in_list_btn", enabled);
			self->childSetEnabled("next_in_list_btn", enabled);
		}
	}
}

void LLFloaterAvatarList::onCommitUpdateRate(LLUICtrl*, void* userdata)
{
	LLFloaterAvatarList* self = (LLFloaterAvatarList*)userdata;

	self->mUpdateRate = gSavedSettings.getU32("RadarUpdateRate") * 3 + 3;
}
