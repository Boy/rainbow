/** 
 * @file lloverlaybar.cpp
 * @brief LLOverlayBar class implementation
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

// Temporary buttons that appear at the bottom of the screen when you
// are in a mode.

#include "llviewerprecompiledheaders.h"

#include "lloverlaybar.h"

#include "aoremotectrl.h"
#include "audioengine.h"
#include "llrender.h"
#include "llagent.h"
#include "llbutton.h"
#include "llchatbar.h"
#include "llfocusmgr.h"
#include "llimview.h"
#include "llmediaremotectrl.h"
#include "llpanelaudiovolume.h"
#include "llparcel.h"
#include "lltextbox.h"
#include "llui.h"
#include "llviewercontrol.h"
#include "llviewerimagelist.h"
#include "llviewermedia.h"
#include "llviewermenu.h"	// handle_reset_view()
#include "llviewermedia.h"
#include "llviewerparcelmedia.h"
#include "llviewerparcelmgr.h"
#include "lluictrlfactory.h"
#include "llviewerwindow.h"
#include "llvoiceclient.h"
#include "llvoavatar.h"
#include "llvoiceremotectrl.h"
#include "llwebbrowserctrl.h"
#include "llselectmgr.h"

//
// Globals
//

LLOverlayBar *gOverlayBar = NULL;

extern S32 MENU_BAR_HEIGHT;

//
// Functions
//



void* LLOverlayBar::createMediaRemote(void* userdata)
{
	LLOverlayBar *self = (LLOverlayBar*)userdata;	
	self->mMediaRemote =  new LLMediaRemoteCtrl ();
	return self->mMediaRemote;
}

void* LLOverlayBar::createVoiceRemote(void* userdata)
{
	LLOverlayBar *self = (LLOverlayBar*)userdata;	
	self->mVoiceRemote = new LLVoiceRemoteCtrl(std::string("voice_remote"));
	return self->mVoiceRemote;
}

void* LLOverlayBar::createAORemote(void* userdata)
{
	LLOverlayBar *self = (LLOverlayBar*)userdata;	
	self->mAORemote = new AORemoteCtrl();
	return self->mAORemote;
}

void* LLOverlayBar::createChatBar(void* userdata)
{
	gChatBar = new LLChatBar();
	return gChatBar;
}

LLOverlayBar::LLOverlayBar()
	:	LLPanel(),
		mMediaRemote(NULL),
		mVoiceRemote(NULL),
		mAORemote(NULL),
		mMusicState(STOPPED)
{
	setMouseOpaque(FALSE);
	setIsChrome(TRUE);

	mBuilt = false;

	LLCallbackMap::map_t factory_map;
	factory_map["media_remote"] = LLCallbackMap(LLOverlayBar::createMediaRemote, this);
	factory_map["voice_remote"] = LLCallbackMap(LLOverlayBar::createVoiceRemote, this);
	factory_map["ao_remote"] = LLCallbackMap(LLOverlayBar::createAORemote, this);
	factory_map["chat_bar"] = LLCallbackMap(LLOverlayBar::createChatBar, this);
	
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_overlaybar.xml", &factory_map);
}

BOOL LLOverlayBar::postBuild()
{
	childSetAction("IM Received",onClickIMReceived,this);
	childSetAction("Set Not Busy",onClickSetNotBusy,this);
	childSetAction("Mouselook",onClickMouselook,this);
	childSetAction("Stand Up",onClickStandUp,this);
	childSetVisible("chat_bar", gSavedSettings.getBOOL("ChatVisible"));

	setFocusRoot(TRUE);
	mBuilt = true;

	layoutButtons();
	return TRUE;
}

LLOverlayBar::~LLOverlayBar()
{
	// LLView destructor cleans up children
}

// virtual
void LLOverlayBar::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	LLView::reshape(width, height, called_from_parent);

	if (mBuilt) 
	{
		layoutButtons();
	}
}

void LLOverlayBar::layoutButtons()
{
	LLView* state_buttons_panel = getChildView("state_buttons");

	if (state_buttons_panel->getVisible())
	{
		LLViewQuery query;
		LLWidgetTypeFilter<LLButton> widget_filter;
		query.addPreFilter(LLEnabledFilter::getInstance());
		query.addPreFilter(&widget_filter);

		child_list_t button_list = query(state_buttons_panel);

		const S32 MAX_BAR_WIDTH = 600;
		S32 bar_width = llclamp(state_buttons_panel->getRect().getWidth(), 0, MAX_BAR_WIDTH);

		// calculate button widths
		const S32 MAX_BUTTON_WIDTH = 150;
		S32 segment_width = llclamp(lltrunc((F32)(bar_width) / (F32)button_list.size()), 0, MAX_BUTTON_WIDTH);
		S32 btn_width = segment_width - gSavedSettings.getS32("StatusBarPad");

		// Evenly space all buttons, starting from left
		S32 left = 0;
		S32 bottom = 1;

		for (child_list_reverse_iter_t child_iter = button_list.rbegin();
			child_iter != button_list.rend(); ++child_iter)
		{
			LLView *view = *child_iter;
			LLRect r = view->getRect();
			r.setOriginAndSize(left, bottom, btn_width, r.getHeight());
			view->setRect(r);
			left += segment_width;
		}
	}
}

// Per-frame updates of visibility
void LLOverlayBar::refresh()
{
	BOOL buttons_changed = FALSE;

	BOOL im_received = gIMMgr->getIMReceived();
	LLButton* button = getChild<LLButton>("IM Received");
	if (button && button->getVisible() != im_received)
	{
		button->setVisible(im_received);
		sendChildToFront(button);
		moveChildToBackOfTabGroup(button);
		buttons_changed = TRUE;
	}

	BOOL busy = gAgent.getBusy();
	button = getChild<LLButton>("Set Not Busy");
	if (button && button->getVisible() != busy)
	{
		button->setVisible(busy);
		sendChildToFront(button);
		moveChildToBackOfTabGroup(button);
		buttons_changed = TRUE;
	}

	BOOL mouselook_grabbed;
	mouselook_grabbed = gAgent.isControlGrabbed(CONTROL_ML_LBUTTON_DOWN_INDEX)
		|| gAgent.isControlGrabbed(CONTROL_ML_LBUTTON_UP_INDEX);
	button = getChild<LLButton>("Mouselook");

	if (button && button->getVisible() != mouselook_grabbed)
	{
		button->setVisible(mouselook_grabbed);
		sendChildToFront(button);
		moveChildToBackOfTabGroup(button);
		buttons_changed = TRUE;
	}

	BOOL sitting = FALSE;
	if (gAgent.getAvatarObject())
	{
//MK
		if (gRRenabled && gAgent.mRRInterface.mContainsUnsit)
		{
			sitting=FALSE;
		} else // sitting = true if agent is sitting
//mk	
		sitting = gAgent.getAvatarObject()->mIsSitting;
	}
	button = getChild<LLButton>("Stand Up");

	if (button && button->getVisible() != sitting)
	{
		button->setVisible(sitting);
		sendChildToFront(button);
		moveChildToBackOfTabGroup(button);
		buttons_changed = TRUE;
	}

	moveChildToBackOfTabGroup(mAORemote);
	moveChildToBackOfTabGroup(mMediaRemote);
	moveChildToBackOfTabGroup(mVoiceRemote);

	// turn off the whole bar in mouselook
	if (gAgent.cameraMouselook())
	{
		childSetVisible("media_remote_container", FALSE);
		childSetVisible("voice_remote_container", FALSE);
		childSetVisible("ao_remote_container", FALSE);
		childSetVisible("state_buttons", FALSE);
	}
	else
	{
		// update "remotes"
		childSetVisible("media_remote_container", TRUE);
		childSetVisible("voice_remote_container", LLVoiceClient::voiceEnabled());
		childSetVisible("ao_remote_container", gSavedSettings.getBOOL("EnableAORemote"));
		childSetVisible("state_buttons", TRUE);
	}

	// always let user toggle into and out of chatbar
	childSetVisible("chat_bar", gSavedSettings.getBOOL("ChatVisible"));

	if (buttons_changed)
	{
		layoutButtons();
	}
}

//-----------------------------------------------------------------------
// Static functions
//-----------------------------------------------------------------------

// static
void LLOverlayBar::onClickIMReceived(void*)
{
	gIMMgr->setFloaterOpen(TRUE);
}


// static
void LLOverlayBar::onClickSetNotBusy(void*)
{
	gAgent.clearBusy();
}


// static
void LLOverlayBar::onClickResetView(void* data)
{
	handle_reset_view();
}

//static
void LLOverlayBar::onClickMouselook(void*)
{
	gAgent.changeCameraToMouselook();
}

//static
void LLOverlayBar::onClickStandUp(void*)
{
//MK
	if (gRRenabled && gAgent.mRRInterface.mContainsUnsit)
	{
		if (gAgent.getAvatarObject() &&	gAgent.getAvatarObject()->mIsSitting)
		{
			return;
		}
	}
//mk
	LLSelectMgr::getInstance()->deselectAllForStandingUp();
	gAgent.setControlFlags(AGENT_CONTROL_STAND_UP);
}

////////////////////////////////////////////////////////////////////////////////
// static media helpers
// *TODO: Move this into an audio manager abstraction
//static
void LLOverlayBar::mediaStop(void*)
{
	if (!gOverlayBar)
	{
		return;
	}
	LLViewerParcelMedia::stop();
}
//static
void LLOverlayBar::toggleMediaPlay(void*)
{
	if (!gOverlayBar)
	{
		return;
	}

	
	if (LLViewerMedia::isMediaPaused())
	{
		LLViewerParcelMedia::start();
	}
	else if(LLViewerMedia::isMediaPlaying())
	{
		LLViewerParcelMedia::pause();
	}
	else
	{
		LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
		if (parcel)
		{
			LLViewerParcelMedia::sIsUserAction = true;
			LLViewerParcelMedia::play(parcel);
		}
	}
}

//static
void LLOverlayBar::toggleMusicPlay(void*)
{
	if (!gOverlayBar)
	{
		return;
	}
	
	if (gOverlayBar->mMusicState != PLAYING)
	{
		gOverlayBar->mMusicState = PLAYING; // desired state
		if (gAudiop)
		{
			LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
			if ( parcel )
			{
				// this doesn't work properly when crossing parcel boundaries - even when the 
				// stream is stopped, it doesn't return the right thing - commenting out for now.
	// 			if ( gAudiop->isInternetStreamPlaying() == 0 )
				{
					LLViewerParcelMedia::sIsUserAction = true;
					LLViewerParcelMedia::playStreamingMusic(parcel);
				}
			}
		}
	}
	//else
	//{
	//	gOverlayBar->mMusicState = PAUSED; // desired state
	//	if (gAudiop)
	//	{
	//		gAudiop->pauseInternetStream(1);
	//	}
	//}
	else
	{
		gOverlayBar->mMusicState = STOPPED; // desired state
		if (gAudiop)
		{
			gAudiop->stopInternetStream();
		}
	}
}

void LLOverlayBar::audioFilterPlay()
{
	if (gOverlayBar && gOverlayBar->mMusicState != PLAYING)
	{
		gOverlayBar->mMusicState = PLAYING;
	}
}

void LLOverlayBar::audioFilterStop()
{
	if (gOverlayBar && gOverlayBar->mMusicState != STOPPED)
	{
		gOverlayBar->mMusicState = STOPPED;
	}
}

