/** 
 * @file llgroupmgr.cpp
 * @brief LLGroupMgr class implementation
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 * 
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

/**
 * Manager for aggregating all client knowledge for specific groups
 * Keeps a cache of group information.
 */

#include "llviewerprecompiledheaders.h"

#include "llgroupmgr.h"

#include <vector>
#include <algorithm>

#include "llagent.h"
#include "llui.h"
#include "message.h"
#include "roles_constants.h"
#include "lltransactiontypes.h"
#include "llstatusbar.h"
#include "lleconomy.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llfloaterdirectory.h"
#include "llfloatergroupinfo.h"
#include "lluictrlfactory.h"
#include <boost/regex.hpp>


const U32 MAX_CACHED_GROUPS = 10;

//
// LLRoleActionSet
//
LLRoleActionSet::LLRoleActionSet()
: mActionSetData(NULL)
{ }

LLRoleActionSet::~LLRoleActionSet()
{
	delete mActionSetData;
	std::for_each(mActions.begin(), mActions.end(), DeletePointer());
}

//
// LLGroupMemberData
//

LLGroupMemberData::LLGroupMemberData(const LLUUID& id, 
										S32 contribution,
										U64 agent_powers,
										const std::string& title,
										const std::string& online_status,
										BOOL is_owner) : 
	mID(id), 
	mContribution(contribution), 
	mAgentPowers(agent_powers), 
	mTitle(title), 
	mOnlineStatus(online_status),
	mIsOwner(is_owner)
{
}

LLGroupMemberData::~LLGroupMemberData()
{
}

void LLGroupMemberData::addRole(const LLUUID& role, LLGroupRoleData* rd)
{
	mRolesList[role] = rd;
}

bool LLGroupMemberData::removeRole(const LLUUID& role)
{
	role_list_t::iterator it = mRolesList.find(role);

	if (it != mRolesList.end())
	{
		mRolesList.erase(it);
		return true;
	}

	return false;
}

//
// LLGroupRoleData
//

LLGroupRoleData::LLGroupRoleData(const LLUUID& role_id, 
								const std::string& role_name,
								const std::string& role_title,
								const std::string& role_desc,
								const U64 role_powers,
								const S32 member_count) :
	mRoleID(role_id),
	mMemberCount(member_count),
	mMembersNeedsSort(FALSE)
{
	mRoleData.mRoleName = role_name;
	mRoleData.mRoleTitle = role_title;
	mRoleData.mRoleDescription = role_desc;
	mRoleData.mRolePowers = role_powers;
	mRoleData.mChangeType = RC_UPDATE_NONE;
}

LLGroupRoleData::LLGroupRoleData(const LLUUID& role_id, 
								LLRoleData role_data,
								const S32 member_count) :
	mRoleID(role_id),
	mRoleData(role_data),
	mMemberCount(member_count),
	mMembersNeedsSort(FALSE)
{

}

LLGroupRoleData::~LLGroupRoleData()
{	
}

S32 LLGroupRoleData::getMembersInRole(std::vector<LLUUID> members,
									  BOOL needs_sort)
{
	if (mRoleID.isNull())
	{
		// This is the everyone role, just return the size of members, 
		// because everyone is in the everyone role.
		return members.size();
	}

	// Sort the members list, if needed.
	if (mMembersNeedsSort)
	{
		std::sort(mMemberIDs.begin(), mMemberIDs.end());
		mMembersNeedsSort = FALSE;
	}
	if (needs_sort)
	{
		// Sort the members parameter.
		std::sort(members.begin(), members.end());
	}

	// Return the number of members in the intersection.
	S32 max_size = llmin( members.size(), mMemberIDs.size() );
	std::vector<LLUUID> in_role( max_size );
	std::vector<LLUUID>::iterator in_role_end;
	in_role_end = std::set_intersection(mMemberIDs.begin(), mMemberIDs.end(),
									members.begin(), members.end(),
									in_role.begin());
	return in_role_end - in_role.begin();
}
			  
void LLGroupRoleData::addMember(const LLUUID& member)
{
	mMembersNeedsSort = TRUE;
	mMemberIDs.push_back(member);
}

bool LLGroupRoleData::removeMember(const LLUUID& member)
{
	std::vector<LLUUID>::iterator it = std::find(mMemberIDs.begin(),mMemberIDs.end(),member);

	if (it != mMemberIDs.end())
	{
		mMembersNeedsSort = TRUE;
		mMemberIDs.erase(it);
		return true;
	}

	return false;
}

void LLGroupRoleData::clearMembers()
{
	mMembersNeedsSort = FALSE;
	mMemberIDs.clear();
}


//
// LLGroupMgrGroupData
//

LLGroupMgrGroupData::LLGroupMgrGroupData(const LLUUID& id) : 
	mID(id), 
	mShowInList(TRUE), 
	mOpenEnrollment(FALSE), 
	mMembershipFee(0),
	mAllowPublish(FALSE),
	mListInProfile(FALSE),
	mMaturePublish(FALSE),
	mChanged(FALSE),
	mMemberCount(0),
	mRoleCount(0),
	mReceivedRoleMemberPairs(0),
	mMemberDataComplete(FALSE),
	mRoleDataComplete(FALSE),
	mRoleMemberDataComplete(FALSE),
	mGroupPropertiesDataComplete(FALSE),
	mPendingRoleMemberRequest(FALSE)
{
}

BOOL LLGroupMgrGroupData::getRoleData(const LLUUID& role_id, LLRoleData& role_data)
{
	role_data_map_t::const_iterator it;

	// Do we have changes for it?
	it = mRoleChanges.find(role_id);
	if (it != mRoleChanges.end()) 
	{
		if ((*it).second.mChangeType == RC_DELETE) return FALSE;

		role_data = (*it).second;
		return TRUE;
	}

	// Ok, no changes, hasn't been deleted, isn't a new role, just find the role.
	role_list_t::const_iterator rit = mRoles.find(role_id);
	if (rit != mRoles.end())
	{
		role_data = (*rit).second->getRoleData();
		return TRUE;
	}

	// This role must not exist.
	return FALSE;
}


void LLGroupMgrGroupData::setRoleData(const LLUUID& role_id, LLRoleData role_data)
{
	// If this is a newly created group, we need to change the data in the created list.
	role_data_map_t::iterator it;
	it = mRoleChanges.find(role_id);
	if (it != mRoleChanges.end())
	{
		if ((*it).second.mChangeType == RC_CREATE)
		{
			role_data.mChangeType = RC_CREATE;
			mRoleChanges[role_id] = role_data;
			return;
		}
		else if ((*it).second.mChangeType == RC_DELETE)
		{
			// Don't do anything for a role being deleted.
			return;
		}
	}

	// Not a new role, so put it in the changes list.
	LLRoleData old_role_data;
	role_list_t::iterator rit = mRoles.find(role_id);
	if (rit != mRoles.end())
	{
		bool data_change = ( ((*rit).second->mRoleData.mRoleDescription != role_data.mRoleDescription)
							|| ((*rit).second->mRoleData.mRoleName != role_data.mRoleName)
							|| ((*rit).second->mRoleData.mRoleTitle != role_data.mRoleTitle) );
		bool powers_change = ((*rit).second->mRoleData.mRolePowers != role_data.mRolePowers);
		
		if (!data_change && !powers_change)
		{
			// We are back to the original state, the changes have been 'undone' so take out the change.
			mRoleChanges.erase(role_id);
			return;
		}

		if (data_change && powers_change)
		{
			role_data.mChangeType = RC_UPDATE_ALL;
		}
		else if (data_change)
		{
			role_data.mChangeType = RC_UPDATE_DATA;
		}
		else
	{
			role_data.mChangeType = RC_UPDATE_POWERS;
		}

		mRoleChanges[role_id] = role_data;
	}
	else
		{
		llwarns << "Change being made to non-existant role " << role_id << llendl;
	}
}

