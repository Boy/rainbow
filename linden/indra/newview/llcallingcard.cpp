/** 
 * @file llcallingcard.cpp
 * @brief Implementation of the LLPreviewCallingCard class
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#if LL_WINDOWS
#pragma warning( disable : 4800 ) // performance warning in <functional>
#endif

#include "llcallingcard.h"

#include <vector>
#include <algorithm>
//#include <iterator>

#include "indra_constants.h"
#include "llcachename.h"
#include "llstl.h"
#include "lltimer.h"
#include "lluuid.h"
#include "message.h"

#include "llagent.h"
#include "llbutton.h"
//#include "llinventory.h"
#include "llinventorymodel.h"
#include "llnotify.h"
#include "llresmgr.h"
#include "llimview.h"
#include "llviewercontrol.h"
#include "llviewernetwork.h"
#include "llviewerobjectlist.h"
#include "llviewerwindow.h"
#include "llvoavatar.h"
#include "llimview.h"
#include "llimpanel.h"

///----------------------------------------------------------------------------
/// Local function declarations, constants, enums, and typedefs
///----------------------------------------------------------------------------

class LLTrackingData
{
public:
	LLTrackingData(const LLUUID& avatar_id, const std::string& name);
	bool haveTrackingInfo();
	void setTrackedCoarseLocation(const LLVector3d& global_pos);
	void agentFound(const LLUUID& prey,
					const LLVector3d& estimated_global_pos);
	
public:
	LLUUID mAvatarID;
	std::string mName;
	LLVector3d mGlobalPositionEstimate;
	bool mHaveInfo;
	bool mHaveCoarseInfo;
	LLTimer mCoarseLocationTimer;
	LLTimer mUpdateTimer;
	LLTimer mAgentGone;
};

const F32 COARSE_FREQUENCY = 2.2f;
const F32 FIND_FREQUENCY = 29.7f;	// This results in a database query, so cut these back
const F32 OFFLINE_SECONDS = FIND_FREQUENCY + 8.0f;

// static
LLAvatarTracker LLAvatarTracker::sInstance;

/*
class LLAvatarTrackerInventoryObserver : public LLInventoryObserver
{
public:
	LLAvatarTrackerInventoryObserver(LLAvatarTracker* at) :
		mAT(at) {}
	virtual ~LLAvatarTrackerInventoryObserver() {}
	virtual void changed(U32 mask);
protected:
	LLAvatarTracker* mAT;
};
*/

/*
void LLAvatarTrackerInventoryObserver::changed(U32 mask)
{
	// if there's a calling card change, just do it.
	if((mask & LLInventoryObserver::CALLING_CARD) != 0)
	{
		mAT->inventoryChanged();
	}
}
*/

///----------------------------------------------------------------------------
/// Class LLAvatarTracker
///----------------------------------------------------------------------------

LLAvatarTracker::LLAvatarTracker() :
	mTrackingData(NULL),
	mTrackedAgentValid(false),
	//mInventory(NULL),
	//mInventoryObserver(NULL),
	mModifyMask(0x0)	
{
}

LLAvatarTracker::~LLAvatarTracker()
{
	deleteTrackingData();
	std::for_each(mObservers.begin(), mObservers.end(), DeletePointer());
	std::for_each(mBuddyInfo.begin(), mBuddyInfo.end(), DeletePairedPointer());
}

void LLAvatarTracker::track(const LLUUID& avatar_id, const std::string& name)
{
	deleteTrackingData();
	mTrackedAgentValid = false;
	mTrackingData = new LLTrackingData(avatar_id, name);
	findAgent();

	// We track here because findAgent() is called on a timer (for now).
	if(avatar_id.notNull())
	{
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_TrackAgent);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_TargetData);
		msg->addUUIDFast(_PREHASH_PreyID, avatar_id);
		gAgent.sendReliableMessage();
	}
}

void LLAvatarTracker::untrack(const LLUUID& avatar_id)
{
	if (mTrackingData && mTrackingData->mAvatarID == avatar_id)
	{
		deleteTrackingData();
		mTrackedAgentValid = false;
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_TrackAgent);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_TargetData);
		msg->addUUIDFast(_PREHASH_PreyID, LLUUID::null);
		gAgent.sendReliableMessage();
	}
}

