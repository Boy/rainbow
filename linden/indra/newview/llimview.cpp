/** 
 * @file LLIMMgr.cpp
 * @brief Container for Instant Messaging
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

#include "llimview.h"

#include "llfontgl.h"
#include "llrect.h"
#include "llerror.h"
#include "llbutton.h"
#include "llhttpclient.h"
#include "llsdutil.h"
#include "llstring.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llcallingcard.h"
#include "llchat.h"
#include "llresmgr.h"
#include "llfloaterchat.h"
#include "llfloaterchatterbox.h"
#include "llfloaternewim.h"
#include "llfocusmgr.h"
#include "llhttpnode.h"
#include "llimpanel.h"
#include "llresizebar.h"
#include "lltabcontainer.h"
#include "llviewercontrol.h"
#include "llfloater.h"
#include "llmutelist.h"
#include "llresizehandle.h"
#include "llkeyboard.h"
#include "llui.h"
#include "llviewermenu.h"
#include "llcallingcard.h"
#include "lltoolbar.h"
#include "llviewermessage.h"
#include "llviewerwindow.h"
#include "llnotify.h"
#include "llviewerregion.h"

#include "llfirstuse.h"

//MK
extern BOOL RRenabled;
//mk

//
// Globals
//
LLIMMgr* gIMMgr = NULL;

//
// Statics
//
// *FIXME: make these all either UIStrings or Strings
static std::string sOnlyUserMessage;
static LLUIString sOfflineMessage;
static std::string sMutedMessage;
static LLUIString sInviteMessage;

std::map<std::string,std::string> LLFloaterIM::sEventStringsMap;
std::map<std::string,std::string> LLFloaterIM::sErrorStringsMap;
std::map<std::string,std::string> LLFloaterIM::sForceCloseSessionMap;

const EInstantMessage GROUP_DIALOG = IM_SESSION_GROUP_START;
const EInstantMessage DEFAULT_DIALOG = IM_NOTHING_SPECIAL;

//
// Helper Functions
//

// returns true if a should appear before b
//static BOOL group_dictionary_sort( LLGroupData* a, LLGroupData* b )
//{
//	return (LLStringUtil::compareDict( a->mName, b->mName ) < 0);
//}


// the other_participant_id is either an agent_id, a group_id, or an inventory
// folder item_id (collection of calling cards)

// static
LLUUID LLIMMgr::computeSessionID(
	EInstantMessage dialog,
	const LLUUID& other_participant_id)
{
	LLUUID session_id;
	if (IM_SESSION_GROUP_START == dialog)
	{
		// slam group session_id to the group_id (other_participant_id)
		session_id = other_participant_id;
	}
	else if (IM_SESSION_CONFERENCE_START == dialog)
	{
		session_id.generate();
	}
	else if (IM_SESSION_INVITE == dialog)
	{
		// use provided session id for invites
		session_id = other_participant_id;
	}
	else
	{
		LLUUID agent_id = gAgent.getID();
		if (other_participant_id == agent_id)
		{
			// if we try to send an IM to ourselves then the XOR would be null
			// so we just make the session_id the same as the agent_id
			session_id = agent_id;
		}
		else
		{
			// peer-to-peer or peer-to-asset session_id is the XOR
			session_id = other_participant_id ^ agent_id;
		}
	}
	return session_id;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// LLFloaterIM
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

LLFloaterIM::LLFloaterIM() 
{
	// autoresize=false is necessary to avoid resizing of the IM window whenever 
	// a session is opened or closed (it would otherwise resize the window to match
	// the size of the im-sesssion when they were created.  This happens in 
	// LLMultiFloater::resizeToContents() when called through LLMultiFloater::addFloater())
	this->mAutoResize = FALSE;
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_im.xml");
}

BOOL LLFloaterIM::postBuild()
{
	// IM session initiation warnings
	sOnlyUserMessage = getString("only_user_message");
	sOfflineMessage = getString("offline_message");
	sMutedMessage = getString("muted_message");

	sInviteMessage = getString("invite_message");

	if ( sErrorStringsMap.find("generic") == sErrorStringsMap.end() )
	{
		sErrorStringsMap["generic"] =
			getString("generic_request_error");
	}

	if ( sErrorStringsMap.find("unverified") ==
		 sErrorStringsMap.end() )
	{
		sErrorStringsMap["unverified"] =
			getString("insufficient_perms_error");
	}

	if ( sErrorStringsMap.end() ==
		 sErrorStringsMap.find("no_ability") )
	{
		sErrorStringsMap["no_ability"] =
			getString("no_ability_error");
	}

	if ( sErrorStringsMap.end() ==
		 sErrorStringsMap.find("muted") )
	{
		sErrorStringsMap["muted"] =
			getString("muted_error");
	}

	if ( sErrorStringsMap.end() ==
		 sErrorStringsMap.find("not_a_moderator") )
	{
		sErrorStringsMap["not_a_moderator"] =
			getString("not_a_mod_error");
	}

	if ( sErrorStringsMap.end() ==
		 sErrorStringsMap.find("does not exist") )
	{
		sErrorStringsMap["does not exist"] =
			getString("session_does_not_exist_error");
	}

	if ( sEventStringsMap.end() == sEventStringsMap.find("add") )
	{
		sEventStringsMap["add"] =
			getString("add_session_event");
	}

	if ( sEventStringsMap.end() == sEventStringsMap.find("message") )
	{
		sEventStringsMap["message"] =
			getString("message_session_event");
	}


	if ( sForceCloseSessionMap.end() ==
		 sForceCloseSessionMap.find("removed") )
	{
		sForceCloseSessionMap["removed"] =
			getString("removed_from_group");
	}

	if ( sForceCloseSessionMap.end() ==
		 sForceCloseSessionMap.find("no ability") )
	{
		sForceCloseSessionMap["no ability"] =
			getString("close_on_no_ability");
	}

	return TRUE;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLIMViewFriendObserver
//
// Bridge to suport knowing when the inventory has changed.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLIMViewFriendObserver : public LLFriendObserver
{
public:
	LLIMViewFriendObserver(LLIMMgr* tv) : mTV(tv) {}
	virtual ~LLIMViewFriendObserver() {}
	virtual void changed(U32 mask)
	{
		if(mask & (LLFriendObserver::ADD | LLFriendObserver::REMOVE | LLFriendObserver::ONLINE))
		{
			mTV->refresh();
		}
	}
protected:
	LLIMMgr* mTV;
};


class LLIMMgr::LLIMSessionInvite
{
public:
	LLIMSessionInvite(
		const LLUUID& session_id,
		const std::string& session_name,
		const LLUUID& caller_id,
		const std::string& caller_name,
		EInstantMessage type,
		EInvitationType inv_type,
		const std::string& session_handle,
		const std::string& notify_box,
		const std::string& session_uri) : 
		mSessionID(session_id),
		mSessionName(session_name),
		mCallerID(caller_id),
		mCallerName(caller_name),
		mType(type),
		mInvType(inv_type),
		mSessionHandle(session_handle),
		mNotifyBox(notify_box),
		mSessionURI(session_uri)
	{};

	LLUUID		mSessionID;
	std::string	mSessionName;
	LLUUID		mCallerID;
	std::string	mCallerName;
	EInstantMessage mType;
	EInvitationType mInvType;
	std::string	mSessionHandle;
	std::string	mNotifyBox;
	std::string	mSessionURI;
};


//
// Public Static Member Functions
//

// This is a helper function to determine what kind of im session
// should be used for the given agent.
// static
EInstantMessage LLIMMgr::defaultIMTypeForAgent(const LLUUID& agent_id)
{
	EInstantMessage type = IM_NOTHING_SPECIAL;
	if(is_agent_friend(agent_id))
	{
		if(LLAvatarTracker::instance().isBuddyOnline(agent_id))
		{
			type = IM_SESSION_CONFERENCE_START;
		}
	}
	return type;
}

// static
//void LLIMMgr::onPinButton(void*)
//{
//	BOOL state = gSavedSettings.getBOOL( "PinTalkViewOpen" );
//	gSavedSettings.setBOOL( "PinTalkViewOpen", !state );
//}

// static 
void LLIMMgr::toggle(void*)
{
	static BOOL return_to_mouselook = FALSE;

	// Hide the button and show the floater or vice versa.
	llassert( gIMMgr );
	BOOL old_state = gIMMgr->getFloaterOpen();
	
	// If we're in mouselook and we triggered the Talk View, we want to talk.
	if( gAgent.cameraMouselook() && old_state )
	{
		return_to_mouselook = TRUE;
		gAgent.changeCameraToDefault();
		return;
	}

	BOOL new_state = !old_state;

	LLFloaterChatterBox* floater_chatterbox = gIMMgr->getFloater();
	LLFloater* floater_current = floater_chatterbox->getActiveFloater();
	LLFloater* floater_new_im = floater_chatterbox->getFloaterNewIM();
	BOOL active_is_im = (floater_current && floater_current->getName() == "im_floater" || floater_current==floater_new_im);

	if (floater_chatterbox) {	// "2" for toggle

		if (LLFloaterChatterBox::instanceVisible(LLSD())) 
		{
			BOOL has_focus = gFocusMgr.childHasKeyboardFocus(floater_chatterbox);

			if (active_is_im && has_focus)
			{
				// toggling away from an im-panel (or with no available im panel) => close
				new_state = FALSE;
			}
			else
			{
				// stay open, but switch to an IM panel or just activate
				new_state = TRUE;
			}
		}
		else 
		{
			new_state = TRUE;
		}
	}	

	if (new_state)
	{
		// ...making visible
		if ( gAgent.cameraMouselook() )
		{
			return_to_mouselook = TRUE;
			gAgent.changeCameraToDefault();
		}
	}
	else
	{
		// ...hiding
		if ( gAgent.cameraThirdPerson() && return_to_mouselook )
		{
			gAgent.changeCameraToMouselook();
		}
		return_to_mouselook = FALSE;
	}

	gIMMgr->setFloaterOpen( new_state );
}

//
// Member Functions
//

LLIMMgr::LLIMMgr() :
	mFriendObserver(NULL),
	mIMReceived(FALSE)
{
	mFriendObserver = new LLIMViewFriendObserver(this);
	LLAvatarTracker::instance().addObserver(mFriendObserver);

	// *HACK: use floater to initialize string constants from xml file
	// then delete it right away
	LLFloaterIM* dummy_floater = new LLFloaterIM();
	delete dummy_floater;

	mPendingInvitations = LLSD::emptyMap();
	mPendingAgentListUpdates = LLSD::emptyMap();
}

LLIMMgr::~LLIMMgr()
{
	LLAvatarTracker::instance().removeObserver(mFriendObserver);
	delete mFriendObserver;
	// Children all cleaned up by default view destructor.
}

// Add a message to a session. 
void LLIMMgr::addMessage(
	const LLUUID& session_id,
	const LLUUID& target_id,
	const std::string& from,
	const std::string& msg,
	const std::string& session_name,
	EInstantMessage dialog,
	U32 parent_estate_id,
	const LLUUID& region_id,
	const LLVector3& position,
	bool link_name) // If this is true, then we insert the name and link it to a profile
{
	LLUUID other_participant_id = target_id;

	// don't process muted IMs
	if (LLMuteList::getInstance()->isMuted(
			other_participant_id,
			LLMute::flagTextChat) && !LLMuteList::getInstance()->isLinden(from))
	{
		return;
	}

	//not sure why...but if it is from ourselves we set the target_id
	//to be NULL
	if( other_participant_id == gAgent.getID() )
	{
		other_participant_id = LLUUID::null;
	}

	LLFloaterIMPanel* floater;
	LLUUID new_session_id = session_id;
	if (new_session_id.isNull())
	{
		//no session ID...compute new one
		new_session_id = computeSessionID(dialog, other_participant_id);
	}
	floater = findFloaterBySession(new_session_id);
	if (!floater)
	{
		floater = findFloaterBySession(other_participant_id);
		if (floater)
		{
			llinfos << "found the IM session " << session_id 
				<< " by participant " << other_participant_id << llendl;
		}
	}

	// create IM window as necessary
	if(!floater)
	{
		std::string name = from;
		if(!session_name.empty() && session_name.size()>1)
		{
			name = session_name;
		}

		
		floater = createFloater(
			new_session_id,
			other_participant_id,
			name,
			dialog,
			FALSE);

		// When we get a new IM, and if you are a god, display a bit
		// of information about the source. This is to help liaisons
		// when answering questions.
		if(gAgent.isGodlike())
		{
			// *TODO:translate (low priority, god ability)
			std::ostringstream bonus_info;
			bonus_info << "*** parent estate: "
				<< parent_estate_id
				<< ((parent_estate_id == 1) ? ", mainland" : "")
				<< ((parent_estate_id == 5) ? ", teen" : "");

			// once we have web-services (or something) which returns
			// information about a region id, we can print this out
			// and even have it link to map-teleport or something.
			//<< "*** region_id: " << region_id << std::endl
			//<< "*** position: " << position << std::endl;

			floater->addHistoryLine(bonus_info.str(), gSavedSettings.getColor4("SystemChatColor"));
		}

		make_ui_sound("UISndNewIncomingIMSession");
	}

	// now add message to floater
	bool is_from_system = target_id.isNull() || (from == SYSTEM_FROM);
	const LLColor4& color = ( is_from_system ? 
							  gSavedSettings.getColor4("SystemChatColor") : 
							  gSavedSettings.getColor("IMChatColor"));
	if ( !link_name )
	{
		floater->addHistoryLine(msg,color); // No name to prepend, so just add the message normally
	}
	else
	{
		floater->addHistoryLine(msg, color, true, other_participant_id, from); // Insert linked name to front of message
	}

	LLFloaterChatterBox* chat_floater = LLFloaterChatterBox::getInstance(LLSD());

	if( !chat_floater->getVisible() && !floater->getVisible())
	{
		//if the IM window is not open and the floater is not visible (i.e. not torn off)
		LLFloater* previouslyActiveFloater = chat_floater->getActiveFloater();

		// select the newly added floater (or the floater with the new line added to it).
		// it should be there.
		chat_floater->selectFloater(floater);

		//there was a previously unseen IM, make that old tab flashing
		//it is assumed that the most recently unseen IM tab is the one current selected/active
		if ( previouslyActiveFloater && getIMReceived() )
		{
			chat_floater->setFloaterFlashing(previouslyActiveFloater, TRUE);
		}

		//notify of a new IM
		notifyNewIM();
	}
}

void LLIMMgr::addSystemMessage(const LLUUID& session_id, const std::string& message_name, const LLStringUtil::format_map_t& args)
{
	LLUIString message;
	
	// null session id means near me (chat history)
	if (session_id.isNull())
	{
		LLFloaterChat* floaterp = LLFloaterChat::getInstance();

		message = floaterp->getString(message_name);
		message.setArgList(args);

		LLChat chat(message);
		chat.mSourceType = CHAT_SOURCE_SYSTEM;
		LLFloaterChat::getInstance()->addChatHistory(chat);
	}
	else // going to IM session
	{
		LLFloaterIMPanel* floaterp = findFloaterBySession(session_id);
		if (floaterp)
		{
			message = floaterp->getString(message_name);
			message.setArgList(args);

			gIMMgr->addMessage(session_id, LLUUID::null, SYSTEM_FROM, message.getString());
		}
	}
}

void LLIMMgr::notifyNewIM()
{
	if(!gIMMgr->getFloaterOpen())
	{
		mIMReceived = TRUE;
	}
}

void LLIMMgr::clearNewIMNotification()
{
	mIMReceived = FALSE;
}

BOOL LLIMMgr::getIMReceived() const
{
	return mIMReceived;
}

// This method returns TRUE if the local viewer has a session
// currently open keyed to the uuid. 
BOOL LLIMMgr::isIMSessionOpen(const LLUUID& uuid)
{
	LLFloaterIMPanel* floater = findFloaterBySession(uuid);
	if(floater) return TRUE;
	return FALSE;
}

LLUUID LLIMMgr::addP2PSession(const std::string& name,
							const LLUUID& other_participant_id,
							const std::string& voice_session_handle,
							const std::string& caller_uri)
{
	LLUUID session_id = addSession(name, IM_NOTHING_SPECIAL, other_participant_id);

	LLFloaterIMPanel* floater = findFloaterBySession(session_id);
	if(floater)
	{
		LLVoiceChannelP2P* voice_channelp = (LLVoiceChannelP2P*)floater->getVoiceChannel();
		voice_channelp->setSessionHandle(voice_session_handle, caller_uri);		
	}

	return session_id;
}

// This adds a session to the talk view. The name is the local name of
// the session, dialog specifies the type of session. If the session
// exists, it is brought forward.  Specifying id = NULL results in an
// im session to everyone. Returns the uuid of the session.
LLUUID LLIMMgr::addSession(
	const std::string& name,
	EInstantMessage dialog,
	const LLUUID& other_participant_id)
{
	LLUUID session_id = computeSessionID(dialog, other_participant_id);

	LLFloaterIMPanel* floater = findFloaterBySession(session_id);
	if(!floater)
	{
		LLDynamicArray<LLUUID> ids;
		ids.put(other_participant_id);

		floater = createFloater(
			session_id,
			other_participant_id,
			name,
			ids,
			dialog,
			TRUE);

		noteOfflineUsers(floater, ids);
		LLFloaterChatterBox::showInstance(session_id);

		// Only warn for regular IMs - not group IMs
		if( dialog == IM_NOTHING_SPECIAL )
		{
			noteMutedUsers(floater, ids);
		}
		LLFloaterChatterBox::getInstance(LLSD())->showFloater(floater);
	}
	else
	{
		floater->open();
	}
	//mTabContainer->selectTabPanel(panel);
	floater->setInputFocus(TRUE);
	return floater->getSessionID();
}

// Adds a session using the given session_id.  If the session already exists 
// the dialog type is assumed correct. Returns the uuid of the session.
LLUUID LLIMMgr::addSession(
	const std::string& name,
	EInstantMessage dialog,
	const LLUUID& other_participant_id,
	const LLDynamicArray<LLUUID>& ids)
{
	if (0 == ids.getLength())
	{
		return LLUUID::null;
	}

	LLUUID session_id = computeSessionID(
		dialog,
		other_participant_id);

	LLFloaterIMPanel* floater = findFloaterBySession(session_id);
	if(!floater)
	{
		// On creation, use the first element of ids as the
		// "other_participant_id"
		floater = createFloater(
			session_id,
			other_participant_id,
			name,
			ids,
			dialog,
			TRUE);

		if ( !floater ) return LLUUID::null;

		noteOfflineUsers(floater, ids);
		LLFloaterChatterBox::showInstance(session_id);

		// Only warn for regular IMs - not group IMs
		if( dialog == IM_NOTHING_SPECIAL )
		{
			noteMutedUsers(floater, ids);
		}
	}
	else
	{
		floater->open();
	}
	//mTabContainer->selectTabPanel(panel);
	floater->setInputFocus(TRUE);
	return floater->getSessionID();
}

// This removes the panel referenced by the uuid, and then restores
// internal consistency. The internal pointer is not deleted.
void LLIMMgr::removeSession(const LLUUID& session_id)
{
	LLFloaterIMPanel* floater = findFloaterBySession(session_id);
	if(floater)
	{
		mFloaters.erase(floater->getHandle());
		LLFloaterChatterBox::getInstance(LLSD())->removeFloater(floater);
		//mTabContainer->removeTabPanel(floater);

		clearPendingInviation(session_id);
		clearPendingAgentListUpdates(session_id);
	}
}

void LLIMMgr::inviteToSession(
	const LLUUID& session_id, 
	const std::string& session_name, 
	const LLUUID& caller_id, 
	const std::string& caller_name,
	EInstantMessage type,
	EInvitationType inv_type,
	const std::string& session_handle,
	const std::string& session_uri)
{
	//ignore invites from muted residents
	if (LLMuteList::getInstance()->isMuted(caller_id))
	{
		return;
	}

	std::string notify_box_type;

	BOOL ad_hoc_invite = FALSE;
	if(type == IM_SESSION_P2P_INVITE)
	{
		//P2P is different...they only have voice invitations
		notify_box_type = "VoiceInviteP2P";
	}
	else if ( gAgent.isInGroup(session_id) )
	{
		//only really old school groups have voice invitations
		notify_box_type = "VoiceInviteGroup";
	}
	else if ( inv_type == INVITATION_TYPE_VOICE )
	{
		//else it's an ad-hoc
		//and a voice ad-hoc
		notify_box_type = "VoiceInviteAdHoc";
		ad_hoc_invite = TRUE;
	}
	else if ( inv_type == INVITATION_TYPE_IMMEDIATE )
	{
		notify_box_type = "InviteAdHoc";
		ad_hoc_invite = TRUE;
	}

	LLIMSessionInvite* invite = new LLIMSessionInvite(
		session_id,
		session_name,
		caller_id,
		caller_name,
		type,
		inv_type,
		session_handle,
		notify_box_type,
		session_uri);
	
	LLVoiceChannel* channelp = LLVoiceChannel::getChannelByID(session_id);
	if (channelp && channelp->callStarted())
	{
		// you have already started a call to the other user, so just accept the invite
		inviteUserResponse(0, invite); // inviteUserResponse deletes
		return;
	}

	if (type == IM_SESSION_P2P_INVITE || ad_hoc_invite)
	{
		// is the inviter a friend?
		if (LLAvatarTracker::instance().getBuddyInfo(caller_id) == NULL)
		{
			// if not, and we are ignoring voice invites from non-friends
			// then silently decline
			if (gSavedSettings.getBOOL("VoiceCallsFriendsOnly"))
			{
				// invite not from a friend, so decline
				inviteUserResponse(1, invite); // inviteUserResponse deletes
				return;
			}
		}
	}

	if ( !mPendingInvitations.has(session_id.asString()) )
	{
		if (caller_name.empty())
		{
			gCacheName->get(caller_id, FALSE, onInviteNameLookup, invite);
		}
		else
		{
			LLStringUtil::format_map_t args;
			args["[NAME]"] = caller_name;
			args["[GROUP]"] = session_name;

			LLNotifyBox::showXml(notify_box_type, 
					     args, 
					     inviteUserResponse, 
					     (void*)invite); // inviteUserResponse deletes

		}
		mPendingInvitations[session_id.asString()] = LLSD();
	}
	else
	{
		delete invite;
	}
}

//static 
void LLIMMgr::onInviteNameLookup(const LLUUID& id, const std::string& first, const std::string& last, BOOL is_group, void* userdata)
{
	LLIMSessionInvite* invite = (LLIMSessionInvite*)userdata;

	invite->mCallerName = first + " " + last;
	invite->mSessionName = invite->mCallerName;

	LLStringUtil::format_map_t args;
	args["[NAME]"] = invite->mCallerName;

	LLNotifyBox::showXml(
		invite->mNotifyBox,
		args, 
		inviteUserResponse, 
		(void*)invite);
}

class LLViewerChatterBoxInvitationAcceptResponder :
	public LLHTTPClient::Responder
{
public:
	LLViewerChatterBoxInvitationAcceptResponder(
		const LLUUID& session_id,
		LLIMMgr::EInvitationType invitation_type)
	{
		mSessionID = session_id;
		mInvitiationType = invitation_type;
	}

	void result(const LLSD& content)
	{
		if ( gIMMgr)
		{
			LLFloaterIMPanel* floaterp =
				gIMMgr->findFloaterBySession(mSessionID);

			if (floaterp)
			{
				//we've accepted our invitation
				//and received a list of agents that were
				//currently in the session when the reply was sent
				//to us.  Now, it is possible that there were some agents
				//to slip in/out between when that message was sent to us
				//and now.

				//the agent list updates we've received have been
				//accurate from the time we were added to the session
				//but unfortunately, our base that we are receiving here
				//may not be the most up to date.  It was accurate at
				//some point in time though.
				floaterp->setSpeakers(content);

				//we now have our base of users in the session
				//that was accurate at some point, but maybe not now
				//so now we apply all of the udpates we've received
				//in case of race conditions
				floaterp->updateSpeakersList(
					gIMMgr->getPendingAgentListUpdates(mSessionID));

				if ( mInvitiationType == LLIMMgr::INVITATION_TYPE_VOICE )
				{
					floaterp->requestAutoConnect();
					LLFloaterIMPanel::onClickStartCall(floaterp);
					// always open IM window when connecting to voice
					LLFloaterChatterBox::showInstance(TRUE);
				}
				else if ( mInvitiationType == LLIMMgr::INVITATION_TYPE_IMMEDIATE )
				{
					LLFloaterChatterBox::showInstance(TRUE);
				}
			}

			gIMMgr->clearPendingAgentListUpdates(mSessionID);
			gIMMgr->clearPendingInviation(mSessionID);
		}
	}

	void error(U32 statusNum, const std::string& reason)
	{		
		//throw something back to the viewer here?
		if ( gIMMgr )
		{
			gIMMgr->clearPendingAgentListUpdates(mSessionID);
			gIMMgr->clearPendingInviation(mSessionID);

			LLFloaterIMPanel* floaterp =
				gIMMgr->findFloaterBySession(mSessionID);

			if ( floaterp )
			{
				if ( 404 == statusNum )
				{
					std::string error_string;
					error_string = "does not exist";

					floaterp->showSessionStartError(
						error_string);
				}
			}
		}
	}

private:
	LLUUID mSessionID;
	LLIMMgr::EInvitationType mInvitiationType;
};

//static
void LLIMMgr::inviteUserResponse(S32 option, void* user_data)
{
	LLIMSessionInvite* invitep = (LLIMSessionInvite*)user_data;

	switch(option) 
	{
	case 0: // accept
		{
			if (invitep->mType == IM_SESSION_P2P_INVITE)
			{
				// create a normal IM session
				invitep->mSessionID = gIMMgr->addP2PSession(
					invitep->mSessionName,
					invitep->mCallerID,
					invitep->mSessionHandle, 
					invitep->mSessionURI );

				LLFloaterIMPanel* im_floater =
					gIMMgr->findFloaterBySession(
						invitep->mSessionID);
				if (im_floater)
				{
					im_floater->requestAutoConnect();
					LLFloaterIMPanel::onClickStartCall(im_floater);
					// always open IM window when connecting to voice
					LLFloaterChatterBox::showInstance(invitep->mSessionID);
				}

				gIMMgr->clearPendingAgentListUpdates(invitep->mSessionID);
				gIMMgr->clearPendingInviation(invitep->mSessionID);
			}
			else
			{
				gIMMgr->addSession(
					invitep->mSessionName,
					invitep->mType,
					invitep->mSessionID);

				std::string url = gAgent.getRegion()->getCapability(
					"ChatSessionRequest");

				LLSD data;
				data["method"] = "accept invitation";
				data["session-id"] = invitep->mSessionID;
				LLHTTPClient::post(
					url,
					data,
					new LLViewerChatterBoxInvitationAcceptResponder(
						invitep->mSessionID,
						invitep->mInvType));
			}
		}
		break;
	case 2: // mute (also implies ignore, so this falls through to the "ignore" case below)
	{
		// mute the sender of this invite
		if (!LLMuteList::getInstance()->isMuted(invitep->mCallerID))
		{
			LLMute mute(invitep->mCallerID, invitep->mCallerName, LLMute::AGENT);
			LLMuteList::getInstance()->add(mute);
		}
	}
	/* FALLTHROUGH */
	
	case 1: // decline
	{
		if (invitep->mType == IM_SESSION_P2P_INVITE)
		{
			if(gVoiceClient)
			{
				gVoiceClient->declineInvite(invitep->mSessionHandle);
			}
		}
		else
		{
			std::string url = gAgent.getRegion()->getCapability(
				"ChatSessionRequest");

			LLSD data;
			data["method"] = "decline invitation";
			data["session-id"] = invitep->mSessionID;
			LLHTTPClient::post(
				url,
				data,
				NULL);				
		}
	}

	gIMMgr->clearPendingAgentListUpdates(invitep->mSessionID);
	gIMMgr->clearPendingInviation(invitep->mSessionID);
	break;
	}
	
	delete invitep;
}

