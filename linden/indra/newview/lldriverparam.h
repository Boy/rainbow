/** 
 * @file lldriverparam.h
 * @brief A visual parameter that drives (controls) other visual parameters.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#ifndef LL_LLDRIVERPARAM_H
#define LL_LLDRIVERPARAM_H

#include "llviewervisualparam.h"

class LLVOAvatar;

//-----------------------------------------------------------------------------

struct LLDrivenEntryInfo
{
	LLDrivenEntryInfo( S32 id, F32 min1, F32 max1, F32 max2, F32 min2 )
		: mDrivenID( id ), mMin1( min1 ), mMax1( max1 ), mMax2( max2 ), mMin2( min2 ) {}
	S32					mDrivenID;
	F32					mMin1;
	F32					mMax1;
	F32					mMax2;
	F32					mMin2;
};

struct LLDrivenEntry
{
	LLDrivenEntry( LLViewerVisualParam* param, LLDrivenEntryInfo *info )
		: mParam( param ), mInfo( info ) {}
	LLViewerVisualParam* mParam;
	LLDrivenEntryInfo*	 mInfo;
};

//-----------------------------------------------------------------------------

class LLDriverParamInfo : public LLViewerVisualParamInfo
{
	friend class LLDriverParam;
public:
	LLDriverParamInfo();
	/*virtual*/ ~LLDriverParamInfo() {};
	
	/*virtual*/ BOOL parseXml(LLXmlTreeNode* node);

protected:
	typedef std::deque<LLDrivenEntryInfo> entry_info_list_t;
	entry_info_list_t mDrivenInfoList;
};

//-----------------------------------------------------------------------------

class LLDriverParam : public LLViewerVisualParam
{
public:
	LLDriverParam(LLVOAvatar *avatarp);
	~LLDriverParam();

	// Special: These functions are overridden by child classes
	LLDriverParamInfo*		getInfo() const { return (LLDriverParamInfo*)mInfo; }
	//   This sets mInfo and calls initialization functions
	BOOL					setInfo(LLDriverParamInfo *info);

	// LLVisualParam Virtual functions
	///*virtual*/ BOOL				parseData(LLXmlTreeNode* node);
	/*virtual*/ void				apply( ESex sex ) {} // apply is called separately for each driven param.
	/*virtual*/ void				setWeight(F32 weight, BOOL set_by_user);
	/*virtual*/ void				setAnimationTarget( F32 target_value, BOOL set_by_user );
	/*virtual*/ void				stopAnimating(BOOL set_by_user);
	
	// LLViewerVisualParam Virtual functions
	/*virtual*/ F32					getTotalDistortion();
	/*virtual*/ const LLVector3&	getAvgDistortion();
	/*virtual*/ F32					getMaxDistortion();
	/*virtual*/ LLVector3			getVertexDistortion(S32 index, LLPolyMesh *poly_mesh);
	/*virtual*/ const LLVector3*	getFirstDistortion(U32 *index, LLPolyMesh **poly_mesh);
	/*virtual*/ const LLVector3*	getNextDistortion(U32 *index, LLPolyMesh **poly_mesh);
protected:
	F32 getDrivenWeight(const LLDrivenEntry* driven, F32 input_weight);


	LLVector3	mDefaultVec; // temp holder
	typedef std::vector<LLDrivenEntry> entry_list_t;
	entry_list_t mDriven;
	LLViewerVisualParam* mCurrentDistortionParam;
	// Backlink only; don't make this an LLPointer.
	LLVOAvatar* mAvatarp;
};

#endif  // LL_LLDRIVERPARAM_H