void LLAvatarTracker::setTrackedCoarseLocation(const LLVector3d& global_pos)
{
	if(mTrackingData)
	{
		mTrackingData->setTrackedCoarseLocation(global_pos);
	}
}

bool LLAvatarTracker::haveTrackingInfo()
{
	if(mTrackingData)
	{
		return mTrackingData->haveTrackingInfo();
	}
	return false;
}

LLVector3d LLAvatarTracker::getGlobalPos()
{
	if(!mTrackedAgentValid || !mTrackingData) return LLVector3d();
	LLVector3d global_pos;
	
	LLViewerObject* object = gObjectList.findObject(mTrackingData->mAvatarID);
	if(object && !object->isDead())
	{
		global_pos = object->getPositionGlobal();
		// HACK - for making the tracker point above the avatar's head
		// rather than its groin
		global_pos.mdV[VZ] += 0.7f * ((LLVOAvatar *)object)->mBodySize.mV[VZ];

		mTrackingData->mGlobalPositionEstimate = global_pos;
	}
	else
	{
		global_pos = mTrackingData->mGlobalPositionEstimate;
	}

	return global_pos;
}

void LLAvatarTracker::getDegreesAndDist(F32& rot,
										F64& horiz_dist,
										F64& vert_dist)
{
	if(!mTrackingData) return;

	LLVector3d global_pos;

	LLViewerObject* object = gObjectList.findObject(mTrackingData->mAvatarID);
	if(object && !object->isDead())
	{
		global_pos = object->getPositionGlobal();
		mTrackingData->mGlobalPositionEstimate = global_pos;
	}
	else
	{
		global_pos = mTrackingData->mGlobalPositionEstimate;
	}
	LLVector3d to_vec = global_pos - gAgent.getPositionGlobal();
	horiz_dist = sqrt(to_vec.mdV[VX] * to_vec.mdV[VX] + to_vec.mdV[VY] * to_vec.mdV[VY]);
	vert_dist = to_vec.mdV[VZ];
	rot = F32(RAD_TO_DEG * atan2(to_vec.mdV[VY], to_vec.mdV[VX]));
}

const std::string& LLAvatarTracker::getName()
{
	if(mTrackingData)
	{
		return mTrackingData->mName;
	}
	else
	{
		return LLStringUtil::null;
	}
}

const LLUUID& LLAvatarTracker::getAvatarID()
{
	if(mTrackingData)
	{
		return mTrackingData->mAvatarID;
	}
	else
	{
		return LLUUID::null;
	}
}

S32 LLAvatarTracker::addBuddyList(const LLAvatarTracker::buddy_map_t& buds)
{
	using namespace std;

	U32 new_buddy_count = 0;
	std::string first,last;
	LLUUID agent_id;
	for(buddy_map_t::const_iterator itr = buds.begin(); itr != buds.end(); ++itr)
	{
		agent_id = (*itr).first;
		buddy_map_t::const_iterator existing_buddy = mBuddyInfo.find(agent_id);
		if(existing_buddy == mBuddyInfo.end())
		{
			++new_buddy_count;
			mBuddyInfo[agent_id] = (*itr).second;
			gCacheName->getName(agent_id, first, last);
			mModifyMask |= LLFriendObserver::ADD;
			lldebugs << "Added buddy " << agent_id
					<< ", " << (mBuddyInfo[agent_id]->isOnline() ? "Online" : "Offline")
					<< ", TO: " << mBuddyInfo[agent_id]->getRightsGrantedTo()
					<< ", FROM: " << mBuddyInfo[agent_id]->getRightsGrantedFrom()
					<< llendl;
		}
		else
		{
			LLRelationship* e_r = (*existing_buddy).second;
			LLRelationship* n_r = (*itr).second;
			llwarns << "!! Add buddy for existing buddy: " << agent_id
					<< " [" << (e_r->isOnline() ? "Online" : "Offline") << "->" << (n_r->isOnline() ? "Online" : "Offline")
					<< ", " <<  e_r->getRightsGrantedTo() << "->" << n_r->getRightsGrantedTo()
					<< ", " <<  e_r->getRightsGrantedTo() << "->" << n_r->getRightsGrantedTo()
					<< "]" << llendl;
		}
	}
	notifyObservers();
	
	return new_buddy_count;
}


