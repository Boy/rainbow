/**
 * @file llviewerparcelmedia.cpp
 * @brief Handlers for multimedia on a per-parcel basis
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
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
#include "llviewerparcelmedia.h"

#include "llagent.h"
#include "llviewercontrol.h"
#include "llviewermedia.h"
#include "llviewerregion.h"
#include "llparcel.h"
#include "llviewerparcelmgr.h"
#include "lluuid.h"
#include "message.h"
#include "llviewerparcelmediaautoplay.h"
#include "llviewerwindow.h"
#include "llfirstuse.h"
#include "llfloaterchat.h"
#include "llnotify.h"
#include "llsdserialize.h"
#include "audioengine.h"
#include "lloverlaybar.h"

// Static Variables

S32 LLViewerParcelMedia::sMediaParcelLocalID = 0;
LLUUID LLViewerParcelMedia::sMediaRegionID;
LLSD LLViewerParcelMedia::sMediaFilterList;

// Local functions
void callback_play_media(S32 option, void* data);
void callback_media_alert(S32 option, void* data);
void callback_audio_alert(S32 option, void* data);
std::string extractdomain(std::string url);

// Move this to its own file.
// helper class that tries to download a URL from a web site and calls a method
// on the Panel Land Media and to discover the MIME type
class LLMimeDiscoveryResponder : public LLHTTPClient::Responder
{
public:
	LLMimeDiscoveryResponder( )
	  {}



	  virtual void completedHeader(U32 status, const std::string& reason, const LLSD& content)
	  {
		  std::string media_type = content["content-type"].asString();
		  std::string::size_type idx1 = media_type.find_first_of(";");
		  std::string mime_type = media_type.substr(0, idx1);
		  completeAny(status, mime_type);
	  }

	  virtual void error( U32 status, const std::string& reason )
	  {
		  completeAny(status, "none/none");
	  }

	  void completeAny(U32 status, const std::string& mime_type)
	  {
		  LLViewerMedia::setMimeType(mime_type);
	  }
};

// static
void LLViewerParcelMedia::initClass()
{
	LLMessageSystem* msg = gMessageSystem;
	msg->setHandlerFunc("ParcelMediaCommandMessage", processParcelMediaCommandMessage );
	msg->setHandlerFunc("ParcelMediaUpdate", processParcelMediaUpdate );
	LLViewerParcelMediaAutoPlay::initClass();
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerParcelMedia::update(LLParcel* parcel)
{
	if (/*LLViewerMedia::hasMedia()*/ true)
	{
		// we have a player
		if (parcel)
		{
			if(!gAgent.getRegion())
			{
				sMediaRegionID = LLUUID() ;
				stop() ;
				return ;				
			}

			// we're in a parcel
			bool new_parcel = false;
			S32 parcelid = parcel->getLocalID();						

			LLUUID regionid = gAgent.getRegion()->getRegionID();
			if (parcelid != sMediaParcelLocalID || regionid != sMediaRegionID)
			{
				sMediaParcelLocalID = parcelid;
				sMediaRegionID = regionid;
				new_parcel = true;
			}

			std::string mediaUrl = std::string ( parcel->getMediaURL () );
			LLStringUtil::trim(mediaUrl);

			// has something changed?
			if (  ( LLViewerMedia::getMediaURL() != mediaUrl )
				|| ( LLViewerMedia::getMediaTextureID() != parcel->getMediaID () ) )
			{
				bool video_was_playing = FALSE;
				bool same_media_id = LLViewerMedia::getMediaTextureID() == parcel->getMediaID ();

				if (LLViewerMedia::isMediaPlaying())
				{
					video_was_playing = TRUE;
				}

				if ( !mediaUrl.empty() && same_media_id && ! new_parcel)
				{
					// Someone has "changed the channel", changing the URL of a video
					// you were already watching.  Automatically play provided the texture ID is the same
					if (video_was_playing)
					{
						// Poke the mime type in before calling play.
						// This is necessary because in this instance we are not waiting
						// for the results of a header curl.  In order to change the channel
						// a mime type MUST be provided.
						LLViewerMedia::setMimeType(parcel->getMediaType());
						if (gSavedSettings.getBOOL("MediaEnableFilter"))
						{
						filterMediaUrl(parcel);
						}
						else
						{
						play(parcel);
						}
					}
				}
				else
				{
					stop();
				}

				// Discover the MIME type
				// Disabled for the time being.  Get the mime type from the parcel.
				if(gSavedSettings.getBOOL("AutoMimeDiscovery"))
				{
					LLHTTPClient::getHeaderOnly( mediaUrl, new LLMimeDiscoveryResponder());
				}
				else
				{
					LLViewerMedia::setMimeType(parcel->getMediaType());
				}

				// First use warning
				if(	gSavedSettings.getWarning("FirstStreamingVideo") )
				{
					gViewerWindow->alertXml("ParcelCanPlayMedia",
						callback_play_media, (void*)parcel);

				}

			}
		}
		else
		{
			stop();
		}
	}
	/*
	else
	{
		// no audio player, do a first use dialog if their is media here
		if (parcel)
		{
			std::string mediaUrl = std::string ( parcel->getMediaURL () );
			if (!mediaUrl.empty ())
			{
				if (gSavedSettings.getWarning("QuickTimeInstalled"))
				{
					gSavedSettings.setWarning("QuickTimeInstalled", FALSE);

					LLNotifyBox::showXml("NoQuickTime" );
				};
			}
		}
	}
	*/
}

