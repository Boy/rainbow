/** 
 * @file llmutelist.cpp
 * @author Richard Nelson, James Cook
 * @brief Management of list of muted players
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 * 
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

/*
 * How should muting work?
 * Mute an avatar
 * Mute a specific object (accidentally spamming)
 *
 * right-click avatar, mute
 * see list of recent chatters, mute
 * type a name to mute?
 *
 * show in list whether chatter is avatar or object
 *
 * need fast lookup by id
 * need lookup by name, doesn't have to be fast
 */

#include "llviewerprecompiledheaders.h"

#include "llmutelist.h"

#include <boost/tokenizer.hpp>

#include "llcrc.h"
#include "lldir.h"
#include "lldispatcher.h"
#include "llsdserialize.h"
#include "llxfermanager.h"
#include "message.h"

#include "llagent.h"
#include "llviewergenericmessage.h"	// for gGenericDispatcher
#include "llviewerwindow.h"
#include "llworld.h" //for particle system banning
#include "llchat.h"
#include "llfloaterchat.h"
#include "llimpanel.h"
#include "llimview.h"
#include "llnotify.h"
#include "lluistring.h"
#include "llviewerobject.h" 
#include "llviewerobjectlist.h"

namespace 
{
	// This method is used to return an object to mute given an object id.
	// Its used by the LLMute constructor and LLMuteList::isMuted.
	LLViewerObject* get_object_to_mute_from_id(LLUUID object_id)
	{
		LLViewerObject *objectp = gObjectList.findObject(object_id);
		if (objectp && !objectp->isAvatar())
		{
			LLViewerObject *parentp = objectp->getRootEdit();
			if (parentp)
			{
				objectp = parentp;
			}
		}
		return objectp;
	}
}

// "emptymutelist"
class LLDispatchEmptyMuteList : public LLDispatchHandler
{
public:
	virtual bool operator()(
		const LLDispatcher* dispatcher,
		const std::string& key,
		const LLUUID& invoice,
		const sparam_t& strings)
	{
		LLMuteList::getInstance()->setLoaded();
		return true;
	}
};

static LLDispatchEmptyMuteList sDispatchEmptyMuteList;

//-----------------------------------------------------------------------------
// LLMute()
//-----------------------------------------------------------------------------
char LLMute::BY_NAME_SUFFIX[] = " (by name)";
char LLMute::AGENT_SUFFIX[] = " (resident)";
char LLMute::OBJECT_SUFFIX[] = " (object)";
char LLMute::GROUP_SUFFIX[] = " (group)";


LLMute::LLMute(const LLUUID& id, const std::string& name, EType type, U32 flags)
  : mID(id),
	mName(name),
	mType(type),
	mFlags(flags)
{
	// muting is done by root objects only - try to find this objects root
	LLViewerObject* mute_object = get_object_to_mute_from_id(id);
	if(mute_object && mute_object->getID() != id)
	{
		mID = mute_object->getID();
		LLNameValue* firstname = mute_object->getNVPair("FirstName");
		LLNameValue* lastname = mute_object->getNVPair("LastName");
		if (firstname && lastname)
		{
			mName.assign( firstname->getString() );
			mName.append(" ");
			mName.append( lastname->getString() );
		}
		mType = mute_object->isAvatar() ? AGENT : OBJECT;
	}

}


std::string LLMute::getDisplayName() const
{
	std::string name_with_suffix = mName;
	switch (mType)
	{
		case BY_NAME:
		default:
			name_with_suffix += BY_NAME_SUFFIX;
			break;
		case AGENT:
			name_with_suffix += AGENT_SUFFIX;
			break;
		case OBJECT:
			name_with_suffix += OBJECT_SUFFIX;
			break;
		case GROUP:
			name_with_suffix += GROUP_SUFFIX;
			break;
	}
	return name_with_suffix;
}

