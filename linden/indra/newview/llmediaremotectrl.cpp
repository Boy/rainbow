/** 
 * @file llmediaremotectrl.cpp
 * @brief A remote control for media (video and music)
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

#include "llmediaremotectrl.h"

#include "audioengine.h"
#include "lliconctrl.h"
#include "llmimetypes.h"
#include "lloverlaybar.h"
#include "llviewermedia.h"
#include "llviewerparcelmedia.h"
#include "llviewerparcelmgr.h"
#include "lluictrlfactory.h"
#include "llpanelaudiovolume.h"
#include "llparcel.h"
#include "llviewercontrol.h"
#include "llbutton.h"

////////////////////////////////////////////////////////////////////////////////
//
//

static LLRegisterWidget<LLMediaRemoteCtrl> r("media_remote");

LLMediaRemoteCtrl::LLMediaRemoteCtrl(const std::string& name,
									 const std::string& label,
									 const LLRect& rect,
									 const std::string& xml_file) :
	LLPanel(name, rect, FALSE)
{
	setIsChrome(TRUE);
	setFocusRoot(TRUE);

	LLUICtrlFactory::getInstance()->buildPanel(this, xml_file);
}

BOOL LLMediaRemoteCtrl::postBuild()
{
	childSetAction("media_play", LLOverlayBar::mediaPlay, this);
	childSetAction("media_stop", LLOverlayBar::mediaStop, this);
	childSetAction("media_pause", LLOverlayBar::mediaPause, this);

	childSetAction("music_play", LLOverlayBar::musicPlay, this);
	childSetAction("music_stop", LLOverlayBar::musicStop, this);
	childSetAction("music_pause", LLOverlayBar::musicPause, this);

	childSetAction("volume", LLOverlayBar::toggleAudioVolumeFloater, this);
	childSetControlName("volume", "ShowAudioVolume");

	return TRUE;
}

void LLMediaRemoteCtrl::draw()
{
	enableMediaButtons(this);
	LLPanel::draw();
}

LLMediaRemoteCtrl::~LLMediaRemoteCtrl()
{
}

// Static
void LLMediaRemoteCtrl::enableMediaButtons(LLPanel* panel)
{
	// Media
	bool play_media_enabled = false;
	bool stop_media_enabled = false;
	bool play_music_enabled = false;
	bool stop_music_enabled = false;
	bool music_show_pause = false;
	bool media_show_pause = false;
	LLColor4 music_icon_color = LLUI::sColorsGroup->getColor( "IconDisabledColor" );
	LLColor4 media_icon_color = LLUI::sColorsGroup->getColor( "IconDisabledColor" );
	std::string media_type = "none/none";
	LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();

	if (gSavedSettings.getBOOL("AudioStreamingVideo") && gOverlayBar)
	{
		if (parcel && parcel->getMediaURL()[0])
		{
			media_type = parcel->getMediaType();

			play_media_enabled = true;
			media_icon_color = LLUI::sColorsGroup->getColor( "IconEnabledColor" );

			LLMediaBase::EStatus status = LLViewerParcelMedia::getStatus();
			switch (status)
			{
			case LLMediaBase::STATUS_STOPPED:
			case LLMediaBase::STATUS_UNKNOWN:
				media_show_pause = false;
				stop_media_enabled = false;
				break;
			case LLMediaBase::STATUS_STARTED:
			case LLMediaBase::STATUS_NAVIGATING:
			case LLMediaBase::STATUS_RESETTING:
				// HACK: only show the pause button for movie types
				media_show_pause = LLMIMETypes::widgetType(parcel->getMediaType()) == "movie" ? true : false;
				stop_media_enabled = true;
				play_media_enabled = false;
				break;
			case LLMediaBase::STATUS_PAUSED:
				media_show_pause = false;
				stop_media_enabled = true;
				break;
			default:
				// inherit defaults above
				break;
			}
		}
	}
	if (gSavedSettings.getBOOL("AudioStreamingMusic") && gAudiop && gOverlayBar)
	{
		if (parcel && parcel->getMusicURL()[0])
		{
			play_music_enabled = true;
			music_icon_color = LLUI::sColorsGroup->getColor("IconEnabledColor");

			if (gOverlayBar->musicPlaying())
			{
				music_show_pause = true;
				stop_music_enabled = true;
			}
			else
			{
				music_show_pause = false;
				stop_music_enabled = false;
			}
		}
		// if no mime type has been set disable play
		if (LLViewerMedia::getMimeType().empty() ||
			LLViewerMedia::getMimeType() == "none/none")
		{
			play_media_enabled = false;
			stop_media_enabled = false;
		}
	}

	const std::string media_icon_name = LLMIMETypes::findIcon(media_type);
	LLIconCtrl* media_icon = panel->getChild<LLIconCtrl>("media_icon");

	panel->childSetEnabled("music_play", play_music_enabled);
	panel->childSetEnabled("music_stop", stop_music_enabled);
	panel->childSetEnabled("music_pause", music_show_pause);
	panel->childSetVisible("music_pause", music_show_pause);
	panel->childSetVisible("music_play", !music_show_pause);

	panel->childSetColor("music_icon", music_icon_color);

	if (!media_icon_name.empty() && media_icon)
	{
		media_icon->setImage(media_icon_name);
	}

	panel->childSetEnabled("media_play", play_media_enabled);
	panel->childSetEnabled("media_stop", stop_media_enabled);
	panel->childSetEnabled("media_pause", media_show_pause);
	panel->childSetVisible("media_pause", media_show_pause);
	panel->childSetVisible("media_play", !media_show_pause);

	panel->childSetColor("media_icon", media_icon_color);
}
