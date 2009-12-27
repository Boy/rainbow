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

#include "audioengine.h"
#include "llrender.h"
#include "llagent.h"
#include "llbutton.h"
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
//MK
extern BOOL RRenabled;
//mk

//
// Functions
//


//static
void* LLOverlayBar::createMasterRemote(void* userdata)
{
	LLOverlayBar *self = (LLOverlayBar*)userdata;	
	self->mMasterRemote =  new LLMediaRemoteCtrl ( "master_volume",
												   "volume",
												   LLRect(),
												   "panel_master_volume.xml");
	return self->mMasterRemote;
}

void* LLOverlayBar::createMediaRemote(void* userdata)
{
	LLOverlayBar *self = (LLOverlayBar*)userdata;	
	self->mMediaRemote =  new LLMediaRemoteCtrl ( "media_remote",
												  "media",
												  LLRect(),
												  "panel_media_remote.xml");
	return self->mMediaRemote;
}

void* LLOverlayBar::createMusicRemote(void* userdata)
{
	LLOverlayBar *self = (LLOverlayBar*)userdata;
	self->mMusicRemote =  new LLMediaRemoteCtrl ( "music_remote",
												  "music",
												  LLRect(),
												  "panel_music_remote.xml" );		
	return self->mMusicRemote;
}

void* LLOverlayBar::createVoiceRemote(void* userdata)
{
	LLOverlayBar *self = (LLOverlayBar*)userdata;	
	self->mVoiceRemote = new LLVoiceRemoteCtrl(std::string("voice_remote"));
	return self->mVoiceRemote;
}




LLOverlayBar::LLOverlayBar(const std::string& name, const LLRect& rect)
	:	LLPanel(name, rect, FALSE),		// not bordered
		mMasterRemote(NULL),
		mMusicRemote(NULL),
		mMediaRemote(NULL),
		mVoiceRemote(NULL),
		mMediaState(STOPPED),
		mMusicState(STOPPED)
{
	setMouseOpaque(FALSE);
	setIsChrome(TRUE);

	mBuilt = false;

	LLCallbackMap::map_t factory_map;
	factory_map["master_volume"] = LLCallbackMap(LLOverlayBar::createMasterRemote, this);
	factory_map["media_remote"] = LLCallbackMap(LLOverlayBar::createMediaRemote, this);
	factory_map["music_remote"] = LLCallbackMap(LLOverlayBar::createMusicRemote, this);
	factory_map["voice_remote"] = LLCallbackMap(LLOverlayBar::createVoiceRemote, this);
	
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_overlaybar.xml", &factory_map);
	
	childSetAction("IM Received",onClickIMReceived,this);
	childSetAction("Set Not Busy",onClickSetNotBusy,this);
	childSetAction("Mouselook",onClickMouselook,this);
	childSetAction("Stand Up",onClickStandUp,this);

	setFocusRoot(TRUE);
	mBuilt = true;

	// make overlay bar conform to window size
	setRect(rect);
	layoutButtons();
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
	S32 width = getRect().getWidth();
	if (width > 1024) width = 1024;

	S32 count = getChildCount();
	const S32 PAD = gSavedSettings.getS32("StatusBarPad");

	const S32 num_media_controls = 3;
	media_remote_width = mMediaRemote ? mMediaRemote->getRect().getWidth() : 0;
	music_remote_width = mMusicRemote ? mMusicRemote->getRect().getWidth() : 0;
	voice_remote_width = mVoiceRemote ? mVoiceRemote->getRect().getWidth() : 0;
	master_remote_width = mMasterRemote ? mMasterRemote->getRect().getWidth() : 0;

	// total reserved width for all media remotes
	const S32 ENDPAD = 8;
	S32 remote_total_width = media_remote_width + PAD + music_remote_width + PAD + voice_remote_width + PAD + master_remote_width + ENDPAD;

	// calculate button widths
	F32 segment_width = (F32)(width - remote_total_width) / (F32)(count - num_media_controls);

	S32 btn_width = lltrunc(segment_width - PAD);

	// Evenly space all views
	LLRect r;
	S32 i = 0;
	for (child_list_const_iter_t child_iter = getChildList()->begin();
		 child_iter != getChildList()->end(); ++child_iter)
	{
		LLView *view = *child_iter;
		r = view->getRect();
		r.mLeft = (width) - llround(remote_total_width + (i-num_media_controls+1)*segment_width);
		r.mRight = r.mLeft + btn_width;
		view->setRect(r);
		i++;
	}

	// Fix up remotes to have constant width because they can't shrink
	S32 right = getRect().getWidth() - ENDPAD;
	if (mMasterRemote)
	{
		r = mMasterRemote->getRect();
		r.mRight = right;
		r.mLeft = right - master_remote_width;
		right = r.mLeft - PAD;
		mMasterRemote->setRect(r);
	}
	if (mMusicRemote)
	{
		r = mMusicRemote->getRect();
		r.mRight = right;
		r.mLeft = right - music_remote_width;
		right = r.mLeft - PAD;
		mMusicRemote->setRect(r);
	}
	if (mMediaRemote)
	{
		r = mMediaRemote->getRect();
		r.mRight = right;
		r.mLeft = right - media_remote_width;
		right = r.mLeft - PAD;
		mMediaRemote->setRect(r);
	}
	if (mVoiceRemote)
	{
		r = mVoiceRemote->getRect();
		r.mRight = right;
		r.mLeft = right - voice_remote_width;
		mVoiceRemote->setRect(r);
	}

	updateBoundingRect();
}