// static
void LLViewerParcelMedia::play(LLParcel* parcel)
{
	lldebugs << "LLViewerParcelMedia::play" << llendl;

	if (!parcel) return;

	if (!gSavedSettings.getBOOL("AudioStreamingVideo"))
		return;

	std::string media_url = parcel->getMediaURL();
	std::string mime_type = parcel->getMediaType();
	LLUUID placeholder_texture_id = parcel->getMediaID();
	U8 media_auto_scale = parcel->getMediaAutoScale();
	U8 media_loop = parcel->getMediaLoop();
	S32 media_width = parcel->getMediaWidth();
	S32 media_height = parcel->getMediaHeight();
	LLViewerMedia::play(media_url, mime_type, placeholder_texture_id,
						media_width, media_height, media_auto_scale,
						media_loop);
	LLFirstUse::useMedia();

	LLViewerParcelMediaAutoPlay::playStarted();
}

// static
void LLViewerParcelMedia::stop()
{

	LLViewerMedia::stop();
}

// static
void LLViewerParcelMedia::pause()
{
	LLViewerMedia::pause();
}

// static
void LLViewerParcelMedia::start()
{
	LLViewerMedia::start();
	LLFirstUse::useMedia();

	LLViewerParcelMediaAutoPlay::playStarted();
}

// static
void LLViewerParcelMedia::seek(F32 time)
{
	LLViewerMedia::seek(time);
}