void LLAvatarTracker::copyBuddyList(buddy_map_t& buddies) const
{
	buddy_map_t::const_iterator it = mBuddyInfo.begin();
	buddy_map_t::const_iterator end = mBuddyInfo.end();
	for(; it != end; ++it)
	{
		buddies[(*it).first] = (*it).second;
	}
}

void LLAvatarTracker::terminateBuddy(const LLUUID& id)
{
	lldebugs << "LLAvatarTracker::terminateBuddy()" << llendl;
	LLRelationship* buddy = get_ptr_in_map(mBuddyInfo, id);
	if(!buddy) return;
	mBuddyInfo.erase(id);
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessage("TerminateFriendship");
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID", gAgent.getID());
	msg->addUUID("SessionID", gAgent.getSessionID());
	msg->nextBlock("ExBlock");
	msg->addUUID("OtherID", id);
	gAgent.sendReliableMessage();
	mModifyMask |= LLFriendObserver::REMOVE;
	delete buddy;
}

// get all buddy info
const LLRelationship* LLAvatarTracker::getBuddyInfo(const LLUUID& id) const
{
	if(id.isNull()) return NULL;
	return get_ptr_in_map(mBuddyInfo, id);
}

// online status
void LLAvatarTracker::setBuddyOnline(const LLUUID& id, bool is_online)
{
	LLRelationship* info = get_ptr_in_map(mBuddyInfo, id);
	if(info)
	{
		info->online(is_online);
		mModifyMask |= LLFriendObserver::ONLINE;
		lldebugs << "Set buddy " << id << (is_online ? " Online" : " Offline") << llendl;
	}
	else
	{
		llwarns << "!! No buddy info found for " << id 
				<< ", setting to " << (is_online ? "Online" : "Offline") << llendl;
	}
}

bool LLAvatarTracker::isBuddyOnline(const LLUUID& id) const
{
	LLRelationship* info = get_ptr_in_map(mBuddyInfo, id);
	if(info)
	{
		return info->isOnline();
	}
	return false;
}

// empowered status
void LLAvatarTracker::setBuddyEmpowered(const LLUUID& id, bool is_empowered)
{
	LLRelationship* info = get_ptr_in_map(mBuddyInfo, id);
	if(info)
	{
		info->grantRights(LLRelationship::GRANT_MODIFY_OBJECTS, 0);
		mModifyMask |= LLFriendObserver::POWERS;
	}
}

bool LLAvatarTracker::isBuddyEmpowered(const LLUUID& id) const
{
	LLRelationship* info = get_ptr_in_map(mBuddyInfo, id);
	if(info)
	{
		return info->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS);
	}
	return false;
}

void LLAvatarTracker::empower(const LLUUID& id, bool grant)
{
	// wrapper for ease of use in some situations.
	buddy_map_t list;
	/*
	list.insert(id);
	empowerList(list, grant);
	*/
}

void LLAvatarTracker::empowerList(const buddy_map_t& list, bool grant)
{
	llwarns << "LLAvatarTracker::empowerList() not implemented." << llendl;
/*
	LLMessageSystem* msg = gMessageSystem;
	const char* message_name;
	const char* block_name;
	const char* field_name;
	if(grant)
	{
		message_name = _PREHASH_GrantModification;
		block_name = _PREHASH_EmpoweredBlock;
		field_name = _PREHASH_EmpoweredID;
	}
	else
	{
		message_name = _PREHASH_RevokeModification;
		block_name = _PREHASH_RevokedBlock;
		field_name = _PREHASH_RevokedID;
	}

	std::string name;
	gAgent.buildFullnameAndTitle(name);

	bool start_new_message = true;
	buddy_list_t::const_iterator it = list.begin();
	buddy_list_t::const_iterator end = list.end();
	for(; it != end; ++it)
	{
		if(NULL == get_ptr_in_map(mBuddyInfo, (*it))) continue;
		setBuddyEmpowered((*it), grant);
		if(start_new_message)
		{
			start_new_message = false;
			msg->newMessageFast(message_name);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
			msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
			msg->addStringFast(_PREHASH_GranterName, name);
		}
		msg->nextBlockFast(block_name);
		msg->addUUIDFast(field_name, (*it));
		if(msg->isSendFullFast(block_name))
		{
			start_new_message = true;
			gAgent.sendReliableMessage();
		}
	}
	if(!start_new_message)
	{
		gAgent.sendReliableMessage();
	}
*/
}

