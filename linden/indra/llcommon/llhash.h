/** 
 * @file llhash.h
 * @brief Wrapper for a hash function.
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 * 
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#ifndef LL_LLHASH_H
#define LL_LLHASH_H

#include "llpreprocessor.h" // for GCC_VERSION

#if (LL_WINDOWS)
#include <hash_map>
#include <algorithm>
#elif LL_DARWIN || LL_LINUX
#  if GCC_VERSION >= 40300 // gcc 4.3 and up
#    include <backward/hashtable.h>
#  elif GCC_VERSION >= 30400 // gcc 3.4 and up
#    include <ext/hashtable.h>
#  elif __GNUC__ >= 3
#    include <ext/stl_hashtable.h>
#  else
#    include <hashtable.h>
#  endif
#elif LL_SOLARIS
#include <ext/hashtable.h>
#else
#error Please define your platform.
#endif

template<class T> inline size_t llhash(T value) 
{ 
#if LL_WINDOWS
	return stdext::hash_value<T>(value);
#elif ( (defined _STLPORT_VERSION) || ((LL_LINUX) && (__GNUC__ <= 2)) )
	std::hash<T> H;
	return H(value);
#elif LL_DARWIN || LL_LINUX || LL_SOLARIS
	__gnu_cxx::hash<T> H;
	return H(value);
#else
#error Please define your platform.
#endif
}

#endif

