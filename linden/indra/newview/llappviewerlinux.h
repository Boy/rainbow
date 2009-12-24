/**
 * @file llappviewerlinux.h
 * @brief The LLAppViewerLinux class declaration
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

#ifndef LL_LLAPPVIEWERLINUX_H
#define LL_LLAPPVIEWERLINUX_H

#if LL_DBUS_ENABLED
extern "C" {
# include <glib.h>
# include <glib-object.h>
# include <dbus/dbus-glib.h>
}
#endif

#ifndef LL_LLAPPVIEWER_H
#include "llappviewer.h"
#endif

class LLCommandLineParser;

class LLAppViewerLinux : public LLAppViewer
{
public:
	LLAppViewerLinux();
	virtual ~LLAppViewerLinux();

	//
	// Main application logic
	//
	virtual bool init();			// Override to do application initialization
	std::string generateSerialNumber();
	bool setupSLURLHandler();

protected:
	virtual bool beingDebugged();
	
	virtual bool restoreErrorTrap();
	virtual void handleCrashReporting(bool reportFreeze);
	virtual void handleSyncCrashTrace();

	virtual bool initLogging();
	virtual bool initParseCommandLine(LLCommandLineParser& clp);

	virtual bool initSLURLHandler();
	virtual bool sendURLToOtherInstance(const std::string& url);
};

#if LL_DBUS_ENABLED
typedef struct
{
        GObject parent;
        DBusGConnection *connection;
} ViewerAppAPI;

extern "C" {
	gboolean viewer_app_api_GoSLURL(ViewerAppAPI *obj, gchar *slurl, gboolean **success_rtn, GError **error);
}

#define VIEWERAPI_SERVICE "com.secondlife.ViewerAppAPIService"
#define VIEWERAPI_PATH "/com/secondlife/ViewerAppAPI"
#define VIEWERAPI_INTERFACE "com.secondlife.ViewerAppAPI"

#endif // LL_DBUS_ENABLED

#endif // LL_LLAPPVIEWERLINUX_H
