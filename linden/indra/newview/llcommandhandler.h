/**
 * @file llcommandhandler.h
 * @brief Central registry for text-driven "commands", most of
 * which manipulate user interface.  For example, the command
 * "agent (uuid) about" will open the UI for an avatar's profile.
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
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
#ifndef LLCOMMANDHANDLER_H
#define LLCOMMANDHANDLER_H

/* To implement a command "foo" that takes one parameter,
   a UUID, do this:

class LLFooHandler : public LLCommandHandler
{
public:
    // Inform the system you handle commands starting
	// with "foo" and they are not allowed from external web
	// browser links.
	LLFooHandler() : LLCommandHandler("foo", false) { }

    // Your code here
	bool handle(const LLSD& tokens, const LLSD& queryMap)
	{
		if (tokens.size() < 1) return false;
		LLUUID id( tokens[0] );
		return doFoo(id);
	}
};

// Creating the object registers with the dispatcher.
LLFooHandler gFooHandler;
*/

class LLCommandHandler
{
public:
	LLCommandHandler(const char* command, bool allow_from_external_browser);
		// Automatically registers object to get called when 
		// command is executed.  All commands can be processed
		// in links from LLWebBrowserCtrl, but some (like teleport)
		// should not be allowed from outside the app.
		
	virtual ~LLCommandHandler();

	virtual bool handle(const LLSD& params,
						const LLSD& queryMap) = 0;
		// Execute the command with a provided (possibly empty)
		// list of parameters.
		// Return true if you did something, false if the parameters
		// are invalid or on error.
};


class LLCommandDispatcher
{
public:
	static bool dispatch(const std::string& cmd,
						 bool from_external_browser,
						 const LLSD& params,
						 const LLSD& queryMap);
		// Execute a command registered via the above mechanism,
		// passing string parameters.
		// Returns true if command was found and executed correctly.
};

#endif
