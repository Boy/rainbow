/** 
 * @file lluistring.h
 * @author: Steve Bennetts
 * @brief A fancy wrapper for std::string supporting argument substitutions.
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

#ifndef LL_LLUISTRING_H
#define LL_LLUISTRING_H

#include "llstring.h"
#include <string>

// Use this class to store translated text that may have arguments
// e.g. "Welcome [USERNAME] to [SECONDLIFE]!"

// Adding or changing an argument will update the result string, preserving the origianl
// Thus, subsequent changes to arguments or even the original string will produce
//  the correct result

// Example Usage:
// LLUIString mMessage("Welcome [USERNAME] to [SECONDLIFE]!");
// mMessage.setArg("[USERNAME]", "Steve");
// mMessage.setArg("[SECONDLIFE]", "Second Life");
// llinfos << mMessage.getString() << llendl; // outputs "Welcome Steve to Second Life"
// mMessage.setArg("[USERNAME]", "Joe");
// llinfos << mMessage.getString() << llendl; // outputs "Welcome Joe to Second Life"
// mMessage = "Recepci�n a la [SECONDLIFE] [USERNAME]"
// mMessage.setArg("[SECONDLIFE]", "Segunda Vida");
// llinfos << mMessage.getString() << llendl; // outputs "Recepci�n a la Segunda Vida Joe"

// Implementation Notes:
// Attempting to have operator[](const std::string& s) return mArgs[s] fails because we have
// to call format() after the assignment happens.

class LLUIString
{
public:
	// These methods all perform appropriate argument substitution
	// and modify mOrig where appropriate
	LLUIString() {}
	LLUIString(const std::string& instring, const LLStringUtil::format_map_t& args);
	LLUIString(const std::string& instring) { assign(instring); }

	void assign(const std::string& instring);
	LLUIString& operator=(const std::string& s) { assign(s); return *this; }

	void setArgList(const LLStringUtil::format_map_t& args);
	void setArg(const std::string& key, const std::string& replacement);

	const std::string& getString() const { return mResult; }
	operator std::string() const { return mResult; }

	const LLWString& getWString() const { return mWResult; }
	operator LLWString() const { return mWResult; }

	bool empty() const { return mWResult.empty(); }
	S32 length() const { return mWResult.size(); }

	void clear();
	void clearArgs() { mArgs.clear(); }
	
	// These utuilty functions are included for text editing.
	// They do not affect mOrig and do not perform argument substitution
	void truncate(S32 maxchars);
	void erase(S32 charidx, S32 len);
	void insert(S32 charidx, const LLWString& wchars);
	void replace(S32 charidx, llwchar wc);
	
	static const LLStringUtil::format_map_t sNullArgs;

private:
	void format();	
	
	std::string mOrig;
	std::string mResult;
	LLWString mWResult; // for displaying
	LLStringUtil::format_map_t mArgs;
};

#endif // LL_LLUISTRING_H
