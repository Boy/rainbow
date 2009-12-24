/**
 * @file llappviewerwin32.h
 * @brief The LLAppViewerWin32 class declaration
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

#ifndef LL_LLAPPVIEWERWIN32_H
#define LL_LLAPPVIEWERWIN32_H

#ifndef LL_LLAPPVIEWER_H
#include "llappviewer.h"
#endif

class LLAppViewerWin32 : public LLAppViewer
{
public:
	LLAppViewerWin32(const char* cmd_line);
	virtual ~LLAppViewerWin32();

	//
	// Main application logic
	//
	virtual bool init(); // Override to do application initialization
	virtual bool cleanup();

protected:
	virtual bool initLogging(); // Override to clean stack_trace info.
	virtual void initConsole(); // Initialize OS level debugging console.
	virtual bool initHardwareTest(); // Win32 uses DX9 to test hardware.
	virtual bool initParseCommandLine(LLCommandLineParser& clp);

	virtual bool restoreErrorTrap();
	virtual void handleCrashReporting(bool reportFreeze); 
	virtual void handleSyncCrashTrace();

	virtual bool sendURLToOtherInstance(const std::string& url);

	std::string generateSerialNumber();

	static const std::string sWindowClass;

private:
	void disableWinErrorReporting();

    std::string mCmdLine;
};

#endif // LL_LLAPPVIEWERWIN32_H
