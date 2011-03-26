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
#include "llnotify.h"
#include "llsdserialize.h"
#include "audioengine.h"
#include "lloverlaybar.h"
#include "llfloatermediafilter.h"

// Static Variables

S32 LLViewerParcelMedia::sMediaParcelLocalID = 0;
LLUUID LLViewerParcelMedia::sMediaRegionID;
bool LLViewerParcelMedia::sIsUserAction = false;
bool LLViewerParcelMedia::sMediaFilterListLoaded = false;
LLSD LLViewerParcelMedia::sMediaFilterList;
std::set<std::string> LLViewerParcelMedia::sMediaQueries;
std::set<std::string> LLViewerParcelMedia::sAllowedMedia;
std::set<std::string> LLViewerParcelMedia::sDeniedMedia;

// Local functions
void callback_play_media(S32 option, void* data);
void callback_media_alert(S32 option, void* data);
void callback_audio_alert(S32 option, void* data);

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
	loadDomainFilterList();
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
						play(parcel);
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
void LLViewerParcelMedia::play(LLParcel* parcel, bool filter)
{
	lldebugs << "LLViewerParcelMedia::play" << llendl;

	if (!parcel) return;

	if (!gSavedSettings.getBOOL("AudioStreamingVideo"))
		return;

	std::string media_url = parcel->getMediaURL();
	LLStringUtil::trim(media_url);

	if (!media_url.empty() && gSavedSettings.getBOOL("MediaEnableFilter") && (filter || !allowedMedia(media_url)))
	{
		// If filtering is needed or in case media_url just changed
		// to something we did not yet approve.
		LLViewerParcelMediaAutoPlay::playStarted();
		filterMediaUrl(parcel);
		return;
	}

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
				play(parcel);
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
			play(parcel);
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

			play(parcel);
		}
	}
}

void callback_play_media(S32 option, void* data)
{
	LLParcel* parcel = (LLParcel*)data;
	if (option == 0)
	{
		gSavedSettings.setBOOL("AudioStreamingVideo", TRUE);
		LLViewerParcelMedia::play(parcel);
	}
	else
	{
		gSavedSettings.setBOOL("AudioStreamingVideo", FALSE);
	}
	gSavedSettings.setWarning("FirstStreamingVideo", FALSE);
}

void LLViewerParcelMedia::playStreamingMusic(LLParcel* parcel, bool filter)
{
	std::string music_url = parcel->getMusicURL();
	LLStringUtil::trim(music_url);
	if (!music_url.empty() && gSavedSettings.getBOOL("MediaEnableFilter") && (filter || !allowedMedia(music_url)))
	{
		// If filtering is needed or in case music_url just changed
		// to something we did not yet approve.
		filterAudioUrl(parcel);
	}
	else if (gAudiop)
	{
		LLStringUtil::trim(music_url);
		gAudiop->startInternetStream(music_url);
		if (music_url.empty())
		{
			LLOverlayBar::audioFilterStop();
		}
		else
		{
			LLOverlayBar::audioFilterPlay();
		}
	}
}

void LLViewerParcelMedia::stopStreamingMusic()
{
	if (gAudiop)
	{
		gAudiop->stopInternetStream();
		LLOverlayBar::audioFilterStop();
	}
}

bool LLViewerParcelMedia::allowedMedia(std::string media_url)
{
	LLStringUtil::trim(media_url);
	std::string domain = extractDomain(media_url);
	LLHost host;
	host.setHostByName(domain);
	std::string ip = host.getIPString();
	if (sAllowedMedia.count(domain) || sAllowedMedia.count(ip))
	{
		return true;
	}
	std::string server;
	for (S32 i = 0; i < (S32)sMediaFilterList.size(); i++)
	{
		server = sMediaFilterList[i]["domain"].asString();
		if (server == domain || server == ip)
		{
			if (sMediaFilterList[i]["action"].asString() == "allow")
			{
				return true;
			}
			else
			{
				return false;
			}
		}
	}
	return false;
}