BOOL LLGroupMgrGroupData::pendingRoleChanges()
{
	return (!mRoleChanges.empty());
}

// This is a no-op if the role has already been created.
void LLGroupMgrGroupData::createRole(const LLUUID& role_id, LLRoleData role_data)
{
	if (mRoleChanges.find(role_id) != mRoleChanges.end())
	{
		llwarns << "create role for existing role! " << role_id << llendl;
	}
	else
	{
		role_data.mChangeType = RC_CREATE;
		mRoleChanges[role_id] = role_data;
	}
}

void LLGroupMgrGroupData::deleteRole(const LLUUID& role_id)
{
	role_data_map_t::iterator it;

	// If this was a new role, just discard it.
	it = mRoleChanges.find(role_id);
	if (it != mRoleChanges.end() 
		&& (*it).second.mChangeType == RC_CREATE)
	{
		mRoleChanges.erase(it);
		return;
	}

	LLRoleData rd;
	rd.mChangeType = RC_DELETE;
	mRoleChanges[role_id] = rd;
}

void LLGroupMgrGroupData::addRolePower(const LLUUID &role_id, U64 power)
{
	LLRoleData rd;
	if (getRoleData(role_id,rd))
	{	
		rd.mRolePowers |= power;
		setRoleData(role_id,rd);
	}
	else
	{
		llwarns << "addRolePower: no role data found for " << role_id << llendl;
	}
}

void LLGroupMgrGroupData::removeRolePower(const LLUUID &role_id, U64 power)
{
	LLRoleData rd;
	if (getRoleData(role_id,rd))
	{
		rd.mRolePowers &= ~power;
		setRoleData(role_id,rd);
	}
	else
	{
		llwarns << "removeRolePower: no role data found for " << role_id << llendl;
	}
}

U64 LLGroupMgrGroupData::getRolePowers(const LLUUID& role_id)
{
	LLRoleData rd;
	if (getRoleData(role_id,rd))
	{
		return rd.mRolePowers;
	}
	else
	{
		llwarns << "getRolePowers: no role data found for " << role_id << llendl;
		return GP_NO_POWERS;
	}
}

void LLGroupMgrGroupData::removeData()
{
	// Remove member data first, because removeRoleData will walk the member list
	removeMemberData();
	removeRoleData();
}

void LLGroupMgrGroupData::removeMemberData()
{
	for (member_list_t::iterator mi = mMembers.begin(); mi != mMembers.end(); ++mi)
	{
		delete mi->second;
	}
	mMembers.clear();
	mMemberDataComplete = FALSE;
}

void LLGroupMgrGroupData::removeRoleData()
{
	for (member_list_t::iterator mi = mMembers.begin(); mi != mMembers.end(); ++mi)
	{
		LLGroupMemberData* data = mi->second;
		if (data)
		{
			data->clearRoles();
		}
	}

	for (role_list_t::iterator ri = mRoles.begin(); ri != mRoles.end(); ++ri)
	{
		LLGroupRoleData* data = ri->second;
		delete data;
	}
	mRoles.clear();
	mReceivedRoleMemberPairs = 0;
	mRoleDataComplete = FALSE;
	mRoleMemberDataComplete = FALSE;
}

void LLGroupMgrGroupData::removeRoleMemberData()
{
	for (member_list_t::iterator mi = mMembers.begin(); mi != mMembers.end(); ++mi)
	{
		LLGroupMemberData* data = mi->second;
		if (data)
		{
			data->clearRoles();
		}
	}

	for (role_list_t::iterator ri = mRoles.begin(); ri != mRoles.end(); ++ri)
	{
		LLGroupRoleData* data = ri->second;
		if (data)
		{
			data->clearMembers();
		}
	}

	mReceivedRoleMemberPairs = 0;
	mRoleMemberDataComplete = FALSE;
}

LLGroupMgrGroupData::~LLGroupMgrGroupData()
{
	removeData();
}

bool LLGroupMgrGroupData::changeRoleMember(const LLUUID& role_id, 
										   const LLUUID& member_id, 
										   LLRoleMemberChangeType rmc)
{
	role_list_t::iterator ri = mRoles.find(role_id);
	member_list_t::iterator mi = mMembers.find(member_id);

	if (ri == mRoles.end()
		|| mi == mMembers.end() )
	{
		if (ri == mRoles.end()) llwarns << "LLGroupMgrGroupData::changeRoleMember couldn't find role " << role_id << llendl;
		if (mi == mMembers.end()) llwarns << "LLGroupMgrGroupData::changeRoleMember couldn't find member " << member_id << llendl;
		return false;
	}

	LLGroupRoleData* grd = ri->second;
	LLGroupMemberData* gmd = mi->second;

	if (!grd || !gmd)
	{
		llwarns << "LLGroupMgrGroupData::changeRoleMember couldn't get member or role data." << llendl;
		return false;
	}

	if (RMC_ADD == rmc)
	{
		llinfos << " adding member to role." << llendl;
		grd->addMember(member_id);
		gmd->addRole(role_id,grd);

		//TODO move this into addrole function
		//see if they added someone to the owner role and update isOwner
		gmd->mIsOwner = (role_id == mOwnerRole) ? TRUE : gmd->mIsOwner;
	}
	else if (RMC_REMOVE == rmc)
	{
		llinfos << " removing member from role." << llendl;
		grd->removeMember(member_id);
		gmd->removeRole(role_id);

		//see if they removed someone from the owner role and update isOwner
		gmd->mIsOwner = (role_id == mOwnerRole) ? FALSE : gmd->mIsOwner;
	}

	lluuid_pair role_member;
	role_member.first = role_id;
	role_member.second = member_id;

	change_map_t::iterator it = mRoleMemberChanges.find(role_member);
	if (it != mRoleMemberChanges.end())
	{
		// There was already a role change for this role_member
		if (it->second.mChange == rmc)
		{
			// Already recorded this change?  Weird.
			llinfos << "Received duplicate change for "
					<< " role: " << role_id << " member " << member_id 
					<< " change " << (rmc == RMC_ADD ? "ADD" : "REMOVE") << llendl;
		}
		else
		{
			// The only two operations (add and remove) currently cancel each other out
			// If that changes this will need more logic
			if (rmc == RMC_NONE)
			{
				llwarns << "changeRoleMember: existing entry with 'RMC_NONE' change! This shouldn't happen." << llendl;
				LLRoleMemberChange rc(role_id,member_id,rmc);
				mRoleMemberChanges[role_member] = rc;
			}
			else
			{
				mRoleMemberChanges.erase(it);
			}
		}
	}
	else
	{
		LLRoleMemberChange rc(role_id,member_id,rmc);
		mRoleMemberChanges[role_member] = rc;
	}

	recalcAgentPowers(member_id);

	mChanged = TRUE;
	return true;
}