void LLMute::setFromDisplayName(const std::string& display_name)
{
	size_t pos = 0;
	mName = display_name;
	
	pos = mName.rfind(GROUP_SUFFIX);
	if (pos != std::string::npos)
	{
		mName.erase(pos);
		mType = GROUP;
		return;
	}
	
	pos = mName.rfind(OBJECT_SUFFIX);
	if (pos != std::string::npos)
	{
		mName.erase(pos);
		mType = OBJECT;
		return;
	}
	
	pos = mName.rfind(AGENT_SUFFIX);
	if (pos != std::string::npos)
	{
		mName.erase(pos);
		mType = AGENT;
		return;
	}
	
	pos = mName.rfind(BY_NAME_SUFFIX);
	if (pos != std::string::npos)
	{
		mName.erase(pos);
		mType = BY_NAME;
		return;
	}
	
	llwarns << "Unable to set mute from display name " << display_name << llendl;
	return;
}

/* static */
LLMuteList* LLMuteList::getInstance()
{
	// Register callbacks at the first time that we find that the message system has been created.
	static BOOL registered = FALSE;
	if( !registered && gMessageSystem != NULL)
	{
		registered = TRUE;
		// Register our various callbacks
		gMessageSystem->setHandlerFuncFast(_PREHASH_MuteListUpdate, processMuteListUpdate);
		gMessageSystem->setHandlerFuncFast(_PREHASH_UseCachedMuteList, processUseCachedMuteList);
	}
	return LLSingleton<LLMuteList>::getInstance(); // Call the "base" implementation.
}

//-----------------------------------------------------------------------------
// LLMuteList()
//-----------------------------------------------------------------------------
LLMuteList::LLMuteList() :
	mIsLoaded(FALSE),
	mUserVolumesLoaded(FALSE)
{
	gGenericDispatcher.addHandler("emptymutelist", &sDispatchEmptyMuteList);
}

void LLMuteList::loadUserVolumes()
{
	// call once, after LLDir::setLindenUserDir() has been called
	if (mUserVolumesLoaded)
		return;
	mUserVolumesLoaded = TRUE;
	
	// load per-resident voice volume information
	// conceptually, this is part of the mute list information, although it is only stored locally
	std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "volume_settings.xml");

	LLSD settings_llsd;
	llifstream file;
	file.open(filename);
	if (file.is_open())
	{
		LLSDSerialize::fromXML(settings_llsd, file);
	}

	for (LLSD::map_const_iterator iter = settings_llsd.beginMap();
		 iter != settings_llsd.endMap(); ++iter)
	{
		mUserVolumeSettings.insert(std::make_pair(LLUUID(iter->first), (F32)iter->second.asReal()));
	}
}

//-----------------------------------------------------------------------------
// ~LLMuteList()
//-----------------------------------------------------------------------------
LLMuteList::~LLMuteList()
{
	// If we quit from the login screen we will not have an SL account
	// name.  Don't try to save, otherwise we'll dump a file in
	// C:\Program Files\SecondLife\  JC
	std::string user_dir = gDirUtilp->getLindenUserDir();
	if (!user_dir.empty())
	{
		std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "volume_settings.xml");
		LLSD settings_llsd;

		for(user_volume_map_t::iterator iter = mUserVolumeSettings.begin(); iter != mUserVolumeSettings.end(); ++iter)
		{
			settings_llsd[iter->first.asString()] = iter->second;
		}

		llofstream file;
		file.open(filename);
		LLSDSerialize::toPrettyXML(settings_llsd, file);
	}
}

BOOL LLMuteList::isLinden(const std::string& name) const
{
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep(" ");
	tokenizer tokens(name, sep);
	tokenizer::iterator token_iter = tokens.begin();
	
	if (token_iter == tokens.end()) return FALSE;
	token_iter++;
	if (token_iter == tokens.end()) return FALSE;
	
	std::string last_name = *token_iter;
	return last_name == "Linden";
}


