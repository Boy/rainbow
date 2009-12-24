/** 
 * @file lltoolselect.h
 * @brief LLToolSelect class header file
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

#ifndef LL_TOOLSELECT_H
#define LL_TOOLSELECT_H

#include "lltool.h"
#include "v3math.h"
#include "lluuid.h"
#include "llviewerwindow.h" // for LLPickInfo

class LLObjectSelection;

class LLToolSelect : public LLTool, public LLSingleton<LLToolSelect>
{
public:
	LLToolSelect( LLToolComposite* composite );

	virtual BOOL		handleMouseDown(S32 x, S32 y, MASK mask);
	virtual BOOL		handleMouseUp(S32 x, S32 y, MASK mask);

	virtual void		stopEditing();

	static LLSafeHandle<LLObjectSelection>	handleObjectSelection(const LLPickInfo& pick, BOOL ignore_group, BOOL temp_select, BOOL select_root = FALSE);

	virtual void		onMouseCaptureLost();
	virtual void		handleDeselect();

protected:
	BOOL				mIgnoreGroup;
	LLUUID				mSelectObjectID;
	LLPickInfo			mPick;
};


#endif  // LL_TOOLSELECTION_H