void LLViewerParcelMedia::filterMediaUrl(LLParcel* parcel)
{
	std::string media_action;

	if (parcel != LLViewerParcelMgr::getInstance()->getAgentParcel())
	{
		// The parcel just changed (may occur right out after a TP)
		sIsUserAction = false;
		return;
	}		

	std::string media_url = parcel->getMediaURL();
	LLStringUtil::trim(media_url);
	std::string domain = extractDomain(media_url);

	if (sMediaQueries.count(domain) > 0)
	{
		sIsUserAction = false;
		return;
	}

	LLHost host;
	host.setHostByName(domain);
	std::string ip = host.getIPString();

	if (sIsUserAction)
	{
		// This was a user manual request to play this media, so give
		// it another chance...
		sIsUserAction = false;
		bool dirty = false;
		if (sDeniedMedia.count(domain))
		{
			sDeniedMedia.erase(domain);
			dirty = true;
		}
		if (sDeniedMedia.count(ip))
		{
			sDeniedMedia.erase(ip);
			dirty = true;
		}
		if (dirty)
		{
			LLFloaterMediaFilter::setDirty();
		}
	}

	if (media_url.empty())
	{
		media_action == "allow";
	}
	else if (!sMediaFilterListLoaded || sDeniedMedia.count(domain) || sDeniedMedia.count(ip))
	{
		media_action = "ignore";
	}
	else if (sAllowedMedia.count(domain) || sAllowedMedia.count(ip))
	{
		media_action = "allow";
	}
	else
	{
		std::string server;
		for (S32 i = 0; i < (S32)sMediaFilterList.size(); i++)
		{
			server = sMediaFilterList[i]["domain"].asString();
			if (server == domain || server == ip)
			{
				media_action = sMediaFilterList[i]["action"].asString();
				break;
			}
		}
	}

	if (media_action == "allow")
	{
		play(parcel, false);
		return;
	}
	
	LLStringUtil::format_map_t args;	
	if (ip != domain && domain.find('/') == std::string::npos)
	{
		args["DOMAIN"] = domain + " (" + ip + ")";
	}
	else
	{
		args["DOMAIN"] = domain;
	}
		
	if (media_action == "deny")
	{
		LLNotifyBox::showXml("MediaBlocked", args);
		// So to avoid other "blocked" messages later in the session
		// for this url should it be requested again by a script.
		// We don't add the IP, on purpose (want to show different
		// blocks for different domains pointing to the same IP).
		sDeniedMedia.insert(domain);
	}
	else
	{
		sMediaQueries.insert(domain);
		args["URL"] = media_url;
		args["TYPE"] = "media";
		gViewerWindow->alertXml("MediaAlert", args, callback_media_alert, parcel);
	}
}

void LLViewerParcelMedia::filterAudioUrl(LLParcel* parcel)
{
	std::string media_action;

	if (parcel != LLViewerParcelMgr::getInstance()->getAgentParcel())
	{
		// The parcel just changed (may occur right out after a TP)
		sIsUserAction = false;
		return;
	}		

	std::string music_url = parcel->getMusicURL();
	LLStringUtil::trim(music_url);
	std::string domain = extractDomain(music_url);

	if (sMediaQueries.count(domain) > 0)
	{
		sIsUserAction = false;
		return;
	}
	
	LLHost host;
	host.setHostByName(domain);
	std::string ip = host.getIPString();
	
	if (sIsUserAction)
	{
		// This was a user manual request to play this media, so give
		// it another chance...
		sIsUserAction = false;
		bool dirty = false;
		if (sDeniedMedia.count(domain))
		{
			sDeniedMedia.erase(domain);
			dirty = true;
		}
		if (sDeniedMedia.count(ip))
		{
			sDeniedMedia.erase(ip);
			dirty = true;
		}
		if (dirty)
		{
			LLFloaterMediaFilter::setDirty();
		}
	}

	if (music_url.empty())
	{
		media_action == "allow";
	}
	else if (!sMediaFilterListLoaded || sDeniedMedia.count(domain) || sDeniedMedia.count(ip))
	{
		media_action = "ignore";
	}
	else if (sAllowedMedia.count(domain) || sAllowedMedia.count(ip))
	{
		media_action = "allow";
	}
	else
	{
		std::string server;
		for (S32 i = 0; i < (S32)sMediaFilterList.size(); i++)
		{
			server = sMediaFilterList[i]["domain"].asString();
			if (server == domain || server == ip)
			{
				media_action = sMediaFilterList[i]["action"].asString();
				break;
			}
		}
	}

	if (media_action == "allow")
	{
		playStreamingMusic(parcel, false);
		return;
	}		
	if (media_action == "ignore")
	{
		LLViewerParcelMedia::stopStreamingMusic();
		return;
	}
	
	LLStringUtil::format_map_t args;	
	if (ip != domain && domain.find('/') == std::string::npos)
	{
		args["DOMAIN"] = domain + " (" + ip + ")";
	}
	else
	{
		args["DOMAIN"] = domain;
	}
	
	if (media_action == "deny")
	{
		LLNotifyBox::showXml("MediaBlocked", args);
		LLViewerParcelMedia::stopStreamingMusic();
		// So to avoid other "blocked" messages later in the session
		// for this url should it be requested again by a script.
		// We don't add the IP, on purpose (want to show different
		// blocks for different domains pointing to the same IP).
		sDeniedMedia.insert(domain);
	}
	else
	{
		sMediaQueries.insert(domain);
		args["URL"] = music_url;
		args["TYPE"] = "audio";
		gViewerWindow->alertXml("MediaAlert", args, callback_audio_alert, parcel);
	}
}