BOOL LLMuteList::add(const LLMute& mute, U32 flags)
{
	// Can't mute text from Lindens
	if ((mute.mType == LLMute::AGENT)
		&& isLinden(mute.mName) && (flags & LLMute::flagTextChat || flags == 0))
	{
		gViewerWindow->alertXml("MuteLinden");
		return FALSE;
	}
	
	// Can't mute self.
	if (mute.mType == LLMute::AGENT
		&& mute.mID == gAgent.getID())
	{
		return FALSE;
	}
	
	if (mute.mType == LLMute::BY_NAME)
	{		
		// Can't mute empty string by name
		if (mute.mName.empty()) 
		{
			llwarns << "Trying to mute empty string by-name" << llendl;
			return FALSE;
		}

		// Null mutes must have uuid null
		if (mute.mID.notNull())
		{
			llwarns << "Trying to add by-name mute with non-null id" << llendl;
			return FALSE;
		}

		std::pair<string_set_t::iterator, bool> result = mLegacyMutes.insert(mute.mName);
		if (result.second)
		{
			llinfos << "Muting by name " << mute.mName << llendl;
			updateAdd(mute);
			notifyObservers();
			return TRUE;
		}
		else
		{
			// was duplicate
			return FALSE;
		}
	}
	else
	{
		// Need a local (non-const) copy to set up flags properly.
		LLMute localmute = mute;
		
		// If an entry for the same entity is already in the list, remove it, saving flags as necessary.
		mute_set_t::iterator it = mMutes.find(localmute);
		if (it != mMutes.end())
		{
			// This mute is already in the list.  Save the existing entry's flags if that's warranted.
			localmute.mFlags = it->mFlags;
			
			mMutes.erase(it);
			// Don't need to call notifyObservers() here, since it will happen after the entry has been re-added below.
		}
		else
		{
			// There was no entry in the list previously.  Fake things up by making it look like the previous entry had all properties unmuted.
			localmute.mFlags = LLMute::flagAll;
		}

		if(flags)
		{
			// The user passed some combination of flags.  Make sure those flag bits are turned off (i.e. those properties will be muted).
			localmute.mFlags &= (~flags);
		}
		else
		{
			// The user passed 0.  Make sure all flag bits are turned off (i.e. all properties will be muted).
			localmute.mFlags = 0;
		}
		
		// (re)add the mute entry.
		{			
			std::pair<mute_set_t::iterator, bool> result = mMutes.insert(localmute);
			if (result.second)
			{
				llinfos << "Muting " << localmute.mName << " id " << localmute.mID << " flags " << localmute.mFlags << llendl;
				updateAdd(localmute);
				notifyObservers();
				if(!(localmute.mFlags & LLMute::flagParticles))
				{
					//Kill all particle systems owned by muted task
					if(localmute.mType == LLMute::AGENT || localmute.mType == LLMute::OBJECT)
					{
						LLViewerPartSim::getInstance()->clearParticlesByOwnerID(localmute.mID);
					}
				}
				return TRUE;
			}
		}
	}
	
	// If we were going to return success, we'd have done it by now.
	return FALSE;
}

void LLMuteList::updateAdd(const LLMute& mute)
{
	// Update the database
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_UpdateMuteListEntry);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_MuteData);
	msg->addUUIDFast(_PREHASH_MuteID, mute.mID);
	msg->addStringFast(_PREHASH_MuteName, mute.mName);
	msg->addS32("MuteType", mute.mType);
	msg->addU32("MuteFlags", mute.mFlags);
	gAgent.sendReliableMessage();

	mIsLoaded = TRUE; // why is this here? -MG
}


BOOL LLMuteList::remove(const LLMute& mute, U32 flags)
{
	BOOL found = FALSE;
	
	// First, remove from main list.
	mute_set_t::iterator it = mMutes.find(mute);
	if (it != mMutes.end())
	{
		LLMute localmute = *it;
		bool remove = true;
		if(flags)
		{
			// If the user passed mute flags, we may only want to turn some flags on.
			localmute.mFlags |= flags;
			
			if(localmute.mFlags == LLMute::flagAll)
			{
				// Every currently available mute property has been masked out.
				// Remove the mute entry entirely.
			}
			else
			{
				// Only some of the properties are masked out.  Update the entry.
				remove = false;
			}
		}
		else
		{
			// The caller didn't pass any flags -- just remove the mute entry entirely.
		}
		
		// Always remove the entry from the set -- it will be re-added with new flags if necessary.
		mMutes.erase(it);

		if(remove)
		{
			// The entry was actually removed.  Notify the server.
			updateRemove(localmute);
			llinfos << "Unmuting " << localmute.mName << " id " << localmute.mID << " flags " << localmute.mFlags << llendl;
		}
		else
		{
			// Flags were updated, the mute entry needs to be retransmitted to the server and re-added to the list.
			mMutes.insert(localmute);
			updateAdd(localmute);
			llinfos << "Updating mute entry " << localmute.mName << " id " << localmute.mID << " flags " << localmute.mFlags << llendl;
		}
		
		// Must be after erase.
		setLoaded();  // why is this here? -MG
	}

	// Clean up any legacy mutes
	string_set_t::iterator legacy_it = mLegacyMutes.find(mute.mName);
	if (legacy_it != mLegacyMutes.end())
	{
		// Database representation of legacy mute is UUID null.
		LLMute mute(LLUUID::null, *legacy_it, LLMute::BY_NAME);
		updateRemove(mute);
		mLegacyMutes.erase(legacy_it);
		// Must be after erase.
		setLoaded(); // why is this here? -MG
	}
	
	return found;
}


