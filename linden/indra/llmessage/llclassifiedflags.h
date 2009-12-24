/** 
 * @file llclassifiedflags.h
 * @brief Flags used in the classifieds.
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 * 
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#ifndef LL_LLCLASSIFIEDFLAGS_H
#define LL_LLCLASSIFIEDFLAGS_H

typedef U8 ClassifiedFlags;

const U8 CLASSIFIED_FLAG_NONE   	= 1 << 0;
const U8 CLASSIFIED_FLAG_MATURE 	= 1 << 1;
//const U8 CLASSIFIED_FLAG_ENABLED	= 1 << 2; // see llclassifiedflags.cpp
//const U8 CLASSIFIED_FLAG_HAS_PRICE= 1 << 3; // deprecated
const U8 CLASSIFIED_FLAG_UPDATE_TIME= 1 << 4;
const U8 CLASSIFIED_FLAG_AUTO_RENEW = 1 << 5;

const U8 CLASSIFIED_QUERY_FILTER_MATURE		= 1 << 1;
//const U8 CLASSIFIED_QUERY_FILTER_ENABLED	= 1 << 2;
//const U8 CLASSIFIED_QUERY_FILTER_PRICE	= 1 << 3;

// These are new with Adult-enabled viewers
const U8 CLASSIFIED_QUERY_INC_PG			= 1 << 2;
const U8 CLASSIFIED_QUERY_INC_MATURE		= 1 << 3;
const U8 CLASSIFIED_QUERY_INC_ADULT			= 1 << 6;
const U8 CLASSIFIED_QUERY_INC_NEW_VIEWER	= (CLASSIFIED_QUERY_INC_PG | CLASSIFIED_QUERY_INC_MATURE | CLASSIFIED_QUERY_INC_ADULT);

const S32 MAX_CLASSIFIEDS = 100;

// This function is used in AO viewers to pack old query flags into the request 
// so that they can talk to old dataservers properly. When the AO servers are deployed on agni
// we can revert back to ClassifiedFlags pack_classified_flags and get rider of this one.
ClassifiedFlags pack_classified_flags_request(BOOL auto_renew, BOOL is_pg, BOOL is_mature, BOOL is_adult);

ClassifiedFlags pack_classified_flags(BOOL auto_renew, BOOL is_pg, BOOL is_mature, BOOL is_adult);
bool is_cf_mature(ClassifiedFlags flags);
//bool is_cf_enabled(ClassifiedFlags flags);
bool is_cf_update_time(ClassifiedFlags flags);
bool is_cf_auto_renew(ClassifiedFlags flags);

#endif