void callback_media_alert(S32 option, void* data)
{
	LLParcel* parcel = (LLParcel*)data;
	std::string media_url = parcel->getMediaURL();
	std::string domain = LLViewerParcelMedia::extractDomain(media_url);
	LLHost host;
	host.setHostByName(domain);
	std::string ip = host.getIPString();
	LLStringUtil::format_map_t args;
	if (ip != domain && domain.find('/') == std::string::npos)
	{
		args["DOMAIN"] = domain + " (" + ip + ")";
	}
	else
	{
		args["DOMAIN"] = domain;
	}

	if (option == 0 || option == 3) // Allow or Whitelist
	{
		LLViewerParcelMedia::sAllowedMedia.insert(domain);
		if (option == 3) // Whitelist
		{
			LLSD newmedia;
			newmedia["domain"] = domain;
			newmedia["action"] = "allow";
			LLViewerParcelMedia::sMediaFilterList.append(newmedia);
			if (ip != domain && domain.find('/') == std::string::npos)
			{
				newmedia["domain"] = ip;
				LLViewerParcelMedia::sMediaFilterList.append(newmedia);
			}
			LLViewerParcelMedia::saveDomainFilterList();
			args["LISTED"] = "whitelisted";
			gViewerWindow->alertXml("MediaListed", args);
		}
		LLViewerParcelMedia::play(parcel, false);
	}
	else if (option == 1 || option == 2) // Deny or Blacklist
	{
		LLViewerParcelMedia::sDeniedMedia.insert(domain);
		if (ip != domain && domain.find('/') == std::string::npos)
		{
			LLViewerParcelMedia::sDeniedMedia.insert(ip);
		}
		if (option == 1) // Deny
		{
			LLNotifyBox::showXml("MediaBlocked", args);
		}
		else // Blacklist
		{
			LLSD newmedia;
			newmedia["domain"] = domain;
			newmedia["action"] = "deny";
			LLViewerParcelMedia::sMediaFilterList.append(newmedia);
			if (ip != domain && domain.find('/') == std::string::npos)
			{
				newmedia["domain"] = ip;
				LLViewerParcelMedia::sMediaFilterList.append(newmedia);
			}
			LLViewerParcelMedia::saveDomainFilterList();
			args["LISTED"] = "blacklisted";
			gViewerWindow->alertXml("MediaListed", args);
		}
	}

	LLViewerParcelMedia::sMediaQueries.erase(domain);
	LLFloaterMediaFilter::setDirty();
}

void callback_audio_alert(S32 option, void* data)
{
	LLParcel* parcel = (LLParcel*)data;
	std::string music_url = parcel->getMusicURL();
	std::string domain = LLViewerParcelMedia::extractDomain(music_url);
	LLHost host;
	host.setHostByName(domain);
	std::string ip = host.getIPString();
		
	LLStringUtil::format_map_t args;
		if (ip != domain && domain.find('/') == std::string::npos)
	{
		args["DOMAIN"] = domain + " (" + ip + ")";
	}
	else
	{
		args["DOMAIN"] = domain;
	}

	if (option == 0 || option == 3) // Allow or Whitelist
	{
		LLViewerParcelMedia::sAllowedMedia.insert(domain);
		if (option == 3) // Whitelist
		{
			LLSD newmedia;
			newmedia["domain"] = domain;
			newmedia["action"] = "allow";
			LLViewerParcelMedia::sMediaFilterList.append(newmedia);
			if (ip != domain && domain.find('/') == std::string::npos)
			{
				newmedia["domain"] = ip;
				LLViewerParcelMedia::sMediaFilterList.append(newmedia);
			}
			LLViewerParcelMedia::saveDomainFilterList();
			args["LISTED"] = "whitelisted";
			gViewerWindow->alertXml("MediaListed", args);
		}
		LLViewerParcelMedia::playStreamingMusic(parcel, false);
	}
	else if (option == 1 || option == 2) // Deny or Blacklist
	{
		LLViewerParcelMedia::sDeniedMedia.insert(domain);
		if (ip != domain && domain.find('/') == std::string::npos)
		{
			LLViewerParcelMedia::sDeniedMedia.insert(ip);
		}
		LLViewerParcelMedia::stopStreamingMusic();
		if (option == 1) // Deny
		{
			LLNotifyBox::showXml("MediaBlocked", args);
		}
		else // Blacklist
		{
			LLSD newmedia;
			newmedia["domain"] = domain;
			newmedia["action"] = "deny";
			LLViewerParcelMedia::sMediaFilterList.append(newmedia);
			if (ip != domain && domain.find('/') == std::string::npos)
			{
				newmedia["domain"] = ip;
				LLViewerParcelMedia::sMediaFilterList.append(newmedia);
			}
			LLViewerParcelMedia::saveDomainFilterList();
			args["LISTED"] = "blacklisted";
			gViewerWindow->alertXml("MediaListed", args);
		}
	}

	LLViewerParcelMedia::sMediaQueries.erase(domain);
	LLFloaterMediaFilter::setDirty();
}