void LLOverlayBar::draw()
{
	// retrieve rounded rect image
	LLUIImagePtr imagep = LLUI::getUIImage("rounded_square.tga");

	if (imagep)
	{
		const S32 PAD = gSavedSettings.getS32("StatusBarPad");

		gGL.getTexUnit(0)->bind(imagep->getImage());

		// draw rounded rect tabs behind all children
		LLRect r;
		// focus highlights
		LLColor4 color = gColors.getColor("FloaterFocusBorderColor");
		gGL.color4fv(color.mV);
		if(gFocusMgr.childHasKeyboardFocus(gBottomPanel))
		{
			for (child_list_const_iter_t child_iter = getChildList()->begin();
				child_iter != getChildList()->end(); ++child_iter)
			{
				LLView *view = *child_iter;
				if(view->getEnabled() && view->getVisible())
				{
					r = view->getRect();
					gl_segmented_rect_2d_tex(r.mLeft - PAD/3 - 1, 
											r.mTop + 3, 
											r.mRight + PAD/3 + 1,
											r.mBottom, 
											imagep->getTextureWidth(), 
											imagep->getTextureHeight(), 
											16, 
											ROUNDED_RECT_TOP);
				}
			}
		}

		// main tabs
		for (child_list_const_iter_t child_iter = getChildList()->begin();
			child_iter != getChildList()->end(); ++child_iter)
		{
			LLView *view = *child_iter;
			if(view->getEnabled() && view->getVisible())
			{
				r = view->getRect();
				// draw a nice little pseudo-3D outline
				color = gColors.getColor("DefaultShadowDark");
				gGL.color4fv(color.mV);
				gl_segmented_rect_2d_tex(r.mLeft - PAD/3 + 1, r.mTop + 2, r.mRight + PAD/3, r.mBottom, 
										 imagep->getTextureWidth(), imagep->getTextureHeight(), 16, ROUNDED_RECT_TOP);
				color = gColors.getColor("DefaultHighlightLight");
				gGL.color4fv(color.mV);
				gl_segmented_rect_2d_tex(r.mLeft - PAD/3, r.mTop + 2, r.mRight + PAD/3 - 3, r.mBottom, 
										 imagep->getTextureWidth(), imagep->getTextureHeight(), 16, ROUNDED_RECT_TOP);
				// here's the main background.  Note that it overhangs on the bottom so as to hide the
				// focus highlight on the bottom panel, thus producing the illusion that the focus highlight
				// continues around the tabs
				color = gColors.getColor("FocusBackgroundColor");
				gGL.color4fv(color.mV);
				gl_segmented_rect_2d_tex(r.mLeft - PAD/3 + 1, r.mTop + 1, r.mRight + PAD/3 - 1, r.mBottom - 1, 
										 imagep->getTextureWidth(), imagep->getTextureHeight(), 16, ROUNDED_RECT_TOP);
			}
		}
	}

	// draw children on top
	LLPanel::draw();
}