//returns true if a should appear before b
static BOOL group_dictionary_sort( LLGroupData* a, LLGroupData* b )
{
	return (LLStringUtil::compareDict( a->mName, b->mName ) < 0);
}

void LLIMMgr::refresh()
{
	LLFloaterNewIM* floaterimp = LLFloaterChatterBox::getInstance(LLSD())->getFloaterNewIM();

	if (!floaterimp) return;

	S32 old_scroll_pos = floaterimp->getScrollPos();
	floaterimp->clearAllTargets();

	// build a list of groups.
	LLLinkedList<LLGroupData> group_list( group_dictionary_sort );

	LLGroupData* group;
	S32 count = gAgent.mGroups.count();
	S32 i;
	// read/sort groups on the first pass.
	for(i = 0; i < count; ++i)
	{
		group = &(gAgent.mGroups.get(i));
		group_list.addDataSorted( group );
	}

	// add groups to the floater on the second pass.
	for(group = group_list.getFirstData();
		group;
		group = group_list.getNextData())
	{
		floaterimp->addGroup(group->mID, (void*)(&GROUP_DIALOG), TRUE, FALSE);
	}

	// build a set of buddies in the current buddy list.
	LLCollectAllBuddies collector;
	LLAvatarTracker::instance().applyFunctor(collector);
	LLCollectAllBuddies::buddy_map_t::iterator it;
	LLCollectAllBuddies::buddy_map_t::iterator end;
	it = collector.mOnline.begin();
	end = collector.mOnline.end();
	for( ; it != end; ++it)
	{
		floaterimp->addAgent((*it).second, (void*)(&DEFAULT_DIALOG), TRUE);
	}
	it = collector.mOffline.begin();
	end = collector.mOffline.end();
	for( ; it != end; ++it)
	{
		floaterimp->addAgent((*it).second, (void*)(&DEFAULT_DIALOG), FALSE);
	}

	floaterimp->setScrollPos( old_scroll_pos );
}