void LLAvatarTracker::deleteTrackingData()
{
	//make sure mTrackingData never points to freed memory
	LLTrackingData* tmp = mTrackingData;
	mTrackingData = NULL;
	delete tmp;
}

void LLAvatarTracker::findAgent()
{
	if (!mTrackingData) return;
	if (mTrackingData->mAvatarID.isNull()) return;
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_FindAgent); // Request
	msg->nextBlockFast(_PREHASH_AgentBlock);
	msg->addUUIDFast(_PREHASH_Hunter, gAgentID);
	msg->addUUIDFast(_PREHASH_Prey, mTrackingData->mAvatarID);
	msg->addU32Fast(_PREHASH_SpaceIP, 0); // will get filled in by simulator
	msg->nextBlockFast(_PREHASH_LocationBlock);
	const F64 NO_LOCATION = 0.0;
	msg->addF64Fast(_PREHASH_GlobalX, NO_LOCATION);
	msg->addF64Fast(_PREHASH_GlobalY, NO_LOCATION);
	gAgent.sendReliableMessage();
}

void LLAvatarTracker::addObserver(LLFriendObserver* observer)
{
	if(observer)
	{
		mObservers.push_back(observer);
	}
}

void LLAvatarTracker::removeObserver(LLFriendObserver* observer)
{
	mObservers.erase(
		std::remove(mObservers.begin(), mObservers.end(), observer),
		mObservers.end());
}

void LLAvatarTracker::notifyObservers()
{
	observer_list_t observers(mObservers);
	observer_list_t::iterator it = observers.begin();
	observer_list_t::iterator end = observers.end();
	for(; it != end; ++it)
	{
		(*it)->changed(mModifyMask);
	}
	mModifyMask = LLFriendObserver::NONE;
}

void LLAvatarTracker::applyFunctor(LLRelationshipFunctor& f)
{
	buddy_map_t::iterator it = mBuddyInfo.begin();
	buddy_map_t::iterator end = mBuddyInfo.end();
	for(; it != end; ++it)
	{
		f((*it).first, (*it).second);
	}
}

void LLAvatarTracker::registerCallbacks(LLMessageSystem* msg)
{
	msg->setHandlerFuncFast(_PREHASH_FindAgent, processAgentFound);
	msg->setHandlerFuncFast(_PREHASH_OnlineNotification,
						processOnlineNotification);
	msg->setHandlerFuncFast(_PREHASH_OfflineNotification,
						processOfflineNotification);
	//msg->setHandlerFuncFast(_PREHASH_GrantedProxies,
	//					processGrantedProxies);
	msg->setHandlerFunc("TerminateFriendship", processTerminateFriendship);
	msg->setHandlerFunc(_PREHASH_ChangeUserRights, processChangeUserRights);
}

// static
void LLAvatarTracker::processAgentFound(LLMessageSystem* msg, void**)
{
	LLUUID id;

	
	msg->getUUIDFast(_PREHASH_AgentBlock, _PREHASH_Hunter, id);
	msg->getUUIDFast(_PREHASH_AgentBlock, _PREHASH_Prey, id);
	// *FIX: should make sure prey id matches.
	LLVector3d estimated_global_pos;
	msg->getF64Fast(_PREHASH_LocationBlock, _PREHASH_GlobalX,
				 estimated_global_pos.mdV[VX]);
	msg->getF64Fast(_PREHASH_LocationBlock, _PREHASH_GlobalY,
				 estimated_global_pos.mdV[VY]);
	LLAvatarTracker::instance().agentFound(id, estimated_global_pos);
}

