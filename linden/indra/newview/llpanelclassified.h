/** 
 * @file llpanelclassified.h
 * @brief LLPanelClassified class definition
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

// Display of a classified used both for the global view in the
// Find directory, and also for each individual user's classified in their
// profile.

#ifndef LL_LLPANELCLASSIFIED_H
#define LL_LLPANELCLASSIFIED_H

#include "llpanel.h"
#include "llclassifiedinfo.h"
#include "v3dmath.h"
#include "lluuid.h"
#include "llfloater.h"
//#include "llrect.h"

class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLIconCtrl;
class LLLineEditor;
class LLTextBox;
class LLTextEditor;
class LLTextureCtrl;
class LLUICtrl;
class LLMessageSystem;

class LLPanelClassified : public LLPanel
{
public:
    LLPanelClassified(bool in_finder, bool from_search);
    /*virtual*/ ~LLPanelClassified();

	void reset();

    /*virtual*/ BOOL postBuild();

    /*virtual*/ void draw();

	/*virtual*/ void refresh();

	void apply();

	// If can close, return TRUE.  If cannot close, pop save/discard dialog
	// and return FALSE.
	BOOL canClose();

	// Setup a new classified, including creating an id, giving a sane
	// initial position, etc.
	void initNewClassified();

	void setClassifiedID(const LLUUID& id);
	void setClickThroughText(const std::string& text);
	static void setClickThrough(const LLUUID& classified_id,
								S32 teleport, S32 map, S32 profile, bool from_new_table);

	// check that the title is valid (E.G. starts with a number or letter)
	BOOL titleIsValid();

	// Schedules the panel to request data
	// from the server next time it is drawn.
	void markForServerRequest();

	std::string getClassifiedName();
	const LLUUID& getClassifiedID() const { return mClassifiedID; }

    void sendClassifiedInfoRequest();
	void sendClassifiedInfoUpdate();
	void resetDirty();

    static void processClassifiedInfoReply(LLMessageSystem* msg, void**);

	// Confirmation dialogs flow in this order
	static void callbackConfirmMature(S32 option, void* data);
	void confirmMature(S32 option);
	void gotMature();
	static void callbackGotPriceForListing(S32 option, std::string text, void* data);
	static void callbackConfirmPublish(S32 option, void* data);
	void confirmPublish(S32 option);

	void sendClassifiedClickMessage(const std::string& type);

protected:
	static void saveCallback(S32 option, void* data);

	static void onClickUpdate(void* data);
    static void onClickTeleport(void* data);
    static void onClickMap(void* data);
	static void onClickProfile(void* data);
    static void onClickSet(void* data);

	static void focusReceived(LLFocusableElement* ctrl, void* data);
	static void onCommitAny(LLUICtrl* ctrl, void* data);

	void setDefaultAccessCombo(); // Default AO and PG regions to proper classified access
	
	BOOL checkDirty();		// Update and return mDirty

protected:
	bool mInFinder;
	bool mFromSearch;		// from web-based "All" search sidebar
	BOOL mDirty;
	bool mForceClose;
	bool mLocationChanged;
	LLUUID mClassifiedID;
	LLUUID mRequestedID;
	LLUUID mCreatorID;
	LLUUID mParcelID;
	S32 mPriceForListing;

	// Needed for stat tracking
	S32 mTeleportClicksOld;
	S32 mMapClicksOld;
	S32 mProfileClicksOld;
	S32 mTeleportClicksNew;
	S32 mMapClicksNew;
	S32 mProfileClicksNew;

	// Data will be requested on first draw
	BOOL mDataRequested;

	// For avatar panel classifieds only, has the user been charged
	// yet for this classified?  That is, have they saved once?
	BOOL mPaidFor;

	std::string mSimName;
	LLVector3d mPosGlobal;

	// Values the user may change
	LLTextureCtrl*	mSnapshotCtrl;
	LLLineEditor*	mNameEditor;
	LLTextEditor*	mDescEditor;
	LLLineEditor*	mLocationEditor;
	LLComboBox*		mCategoryCombo;
	LLComboBox*		mMatureCombo;
	LLCheckBoxCtrl* mAutoRenewCheck;

	LLButton*    mUpdateBtn;
	LLButton*    mTeleportBtn;
	LLButton*    mMapBtn;
	LLButton*	 mProfileBtn;

	LLTextBox*		mInfoText;
	LLButton*		mSetBtn;
	LLTextBox*		mClickThroughText;

	LLRect		mSnapshotSize;
	typedef std::list<LLPanelClassified*> panel_list_t;
	static panel_list_t sAllPanels;
};


class LLFloaterPriceForListing
: public LLFloater
{
public:
	LLFloaterPriceForListing();
	virtual ~LLFloaterPriceForListing();
	virtual BOOL postBuild();

	static void show( void (*callback)(S32 option, std::string value, void* userdata), void* userdata );

private:
	static void onClickSetPrice(void*);
	static void onClickCancel(void*);
	static void buttonCore(S32 button, void* data);

private:
	void (*mCallback)(S32 option, std::string, void*);
	void* mUserData;
};


#endif // LL_LLPANELCLASSIFIED_H