// Per-frame updates of visibility
void LLOverlayBar::refresh()
{
	BOOL im_received = gIMMgr->getIMReceived();
	childSetVisible("IM Received", im_received);
	childSetEnabled("IM Received", im_received);

	BOOL busy = gAgent.getBusy();
	childSetVisible("Set Not Busy", busy);
	childSetEnabled("Set Not Busy", busy);


	BOOL mouselook_grabbed;
	mouselook_grabbed = gAgent.isControlGrabbed(CONTROL_ML_LBUTTON_DOWN_INDEX)
						|| gAgent.isControlGrabbed(CONTROL_ML_LBUTTON_UP_INDEX);

	
	childSetVisible("Mouselook", mouselook_grabbed);
	childSetEnabled("Mouselook", mouselook_grabbed);

	BOOL sitting = FALSE;
	if (gAgent.getAvatarObject())
	{
//MK
		if (RRenabled && gAgent.mRRInterface.mContainsUnsit)
		{
			sitting=FALSE;
		} else // sitting = true if agent is sitting
//mk	
		sitting = gAgent.getAvatarObject()->mIsSitting;
		childSetVisible("Stand Up", sitting);
		childSetEnabled("Stand Up", sitting);
		
	}

	const S32 PAD = gSavedSettings.getS32("StatusBarPad");
	const S32 ENDPAD = 8;
	S32 right = getRect().getWidth() - master_remote_width - PAD - ENDPAD;
	LLRect r;

	BOOL master_remote = !gSavedSettings.getBOOL("HideMasterRemote");

	LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();

	if (mMusicRemote && gAudiop)
	{
		if (!parcel 
			|| parcel->getMusicURL().empty()
			|| !gSavedSettings.getBOOL("AudioStreamingMusic"))
		{
			mMusicRemote->setVisible(FALSE);
			mMusicRemote->setEnabled(FALSE);
		}
		else
		{
			mMusicRemote->setEnabled(TRUE);
			r = mMusicRemote->getRect();
			r.mRight = right;
			r.mLeft = right - music_remote_width;
			right = r.mLeft - PAD;
			mMusicRemote->setRect(r);
			mMusicRemote->setVisible(TRUE);
			master_remote = TRUE;
		}
	}

	if (mMediaRemote)
	{
		if (parcel && parcel->getMediaURL()[0] &&
			gSavedSettings.getBOOL("AudioStreamingVideo"))
		{
			// display remote control 
			mMediaRemote->setEnabled(TRUE);
			r = mMediaRemote->getRect();
			r.mRight = right;
			r.mLeft = right - media_remote_width;
			right = r.mLeft - PAD;
			mMediaRemote->setRect(r);
			mMediaRemote->setVisible(TRUE);
			master_remote = TRUE;
		}
		else
		{
			mMediaRemote->setVisible(FALSE);
			mMediaRemote->setEnabled(FALSE);
		}
	}
	if (mVoiceRemote)
	{
		if (LLVoiceClient::voiceEnabled())
		{
			r = mVoiceRemote->getRect();
			r.mRight = right;
			r.mLeft = right - voice_remote_width;
			mVoiceRemote->setRect(r);
			mVoiceRemote->setVisible(TRUE);
			master_remote = TRUE;
		}
		else
		{
			mVoiceRemote->setVisible(FALSE);
		}
	}

	mMasterRemote->setVisible(master_remote);
	mMasterRemote->setEnabled(master_remote);

	// turn off the whole bar in mouselook
	if (gAgent.cameraMouselook())
	{
		setVisible(FALSE);
	}
	else
	{
		setVisible(TRUE);
	}

	updateBoundingRect();
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
	if (RRenabled && gAgent.mRRInterface.mContainsUnsit) {
		if (gAgent.getAvatarObject() &&
			gAgent.getAvatarObject()->mIsSitting) {
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
void LLOverlayBar::mediaPlay(void*)
{
	if (!gOverlayBar)
	{
		return;
	}
	gOverlayBar->mMediaState = PLAYING; // desired state
	LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	if (parcel)
	{
		std::string path("");
		LLViewerParcelMedia::play(parcel);
	}
}
//static
void LLOverlayBar::mediaPause(void*)
{
	if (!gOverlayBar)
	{
		return;
	}
	gOverlayBar->mMediaState = PAUSED; // desired state
	LLViewerParcelMedia::pause();
}
//static
void LLOverlayBar::mediaStop(void*)
{
	if (!gOverlayBar)
	{
		return;
	}
	gOverlayBar->mMediaState = STOPPED; // desired state
	LLViewerParcelMedia::stop();
}

//static
void LLOverlayBar::musicPlay(void*)
{
	if (!gOverlayBar)
	{
		return;
	}
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
				gAudiop->startInternetStream(parcel->getMusicURL().c_str());
			}
		}
	}
}
//static
void LLOverlayBar::musicPause(void*)
{
	if (!gOverlayBar)
	{
		return;
	}
	gOverlayBar->mMusicState = PAUSED; // desired state
	if (gAudiop)
	{
		gAudiop->pauseInternetStream(1);
	}
}
//static
void LLOverlayBar::musicStop(void*)
{
	if (!gOverlayBar)
	{
		return;
	}
	gOverlayBar->mMusicState = STOPPED; // desired state
	if (gAudiop)
	{
		gAudiop->stopInternetStream();
	}
}

void LLOverlayBar::toggleAudioVolumeFloater(void* user_data)
{
	LLFloaterAudioVolume::toggleInstance(LLSD());
}
