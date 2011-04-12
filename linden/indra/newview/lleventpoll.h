/** 
 * @file lleventpoll.h
 * @brief LLEvDescription of the LLEventPoll class.
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 * 
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#ifndef LL_LLEVENTPOLL_H
#define LL_LLEVENTPOLL_H

#include "llhttpclient.h"

class LLEventPoll
	///< implements the viewer side of server-to-viewer pushed events.
{
public:
	LLEventPoll(const std::string& pollURL, const LLHost& sender);
		///< Start polling the URL.

	virtual ~LLEventPoll();
		///< will stop polling, cancelling any poll in progress.


private:
	LLHTTPClient::ResponderPtr mImpl;
};

// Just like the region uses the event poll to invoke services on the viewer,
// the agent domain also does. There will be lots of changes coming to this code,
// for now a nice clean split from the region's event code. 
class LLAgentEventPoll //OGPX
	///< implements the viewer side of server-to-viewer pushed events.
{	
public:
	LLAgentEventPoll(const std::string& pollURL);
		///< Start polling the URL.

	virtual ~LLAgentEventPoll();
		///< will stop polling, cancelling any poll in progress.

private:
	LLHTTPClient::ResponderPtr mImpl;
};
#endif // LL_LLEVENTPOLL_H