void LLGroupMgrGroupData::recalcAllAgentPowers()
{
	LLGroupMemberData* gmd;

	for (member_list_t::iterator mit = mMembers.begin();
		 mit != mMembers.end(); ++mit)
	{
		gmd = mit->second;
		if (!gmd) continue;

		gmd->mAgentPowers = 0;
		for (LLGroupMemberData::role_list_t::iterator it = gmd->mRolesList.begin();
			 it != gmd->mRolesList.end(); ++it)
		{
			LLGroupRoleData* grd = (*it).second;
			if (!grd) continue;

			gmd->mAgentPowers |= grd->mRoleData.mRolePowers;
		}
	}
}

void LLGroupMgrGroupData::recalcAgentPowers(const LLUUID& agent_id)
{
	member_list_t::iterator mi = mMembers.find(agent_id);
	if (mi == mMembers.end()) return;

	LLGroupMemberData* gmd = mi->second;

	if (!gmd) return;

	gmd->mAgentPowers = 0;
	for (LLGroupMemberData::role_list_t::iterator it = gmd->mRolesList.begin();
		 it != gmd->mRolesList.end(); ++it)
	{
		LLGroupRoleData* grd = (*it).second;
		if (!grd) continue;

		gmd->mAgentPowers |= grd->mRoleData.mRolePowers;
	}
}

bool packRoleUpdateMessageBlock(LLMessageSystem* msg, 
								const LLUUID& group_id,
								const LLUUID& role_id, 
								const LLRoleData& role_data, 
								bool start_message)
{
	if (start_message)
	{
		msg->newMessage("GroupRoleUpdate");
		msg->nextBlock("AgentData");
		msg->addUUID("AgentID",gAgent.getID());
		msg->addUUID("SessionID",gAgent.getSessionID());
		msg->addUUID("GroupID",group_id);
		start_message = false;
	}

	msg->nextBlock("RoleData");
	msg->addUUID("RoleID",role_id);
	msg->addString("Name", role_data.mRoleName);
	msg->addString("Description", role_data.mRoleDescription);
	msg->addString("Title", role_data.mRoleTitle);
	msg->addU64("Powers", role_data.mRolePowers);
	msg->addU8("UpdateType", (U8)role_data.mChangeType);

	if (msg->isSendFullFast())
	{
		gAgent.sendReliableMessage();
		start_message = true;
	}

	return start_message;
}

void LLGroupMgrGroupData::sendRoleChanges()
{
	// Commit changes locally
	LLGroupRoleData* grd;
	role_list_t::iterator role_it;
	LLMessageSystem* msg = gMessageSystem;
	bool start_message = true;

	bool need_role_cleanup = false;
	bool need_role_data = false;
	bool need_power_recalc = false;

	// Apply all changes
	for (role_data_map_t::iterator iter = mRoleChanges.begin();
		 iter != mRoleChanges.end(); )
	{
		role_data_map_t::iterator it = iter++; // safely incrament iter
		const LLUUID& role_id = (*it).first;
		const LLRoleData& role_data = (*it).second;

		// Commit to local data set
		role_it = mRoles.find((*it).first);
		if ( (mRoles.end() == role_it 
				&& RC_CREATE != role_data.mChangeType)
			|| (mRoles.end() != role_it
				&& RC_CREATE == role_data.mChangeType))
		{
			continue;
		}
	
		// NOTE: role_it is valid EXCEPT for the RC_CREATE case
		switch (role_data.mChangeType)
		{
			case RC_CREATE:
			{
				// NOTE: role_it is NOT valid in this case
				grd = new LLGroupRoleData(role_id, role_data, 0);
				mRoles[role_id] = grd;
				need_role_data = true;
				break;
			}
			case RC_DELETE:
			{
				LLGroupRoleData* group_role_data = (*role_it).second;
				delete group_role_data;
				mRoles.erase(role_it);
				need_role_cleanup = true;
				need_power_recalc = true;
				break;
			}
			case RC_UPDATE_ALL:
			case RC_UPDATE_POWERS:
				need_power_recalc = true;
			case RC_UPDATE_DATA:
			default: 
			{
				LLGroupRoleData* group_role_data = (*role_it).second;
				group_role_data->setRoleData(role_data); // NOTE! might modify mRoleChanges!
				break;
			}
		}

		// Update dataserver
		start_message = packRoleUpdateMessageBlock(msg,getID(),role_id,role_data,start_message);
	}

	if (!start_message)
	{
		gAgent.sendReliableMessage();
	}

	// If we delete a role then all the role-member pairs are invalid!
	if (need_role_cleanup)
	{
		removeRoleMemberData();
	}

	// If we create a new role, then we need to re-fetch all the role data.
	if (need_role_data)
	{
		LLGroupMgr::getInstance()->sendGroupRoleDataRequest(getID());
	}

	// Clean up change lists	
	mRoleChanges.clear();

	// Recalculate all the agent powers because role powers have now changed.
	if (need_power_recalc)
	{
		recalcAllAgentPowers();
	}
}

void LLGroupMgrGroupData::cancelRoleChanges()
{
	// Clear out all changes!
	mRoleChanges.clear();
}
//
// LLGroupMgr
//

LLGroupMgr::LLGroupMgr()
{
}

LLGroupMgr::~LLGroupMgr()
{
	clearGroups();
}

void LLGroupMgr::clearGroups()
{
	std::for_each(mRoleActionSets.begin(), mRoleActionSets.end(), DeletePointer());
	mRoleActionSets.clear();
	std::for_each(mGroups.begin(), mGroups.end(), DeletePairedPointer());
	mGroups.clear();
	mObservers.clear();
}

void LLGroupMgr::clearGroupData(const LLUUID& group_id)
{
	group_map_t::iterator iter = mGroups.find(group_id);
	if (iter != mGroups.end())
	{
		delete (*iter).second;
		mGroups.erase(iter);
	}
}

void LLGroupMgr::addObserver(LLGroupMgrObserver* observer) 
{ 
	mObservers.insert(std::pair<LLUUID, LLGroupMgrObserver*>(observer->getID(), observer));
}

void LLGroupMgr::removeObserver(LLGroupMgrObserver* observer)
{
	if (!observer)
	{
		return;
	}
	observer_multimap_t::iterator it;
	it = mObservers.find(observer->getID());
	while (it != mObservers.end())
	{
		if (it->second == observer)
		{
			mObservers.erase(it);
			break;
		}
		else
		{
			++it;
		}
	}
}

LLGroupMgrGroupData* LLGroupMgr::getGroupData(const LLUUID& id)
{
	group_map_t::iterator gi = mGroups.find(id);

	if (gi != mGroups.end())
	{
		return gi->second;
	}
	return NULL;
}