void LLIMMgr::setFloaterOpen(BOOL set_open)
{
	if (set_open)
	{
		LLFloaterChatterBox::showInstance();

		LLFloaterChatterBox* floater_chatterbox = getFloater();
		LLFloater* floater_current = floater_chatterbox->getActiveFloater();
		LLFloater* floater_new_im = floater_chatterbox->getFloaterNewIM();
		BOOL active_is_im = (floater_current && floater_current->getName() == "im_floater" || floater_current==floater_new_im);
		LLFloater* floater_to_show = active_is_im ? floater_current : NULL;
		LLTabContainer*  tabs = floater_chatterbox->getChild<LLTabContainer>("chatterbox_tabs");

		for (S32 i = 0; i < floater_chatterbox->getFloaterCount(); i++)
		{
			LLPanel* panelp = tabs->getPanelByIndex(i);
			if (panelp->getName() == "im_floater")
			{
				// only LLFloaterIMPanels are called "im_floater"
				LLFloaterIMPanel* im_floaterp = (LLFloaterIMPanel*)panelp;
				if(im_floaterp)
				{
					if (!floater_to_show || floater_chatterbox->isFloaterFlashing(im_floaterp))
					{
						floater_to_show = im_floaterp; // the first im_floater or the flashing im_floater
					}
				}
			}
		}

		if (floater_to_show) 
		{
			floater_to_show->open();
		}
		else 
		if (floater_chatterbox && floater_chatterbox->getFloaterNewIM())
		{
			floater_chatterbox->getFloaterNewIM()->open();
		}
	}
	else
	{
		LLFloaterChatterBox::hideInstance();
	}
}


