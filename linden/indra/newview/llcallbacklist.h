/** 
 * @file llcallbacklist.h
 * @brief A simple list of callback functions to call.
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

#ifndef LL_LLCALLBACKLIST_H
#define LL_LLCALLBACKLIST_H

#include "llstl.h"

class LLCallbackList
{
public:
	typedef void (*callback_t)(void*);
	
	LLCallbackList();
	~LLCallbackList();

	void addFunction( callback_t func, void *data = NULL );		// register a callback, which will be called as func(data)
	BOOL containsFunction( callback_t func, void *data = NULL );	// true if list already contains the function/data pair
	BOOL deleteFunction( callback_t func, void *data = NULL );		// removes the first instance of this function/data pair from the list, false if not found
	void callFunctions();												// calls all functions
	void deleteAllFunctions();

	static void test();

protected:
	// Use a list so that the callbacks are ordered in case that matters
	typedef std::pair<callback_t,void*> callback_pair_t;
	typedef std::list<callback_pair_t > callback_list_t;
	callback_list_t	mCallbackList;
};

extern LLCallbackList gIdleCallbacks;

#endif
