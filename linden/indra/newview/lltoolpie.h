/** 
 * @file lltoolpie.h
 * @brief LLToolPie class header file
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#ifndef LL_TOOLPIE_H
#define LL_TOOLPIE_H

#include "lltool.h"
#include "lluuid.h"
#include "llviewerwindow.h" // for LLPickInfo

class LLViewerObject;
class LLObjectSelection;

class LLToolPie : public LLTool, public LLSingleton<LLToolPie>
{
public:
	LLToolPie( );

	virtual BOOL		handleMouseDown(S32 x, S32 y, MASK mask);
	virtual BOOL		handleRightMouseDown(S32 x, S32 y, MASK mask);
	virtual BOOL		handleMouseUp(S32 x, S32 y, MASK mask);
	virtual BOOL		handleRightMouseUp(S32 x, S32 y, MASK mask);
	virtual BOOL		handleHover(S32 x, S32 y, MASK mask);
	virtual BOOL		handleDoubleClick(S32 x, S32 y, MASK mask);

	virtual void		render();

	virtual void		stopEditing();

	virtual void		onMouseCaptureLost();
	virtual void		handleDeselect();
	virtual LLTool*		getOverrideTool(MASK mask);

	LLPickInfo&			getPick() { return mPick; }
	U8					getClickAction() { return mClickAction; }
	LLViewerObject*		getClickActionObject() { return mClickActionObject; }
	LLObjectSelection*	getLeftClickSelection() { return (LLObjectSelection*)mLeftClickSelection; }
	void 				resetSelection();
	
	static void			leftMouseCallback(const LLPickInfo& pick_info);
	static void			rightMouseCallback(const LLPickInfo& pick_info);

	static void			selectionPropertiesReceived();


private:
	BOOL outsideSlop(S32 x, S32 y, S32 start_x, S32 start_y);
	BOOL pickAndShowMenu(BOOL edit_menu);
	BOOL useClickAction(BOOL always_show, MASK mask, LLViewerObject* object,
						LLViewerObject* parent);

private:
	BOOL				mPieMouseButtonDown;
	BOOL				mGrabMouseButtonDown;
	BOOL				mMouseOutsideSlop;				// for this drag, has mouse moved outside slop region
	LLPickInfo			mPick;
	LLPointer<LLViewerObject> mClickActionObject;
	U8					mClickAction;
	LLSafeHandle<LLObjectSelection> mLeftClickSelection;
};


#endif