BOOL LLIMMgr::getFloaterOpen()
{
	return LLFloaterChatterBox::instanceVisible(LLSD());
}
 
void LLIMMgr::disconnectAllSessions()
{
	LLFloaterIMPanel* floater = NULL;
	std::set<LLHandle<LLFloater> >::iterator handle_it;
	for(handle_it = mFloaters.begin();
		handle_it != mFloaters.end();
		)
	{
		floater = (LLFloaterIMPanel*)handle_it->get();

		// MUST do this BEFORE calling floater->onClose() because that may remove the item from the set, causing the subsequent increment to crash.
		++handle_it;

		if (floater)
		{
			floater->setEnabled(FALSE);
			floater->close(TRUE);
		}
	}
}


// This method returns the im panel corresponding to the uuid
// provided. The uuid can either be a session id or an agent
// id. Returns NULL if there is no matching panel.
LLFloaterIMPanel* LLIMMgr::findFloaterBySession(const LLUUID& session_id)
{
	LLFloaterIMPanel* rv = NULL;
	std::set<LLHandle<LLFloater> >::iterator handle_it;
	for(handle_it = mFloaters.begin();
		handle_it != mFloaters.end();
		++handle_it)
	{
		rv = (LLFloaterIMPanel*)handle_it->get();
		if(rv && session_id == rv->getSessionID())
		{
			break;
		}
		rv = NULL;
	}
	return rv;
}