// static
void LLGroupMgr::processGroupMembersReply(LLMessageSystem* msg, void** data)
{
	lldebugs << "LLGroupMgr::processGroupMembersReply" << llendl;
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id );
	if (gAgent.getID() != agent_id)
	{
		llwarns << "Got group properties reply for another agent!" << llendl;
		return;
	}

	LLUUID group_id;
	msg->getUUIDFast(_PREHASH_GroupData, _PREHASH_GroupID, group_id );

	LLUUID request_id;
	msg->getUUIDFast(_PREHASH_GroupData, _PREHASH_RequestID, request_id);

	LLGroupMgrGroupData* group_datap = LLGroupMgr::getInstance()->createGroupData(group_id);
	if (group_datap->mMemberRequestID != request_id)
	{
		llwarns << "processGroupMembersReply: Received incorrect (stale?) request id" << llendl;
		return;
	}

	msg->getS32(_PREHASH_GroupData, "MemberCount", group_datap->mMemberCount );

	if (group_datap->mMemberCount > 0)
	{
		S32 contribution = 0;
		std::string online_status;
		std::string title;
		U64 agent_powers = 0;
		BOOL is_owner = FALSE;

		S32 num_members = msg->getNumberOfBlocksFast(_PREHASH_MemberData);
		for (S32 i = 0; i < num_members; i++)
		{
			LLUUID member_id;

			msg->getUUIDFast(_PREHASH_MemberData, _PREHASH_AgentID, member_id, i );
			msg->getS32(_PREHASH_MemberData, _PREHASH_Contribution, contribution, i);
			msg->getU64(_PREHASH_MemberData, "AgentPowers", agent_powers, i);
			msg->getStringFast(_PREHASH_MemberData, _PREHASH_OnlineStatus, online_status, i);
			msg->getString(_PREHASH_MemberData, "Title", title, i);
			msg->getBOOL(_PREHASH_MemberData,"IsOwner",is_owner,i);

			if (member_id.notNull())
			{
				tm t;
				if (sscanf(online_status.c_str(), "%u/%u/%u", &t.tm_mon, &t.tm_mday, &t.tm_year) == 3 && t.tm_year > 1900)
				{
					t.tm_year -= 1900;
					t.tm_mon--;
					t.tm_hour = t.tm_min = t.tm_sec = 0;
					timeStructToFormattedString(&t, gSavedSettings.getString("ShortDateFormat"), online_status);
				}
				
				//llinfos << "Member " << member_id << " has powers " << std::hex << agent_powers << std::dec << llendl;
				LLGroupMemberData* newdata = new LLGroupMemberData(member_id, 
																	contribution, 
																	agent_powers, 
																	title,
																	online_status,
																	is_owner);
#if LL_DEBUG
				LLGroupMgrGroupData::member_list_t::iterator mit = group_datap->mMembers.find(member_id);
				if (mit != group_datap->mMembers.end())
				{
					llinfos << " *** Received duplicate member data for agent " << member_id << llendl;
				}
#endif
				group_datap->mMembers[member_id] = newdata;
			}
			else
			{
				llinfos << "Received null group member data." << llendl;
			}
		}

		//if group members are loaded while titles are missing, load the titles.
		if(group_datap->mTitles.size() < 1)
		{
			LLGroupMgr::getInstance()->sendGroupTitlesRequest(group_id);
		}
	}

	if (group_datap->mMembers.size() ==  (U32)group_datap->mMemberCount)
	{
		group_datap->mMemberDataComplete = TRUE;
		group_datap->mMemberRequestID.setNull();
		// We don't want to make role-member data requests until we have all the members
		if (group_datap->mPendingRoleMemberRequest)
		{
			group_datap->mPendingRoleMemberRequest = FALSE;
			LLGroupMgr::getInstance()->sendGroupRoleMembersRequest(group_datap->mID);
		}
	}

	group_datap->mChanged = TRUE;
	LLGroupMgr::getInstance()->notifyObservers(GC_MEMBER_DATA);
}

//static 
void LLGroupMgr::processGroupPropertiesReply(LLMessageSystem* msg, void** data)
{
	lldebugs << "LLGroupMgr::processGroupPropertiesReply" << llendl;
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id );
	if (gAgent.getID() != agent_id)
	{
		llwarns << "Got group properties reply for another agent!" << llendl;
		return;
	}

	LLUUID group_id;
	std::string	name;
	std::string	charter;
	BOOL	show_in_list = FALSE;
	LLUUID	founder_id;
	U64		powers_mask = GP_NO_POWERS;
	S32		money = 0;
	std::string	member_title;
	LLUUID	insignia_id;
	LLUUID	owner_role;
	U32		membership_fee = 0;
	BOOL	open_enrollment = FALSE;
	S32		num_group_members = 0;
	S32		num_group_roles = 0;
	BOOL	allow_publish = FALSE;
	BOOL	mature = FALSE;

	msg->getUUIDFast(_PREHASH_GroupData, _PREHASH_GroupID, group_id );
	msg->getUUIDFast(_PREHASH_GroupData, _PREHASH_FounderID, founder_id);	
	msg->getStringFast(_PREHASH_GroupData, _PREHASH_Name, name );
	msg->getStringFast(_PREHASH_GroupData, _PREHASH_Charter, charter );
	msg->getBOOLFast(_PREHASH_GroupData, _PREHASH_ShowInList, show_in_list );
	msg->getStringFast(_PREHASH_GroupData, _PREHASH_MemberTitle, member_title );
	msg->getUUIDFast(_PREHASH_GroupData, _PREHASH_InsigniaID, insignia_id );
	msg->getU64Fast(_PREHASH_GroupData, _PREHASH_PowersMask, powers_mask );
	msg->getU32Fast(_PREHASH_GroupData, _PREHASH_MembershipFee, membership_fee );
	msg->getBOOLFast(_PREHASH_GroupData, _PREHASH_OpenEnrollment, open_enrollment );
	msg->getS32Fast(_PREHASH_GroupData, _PREHASH_GroupMembershipCount, num_group_members);
	msg->getS32(_PREHASH_GroupData, "GroupRolesCount", num_group_roles);
	msg->getS32Fast(_PREHASH_GroupData, _PREHASH_Money, money);
	msg->getBOOL("GroupData", "AllowPublish", allow_publish);
	msg->getBOOL("GroupData", "MaturePublish", mature);
	msg->getUUID(_PREHASH_GroupData, "OwnerRole", owner_role);

	LLGroupMgrGroupData* group_datap = LLGroupMgr::getInstance()->createGroupData(group_id);

	group_datap->mName = name;
	group_datap->mCharter = charter;
	group_datap->mShowInList = show_in_list;
	group_datap->mInsigniaID = insignia_id;
	group_datap->mFounderID = founder_id;
	group_datap->mMembershipFee = membership_fee;
	group_datap->mOpenEnrollment = open_enrollment;
	group_datap->mAllowPublish = allow_publish;
	group_datap->mMaturePublish = mature;
	group_datap->mOwnerRole = owner_role;
	group_datap->mMemberCount = num_group_members;
	group_datap->mRoleCount = num_group_roles + 1; // Add the everyone role.
	
	group_datap->mGroupPropertiesDataComplete = TRUE;
	group_datap->mChanged = TRUE;

	LLGroupMgr::getInstance()->notifyObservers(GC_PROPERTIES);
}