void LLMuteList::updateRemove(const LLMute& mute)
{
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_RemoveMuteListEntry);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_MuteData);
	msg->addUUIDFast(_PREHASH_MuteID, mute.mID);
	msg->addString("MuteName", mute.mName);
	gAgent.sendReliableMessage();
}

void notify_automute_callback(const LLUUID& agent_id, const std::string& first_name, const std::string& last_name, BOOL is_group, void* user_data)
{
	U32 temp_data = (U32) (uintptr_t) user_data;
	LLMuteList::EAutoReason reason = (LLMuteList::EAutoReason)temp_data;
	LLUIString auto_message;

	switch (reason)
	{
	default:
	case LLMuteList::AR_IM:
		auto_message = LLNotifyBox::getTemplateMessage("AutoUnmuteByIM");
		break;
	case LLMuteList::AR_INVENTORY:
		auto_message = LLNotifyBox::getTemplateMessage("AutoUnmuteByInventory");
		break;
	case LLMuteList::AR_MONEY:
		auto_message = LLNotifyBox::getTemplateMessage("AutoUnmuteByMoney");
		break;
	}

	auto_message.setArg("[FIRST]", first_name);
	auto_message.setArg("[LAST]", last_name);

	if (reason == LLMuteList::AR_IM)
	{
		LLFloaterIMPanel *timp = gIMMgr->findFloaterBySession(agent_id);
		if (timp)
		{
			timp->addHistoryLine(auto_message.getString());
		}
	}

	LLChat auto_chat(auto_message.getString());
	LLFloaterChat::addChat(auto_chat, FALSE, FALSE);
}


BOOL LLMuteList::autoRemove(const LLUUID& agent_id, const EAutoReason reason, const std::string& first_name, const std::string& last_name)
{
	BOOL removed = FALSE;

	if (isMuted(agent_id))
	{
		LLMute automute(agent_id, LLStringUtil::null, LLMute::AGENT);
		removed = TRUE;
		remove(automute);

		if (first_name.empty() && last_name.empty())
		{
			std::string cache_first, cache_last;
			if (gCacheName->getName(agent_id, cache_first, cache_last))
			{
				// name in cache, call callback directly
				notify_automute_callback(agent_id, cache_first, cache_last, FALSE, (void *)reason);
			}
			else
			{
				// not in cache, lookup name from cache
				gCacheName->get(agent_id, FALSE, notify_automute_callback, (void *)reason);
			}
		}
		else
		{
			// call callback directly
			notify_automute_callback(agent_id, first_name, last_name, FALSE, (void *)reason);
		}
	}

	return removed;
}


std::vector<LLMute> LLMuteList::getMutes() const
{
	std::vector<LLMute> mutes;
	
	for (mute_set_t::const_iterator it = mMutes.begin();
		 it != mMutes.end();
		 ++it)
	{
		mutes.push_back(*it);
	}
	
	for (string_set_t::const_iterator it = mLegacyMutes.begin();
		 it != mLegacyMutes.end();
		 ++it)
	{
		LLMute legacy(LLUUID::null, *it);
		mutes.push_back(legacy);
	}
	
	std::sort(mutes.begin(), mutes.end(), compare_by_name());
	return mutes;
}