BOOL LLIMMgr::hasSession(const LLUUID& session_id)
{
	return (findFloaterBySession(session_id) != NULL);
}

void LLIMMgr::clearPendingInviation(const LLUUID& session_id)
{
	if ( mPendingInvitations.has(session_id.asString()) )
	{
		mPendingInvitations.erase(session_id.asString());
	}
}

LLSD LLIMMgr::getPendingAgentListUpdates(const LLUUID& session_id)
{
	if ( mPendingAgentListUpdates.has(session_id.asString()) )
	{
		return mPendingAgentListUpdates[session_id.asString()];
	}
	else
	{
		return LLSD();
	}
}

void LLIMMgr::addPendingAgentListUpdates(
	const LLUUID& session_id,
	const LLSD& updates)
{
	LLSD::map_const_iterator iter;

	if ( !mPendingAgentListUpdates.has(session_id.asString()) )
	{
		//this is a new agent list update for this session
		mPendingAgentListUpdates[session_id.asString()] = LLSD::emptyMap();
	}

	if (
		updates.has("agent_updates") &&
		updates["agent_updates"].isMap() &&
		updates.has("updates") &&
		updates["updates"].isMap() )
	{
		//new school update
		LLSD update_types = LLSD::emptyArray();
		LLSD::array_iterator array_iter;

		update_types.append("agent_updates");
		update_types.append("updates");

		for (
			array_iter = update_types.beginArray();
			array_iter != update_types.endArray();
			++array_iter)
		{
			//we only want to include the last update for a given agent
			for (
				iter = updates[array_iter->asString()].beginMap();
				iter != updates[array_iter->asString()].endMap();
				++iter)
			{
				mPendingAgentListUpdates[session_id.asString()][array_iter->asString()][iter->first] =
					iter->second;
			}
		}
	}
	else if (
		updates.has("updates") &&
		updates["updates"].isMap() )
	{
		//old school update where the SD contained just mappings
		//of agent_id -> "LEAVE"/"ENTER"

		//only want to keep last update for each agent
		for (
			iter = updates["updates"].beginMap();
			iter != updates["updates"].endMap();
			++iter)
		{
			mPendingAgentListUpdates[session_id.asString()]["updates"][iter->first] =
				iter->second;
		}
	}
}