// static
void LLGroupMgr::processGroupRoleDataReply(LLMessageSystem* msg, void** data)
{
	lldebugs << "LLGroupMgr::processGroupRoleDataReply" << llendl;
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id );
	if (gAgent.getID() != agent_id)
	{
		llwarns << "Got group properties reply for another agent!" << llendl;
		return;
	}

	LLUUID group_id;
	msg->getUUIDFast(_PREHASH_GroupData, _PREHASH_GroupID, group_id );

	LLUUID request_id;
	msg->getUUIDFast(_PREHASH_GroupData, _PREHASH_RequestID, request_id);

	LLGroupMgrGroupData* group_data = LLGroupMgr::getInstance()->createGroupData(group_id);
	if (group_data->mRoleDataRequestID != request_id)
	{
		llwarns << "processGroupRoleDataReply: Received incorrect (stale?) request id" << llendl;
		return;
	}

	msg->getS32(_PREHASH_GroupData, "RoleCount", group_data->mRoleCount );

	std::string	name;
	std::string	title;
	std::string	desc;
	U64		powers = 0;
	U32		member_count = 0;
	LLUUID role_id;

	U32 num_blocks = msg->getNumberOfBlocks("RoleData");
	U32 i = 0;
	for (i=0; i< num_blocks; ++i)
	{
		msg->getUUID("RoleData", "RoleID", role_id, i );
		
		msg->getString("RoleData","Name",name,i);
		msg->getString("RoleData","Title",title,i);
		msg->getString("RoleData","Description",desc,i);
		msg->getU64("RoleData","Powers",powers,i);
		msg->getU32("RoleData","Members",member_count,i);

		lldebugs << "Adding role data: " << name << " {" << role_id << "}" << llendl;
		LLGroupRoleData* rd = new LLGroupRoleData(role_id,name,title,desc,powers,member_count);
		group_data->mRoles[role_id] = rd;
	}

	if (group_data->mRoles.size() == (U32)group_data->mRoleCount)
	{
		group_data->mRoleDataComplete = TRUE;
		group_data->mRoleDataRequestID.setNull();
		// We don't want to make role-member data requests until we have all the role data
		if (group_data->mPendingRoleMemberRequest)
		{
			group_data->mPendingRoleMemberRequest = FALSE;
			LLGroupMgr::getInstance()->sendGroupRoleMembersRequest(group_data->mID);
		}
	}

	group_data->mChanged = TRUE;
	LLGroupMgr::getInstance()->notifyObservers(GC_ROLE_DATA);
}

// static
void LLGroupMgr::processGroupRoleMembersReply(LLMessageSystem* msg, void** data)
{
	lldebugs << "LLGroupMgr::processGroupRoleMembersReply" << llendl;
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id );
	if (gAgent.getID() != agent_id)
	{
		llwarns << "Got group properties reply for another agent!" << llendl;
		return;
	}

	LLUUID request_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_RequestID, request_id);

	LLUUID group_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_GroupID, group_id );

	U32 total_pairs;
	msg->getU32(_PREHASH_AgentData, "TotalPairs", total_pairs);

	LLGroupMgrGroupData* group_data = LLGroupMgr::getInstance()->createGroupData(group_id);

	if (group_data->mRoleMembersRequestID != request_id)
	{
		llwarns << "processGroupRoleMembersReply: Received incorrect (stale?) role member request id" << llendl;
		return;
	}

	U32 num_blocks = msg->getNumberOfBlocks("MemberData");
	U32 i;
	LLUUID member_id;
	LLUUID role_id;
	LLGroupRoleData* rd = NULL;
	LLGroupMemberData* md = NULL;

	LLGroupMgrGroupData::role_list_t::iterator ri;
	LLGroupMgrGroupData::member_list_t::iterator mi;

	// If total_pairs == 0, there are no members in any custom roles.
	if (total_pairs > 0)
	{
		for (i = 0;i < num_blocks; ++i)
		{
			msg->getUUID("MemberData","RoleID",role_id,i);
			msg->getUUID("MemberData","MemberID",member_id,i);

			if (role_id.notNull() && member_id.notNull() )
			{
				rd = NULL;
				ri = group_data->mRoles.find(role_id);
				if (ri != group_data->mRoles.end())
				{
					rd = ri->second;
				}

				md = NULL;
				mi = group_data->mMembers.find(member_id);
				if (mi != group_data->mMembers.end())
				{
					md = mi->second;
				}

				if (rd && md)
				{
					lldebugs << "Adding role-member pair: " << role_id << ", " << member_id << llendl;
					rd->addMember(member_id);
					md->addRole(role_id,rd);
				}
				else
				{
					if (!rd) llwarns << "Received role data for unkown role " << role_id << " in group " << group_id << llendl;
					if (!md) llwarns << "Received role data for unkown member " << member_id << " in group " << group_id << llendl;
				}
			}
		}

		group_data->mReceivedRoleMemberPairs += num_blocks;
	}

	if (group_data->mReceivedRoleMemberPairs == total_pairs)
	{
		// Add role data for the 'everyone' role to all members
		LLGroupRoleData* everyone = group_data->mRoles[LLUUID::null];
		if (!everyone)
		{
			llwarns << "Everyone role not found!" << llendl;
		}
		else
		{
			for (LLGroupMgrGroupData::member_list_t::iterator mi = group_data->mMembers.begin();
				 mi != group_data->mMembers.end(); ++mi)
			{
				LLGroupMemberData* data = mi->second;
				if (data)
				{
					data->addRole(LLUUID::null,everyone);
				}
			}
		}
		
        group_data->mRoleMemberDataComplete = TRUE;
		group_data->mRoleMembersRequestID.setNull();
	}

	group_data->mChanged = TRUE;
	LLGroupMgr::getInstance()->notifyObservers(GC_ROLE_MEMBER_DATA);
}

// static
void LLGroupMgr::processGroupTitlesReply(LLMessageSystem* msg, void** data)
{
	lldebugs << "LLGroupMgr::processGroupTitlesReply" << llendl;
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id );
	if (gAgent.getID() != agent_id)
	{
		llwarns << "Got group properties reply for another agent!" << llendl;
		return;
	}

	LLUUID group_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_GroupID, group_id );

	LLGroupMgrGroupData* group_data = LLGroupMgr::getInstance()->createGroupData(group_id);

	LLUUID request_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_RequestID, request_id);

	if (group_data->mTitlesRequestID != request_id)
	{
		llwarns << "processGroupTitlesReply: Received incorrect (stale?) title request id" << llendl;
		return;
	}

	LLGroupTitle title;

	S32 i = 0;
	S32 blocks = msg->getNumberOfBlocksFast(_PREHASH_GroupData);
	for (i=0; i<blocks; ++i)
	{
		msg->getString("GroupData","Title",title.mTitle,i);
		msg->getUUID("GroupData","RoleID",title.mRoleID,i);
		msg->getBOOL("GroupData","Selected",title.mSelected,i);

		if (!title.mTitle.empty())
		{
			lldebugs << "LLGroupMgr adding title: " << title.mTitle << ", " << title.mRoleID << ", " << (title.mSelected ? 'Y' : 'N') << llendl;
			group_data->mTitles.push_back(title);
		}
	}

	group_data->mChanged = TRUE;
	LLGroupMgr::getInstance()->notifyObservers(GC_TITLES);
}

// static
void LLGroupMgr::processEjectGroupMemberReply(LLMessageSystem* msg, void ** data)
{
	lldebugs << "processEjectGroupMemberReply" << llendl;
	LLUUID group_id;
	msg->getUUIDFast(_PREHASH_GroupData, _PREHASH_GroupID, group_id);
	BOOL success;
	msg->getBOOLFast(_PREHASH_EjectData, _PREHASH_Success, success);

	// If we had a failure, the group panel needs to be updated.
	if (!success)
	{
		LLFloaterGroupInfo::refreshGroup(group_id);
	}
}