//-----------------------------------------------------------------------------
// loadFromFile()
//-----------------------------------------------------------------------------
BOOL LLMuteList::loadFromFile(const std::string& filename)
{
	if(!filename.size())
	{
		llwarns << "Mute List Filename is Empty!" << llendl;
		return FALSE;
	}

	LLFILE* fp = LLFile::fopen(filename, "rb");		/*Flawfinder: ignore*/
	if (!fp)
	{
		llwarns << "Couldn't open mute list " << filename << llendl;
		return FALSE;
	}

	// *NOTE: Changing the size of these buffers will require changes
	// in the scanf below.
	char id_buffer[MAX_STRING];		/*Flawfinder: ignore*/
	char name_buffer[MAX_STRING];		/*Flawfinder: ignore*/
	char buffer[MAX_STRING];		/*Flawfinder: ignore*/
	while (!feof(fp) 
		   && fgets(buffer, MAX_STRING, fp))
	{
		id_buffer[0] = '\0';
		name_buffer[0] = '\0';
		S32 type = 0;
		U32 flags = 0;
		sscanf(	/* Flawfinder: ignore */
			buffer, " %d %254s %254[^|]| %u\n", &type, id_buffer, name_buffer,
			&flags);
		LLUUID id = LLUUID(id_buffer);
		LLMute mute(id, std::string(name_buffer), (LLMute::EType)type, flags);
		if (mute.mID.isNull()
			|| mute.mType == LLMute::BY_NAME)
		{
			mLegacyMutes.insert(mute.mName);
		}
		else
		{
			mMutes.insert(mute);
		}
	}
	fclose(fp);
	setLoaded();
	return TRUE;
}

//-----------------------------------------------------------------------------
// saveToFile()
//-----------------------------------------------------------------------------
BOOL LLMuteList::saveToFile(const std::string& filename)
{
	if(!filename.size())
	{
		llwarns << "Mute List Filename is Empty!" << llendl;
		return FALSE;
	}

	LLFILE* fp = LLFile::fopen(filename, "wb");		/*Flawfinder: ignore*/
	if (!fp)
	{
		llwarns << "Couldn't open mute list " << filename << llendl;
		return FALSE;
	}
	// legacy mutes have null uuid
	std::string id_string;
	LLUUID::null.toString(id_string);
	for (string_set_t::iterator it = mLegacyMutes.begin();
		 it != mLegacyMutes.end();
		 ++it)
	{
		fprintf(fp, "%d %s %s|\n", (S32)LLMute::BY_NAME, id_string.c_str(), it->c_str());
	}
	for (mute_set_t::iterator it = mMutes.begin();
		 it != mMutes.end();
		 ++it)
	{
		it->mID.toString(id_string);
		const std::string& name = it->mName;
		fprintf(fp, "%d %s %s|%u\n", (S32)it->mType, id_string.c_str(), name.c_str(), it->mFlags);
	}
	fclose(fp);
	return TRUE;
}


BOOL LLMuteList::isMuted(const LLUUID& id, const std::string& name, U32 flags) const
{
	// for objects, check for muting on their parent prim
	LLViewerObject* mute_object = get_object_to_mute_from_id(id);
	LLUUID id_to_check  = (mute_object) ? mute_object->getID() : id;

	// don't need name or type for lookup
	LLMute mute(id_to_check);
	mute_set_t::const_iterator mute_it = mMutes.find(mute);
	if (mute_it != mMutes.end())
	{
		// If any of the flags the caller passed are set, this item isn't considered muted for this caller.
		if(flags & mute_it->mFlags)
		{
			return FALSE;
		}
		return TRUE;
	}

	// empty names can't be legacy-muted
	if (name.empty()) return FALSE;

	// Look in legacy pile
	string_set_t::const_iterator legacy_it = mLegacyMutes.find(name);
	return legacy_it != mLegacyMutes.end();
}