void LLIMMgr::clearPendingAgentListUpdates(const LLUUID& session_id)
{
	if ( mPendingAgentListUpdates.has(session_id.asString()) )
	{
		mPendingAgentListUpdates.erase(session_id.asString());
	}
}

// create a floater and update internal representation for
// consistency. Returns the pointer, caller (the class instance since
// it is a private method) is not responsible for deleting the
// pointer.  Add the floater to this but do not select it.
LLFloaterIMPanel* LLIMMgr::createFloater(
	const LLUUID& session_id,
	const LLUUID& other_participant_id,
	const std::string& session_label,
	EInstantMessage dialog,
	BOOL user_initiated)
{
	if (session_id.isNull())
	{
		llwarns << "Creating LLFloaterIMPanel with null session ID" << llendl;
	}

	llinfos << "LLIMMgr::createFloater: from " << other_participant_id 
			<< " in session " << session_id << llendl;
	LLFloaterIMPanel* floater = new LLFloaterIMPanel(session_label,
													 session_id,
													 other_participant_id,
													 dialog);
	LLTabContainer::eInsertionPoint i_pt = user_initiated ? LLTabContainer::RIGHT_OF_CURRENT : LLTabContainer::END;
	LLFloaterChatterBox::getInstance(LLSD())->addFloater(floater, FALSE, i_pt);
	mFloaters.insert(floater->getHandle());
	return floater;
}

LLFloaterIMPanel* LLIMMgr::createFloater(
	const LLUUID& session_id,
	const LLUUID& other_participant_id,
	const std::string& session_label,
	const LLDynamicArray<LLUUID>& ids,
	EInstantMessage dialog,
	BOOL user_initiated)
{
	if (session_id.isNull())
	{
		llwarns << "Creating LLFloaterIMPanel with null session ID" << llendl;
	}

	llinfos << "LLIMMgr::createFloater: from " << other_participant_id 
			<< " in session " << session_id << llendl;
	LLFloaterIMPanel* floater = new LLFloaterIMPanel(session_label,
													 session_id,
													 other_participant_id,
													 ids,
													 dialog);
	LLTabContainer::eInsertionPoint i_pt = user_initiated ? LLTabContainer::RIGHT_OF_CURRENT : LLTabContainer::END;
	LLFloaterChatterBox::getInstance(LLSD())->addFloater(floater, FALSE, i_pt);
	mFloaters.insert(floater->getHandle());
	return floater;
}