// static
void LLGroupMgr::processJoinGroupReply(LLMessageSystem* msg, void ** data)
{
	lldebugs << "processJoinGroupReply" << llendl;
	LLUUID group_id;
	BOOL success;
	msg->getUUIDFast(_PREHASH_GroupData, _PREHASH_GroupID, group_id);
	msg->getBOOLFast(_PREHASH_GroupData, _PREHASH_Success, success);

	if (success)
	{
		// refresh all group information
		gAgent.sendAgentDataUpdateRequest();

		LLGroupMgr::getInstance()->clearGroupData(group_id);
		// refresh the floater for this group, if any.
		LLFloaterGroupInfo::refreshGroup(group_id);
		// refresh the group panel of the search window, if necessary.
		LLFloaterDirectory::refreshGroup(group_id);
	}
}

// static
void LLGroupMgr::processLeaveGroupReply(LLMessageSystem* msg, void ** data)
{
	lldebugs << "processLeaveGroupReply" << llendl;
	LLUUID group_id;
	BOOL success;
	msg->getUUIDFast(_PREHASH_GroupData, _PREHASH_GroupID, group_id);
	msg->getBOOLFast(_PREHASH_GroupData, _PREHASH_Success, success);

	if (success)
	{
		// refresh all group information
		gAgent.sendAgentDataUpdateRequest();

		LLGroupMgr::getInstance()->clearGroupData(group_id);
		// close the floater for this group, if any.
		LLFloaterGroupInfo::closeGroup(group_id);
		// refresh the group panel of the search window, if necessary.
		LLFloaterDirectory::refreshGroup(group_id);
	}
}

// static
void LLGroupMgr::processCreateGroupReply(LLMessageSystem* msg, void ** data)
{
	LLUUID group_id;
	BOOL success;
	std::string message;

	msg->getUUIDFast(_PREHASH_ReplyData, _PREHASH_GroupID, group_id );

	msg->getBOOLFast(_PREHASH_ReplyData, _PREHASH_Success,	success );
	msg->getStringFast(_PREHASH_ReplyData, _PREHASH_Message, message );

	if (success)
	{
		// refresh all group information
		gAgent.sendAgentDataUpdateRequest();

		// HACK! We haven't gotten the agent group update yet, so ... um ... fake it.
		// This is so when we go to modify the group we will be able to do so.
		// This isn't actually too bad because real data will come down in 2 or 3 miliseconds and replace this.
		LLGroupData gd;
		gd.mAcceptNotices = TRUE;
		gd.mListInProfile = TRUE;
		gd.mContribution = 0;
		gd.mID = group_id;
		gd.mName = "new group";
		gd.mPowers = GP_ALL_POWERS;

		gAgent.mGroups.push_back(gd);

		LLFloaterGroupInfo::closeCreateGroup();
		LLFloaterGroupInfo::showFromUUID(group_id,"roles_tab");
	}
	else
	{
		// *TODO:translate
		LLStringUtil::format_map_t args;
		args["[MESSAGE]"] = message;
		gViewerWindow->alertXml("UnableToCreateGroup", args);
	}
}

LLGroupMgrGroupData* LLGroupMgr::createGroupData(const LLUUID& id)
{
	LLGroupMgrGroupData* group_datap;

	group_map_t::iterator existing_group = LLGroupMgr::getInstance()->mGroups.find(id);
	if (existing_group == LLGroupMgr::getInstance()->mGroups.end())
	{
		group_datap = new LLGroupMgrGroupData(id);
		LLGroupMgr::getInstance()->addGroup(group_datap);
	}
	else
	{
		group_datap = existing_group->second;
	}

	return group_datap;
}

void LLGroupMgr::notifyObservers(LLGroupChange gc)
{
	for (group_map_t::iterator gi = mGroups.begin(); gi != mGroups.end(); ++gi)
	{
		if (gi->second->mChanged)
		{
			// find all observers for this group id
			observer_multimap_t::iterator oi = mObservers.find(gi->first);
			for (; oi != mObservers.end(); ++oi)
			{
				oi->second->changed(gc);
			}
			gi->second->mChanged = FALSE;
		}
	}
}

void LLGroupMgr::addGroup(LLGroupMgrGroupData* group_datap)
{
	if (mGroups.size() > MAX_CACHED_GROUPS)
	{
		// get rid of groups that aren't observed
		for (group_map_t::iterator gi = mGroups.begin(); gi != mGroups.end() && mGroups.size() > MAX_CACHED_GROUPS / 2; )
		{
			observer_multimap_t::iterator oi = mObservers.find(gi->first);
			if (oi == mObservers.end())
			{
				// not observed
				LLGroupMgrGroupData* unobserved_groupp = gi->second;
				delete unobserved_groupp;
				mGroups.erase(gi++);
			}
			else
			{
				++gi;
			}
		}
	}
	mGroups[group_datap->getID()] = group_datap;
}


void LLGroupMgr::sendGroupPropertiesRequest(const LLUUID& group_id)
{
	lldebugs << "LLGroupMgr::sendGroupPropertiesRequest" << llendl;
	// This will happen when we get the reply
	//LLGroupMgrGroupData* group_datap = createGroupData(group_id);
	
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessage("GroupProfileRequest");
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID",gAgent.getID());
	msg->addUUID("SessionID",gAgent.getSessionID());
	msg->nextBlock("GroupData");
	msg->addUUID("GroupID",group_id);
	gAgent.sendReliableMessage();
}

void LLGroupMgr::sendGroupMembersRequest(const LLUUID& group_id)
{
	lldebugs << "LLGroupMgr::sendGroupMembersRequest" << llendl;
	LLGroupMgrGroupData* group_datap = createGroupData(group_id);
	if (group_datap->mMemberRequestID.isNull())
	{
		group_datap->removeMemberData();
		group_datap->mMemberRequestID.generate();

		LLMessageSystem* msg = gMessageSystem;
		msg->newMessage("GroupMembersRequest");
		msg->nextBlock("AgentData");
		msg->addUUID("AgentID",gAgent.getID());
		msg->addUUID("SessionID",gAgent.getSessionID());
		msg->nextBlock("GroupData");
		msg->addUUID("GroupID",group_id);
		msg->addUUID("RequestID",group_datap->mMemberRequestID);
		gAgent.sendReliableMessage();
	}
}

void LLGroupMgr::sendGroupRoleDataRequest(const LLUUID& group_id)
{
	lldebugs << "LLGroupMgr::sendGroupRoleDataRequest" << llendl;
	LLGroupMgrGroupData* group_datap = createGroupData(group_id);
	if (group_datap->mRoleDataRequestID.isNull())
	{
		group_datap->removeRoleData();
		group_datap->mRoleDataRequestID.generate();

		LLMessageSystem* msg = gMessageSystem;
		msg->newMessage("GroupRoleDataRequest");
		msg->nextBlock("AgentData");
		msg->addUUID("AgentID",gAgent.getID());
		msg->addUUID("SessionID",gAgent.getSessionID());
		msg->nextBlock("GroupData");
		msg->addUUID("GroupID",group_id);
		msg->addUUID("RequestID",group_datap->mRoleDataRequestID);
		gAgent.sendReliableMessage();
	}
}

