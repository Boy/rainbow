/** 
 * @file llappviewerlinux_api_dbus.h
 * @brief DBus-glib symbol handling
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 * 
 * Copyright (c) 2008-2009, Linden Research, Inc.
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

#include "linden_common.h"

#if LL_DBUS_ENABLED

extern "C" {
#include <dbus/dbus-glib.h>
}

#define DBUSGLIB_DYLIB_DEFAULT_NAME "libdbus-glib-1.so.2"

bool grab_dbus_syms(std::string dbus_dso_name);
void ungrab_dbus_syms();

#define LL_DBUS_SYM(REQUIRED, DBUSSYM, RTN, ...) extern RTN (*ll##DBUSSYM)(__VA_ARGS__)
#include "llappviewerlinux_api_dbus_syms_raw.inc"
#undef LL_DBUS_SYM

#endif // LL_DBUS_ENABLED