// static
LLMediaBase::EStatus LLViewerParcelMedia::getStatus()
{
	return LLViewerMedia::getStatus();
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerParcelMedia::processParcelMediaCommandMessage( LLMessageSystem *msg, void ** )
{
	// extract the agent id
	//	LLUUID agent_id;
	//	msg->getUUID( agent_id );

	U32 flags;
	U32 command;
	F32 time;
	msg->getU32( "CommandBlock", "Flags", flags );
	msg->getU32( "CommandBlock", "Command", command);
	msg->getF32( "CommandBlock", "Time", time );

	if (flags &( (1<<PARCEL_MEDIA_COMMAND_STOP)
				| (1<<PARCEL_MEDIA_COMMAND_PAUSE)
				| (1<<PARCEL_MEDIA_COMMAND_PLAY)
				| (1<<PARCEL_MEDIA_COMMAND_LOOP)
				| (1<<PARCEL_MEDIA_COMMAND_UNLOAD) ))
	{
		// stop
		if( command == PARCEL_MEDIA_COMMAND_STOP )
		{
			stop();
		}
		else
		// pause
		if( command == PARCEL_MEDIA_COMMAND_PAUSE )
		{
			pause();
		}
		else
		// play
		if(( command == PARCEL_MEDIA_COMMAND_PLAY ) ||
		   ( command == PARCEL_MEDIA_COMMAND_LOOP ))
		{
			if (LLViewerMedia::isMediaPaused())
			{
				start();
			}
			else
			{
				LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
				if (gSavedSettings.getBOOL("MediaEnableFilter"))
				{
					filterMediaUrl(parcel);
				}
				else
				{
					play(parcel);
				}
			}
		}
		else
		// unload
		if( command == PARCEL_MEDIA_COMMAND_UNLOAD )
		{
			stop();
		}
	}

	if (flags & (1<<PARCEL_MEDIA_COMMAND_TIME))
	{
		if(! LLViewerMedia::hasMedia())
		{
			LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
			if (gSavedSettings.getBOOL("MediaEnableFilter"))
			{
				filterMediaUrl(parcel);
			}
			else
			{
				play(parcel);
			}
		}
		seek(time);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerParcelMedia::processParcelMediaUpdate( LLMessageSystem *msg, void ** )
{
	LLUUID media_id;
	std::string media_url;
	std::string media_type;
	S32 media_width = 0;
	S32 media_height = 0;
	U8 media_auto_scale = FALSE;
	U8 media_loop = FALSE;

	msg->getUUID( "DataBlock", "MediaID", media_id );
	char media_url_buffer[257];
	msg->getString( "DataBlock", "MediaURL", 255, media_url_buffer );
	media_url = media_url_buffer;
	msg->getU8("DataBlock", "MediaAutoScale", media_auto_scale);

	if (msg->has("DataBlockExtended")) // do we have the extended data?
	{
		char media_type_buffer[257];
		msg->getString("DataBlockExtended", "MediaType", 255, media_type_buffer);
		media_type = media_type_buffer;
		msg->getU8("DataBlockExtended", "MediaLoop", media_loop);
		msg->getS32("DataBlockExtended", "MediaWidth", media_width);
		msg->getS32("DataBlockExtended", "MediaHeight", media_height);
	}

	LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	BOOL same = FALSE;
	if (parcel)
	{
		same = ((parcel->getMediaURL() == media_url) &&
				(parcel->getMediaType() == media_type) &&
				(parcel->getMediaID() == media_id) &&
				(parcel->getMediaWidth() == media_width) &&
				(parcel->getMediaHeight() == media_height) &&
				(parcel->getMediaAutoScale() == media_auto_scale) &&
				(parcel->getMediaLoop() == media_loop));

		if (!same)
		{
			// temporarily store these new values in the parcel
			parcel->setMediaURL(media_url);
			parcel->setMediaType(media_type);
			parcel->setMediaID(media_id);
			parcel->setMediaWidth(media_width);
			parcel->setMediaHeight(media_height);
			parcel->setMediaAutoScale(media_auto_scale);
			parcel->setMediaLoop(media_loop);

			if (gSavedSettings.getBOOL("MediaEnableFilter"))
			{
				filterMediaUrl(parcel);
			}
			else
			{
				play(parcel);
			}
		}
	}
}

void callback_play_media(S32 option, void* data)
{
	LLParcel* parcel = (LLParcel*)data;
	if (option == 0)
	{
		gSavedSettings.setBOOL("AudioStreamingVideo", TRUE);
		if (gSavedSettings.getBOOL("MediaEnableFilter"))
		{
			LLViewerParcelMedia::filterMediaUrl(parcel);
		}
		else
		{
			LLViewerParcelMedia::play(parcel);
		}
	}
	else
	{
		gSavedSettings.setBOOL("AudioStreamingVideo", FALSE);
	}
	gSavedSettings.setWarning("FirstStreamingVideo", FALSE);
}

void LLViewerParcelMedia::filterMediaUrl(LLParcel* parcel)
{
	std::string media_url = parcel->getMediaURL();
	std::string media_action;
	std::string domain = extractdomain(media_url);
    
	for(int i = 0;i<(int)sMediaFilterList.size();i++)
	{
		if (sMediaFilterList[i].has(domain))
		{
			media_action = sMediaFilterList[i][domain].asString();
			break;
		}
	}
	if (media_action=="allow")
	{
		play(parcel);
	}
	else if (media_action=="deny")
	{
		LLChat chat;
		chat.mText = "Media URL "+media_url+" blocked"+" - Blacklisted domain: "+domain;
		chat.mSourceType = CHAT_SOURCE_SYSTEM;
		LLFloaterChat::addChat(chat,FALSE,FALSE);
	}
	else
	{
		LLStringUtil::format_map_t args;
		args["[MEDIAURL]"] = media_url;
		gViewerWindow->alertXml("MediaAlert", args, callback_media_alert, parcel);
	}
}

void callback_media_alert(S32 option, void* data)
{
	LLParcel* parcel = (LLParcel*)data;
	std::string media_url = parcel->getMediaURL();
	std::string domain = extractdomain(media_url);

	LLChat chat;
	chat.mSourceType = CHAT_SOURCE_SYSTEM;

	if (option== 0) //allow
	{
		LLViewerParcelMedia::play(parcel);
	}
	else if (option== 2) //Blacklist
	{
		LLSD newmedia;
		newmedia[domain] = "deny";
		LLViewerParcelMedia::sMediaFilterList.append(newmedia);
		LLViewerParcelMedia::saveDomainFilterList();
		chat.mText = "Domain "+domain+" is now blacklisted";
		LLFloaterChat::addChat(chat,FALSE,FALSE);
	}
	else if (option== 3) // Whitelist
	{
		LLSD newmedia;
		newmedia[domain] = "allow";
		LLViewerParcelMedia::sMediaFilterList.append(newmedia);
		LLViewerParcelMedia::saveDomainFilterList();
		chat.mText = "Domain "+domain+" is now whitelisted";
		LLFloaterChat::addChat(chat,FALSE,FALSE);
		LLViewerParcelMedia::play(parcel);
	}	
}

void LLViewerParcelMedia::filterAudioUrl(LLParcel* parcel)
{
	std::string media_url = parcel->getMusicURL();
	std::string media_action;
	std::string domain = extractdomain(media_url);
    
	for(int i = 0;i<(int)sMediaFilterList.size();i++)
	{
		if (sMediaFilterList[i].has(domain))
		{
			media_action = sMediaFilterList[i][domain].asString();
			break;
		}
	}
	if (media_action=="allow")
	{
		gAudiop->startInternetStream(media_url);
		LLOverlayBar::audioFilterPlay();
	}
	else if (media_action=="deny")
	{
		LLChat chat;
		chat.mText = "Audio URL "+media_url+" blocked"+" - Blacklisted domain: "+domain;
		chat.mSourceType = CHAT_SOURCE_SYSTEM;
		LLFloaterChat::addChat(chat,FALSE,FALSE);
		LLOverlayBar::audioFilterStop();
	}
	else
	{
		LLStringUtil::format_map_t args;
		args["[AUDIOURL]"] = media_url;
		gViewerWindow->alertXml("AudioAlert", args, callback_audio_alert, parcel);
	}
}

void callback_audio_alert(S32 option, void* data)
{
	LLParcel* parcel = (LLParcel*)data;
	std::string media_url = parcel->getMusicURL();
	std::string domain = extractdomain(media_url);

	LLChat chat;
	chat.mSourceType = CHAT_SOURCE_SYSTEM;

	if (option== 0) //allow
	{
		gAudiop->startInternetStream(media_url);
		LLOverlayBar::audioFilterPlay();	
	}
	else if (option== 1) //deny
	{
		gAudiop->stopInternetStream();
		LLOverlayBar::audioFilterStop();
	}
	else if (option== 2) //Blacklist
	{
		LLSD newmedia;
		newmedia[domain] = "deny";
		LLViewerParcelMedia::sMediaFilterList.append(newmedia);
		LLViewerParcelMedia::saveDomainFilterList();
		chat.mText = "Domain "+domain+" is now blacklisted";
		LLFloaterChat::addChat(chat,FALSE,FALSE);
		gAudiop->stopInternetStream();
		LLOverlayBar::audioFilterStop();
	}
	else if (option== 3) // Whitelist
	{
		LLSD newmedia;
		newmedia[domain] = "allow";
		LLViewerParcelMedia::sMediaFilterList.append(newmedia);
		LLViewerParcelMedia::saveDomainFilterList();
		chat.mText = "Domain "+domain+" is now whitelisted";
		LLFloaterChat::addChat(chat,FALSE,FALSE);
		gAudiop->startInternetStream(media_url);
		LLOverlayBar::audioFilterPlay();
	}
}

bool LLViewerParcelMedia::saveDomainFilterList()
{
	std::string medialist_filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "medialist.xml");

	llofstream medialistFile(medialist_filename);
	LLSDSerialize::toPrettyXML(sMediaFilterList, medialistFile);
	medialistFile.close();
	return true;
}

bool LLViewerParcelMedia::loadDomainFilterList()
{
	std::string medialist_filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "medialist.xml");

	if(!LLFile::isfile(medialist_filename))
	{
		LLSD emptyllsd;
		llofstream medialistFile(medialist_filename);
		LLSDSerialize::toPrettyXML(emptyllsd, medialistFile);
		medialistFile.close();
	}

	if(LLFile::isfile(medialist_filename))
	{
		llifstream medialistFile(medialist_filename);
		LLSDSerialize::fromXML(sMediaFilterList, medialistFile);
		medialistFile.close();
		return true;
	}
	else
	{
		return false;
	}
}

std::string extractdomain(std::string url)
{
	size_t pos = url.find("//");

	if (pos != std::string::npos)
	{
		int count = url.size()- pos+2;
		url = url.substr(pos+2, count);
	}

	pos = url.find("/");

	if (pos != std::string::npos)
	{
		url = url.substr(0, pos);
	}
pos = url.find(":");  
if (pos != std::string::npos)
{
	url = url.substr(0, pos);
}
return url;
}