void LLGroupMgr::sendGroupRoleMembersRequest(const LLUUID& group_id)
{
	lldebugs << "LLGroupMgr::sendGroupRoleMembersRequest" << llendl;
	LLGroupMgrGroupData* group_datap = createGroupData(group_id);
	
	if (group_datap->mRoleMembersRequestID.isNull())
	{
		// Don't send the request if we don't have all the member or role data
		if (!group_datap->isMemberDataComplete()
			|| !group_datap->isRoleDataComplete())
		{
			// *TODO: KLW FIXME: Should we start a member or role data request?
			llinfos << " Pending: " << (group_datap->mPendingRoleMemberRequest ? "Y" : "N")
				<< " MemberDataComplete: " << (group_datap->mMemberDataComplete ? "Y" : "N")
				<< " RoleDataComplete: " << (group_datap->mRoleDataComplete ? "Y" : "N") << llendl;
			group_datap->mPendingRoleMemberRequest = TRUE;
			return;
		}

		group_datap->removeRoleMemberData();
		group_datap->mRoleMembersRequestID.generate();

		LLMessageSystem* msg = gMessageSystem;
		msg->newMessage("GroupRoleMembersRequest");
		msg->nextBlock("AgentData");
		msg->addUUID("AgentID",gAgent.getID());
		msg->addUUID("SessionID",gAgent.getSessionID());
		msg->nextBlock("GroupData");
		msg->addUUID("GroupID",group_id);
		msg->addUUID("RequestID",group_datap->mRoleMembersRequestID);
		gAgent.sendReliableMessage();
	}
}

void LLGroupMgr::sendGroupTitlesRequest(const LLUUID& group_id)
{
	lldebugs << "LLGroupMgr::sendGroupTitlesRequest" << llendl;
	LLGroupMgrGroupData* group_datap = createGroupData(group_id);
	
	group_datap->mTitles.clear();
	group_datap->mTitlesRequestID.generate();

	LLMessageSystem* msg = gMessageSystem;
	msg->newMessage("GroupTitlesRequest");
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID",gAgent.getID());
	msg->addUUID("SessionID",gAgent.getSessionID());
	msg->addUUID("GroupID",group_id);
	msg->addUUID("RequestID",group_datap->mTitlesRequestID);

	gAgent.sendReliableMessage();
}

void LLGroupMgr::sendGroupTitleUpdate(const LLUUID& group_id, const LLUUID& title_role_id)
{
	lldebugs << "LLGroupMgr::sendGroupTitleUpdate" << llendl;

	LLMessageSystem* msg = gMessageSystem;
	msg->newMessage("GroupTitleUpdate");
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID",gAgent.getID());
	msg->addUUID("SessionID",gAgent.getSessionID());
	msg->addUUID("GroupID",group_id);
	msg->addUUID("TitleRoleID",title_role_id);

	gAgent.sendReliableMessage();

	// Save the change locally
	LLGroupMgrGroupData* group_datap = createGroupData(group_id);
	for (std::vector<LLGroupTitle>::iterator iter = group_datap->mTitles.begin();
		 iter != group_datap->mTitles.end(); ++iter)
	{
		if (iter->mRoleID == title_role_id)
		{
			iter->mSelected = TRUE;
		}
		else if (iter->mSelected)
		{
			iter->mSelected = FALSE;
		}
	}
}

// static
void LLGroupMgr::sendCreateGroupRequest(const std::string& name,
										const std::string& charter,
										U8 show_in_list,
										const LLUUID& insignia,
										S32 membership_fee,
										BOOL open_enrollment,
										BOOL allow_publish,
										BOOL mature_publish)
{
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessage("CreateGroupRequest");
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID",gAgent.getID());
	msg->addUUID("SessionID",gAgent.getSessionID());

	msg->nextBlock("GroupData");
	msg->addString("Name",name);
	msg->addString("Charter",charter);
	msg->addBOOL("ShowInList",show_in_list);
	msg->addUUID("InsigniaID",insignia);
	msg->addS32("MembershipFee",membership_fee);
	msg->addBOOL("OpenEnrollment",open_enrollment);
	msg->addBOOL("AllowPublish",allow_publish);
	msg->addBOOL("MaturePublish",mature_publish);

	gAgent.sendReliableMessage();
}

void LLGroupMgr::sendUpdateGroupInfo(const LLUUID& group_id)
{
	lldebugs << "LLGroupMgr::sendUpdateGroupInfo" << llendl;
	LLGroupMgrGroupData* group_datap = createGroupData(group_id);

	LLMessageSystem* msg = gMessageSystem;

	msg->newMessageFast(_PREHASH_UpdateGroupInfo);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID,gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID,gAgent.getSessionID());

	msg->nextBlockFast(_PREHASH_GroupData);
	msg->addUUIDFast(_PREHASH_GroupID,group_datap->getID());
	msg->addStringFast(_PREHASH_Charter,group_datap->mCharter);
	msg->addBOOLFast(_PREHASH_ShowInList,group_datap->mShowInList);
	msg->addUUIDFast(_PREHASH_InsigniaID,group_datap->mInsigniaID);
	msg->addS32Fast(_PREHASH_MembershipFee,group_datap->mMembershipFee);
	msg->addBOOLFast(_PREHASH_OpenEnrollment,group_datap->mOpenEnrollment);
	msg->addBOOLFast(_PREHASH_AllowPublish,group_datap->mAllowPublish);
	msg->addBOOLFast(_PREHASH_MaturePublish,group_datap->mMaturePublish);

	gAgent.sendReliableMessage();

	// Not expecting a response, so let anyone else watching know the data has changed.
	group_datap->mChanged = TRUE;
	notifyObservers(GC_PROPERTIES);
}

void LLGroupMgr::sendGroupRoleMemberChanges(const LLUUID& group_id)
{
	lldebugs << "LLGroupMgr::sendGroupRoleMemberChanges" << llendl;
	LLGroupMgrGroupData* group_datap = createGroupData(group_id);

	if (group_datap->mRoleMemberChanges.empty()) return;

	LLMessageSystem* msg = gMessageSystem;

	bool start_message = true;
	for (LLGroupMgrGroupData::change_map_t::const_iterator citer = group_datap->mRoleMemberChanges.begin();
		 citer != group_datap->mRoleMemberChanges.end(); ++citer)
	{
		if (start_message)
		{
			msg->newMessage("GroupRoleChanges");
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID,gAgent.getID());
			msg->addUUIDFast(_PREHASH_SessionID,gAgent.getSessionID());
			msg->addUUIDFast(_PREHASH_GroupID,group_id);
			start_message = false;
		}
		msg->nextBlock("RoleChange");
		msg->addUUID("RoleID",citer->second.mRole);
		msg->addUUID("MemberID",citer->second.mMember);
		msg->addU32("Change",(U32)citer->second.mChange);

		if (msg->isSendFullFast())
		{
			gAgent.sendReliableMessage();
			start_message = true;
		}
	}

	if (!start_message)
	{
		gAgent.sendReliableMessage();
	}

	group_datap->mRoleMemberChanges.clear();

	// Not expecting a response, so let anyone else watching know the data has changed.
	group_datap->mChanged = TRUE;
	notifyObservers(GC_ROLE_MEMBER_DATA);
}

