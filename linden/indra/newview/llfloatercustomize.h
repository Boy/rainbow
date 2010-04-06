/** 
 * @file llfloatercustomize.h
 * @brief The customize avatar floater, triggered by "Appearance..."
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

#ifndef LL_LLFLOATERCUSTOMIZE_H
#define LL_LLFLOATERCUSTOMIZE_H

#include <map>

#include "llfloater.h"
#include "llstring.h"
#include "v3dmath.h"
#include "lltimer.h"
#include "llundo.h"
#include "llviewermenu.h"
#include "llwearable.h"
#include "lliconctrl.h"

class LLButton;
class LLIconCtrl;
class LLColorSwatchCtrl;
class LLGenePool;
class LLInventoryObserver;
class LLJoint;
class LLLineEditor;
class LLMakeOutfitDialog;
class LLRadioGroup;
class LLScrollableContainerView;
class LLScrollingPanelList;
class LLTabContainerVertical;
class LLTextBox;
class LLTextureCtrl;
class LLViewerJointMesh;
class LLViewerVisualParam;
class LLVisualParam;
class LLVisualParamReset;
class LLWearableSaveAsDialog;
class LLPanelEditWearable;

/////////////////////////////////////////////////////////////////////
// LLFloaterCustomize

class LLFloaterCustomize : public LLFloater
{
public:
	typedef std::pair<BOOL, LLViewerVisualParam*> editable_param;
	typedef std::map<F32, editable_param> param_map;

public:
	LLFloaterCustomize();
	virtual ~LLFloaterCustomize();
	virtual BOOL 	postBuild();

	// Inherted methods from LLFloater (and above)
	virtual void	onClose(bool app_quitting);
	virtual void	draw();

	// New methods
	void			clearScrollingPanelList();
	void			generateVisualParamHints(LLViewerJointMesh* joint_mesh,
											 param_map& params);

	const std::string& getEditGroup();
	void			addVisualParamToUndoBuffer( LLViewerVisualParam* param, F32 current_weight );

	void 			updateScrollingPanelList(BOOL allow_modify);

	void			setWearable(EWearableType type, LLWearable* wearable, U32 perm_mask, BOOL is_complete);
	LLPanelEditWearable* getCurrentWearablePanel() { return mWearablePanelList[ sCurrentWearableType ]; }

	virtual BOOL	isDirty() const;

	void			askToSaveAllIfDirty( void(*next_step_callback)(BOOL proceed, void* userdata), void* userdata );

	void			spawnWearableAppearance( EWearableType type );

	void			switchToDefaultSubpart();

	static void		setCurrentWearableType( EWearableType type );
	static EWearableType getCurrentWearableType()					{ return sCurrentWearableType; }

	// Callbacks
	// reX: new function (thx dudes)
	static void		onBtnImport( void* userdata );
	// reX: new function	
	static void		onBtnExport( void* userdata );
	static void		onBtnSaveAll( void* userdata );
	static void		onBtnSnapshot( void* userdata );
	static void		onBtnMakeOutfit( void* userdata );
	static void		onMakeOutfitCommit( LLMakeOutfitDialog* dialog, void* userdata );

	static void		onTabChanged( void* userdata, bool from_click );
	static void		onSaveAllDialog( S32 option, void* userdata );

	void fetchInventory();
	void updateInventoryUI();
	void updateScrollingPanelUI();

protected:
	LLPanelEditWearable*	mWearablePanelList[ WT_COUNT ];

	static EWearableType	sCurrentWearableType;

	LLScrollingPanelList*	mScrollingPanelList;
	LLScrollableContainerView* mScrollContainer;
	LLGenePool*				mGenePool;
	LLVisualParamReset*		mResetParams;

	LLInventoryObserver* mInventoryObserver;

	void					(*mNextStepAfterSaveAllCallback)(BOOL proceed, void* userdata);
	void*					mNextStepAfterSaveAllUserdata;


protected:
	
	static void* createWearablePanel(void* userdata);
	
	void			initWearablePanels();
	void			initScrollingPanelList();
};

extern LLFloaterCustomize* gFloaterCustomize;


#endif  // LL_LLFLOATERCUSTOMIZE_H
