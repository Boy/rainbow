/** 
 * @file llfloatertos.h
 * @brief Terms of Service Agreement dialog
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

#ifndef LL_LLFLOATERTOS_H
#define LL_LLFLOATERTOS_H

#include "llmodaldialog.h"
#include "llassetstorage.h"
#include "llwebbrowserctrl.h"

class LLButton;
class LLRadioGroup;
class LLVFS;
class LLTextEditor;
class LLUUID;

class LLFloaterTOS : 
	public LLModalDialog,
	public LLWebBrowserCtrlObserver
{
public:
	virtual ~LLFloaterTOS();

	// Types of dialog.
	enum ETOSType
	{
		TOS_TOS = 0,
		TOS_CRITICAL_MESSAGE = 1
	};

	// Asset_id is overwritten with LLUUID::null when agree is clicked.
	static LLFloaterTOS* show(ETOSType type, const std::string & message);

	BOOL postBuild();
	
	virtual void draw();

	static void		updateAgree( LLUICtrl *, void* userdata );
	static void		onContinue( void* userdata );
	static void		onCancel( void* userdata );

	void			setSiteIsAlive( bool alive );

	virtual void onNavigateComplete( const EventType& eventIn );

private:
	// Asset_id is overwritten with LLUUID::null when agree is clicked.
	LLFloaterTOS(ETOSType type, const std::string & message);

private:
	ETOSType		mType;
	std::string		mMessage;
	int				mWebBrowserWindowId;
	int				mLoadCompleteCount;

	static LLFloaterTOS* sInstance;
};

#endif // LL_LLFLOATERTOS_H