void LLViewerParcelMedia::saveDomainFilterList()
{
	std::string medialist_filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "media_filter.xml");

	llofstream medialistFile(medialist_filename);
	LLSDSerialize::toPrettyXML(sMediaFilterList, medialistFile);
	medialistFile.close();
}

bool LLViewerParcelMedia::loadDomainFilterList()
{
	sMediaFilterListLoaded = true;

	std::string medialist_filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "media_filter.xml");

	if (!LLFile::isfile(medialist_filename))
	{
		LLSD emptyllsd;
		llofstream medialistFile(medialist_filename);
		LLSDSerialize::toPrettyXML(emptyllsd, medialistFile);
		medialistFile.close();
	}

	if (LLFile::isfile(medialist_filename))
	{
		llifstream medialistFile(medialist_filename);
		LLSDSerialize::fromXML(sMediaFilterList, medialistFile);
		medialistFile.close();
		LLFloaterMediaFilter::setDirty();
		return true;
	}
	else
	{
		return false;
	}
}

void LLViewerParcelMedia::clearDomainFilterList()
{
	sMediaFilterList.clear();
	sAllowedMedia.clear();
	sDeniedMedia.clear();
	saveDomainFilterList();
	gViewerWindow->alertXml("MediaFiltersCleared");
	LLFloaterMediaFilter::setDirty();
}

std::string LLViewerParcelMedia::extractDomain(std::string url)
{
	static std::string last_region = "@";

	if (url.empty())
	{
		return url;
	}

	LLStringUtil::toLower(url);

	size_t pos = url.find("//");

	if (pos != std::string::npos)
	{
		size_t count = url.size() - pos + 2;
		url = url.substr(pos + 2, count);
	}

	// Check that there is at least one slash in the URL and add a trailing
	// one if not (for media/audio URLs such as http://mydomain.net)
	if (url.find('/') == std::string::npos)
	{
		url += '/';
	}

	// If there's a user:password@ part, remove it
	pos = url.find('@');
	if (pos != std::string::npos && pos < url.find('/'))	// if '@' is not before the first '/', then it's not a user:password
	{
		size_t count = url.size() - pos + 1;
		url = url.substr(pos + 1, count);
	}

	if (url.find(gAgent.getRegion()->getHost().getHostName()) == 0 || url.find(last_region) == 0)
	{
		// This must be a scripted object rezzed in the region:
		// extend the concept of "domain" to encompass the
		// scripted object server id and avoid blocking all other
		// objects at once in this region...

		// Get rid of any port number
		pos = url.find('/');		// We earlier made sure that there's one
		url = gAgent.getRegion()->getHost().getHostName() + url.substr(pos);

		pos = url.find('?');
		if (pos != std::string::npos)
		{
			// Get rid of any parameter
			url = url.substr(0, pos);
		}

		pos = url.rfind('/');
		if (pos != std::string::npos)
		{
			// Get rid of the filename, if any, keeping only the server + path
			url = url.substr(0, pos);
		}
	}
	else
	{
		pos = url.find(':');  
		if (pos != std::string::npos && pos < url.find('/'))
		{
			// Keep anything before the port number and strip the rest off
			url = url.substr(0, pos);
		}
		else
		{
			pos = url.find('/');	// We earlier made sure that there's one
			url = url.substr(0, pos);
		}
	}

		
	// Remember this region, so to cope with requests occuring just after a
	// TP out of it.
	last_region = gAgent.getRegion()->getHost().getHostName();

	return url;
}