void LLIMMgr::noteOfflineUsers(
	LLFloaterIMPanel* floater,
	const LLDynamicArray<LLUUID>& ids)
{
	S32 count = ids.count();
	if(count == 0)
	{
		floater->addHistoryLine(sOnlyUserMessage, gSavedSettings.getColor4("SystemChatColor"));
	}
	else
	{
		const LLRelationship* info = NULL;
		LLAvatarTracker& at = LLAvatarTracker::instance();
		for(S32 i = 0; i < count; ++i)
		{
			info = at.getBuddyInfo(ids.get(i));
			std::string first, last;
			if(info && !info->isOnline()
			   && gCacheName->getName(ids.get(i), first, last))
			{
				LLUIString offline = sOfflineMessage;
				offline.setArg("[FIRST]", first);
				offline.setArg("[LAST]", last);
				floater->addHistoryLine(offline, gSavedSettings.getColor4("SystemChatColor"));
			}
		}
	}
}

void LLIMMgr::noteMutedUsers(LLFloaterIMPanel* floater,
								  const LLDynamicArray<LLUUID>& ids)
{
	// Don't do this if we don't have a mute list.
	LLMuteList *ml = LLMuteList::getInstance();
	if( !ml )
	{
		return;
	}

	S32 count = ids.count();
	if(count > 0)
	{
		for(S32 i = 0; i < count; ++i)
		{
			if( ml->isMuted(ids.get(i)) )
			{
				LLUIString muted = sMutedMessage;
				floater->addHistoryLine(muted);
				break;
			}
		}
	}
}

void LLIMMgr::processIMTypingStart(const LLIMInfo* im_info)
{
	processIMTypingCore(im_info, TRUE);
}

void LLIMMgr::processIMTypingStop(const LLIMInfo* im_info)
{
	processIMTypingCore(im_info, FALSE);
}

void LLIMMgr::processIMTypingCore(const LLIMInfo* im_info, BOOL typing)
{
	LLUUID session_id = computeSessionID(im_info->mIMType, im_info->mFromID);
	LLFloaterIMPanel* floater = findFloaterBySession(session_id);
	if (floater)
	{
		floater->processIMTyping(im_info, typing);
	}
}

void LLIMMgr::updateFloaterSessionID(
	const LLUUID& old_session_id,
	const LLUUID& new_session_id)
{
	LLFloaterIMPanel* floater = findFloaterBySession(old_session_id);
	if (floater)
	{
		floater->sessionInitReplyReceived(new_session_id);
	}
}

LLFloaterChatterBox* LLIMMgr::getFloater()
{ 
	return LLFloaterChatterBox::getInstance(LLSD()); 
}

class LLViewerChatterBoxSessionStartReply : public LLHTTPNode
{
public:
	virtual void describe(Description& desc) const
	{
		desc.shortInfo("Used for receiving a reply to a request to initialize an ChatterBox session");
		desc.postAPI();
		desc.input(
			"{\"client_session_id\": UUID, \"session_id\": UUID, \"success\" boolean, \"reason\": string");
		desc.source(__FILE__, __LINE__);
	}

	virtual void post(ResponsePtr response,
					  const LLSD& context,
					  const LLSD& input) const
	{
		LLSD body;
		LLUUID temp_session_id;
		LLUUID session_id;
		bool success;

		body = input["body"];
		success = body["success"].asBoolean();
		temp_session_id = body["temp_session_id"].asUUID();

		if ( success )
		{
			session_id = body["session_id"].asUUID();
			gIMMgr->updateFloaterSessionID(
				temp_session_id,
				session_id);
			LLFloaterIMPanel* floaterp = gIMMgr->findFloaterBySession(session_id);
			if (floaterp)
			{
				floaterp->setSpeakers(body);

				//apply updates we've possibly received previously
				floaterp->updateSpeakersList(
					gIMMgr->getPendingAgentListUpdates(session_id));

				if ( body.has("session_info") )
				{
					floaterp->processSessionUpdate(body["session_info"]);
				}

				//aply updates we've possibly received previously
				floaterp->updateSpeakersList(
					gIMMgr->getPendingAgentListUpdates(session_id));
			}
			gIMMgr->clearPendingAgentListUpdates(session_id);
		}
		else
		{
			//throw an error dialog and close the temp session's
			//floater
			LLFloaterIMPanel* floater = 
				gIMMgr->findFloaterBySession(temp_session_id);

			if ( floater )
			{
				floater->showSessionStartError(body["error"].asString());
			}
		}

		gIMMgr->clearPendingAgentListUpdates(session_id);
	}
};

class LLViewerChatterBoxSessionEventReply : public LLHTTPNode
{
public:
	virtual void describe(Description& desc) const
	{
		desc.shortInfo("Used for receiving a reply to a ChatterBox session event");
		desc.postAPI();
		desc.input(
			"{\"event\": string, \"reason\": string, \"success\": boolean, \"session_id\": UUID");
		desc.source(__FILE__, __LINE__);
	}

	virtual void post(ResponsePtr response,
					  const LLSD& context,
					  const LLSD& input) const
	{
		LLUUID session_id;
		bool success;

		LLSD body = input["body"];
		success = body["success"].asBoolean();
		session_id = body["session_id"].asUUID();

		if ( !success )
		{
			//throw an error dialog
			LLFloaterIMPanel* floater = 
				gIMMgr->findFloaterBySession(session_id);

			if (floater)
			{
				floater->showSessionEventError(
					body["event"].asString(),
					body["error"].asString());
			}
		}
	}
};

class LLViewerForceCloseChatterBoxSession: public LLHTTPNode
{
public:
	virtual void post(ResponsePtr response,
					  const LLSD& context,
					  const LLSD& input) const
	{
		LLUUID session_id;
		std::string reason;

		session_id = input["body"]["session_id"].asUUID();
		reason = input["body"]["reason"].asString();

		LLFloaterIMPanel* floater =
			gIMMgr ->findFloaterBySession(session_id);

		if ( floater )
		{
			floater->showSessionForceClose(reason);
		}
	}
};

class LLViewerChatterBoxSessionAgentListUpdates : public LLHTTPNode
{
public:
	virtual void post(
		ResponsePtr responder,
		const LLSD& context,
		const LLSD& input) const
	{
		LLFloaterIMPanel* floaterp = gIMMgr->findFloaterBySession(input["body"]["session_id"].asUUID());
		if (floaterp)
		{
			floaterp->updateSpeakersList(
				input["body"]);
		}
		else
		{
			//we don't have a floater yet..something went wrong
			//we are probably receiving an update here before
			//a start or an acceptance of an invitation.  Race condition.
			gIMMgr->addPendingAgentListUpdates(
				input["body"]["session_id"].asUUID(),
				input["body"]);
		}
	}
};

