/** 
 * @file llpanellogin.h
 * @brief Login username entry fields.
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

#ifndef LL_LLPANELLOGIN_H
#define LL_LLPANELLOGIN_H

#include "llpanel.h"
#include "llcommandhandler.h"
#include "lldbstrings.h"
#include "llmemory.h"
#include "llviewerimage.h"
#include "llstring.h"
#include "llmd5.h"
#include "llwebbrowserctrl.h"

class LLTextBox;
class LLLineEditor;
class LLCheckBoxCtrl;
class LLButton;
class LLComboBox;


class LLLoginHandler : public LLCommandHandler
{
 public:
	// allow from external browsers
	LLLoginHandler() : LLCommandHandler("login", true) { }
	bool handle(const LLSD& tokens, const LLSD& queryMap);
	bool parseDirectLogin(std::string url);
	void parse(const LLSD& queryMap);

	LLUUID mWebLoginKey;
	std::string mFirstName;
	std::string mLastName;
};

extern LLLoginHandler gLoginHandler;

class LLPanelLogin:	
	public LLPanel,
	public LLWebBrowserCtrlObserver
{
	LOG_CLASS(LLPanelLogin);
public:
	LLPanelLogin(const LLRect &rect, BOOL show_server, 
				void (*callback)(S32 option, void* user_data),
				void *callback_data);
	~LLPanelLogin();

	virtual BOOL handleKeyHere(KEY key, MASK mask);
	virtual void draw();
	virtual void setFocus( BOOL b );

	static void show(const LLRect &rect, BOOL show_server, 
		void (*callback)(S32 option, void* user_data), 
		void* callback_data);

	static void setFields(const std::string& firstname, const std::string& lastname, 
		const std::string& password, BOOL remember);

	static void addServer(const std::string& server, S32 domain_name);
	static void refreshLocation( bool force_visible );

	static void getFields(std::string& firstname, std::string& lastname,
						  std::string& password, BOOL& remember);

	static BOOL isGridComboDirty();
	static void getLocation(std::string &location);

	static void close();

	void setSiteIsAlive( bool alive );

	static void loadLoginPage();	
	static void giveFocus();
	static void setAlwaysRefresh(bool refresh); 
	static void mungePassword(LLUICtrl* caller, void* user_data);
	
private:
	static void onClickConnect(void*);
	static void onClickNewAccount(void*);
	static void newAccountAlertCallback(S32 option, void*);
	static void onClickQuit(void*);
	static void onClickVersion(void*);
	virtual void onNavigateComplete( const EventType& eventIn );
	static void onClickForgotPassword(void*);
	static void onPassKey(LLLineEditor* caller, void* user_data);
	static void onSelectServer(LLUICtrl*, void*);
	static void onServerComboLostFocus(LLFocusableElement*, void*);
	
private:
	LLPointer<LLUIImage> mLogoImage;

	void			(*mCallback)(S32 option, void *userdata);
	void*			mCallbackData;

	std::string mIncomingPassword;
	std::string mMungedPassword;

	static LLPanelLogin* sInstance;
	static BOOL		sCapslockDidNotification;
	BOOL			mHtmlAvailable;
};

#endif
