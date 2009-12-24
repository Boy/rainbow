/** 
 * @file imageids.h
 * @brief Temporary holder for image IDs
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

#ifndef LL_IMAGEIDS_H
#define LL_IMAGEIDS_H

#include "lluuid.h"

//
// USE OF THIS FILE IS DEPRECATED
//
// Please use viewerart.ini and the standard
// art import path.																			// indicates if file is only
			 															// on dataserver, or also
																			// pre-cached on viewer

// Grass Images
const LLUUID IMG_SMOKE			("b4ba225c-373f-446d-9f7e-6cb7b5cf9b3d");  // VIEWER

const LLUUID IMG_DEFAULT		("d2114404-dd59-4a4d-8e6c-49359e91bbf0");  // VIEWER

const LLUUID IMG_SUN			("cce0f112-878f-4586-a2e2-a8f104bba271"); // dataserver
const LLUUID IMG_MOON			("d07f6eed-b96a-47cd-b51d-400ad4a1c428"); // dataserver
const LLUUID IMG_CLOUD_POOF		("fc4b9f0b-d008-45c6-96a4-01dd947ac621"); // dataserver
const LLUUID IMG_SHOT			("35f217a3-f618-49cf-bbca-c86d486551a9"); // dataserver
const LLUUID IMG_SPARK			("d2e75ac1-d0fb-4532-820e-a20034ac814d"); // dataserver
const LLUUID IMG_FIRE			("aca40aa8-44cf-44ca-a0fa-93e1a2986f82"); // dataserver
const LLUUID IMG_FACE_SELECT    ("a85ac674-cb75-4af6-9499-df7c5aaf7a28"); // face selector
const LLUUID IMG_DEFAULT_AVATAR ("c228d1cf-4b5d-4ba8-84f4-899a0796aa97"); // dataserver

const LLUUID IMG_EXPLOSION				("68edcf47-ccd7-45b8-9f90-1649d7f12806"); // On dataserver
const LLUUID IMG_EXPLOSION_2			("21ce046c-83fe-430a-b629-c7660ac78d7c"); // On dataserver
const LLUUID IMG_EXPLOSION_3			("fedea30a-1be8-47a6-bc06-337a04a39c4b"); // On dataserver
const LLUUID IMG_EXPLOSION_4			("abf0d56b-82e5-47a2-a8ad-74741bb2c29e"); // On dataserver
const LLUUID IMG_SMOKE_POOF				("1e63e323-5fe0-452e-92f8-b98bd0f764e3"); // On dataserver

const LLUUID IMG_BIG_EXPLOSION_1		("5e47a0dc-97bf-44e0-8b40-de06718cee9d"); // On dataserver
const LLUUID IMG_BIG_EXPLOSION_2		("9c8eca51-53d5-42a7-bb58-cef070395db8"); // On dataserver

const LLUUID IMG_BLOOM1	  			    ("3c59f7fe-9dc8-47f9-8aaf-a9dd1fbc3bef"); // VIEWER
const LLUUID TERRAIN_DIRT_DETAIL		("0bc58228-74a0-7e83-89bc-5c23464bcec5"); // VIEWER
const LLUUID TERRAIN_GRASS_DETAIL		("63338ede-0037-c4fd-855b-015d77112fc8"); // VIEWER
const LLUUID TERRAIN_MOUNTAIN_DETAIL	("303cd381-8560-7579-23f1-f0a880799740"); // VIEWER
const LLUUID TERRAIN_ROCK_DETAIL		("53a2f406-4895-1d13-d541-d2e3b86bc19c"); // VIEWER

const LLUUID DEFAULT_WATER_NORMAL		("822ded49-9a6c-f61c-cb89-6df54f42cdf4"); // VIEWER

#endif