void LLAvatarTracker::agentFound(const LLUUID& prey,
								 const LLVector3d& estimated_global_pos)
{
	if(!mTrackingData) return;
	//if we get a valid reply from the server, that means the agent
	//is our friend and mappable, so enable interest list based updates
	LLAvatarTracker::instance().setTrackedAgentValid(true);
	mTrackingData->agentFound(prey, estimated_global_pos);
}

// 	static
void LLAvatarTracker::processOnlineNotification(LLMessageSystem* msg, void**)
{
	lldebugs << "LLAvatarTracker::processOnlineNotification()" << llendl;
	instance().processNotify(msg, true);
}

// 	static
void LLAvatarTracker::processOfflineNotification(LLMessageSystem* msg, void**)
{
	lldebugs << "LLAvatarTracker::processOfflineNotification()" << llendl;
	instance().processNotify(msg, false);
}

void LLAvatarTracker::processChange(LLMessageSystem* msg)
{
	S32 count = msg->getNumberOfBlocksFast(_PREHASH_Rights);
	LLUUID agent_id, agent_related;
	S32 new_rights;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	for(int i = 0; i < count; ++i)
	{
		msg->getUUIDFast(_PREHASH_Rights, _PREHASH_AgentRelated, agent_related, i);
		msg->getS32Fast(_PREHASH_Rights,_PREHASH_RelatedRights, new_rights, i);
		if(agent_id == gAgent.getID())
		{
			if(mBuddyInfo.find(agent_related) != mBuddyInfo.end())
			{
				(mBuddyInfo[agent_related])->setRightsTo(new_rights);
			}
		}
		else
		{
			if(mBuddyInfo.find(agent_id) != mBuddyInfo.end())
			{
				if((mBuddyInfo[agent_id]->getRightsGrantedFrom() ^  new_rights) & LLRelationship::GRANT_MODIFY_OBJECTS)
				{
					std::string first, last;
					LLStringUtil::format_map_t args;
					if(gCacheName->getName(agent_id, first, last))
					{
						if (LLAvatarName::sOmitResidentAsLastName && last == "Resident")
						{
							last = "";
						}
						if (LLAvatarNameCache::useDisplayNames() && !gSavedSettings.getBOOL("LegacyNamesForFriends"))
						{
							LLAvatarName avatar_name;
							if (LLAvatarNameCache::get(agent_id, &avatar_name))
							{
								// Always show "Display Name [Legacy Name]" for security reasons
								first = avatar_name.getNames();
								size_t i = first.find(" ");
								if (i != std::string::npos)
								{
									last = first.substr(i + 1);
									first = first.substr(0, i);
								}
								else
								{
									last = "";
								}
							}
						}
						args["[FIRST_NAME]"] = first;
						args["[LAST_NAME]"] = last;	
					}
					if(LLRelationship::GRANT_MODIFY_OBJECTS & new_rights)
					{
						gViewerWindow->alertXml("GrantedModifyRights",args);
					}
					else
					{
						gViewerWindow->alertXml("RevokedModifyRights",args);
					}
				}
				(mBuddyInfo[agent_id])->setRightsFrom(new_rights);
			}
		}
	}
	mModifyMask |= LLFriendObserver::POWERS;

	notifyObservers();
}

void LLAvatarTracker::processChangeUserRights(LLMessageSystem* msg, void**)
{
	lldebugs << "LLAvatarTracker::processChangeUserRights()" << llendl;
	instance().processChange(msg);
}

