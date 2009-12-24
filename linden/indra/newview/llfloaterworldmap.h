/** 
 * @file llfloaterworldmap.h
 * @brief LLFloaterWorldMap class definition
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
 * Map of the entire world, with multiple background images,
 * avatar tracking, teleportation by double-click, etc.
 */

#ifndef LL_LLFLOATERWORLDMAP_H
#define LL_LLFLOATERWORLDMAP_H

#include "lldarray.h"
#include "llfloater.h"
#include "llhudtext.h"
#include "llmapimagetype.h"
#include "lltracker.h"

class LLEventInfo;
class LLFriendObserver;
class LLInventoryModel;
class LLInventoryObserver;
class LLItemInfo;
class LLTabContainer;
class LLWorldMapView;

class LLFloaterWorldMap : public LLFloater
{
public:
	LLFloaterWorldMap();
	virtual ~LLFloaterWorldMap();

	static void *createWorldMapView(void* data);
	BOOL postBuild();

	/*virtual*/ void onClose(bool app_quitting);

	static void show(void*, BOOL center_on_target );
	static void reloadIcons(void*);
	static void toggle(void*);
	static void hide(void*); 

	/*virtual*/ void reshape( S32 width, S32 height, BOOL called_from_parent = TRUE );
	/*virtual*/ BOOL handleHover(S32 x, S32 y, MASK mask);
	/*virtual*/ BOOL handleScrollWheel(S32 x, S32 y, S32 clicks);
	/*virtual*/ void setVisible(BOOL visible);
	/*virtual*/ void draw();

	// methods for dealing with inventory. The observe() method is
	// called during program startup. inventoryUpdated() will be
	// called by a helper object when an interesting change has
	// occurred.
	void observeInventory(LLInventoryModel* inventory);
	void inventoryChanged();

	// Calls for dealing with changes in friendship
	void observeFriends();
	void friendsChanged();

	// tracking methods
	void			trackAvatar( const LLUUID& avatar_id, const std::string& name );
	void			trackLandmark( const LLUUID& landmark_item_id ); 
	void			trackLocation(const LLVector3d& pos);
	void			trackEvent(const LLItemInfo &event_info);
	void			trackGenericItem(const LLItemInfo &item);
	void			trackURL(const std::string& region_name, S32 x_coord, S32 y_coord, S32 z_coord);

	static const LLUUID& getHomeID() { return sHomeID; }

	// A z_attenuation of 0.0f collapses the distance into the X-Y plane
	F32			getDistanceToDestination(const LLVector3d& pos_global, F32 z_attenuation = 0.5f) const;

	void			clearLocationSelection(BOOL clear_ui = FALSE);
	void			clearAvatarSelection(BOOL clear_ui = FALSE);
	void			clearLandmarkSelection(BOOL clear_ui = FALSE);

	// Adjust the maximally zoomed out limit of the zoom slider so you can
	// see the whole world, plus a little.
	void			adjustZoomSliderBounds();

	// Catch changes in the sim list
	void			updateSims(bool found_null_sim);

	// teleport to the tracked item, if there is one
	void			teleport();
//MK
    /*virtual*/ void    open();
//mk

protected:
	static void		onPanBtn( void* userdata );

	static void		onGoHome(void* data);

	static void		onLandmarkComboPrearrange( LLUICtrl* ctrl, void* data );
	static void		onLandmarkComboCommit( LLUICtrl* ctrl, void* data );

	static void		onAvatarComboPrearrange( LLUICtrl* ctrl, void* data );
	static void		onAvatarComboCommit( LLUICtrl* ctrl, void* data );

	static void		onCommitBackground(void* data, bool from_click);

	static void		onComboTextEntry( LLLineEditor* ctrl, void* data );
	static void		onSearchTextEntry( LLLineEditor* ctrl, void* data );

	static void		onClearBtn(void*);
	static void		onFlyBtn(void*);
	static void		onClickTeleportBtn(void*);
	static void		onShowTargetBtn(void*);
	static void		onShowAgentBtn(void*);
	static void		onCopySLURL(void*);

	static void onCheckEvents(LLUICtrl* ctrl, void*);

	void			centerOnTarget(BOOL animate);
	void			updateLocation();

	// fly to the tracked item, if there is one
	void			fly();

	void			buildLandmarkIDLists();
	static void		onGoToLandmarkDialog(S32 option,void* userdata);
	void			flyToLandmark();
	void			teleportToLandmark();
	void			setLandmarkVisited();

	void			buildAvatarIDList();
	void			flyToAvatar();
	void			teleportToAvatar();

	static void		updateSearchEnabled( LLUICtrl* ctrl, void* userdata );
	static void		onLocationFocusChanged( LLFocusableElement* ctrl, void* userdata );
	static void		onLocationCommit( void* userdata );
	static void		onCommitLocation( LLUICtrl* ctrl, void* userdata );
	static void		onCommitSearchResult( LLUICtrl* ctrl, void* userdata );

	void			cacheLandmarkPosition();

protected:
	LLTabContainer*	mTabs;

	// Sets gMapScale, in pixels per region
	F32						mCurZoomVal;
	LLFrameTimer			mZoomTimer;

	LLDynamicArray<LLUUID>	mLandmarkAssetIDList;
	LLDynamicArray<LLUUID>	mLandmarkItemIDList;
	BOOL					mHasLandmarkPosition;

	static const LLUUID	sHomeID;

	LLInventoryModel* mInventory;
	LLInventoryObserver* mInventoryObserver;
	LLFriendObserver* mFriendObserver;

	std::string				mCompletingRegionName;
	std::string				mLastRegionName;
	BOOL					mWaitingForTracker;
	BOOL					mExactMatch;

	BOOL					mIsClosing;
	BOOL					mSetToUserPosition;

	LLVector3d				mTrackedLocation;
	LLTracker::ETrackingStatus mTrackedStatus;
	std::string				mTrackedSimName;
	std::string				mTrackedAvatarName;
	std::string				mSLURL;
};

extern LLFloaterWorldMap* gFloaterWorldMap;

#endif