//static
void LLGroupMgr::sendGroupMemberJoin(const LLUUID& group_id)
{
	LLMessageSystem *msg = gMessageSystem;

	msg->newMessageFast(_PREHASH_JoinGroupRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID() );
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_GroupData);
	msg->addUUIDFast(_PREHASH_GroupID, group_id);

	gAgent.sendReliableMessage();
}

// member_role_pairs is <member_id,role_id>
// static
void LLGroupMgr::sendGroupMemberInvites(const LLUUID& group_id, std::map<LLUUID,LLUUID>& member_role_pairs)
{
	bool start_message = true;
	LLMessageSystem* msg = gMessageSystem;

	for (std::map<LLUUID,LLUUID>::iterator it = member_role_pairs.begin();
		 it != member_role_pairs.end(); ++it)
	{
		if (start_message)
		{
			msg->newMessage("InviteGroupRequest");
			msg->nextBlock("AgentData");
			msg->addUUID("AgentID",gAgent.getID());
			msg->addUUID("SessionID",gAgent.getSessionID());
			msg->nextBlock("GroupData");
			msg->addUUID("GroupID",group_id);
			start_message = false;
		}

		msg->nextBlock("InviteData");
		msg->addUUID("InviteeID",(*it).first);
		msg->addUUID("RoleID",(*it).second);

		if (msg->isSendFull())
		{
			gAgent.sendReliableMessage();
			start_message = true;
		}
	}

	if (!start_message)
	{
		gAgent.sendReliableMessage();
	}
}

//static
void LLGroupMgr::sendGroupMemberEjects(const LLUUID& group_id,
									   std::vector<LLUUID>& member_ids)
{
	bool start_message = true;
	LLMessageSystem* msg = gMessageSystem;

	LLGroupMgrGroupData* group_datap = LLGroupMgr::getInstance()->getGroupData(group_id);
	if (!group_datap) return;

	for (std::vector<LLUUID>::iterator it = member_ids.begin();
		 it != member_ids.end(); ++it)
	{
		// Can't use 'eject' to leave a group.
		if ((*it) == gAgent.getID()) continue;

		// Make sure they are in the group, and we need the member data
		LLGroupMgrGroupData::member_list_t::iterator mit = group_datap->mMembers.find(*it);
		if (mit != group_datap->mMembers.end())
		{
			// Add them to the message
			if (start_message)
			{
				msg->newMessage("EjectGroupMemberRequest");
				msg->nextBlock("AgentData");
				msg->addUUID("AgentID",gAgent.getID());
				msg->addUUID("SessionID",gAgent.getSessionID());
				msg->nextBlock("GroupData");
				msg->addUUID("GroupID",group_id);
				start_message = false;
			}
			
			msg->nextBlock("EjectData");
			msg->addUUID("EjecteeID",(*it));

			if (msg->isSendFull())
			{
				gAgent.sendReliableMessage();
				start_message = true;
			}

			// Clean up groupmgr
			for (LLGroupMemberData::role_list_t::iterator rit = (*mit).second->roleBegin();
				 rit != (*mit).second->roleEnd(); ++rit)
			{
				if ((*rit).first.notNull())
				{
					(*rit).second->removeMember(*it);
				}
			}
			delete (*mit).second;
			group_datap->mMembers.erase(*it);
		}
	}

	if (!start_message)
	{
		gAgent.sendReliableMessage();
	}
}

void LLGroupMgr::sendGroupRoleChanges(const LLUUID& group_id)
{
	lldebugs << "LLGroupMgr::sendGroupRoleChanges" << llendl;
	LLGroupMgrGroupData* group_datap = getGroupData(group_id);

	if (group_datap && group_datap->pendingRoleChanges())
	{
		group_datap->sendRoleChanges();
	
		// Not expecting a response, so let anyone else watching know the data has changed.
		group_datap->mChanged = TRUE;
		notifyObservers(GC_ROLE_DATA);
	}
}

void LLGroupMgr::cancelGroupRoleChanges(const LLUUID& group_id)
{
	lldebugs << "LLGroupMgr::cancelGroupRoleChanges" << llendl;
	LLGroupMgrGroupData* group_datap = getGroupData(group_id);

	if (group_datap) group_datap->cancelRoleChanges();
}

//static
bool LLGroupMgr::parseRoleActions(const std::string& xml_filename)
{
	LLXMLNodePtr root;

	BOOL success = LLUICtrlFactory::getLayeredXMLNode(xml_filename, root);	
	
	if (!success || !root || !root->hasName( "role_actions" ))
	{
		llerrs << "Problem reading UI role_actions file: " << xml_filename << llendl;
		return false;
	}

	LLXMLNodeList role_list;

	root->getChildren("action_set", role_list, false);
	
	for (LLXMLNodeList::iterator role_iter = role_list.begin(); role_iter != role_list.end(); ++role_iter)
	{
		LLXMLNodePtr action_set = role_iter->second;

		LLRoleActionSet* role_action_set = new LLRoleActionSet();
		LLRoleAction* role_action_data = new LLRoleAction();
		
		// name=
		std::string action_set_name;
		if (action_set->getAttributeString("name", action_set_name))
		{
			lldebugs << "Loading action set " << action_set_name << llendl;
			role_action_data->mName = action_set_name;
		}
		else
		{
			llwarns << "Unable to parse action set with no name" << llendl;
			delete role_action_set;
			delete role_action_data;
			continue;
		}
		// description=
		std::string set_description;
		if (action_set->getAttributeString("description", set_description))
		{
			role_action_data->mDescription = set_description;
		}
		// long description=
		std::string set_longdescription;
		if (action_set->getAttributeString("longdescription", set_longdescription))
		{
			role_action_data->mLongDescription = set_longdescription;
		}

		// power mask=
		U64 set_power_mask = 0;

		LLXMLNodeList action_list;
		LLXMLNodeList::iterator action_iter;

		action_set->getChildren("action", action_list, false);

		for (action_iter = action_list.begin(); action_iter != action_list.end(); ++action_iter)
		{
			LLXMLNodePtr action = action_iter->second;

			LLRoleAction* role_action = new LLRoleAction();

			// name=
			std::string action_name;
			if (action->getAttributeString("name", action_name))
			{
				lldebugs << "Loading action " << action_name << llendl;
				role_action->mName = action_name;
			}
			else
			{
				llwarns << "Unable to parse action with no name" << llendl;
				delete role_action;
				continue;
			}
			// description=
			std::string description;
			if (action->getAttributeString("description", description))
			{
				role_action->mDescription = description;
			}
			// long description=
			std::string longdescription;
			if (action->getAttributeString("longdescription", longdescription))
			{
				role_action->mLongDescription = longdescription;
			}
			// description=
			S32 power_bit = 0;
			if (action->getAttributeS32("value", power_bit))
			{
				if (0 <= power_bit && power_bit < 64)
				{
					role_action->mPowerBit = 0x1LL << power_bit;
				}
			}

			set_power_mask |= role_action->mPowerBit;
	
			role_action_set->mActions.push_back(role_action);
		}

		role_action_data->mPowerBit = set_power_mask;
		role_action_set->mActionSetData = role_action_data;

		LLGroupMgr::getInstance()->mRoleActionSets.push_back(role_action_set);
	}
	return true;
}

// static
void LLGroupMgr::debugClearAllGroups(void*)
{
	LLGroupMgr::getInstance()->clearGroups();
	LLGroupMgr::parseRoleActions("role_actions.xml");
}