void LLAvatarTracker::processNotify(LLMessageSystem* msg, bool online)
{
	S32 count = msg->getNumberOfBlocksFast(_PREHASH_AgentBlock);
	BOOL chat_notify = gSavedSettings.getBOOL("ChatOnlineNotification");

	lldebugs << "Received " << count << " online notifications **** " << llendl;
	if(count > 0)
	{
		LLUUID agent_id;
		const LLRelationship* info = NULL;
		LLUUID tracking_id;
		if(mTrackingData)
		{
			tracking_id = mTrackingData->mAvatarID;
		}
		BOOL notify = FALSE;
		LLStringUtil::format_map_t args;
		for(S32 i = 0; i < count; ++i)
		{
			msg->getUUIDFast(_PREHASH_AgentBlock, _PREHASH_AgentID, agent_id, i);
			info = getBuddyInfo(agent_id);
			if(info)
			{
				setBuddyOnline(agent_id,online);
				if(chat_notify)
				{
					std::string first, last;
					if(gCacheName->getName(agent_id, first, last))
					{
						if (LLAvatarName::sOmitResidentAsLastName && last == "Resident")
						{
							last = "";
						}
						if (LLAvatarNameCache::useDisplayNames() && !gSavedSettings.getBOOL("LegacyNamesForFriends"))
						{
							LLAvatarName avatar_name;
							if (LLAvatarNameCache::get(agent_id, &avatar_name))
							{
								if (LLAvatarNameCache::useDisplayNames() == 2)
								{
									first = avatar_name.mDisplayName;
								}
								else
								{
									first = avatar_name.getNames();
								}
								size_t i = first.find(" ");
								if (i != std::string::npos)
								{
									last = first.substr(i + 1);
									first = first.substr(0, i);
								}
								else
								{
									last = "";
								}
							}
						}
						notify = TRUE;
						args["[FIRST]"] = first;
						args["[LAST]"] = last;
					}
				}
			}
			else
			{
				llwarns << "Received online notification for unknown buddy: " 
					<< agent_id << " is " << (online ? "ONLINE" : "OFFLINE") << llendl;
			}

			if(tracking_id == agent_id)
			{
				// we were tracking someone who went offline
				deleteTrackingData();
			}
			// *TODO: get actual inventory id
			gInventory.addChangedMask(LLInventoryObserver::CALLING_CARD, LLUUID::null);
		}
		if(notify)
		{
			// Popup a notify box with online status of this agent
			LLNotifyBox::showXml(online ? "FriendOnline" : "FriendOffline", args);

			// If there's an open IM session with this agent, send a notification there too.
			LLUUID session_id = LLIMMgr::computeSessionID(IM_NOTHING_SPECIAL, agent_id);
			LLFloaterIMPanel *floater = gIMMgr->findFloaterBySession(session_id);
			if (floater)
			{
				LLUIString notifyMsg = LLNotifyBox::getTemplateMessage((online ? "FriendOnline" : "FriendOffline"),args);
				if (!notifyMsg.empty())
					floater->addHistoryLine(notifyMsg,gSavedSettings.getColor4("SystemChatColor"));
			}
		}

		mModifyMask |= LLFriendObserver::ONLINE;
		instance().notifyObservers();
		gInventory.notifyObservers();
	}
}

void LLAvatarTracker::formFriendship(const LLUUID& id)
{
	if(id.notNull())
	{
		LLRelationship* buddy_info = get_ptr_in_map(instance().mBuddyInfo, id);
		if(!buddy_info)
		{
			LLAvatarTracker& at = LLAvatarTracker::instance();
			//The default for relationship establishment is to have both parties
			//visible online to each other.
			buddy_info = new LLRelationship(LLRelationship::GRANT_ONLINE_STATUS,LLRelationship::GRANT_ONLINE_STATUS, false);
			at.mBuddyInfo[id] = buddy_info;
			at.mModifyMask |= LLFriendObserver::ADD;
			at.notifyObservers();
		}
	}
}

void LLAvatarTracker::processTerminateFriendship(LLMessageSystem* msg, void**)
{
	LLUUID id;
	msg->getUUID("ExBlock", "OtherID", id);
	if(id.notNull())
	{
		LLAvatarTracker& at = LLAvatarTracker::instance();
		LLRelationship* buddy = get_ptr_in_map(at.mBuddyInfo, id);
		if(!buddy) return;
		at.mBuddyInfo.erase(id);
		at.mModifyMask |= LLFriendObserver::REMOVE;
		delete buddy;
		at.notifyObservers();
	}
}

void LLAvatarTracker::dirtyBuddies()
{
	mModifyMask |=  LLFriendObserver::REMOVE | LLFriendObserver::ADD;
	notifyObservers();
}

///----------------------------------------------------------------------------
/// Tracking Data
///----------------------------------------------------------------------------

