/** 
 * @file llloginhandler.cpp
 * @brief Handles filling in the login panel information from a SLURL
 * such as secondlife:///app/login?first=Bob&last=Dobbs
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 * 
 * Copyright (c) 2008-2010, Linden Research, Inc.
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
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
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
#include "llviewerprecompiledheaders.h"

#include "llloginhandler.h"

// viewer includes
#include "llpanellogin.h"			// save_password_to_disk()
#include "llstartup.h"				// getStartupState()
#include "llurlsimstring.h"
#include "llviewercontrol.h"		// gSavedSettings
#include "llviewernetwork.h"		// EGridInfo

// library includes
#include "llmd5.h"


// Must have instance to auto-register with LLCommandDispatcher
LLLoginHandler gLoginHandler;


//parses the input url and returns true if afterwards
//a web-login-key, firstname and lastname  is set
bool LLLoginHandler::parseDirectLogin(std::string url)
{
	LLURI uri(url);
	parse(uri.queryMap());

	if (mWebLoginKey.isNull() ||
		mFirstName.empty() ||
		mLastName.empty())
	{
		return false;
	}
	else
	{
		return true;
	}
}


void LLLoginHandler::parse(const LLSD& queryMap)
{
	mWebLoginKey = queryMap["web_login_key"].asUUID();
	mFirstName = queryMap["first_name"].asString();
	mLastName = queryMap["last_name"].asString();

	LLViewerLogin::getInstance()->setGridChoice(queryMap["grid"].asString()); // Note: this call is no op when string is empty.

	std::string startLocation = queryMap["location"].asString();

	if (startLocation == "specify")
	{
		LLURLSimString::setString(queryMap["region"].asString());
	}
	else if (startLocation == "home")
	{
		gSavedSettings.setBOOL("LoginLastLocation", FALSE);
		LLURLSimString::setString(LLStringUtil::null);
	}
	else if (startLocation == "last")
	{
		gSavedSettings.setBOOL("LoginLastLocation", TRUE);
		LLURLSimString::setString(LLStringUtil::null);
	}
}

bool LLLoginHandler::handle(const LLSD& tokens,
							const LLSD& query_map,
							LLWebBrowserCtrl* web)
{	
	parse(query_map);
	
	//if we haven't initialized stuff yet, this is 
	//coming in from the GURL handler, just parse
	if (STATE_FIRST == LLStartUp::getStartupState())
	{
		return true;
	}
	
	std::string password = query_map["password"].asString();

	if (!password.empty())
	{
		gSavedSettings.setBOOL("RememberPassword", TRUE);

		if (password.substr(0,3) != "$1$")
		{
			LLMD5 pass((unsigned char*)password.c_str());
			char md5pass[33];		/* Flawfinder: ignore */
			pass.hex_digest(md5pass);
			std::string hashed_password = ll_safe_string(md5pass, 32);
			LLStartUp::savePasswordToDisk(hashed_password);
		}
	}
			

	if (LLStartUp::getStartupState() < STATE_LOGIN_CLEANUP)  //on splash page
	{
		if (!mFirstName.empty() || !mLastName.empty())
		{
			// Fill in the name, and maybe the password
			LLPanelLogin::setFields(mFirstName, mLastName, password);
		}

		if (mWebLoginKey.isNull())
		{
			LLPanelLogin::loadLoginPage();
		}
		else
		{
			LLStartUp::setStartupState( STATE_LOGIN_CLEANUP );
		}
	}
	return true;
}