class LLViewerChatterBoxSessionUpdate : public LLHTTPNode
{
public:
	virtual void post(
		ResponsePtr responder,
		const LLSD& context,
		const LLSD& input) const
	{
		LLFloaterIMPanel* floaterp = gIMMgr->findFloaterBySession(input["body"]["session_id"].asUUID());
		if (floaterp)
		{
			floaterp->processSessionUpdate(input["body"]["info"]);
		}
	}
};


class LLViewerChatterBoxInvitation : public LLHTTPNode
{
public:

	virtual void post(
		ResponsePtr response,
		const LLSD& context,
		const LLSD& input) const
	{
		//for backwards compatiblity reasons...we need to still
		//check for 'text' or 'voice' invitations...bleh
		if ( input["body"].has("instantmessage") )
		{
			LLSD message_params =
				input["body"]["instantmessage"]["message_params"];

			//do something here to have the IM invite behave
			//just like a normal IM
			//this is just replicated code from process_improved_im
			//and should really go in it's own function -jwolk
			if (gNoRender)
			{
				return;
			}
			LLChat chat;

			std::string message = message_params["message"].asString();
			std::string name = message_params["from_name"].asString();
			LLUUID from_id = message_params["from_id"].asUUID();
			LLUUID session_id = message_params["id"].asUUID();
			std::vector<U8> bin_bucket = message_params["data"]["binary_bucket"].asBinary();
			U8 offline = (U8)message_params["offline"].asInteger();
			
			time_t timestamp =
				(time_t) message_params["timestamp"].asInteger();

			BOOL is_busy = gAgent.getBusy();
			BOOL is_muted = LLMuteList::getInstance()->isMuted(
				from_id,
				name,
				LLMute::flagTextChat);

			BOOL is_linden = LLMuteList::getInstance()->isLinden(name);
			std::string separator_string(": ");
			int message_offset=0;

			//Handle IRC styled /me messages.
			std::string prefix = message.substr(0, 4);
			if (prefix == "/me " || prefix == "/me'")
			{
				separator_string = "";
				message_offset = 3;
			}
			
			chat.mMuted = is_muted && !is_linden;
			chat.mFromID = from_id;
			chat.mFromName = name;

			if (!is_linden && (is_busy || is_muted))
			{
				return;
			}
//MK            
			if (RRenabled && gAgent.mRRInterface.containsWithoutException ("recvim"))
			{
				return;
			}
//mk
			// standard message, not from system
			std::string saved;
			if(offline == IM_OFFLINE)
			{
				saved = llformat("(Saved %s) ", formatted_time(timestamp).c_str());
			}
			std::string buffer = separator_string + saved + message.substr(message_offset);

			BOOL is_this_agent = FALSE;
			if(from_id == gAgentID || gSavedSettings.getBOOL("HideIMInChat"))
			{
				is_this_agent = TRUE;
			}
			gIMMgr->addMessage(
				session_id,
				from_id,
				name,
				buffer,
				std::string((char*)&bin_bucket[0]),
				IM_SESSION_INVITE,
				message_params["parent_estate_id"].asInteger(),
				message_params["region_id"].asUUID(),
				ll_vector3_from_sd(message_params["position"]),
				true);

			chat.mText = std::string("IM: ") + name + separator_string + saved + message.substr(message_offset);
			LLFloaterChat::addChat(chat, TRUE, is_this_agent);

			//K now we want to accept the invitation
			std::string url = gAgent.getRegion()->getCapability(
				"ChatSessionRequest");

			if ( url != "" )
			{
				LLSD data;
				data["method"] = "accept invitation";
				data["session-id"] = session_id;
				LLHTTPClient::post(
					url,
					data,
					new LLViewerChatterBoxInvitationAcceptResponder(
						session_id,
						LLIMMgr::INVITATION_TYPE_INSTANT_MESSAGE));
			}
		} //end if invitation has instant message
		else if ( input["body"].has("voice") )
		{
			if (gNoRender)
			{
				return;
			}
			
			if(!LLVoiceClient::voiceEnabled())
			{
				// Don't display voice invites unless the user has voice enabled.
				return;
			}

			gIMMgr->inviteToSession(
				input["body"]["session_id"].asUUID(), 
				input["body"]["session_name"].asString(), 
				input["body"]["from_id"].asUUID(),
				input["body"]["from_name"].asString(),
				IM_SESSION_INVITE,
				LLIMMgr::INVITATION_TYPE_VOICE);
		}
		else if ( input["body"].has("immediate") )
		{
			gIMMgr->inviteToSession(
				input["body"]["session_id"].asUUID(), 
				input["body"]["session_name"].asString(), 
				input["body"]["from_id"].asUUID(),
				input["body"]["from_name"].asString(),
				IM_SESSION_INVITE,
				LLIMMgr::INVITATION_TYPE_IMMEDIATE);
		}
	}
};

LLHTTPRegistration<LLViewerChatterBoxSessionStartReply>
   gHTTPRegistrationMessageChatterboxsessionstartreply(
	   "/message/ChatterBoxSessionStartReply");

LLHTTPRegistration<LLViewerChatterBoxSessionEventReply>
   gHTTPRegistrationMessageChatterboxsessioneventreply(
	   "/message/ChatterBoxSessionEventReply");

LLHTTPRegistration<LLViewerForceCloseChatterBoxSession>
    gHTTPRegistrationMessageForceclosechatterboxsession(
		"/message/ForceCloseChatterBoxSession");

LLHTTPRegistration<LLViewerChatterBoxSessionAgentListUpdates>
    gHTTPRegistrationMessageChatterboxsessionagentlistupdates(
	    "/message/ChatterBoxSessionAgentListUpdates");

LLHTTPRegistration<LLViewerChatterBoxSessionUpdate>
    gHTTPRegistrationMessageChatterBoxSessionUpdate(
	    "/message/ChatterBoxSessionUpdate");

LLHTTPRegistration<LLViewerChatterBoxInvitation>
    gHTTPRegistrationMessageChatterBoxInvitation(
		"/message/ChatterBoxInvitation");