LLTrackingData::LLTrackingData(const LLUUID& avatar_id, const std::string& name)
:	mAvatarID(avatar_id),
	mHaveInfo(false),
	mHaveCoarseInfo(false)
{
	mCoarseLocationTimer.setTimerExpirySec(COARSE_FREQUENCY);
	mUpdateTimer.setTimerExpirySec(FIND_FREQUENCY);
	mAgentGone.setTimerExpirySec(OFFLINE_SECONDS);
	if(!name.empty())
	{
		mName = name;
	}
}

void LLTrackingData::agentFound(const LLUUID& prey,
								const LLVector3d& estimated_global_pos)
{
	if(prey != mAvatarID)
	{
		llwarns << "LLTrackingData::agentFound() - found " << prey
				<< " but looking for " << mAvatarID << llendl;
	}
	mHaveInfo = true;
	mAgentGone.setTimerExpirySec(OFFLINE_SECONDS);
	mGlobalPositionEstimate = estimated_global_pos;
}

bool LLTrackingData::haveTrackingInfo()
{
	LLViewerObject* object = gObjectList.findObject(mAvatarID);
	if(object && !object->isDead())
	{
		mCoarseLocationTimer.checkExpirationAndReset(COARSE_FREQUENCY);
		mUpdateTimer.setTimerExpirySec(FIND_FREQUENCY);
		mAgentGone.setTimerExpirySec(OFFLINE_SECONDS);
		mHaveInfo = true;
		return true;
	}
	if(mHaveCoarseInfo &&
	   !mCoarseLocationTimer.checkExpirationAndReset(COARSE_FREQUENCY))
	{
		// if we reach here, then we have a 'recent' coarse update
		mUpdateTimer.setTimerExpirySec(FIND_FREQUENCY);
		mAgentGone.setTimerExpirySec(OFFLINE_SECONDS);
		return true;
	}
	if(mUpdateTimer.checkExpirationAndReset(FIND_FREQUENCY))
	{
		LLAvatarTracker::instance().findAgent();
		mHaveCoarseInfo = false;
	}
	if(mAgentGone.checkExpirationAndReset(OFFLINE_SECONDS))
	{
		mHaveInfo = false;
		mHaveCoarseInfo = false;
	}
	return mHaveInfo;
}

void LLTrackingData::setTrackedCoarseLocation(const LLVector3d& global_pos)
{
	mCoarseLocationTimer.setTimerExpirySec(COARSE_FREQUENCY);
	mGlobalPositionEstimate = global_pos;
	mHaveInfo = true;
	mHaveCoarseInfo = true;
}

///----------------------------------------------------------------------------
// various buddy functors
///----------------------------------------------------------------------------

bool LLCollectProxyBuddies::operator()(const LLUUID& buddy_id, LLRelationship* buddy)
{
	if(buddy->isRightGrantedFrom(LLRelationship::GRANT_MODIFY_OBJECTS))
	{
		mProxy.insert(buddy_id);
	}
	return true;
}

bool LLCollectMappableBuddies::operator()(const LLUUID& buddy_id, LLRelationship* buddy)
{
	gCacheName->getName(buddy_id, mFirst, mLast);
	std::ostringstream fullname;
	fullname << mFirst << " " << mLast;
	buddy_map_t::value_type value(fullname.str(), buddy_id);
	if(buddy->isOnline() && buddy->isRightGrantedFrom(LLRelationship::GRANT_MAP_LOCATION))
	{
		mMappable.insert(value);
	}
	return true;
}

bool LLCollectOnlineBuddies::operator()(const LLUUID& buddy_id, LLRelationship* buddy)
{
	gCacheName->getName(buddy_id, mFirst, mLast);
	std::ostringstream fullname;
	fullname << mFirst << " " << mLast;
	buddy_map_t::value_type value(fullname.str(), buddy_id);
	if(buddy->isOnline())
	{
		mOnline.insert(value);
	}
	return true;
}

bool LLCollectAllBuddies::operator()(const LLUUID& buddy_id, LLRelationship* buddy)
{
	gCacheName->getName(buddy_id, mFirst, mLast);
	std::ostringstream fullname;
	fullname << mFirst << " " << mLast;
	buddy_map_t::value_type value(fullname.str(), buddy_id);
	if(buddy->isOnline())
	{
		mOnline.insert(value);
	}
	else
	{
		mOffline.insert(value);
	}
	return true;
}