//-----------------------------------------------------------------------------
// requestFromServer()
//-----------------------------------------------------------------------------
void LLMuteList::requestFromServer(const LLUUID& agent_id)
{
	loadUserVolumes();
	
	std::string agent_id_string;
	std::string filename;
	agent_id.toString(agent_id_string);
	filename = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,agent_id_string) + ".cached_mute";
	LLCRC crc;
	crc.update(filename);

	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_MuteListRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, agent_id);
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_MuteData);
	msg->addU32Fast(_PREHASH_MuteCRC, crc.getCRC());
	gAgent.sendReliableMessage();
}

//-----------------------------------------------------------------------------
// cache()
//-----------------------------------------------------------------------------

void LLMuteList::cache(const LLUUID& agent_id)
{
	// Write to disk even if empty.
	if(mIsLoaded)
	{
		std::string agent_id_string;
		std::string filename;
		agent_id.toString(agent_id_string);
		filename = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,agent_id_string) + ".cached_mute";
		saveToFile(filename);
	}
}

void LLMuteList::setSavedResidentVolume(const LLUUID& id, F32 volume)
{
	// store new value in volume settings file
	mUserVolumeSettings[id] = volume;
}

F32 LLMuteList::getSavedResidentVolume(const LLUUID& id)
{
	const F32 DEFAULT_VOLUME = 0.5f;

	user_volume_map_t::iterator found_it = mUserVolumeSettings.find(id);
	if (found_it != mUserVolumeSettings.end())
	{
		return found_it->second;
	}
	//FIXME: assumes default, should get this from somewhere
	return DEFAULT_VOLUME;
}


//-----------------------------------------------------------------------------
// Static message handlers
//-----------------------------------------------------------------------------

void LLMuteList::processMuteListUpdate(LLMessageSystem* msg, void**)
{
	llinfos << "LLMuteList::processMuteListUpdate()" << llendl;
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_MuteData, _PREHASH_AgentID, agent_id);
	if(agent_id != gAgent.getID())
	{
		llwarns << "Got an mute list update for the wrong agent." << llendl;
		return;
	}
	std::string unclean_filename;
	msg->getStringFast(_PREHASH_MuteData, _PREHASH_Filename, unclean_filename);
	std::string filename = LLDir::getScrubbedFileName(unclean_filename);
	
	std::string *local_filename_and_path = new std::string(gDirUtilp->getExpandedFilename( LL_PATH_CACHE, filename ));
	gXferManager->requestFile(*local_filename_and_path,
							  filename,
							  LL_PATH_CACHE,
							  msg->getSender(),
							  TRUE, // make the remote file temporary.
							  onFileMuteList,
							  (void**)local_filename_and_path,
							  LLXferManager::HIGH_PRIORITY);
}

void LLMuteList::processUseCachedMuteList(LLMessageSystem* msg, void**)
{
	llinfos << "LLMuteList::processUseCachedMuteList()" << llendl;

	std::string agent_id_string;
	gAgent.getID().toString(agent_id_string);
	std::string filename;
	filename = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,agent_id_string) + ".cached_mute";
	LLMuteList::getInstance()->loadFromFile(filename);
}

void LLMuteList::onFileMuteList(void** user_data, S32 error_code, LLExtStat ext_status)
{
	llinfos << "LLMuteList::processMuteListFile()" << llendl;

	std::string* local_filename_and_path = (std::string*)user_data;
	if(local_filename_and_path && !local_filename_and_path->empty() && (error_code == 0))
	{
		LLMuteList::getInstance()->loadFromFile(*local_filename_and_path);
		LLFile::remove(*local_filename_and_path);
	}
	delete local_filename_and_path;
}

void LLMuteList::addObserver(LLMuteListObserver* observer)
{
	mObservers.insert(observer);
}

void LLMuteList::removeObserver(LLMuteListObserver* observer)
{
	mObservers.erase(observer);
}

void LLMuteList::setLoaded()
{
	mIsLoaded = TRUE;
	notifyObservers();
}

void LLMuteList::notifyObservers()
{
	for (observer_set_t::iterator it = mObservers.begin();
		it != mObservers.end();
		)
	{
		LLMuteListObserver* observer = *it;
		observer->onChange();
		// In case onChange() deleted an entry.
		it = mObservers.upper_bound(observer);
	}
}
