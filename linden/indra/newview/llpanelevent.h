/** 
 * @file llpanelevent.h
 * @brief Display for events in the finder
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

#ifndef LL_LLPANELEVENT_H
#define LL_LLPANELEVENT_H

#include "llpanel.h"

#include "lleventinfo.h"
#include "lluuid.h"
#include "v3dmath.h"

class LLTextBox;
class LLTextEditor;
class LLButton;
class LLMessageSystem;

class LLPanelEvent : public LLPanel
{
public:
	LLPanelEvent();
	/*virtual*/ ~LLPanelEvent();

	/*virtual*/ BOOL postBuild();
	/*virtual*/ void draw();

	void setEventID(const U32 event_id);
	void sendEventInfoRequest();

	static void processEventInfoReply(LLMessageSystem *msg, void **);

	U32 getEventID() { return mEventID; }

protected:
	void resetInfo();

	static void onClickTeleport(void*);
	static void onClickMap(void*);
	//static void onClickLandmark(void*);
	static void onClickCreateEvent(void*);
	static void onClickNotify(void*);

	static void callbackCreateEventWebPage(S32 options, void* data);

protected:
	U32				mEventID;
	LLEventInfo		mEventInfo;

	LLTextBox*		mTBName;
	LLTextBox*		mTBCategory;
	LLTextBox*		mTBDate;
	LLTextBox*		mTBDuration;
	LLTextEditor*	mTBDesc;

	LLTextBox*		mTBRunBy;
	LLTextBox*		mTBLocation;
	LLTextBox*		mTBCover;

	LLButton*		mTeleportBtn;
	LLButton*		mMapBtn;
	LLButton*		mCreateEventBtn;
	LLButton*		mNotifyBtn;

	typedef std::list<LLPanelEvent*> panel_list_t;
	static panel_list_t sAllPanels;
};

#endif // LL_LLPANELEVENT_H
