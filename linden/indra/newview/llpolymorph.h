/** 
 * @file llpolymorph.h
 * @brief Implementation of LLPolyMesh class
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

#ifndef LL_LLPOLYMORPH_H
#define LL_LLPOLYMORPH_H

#include <string>
#include <vector>

#include "llviewervisualparam.h"

class LLPolyMeshSharedData;
class LLVOAvatar;
class LLVector2;
class LLViewerJointCollisionVolume;

//-----------------------------------------------------------------------------
// LLPolyMorphData()
//-----------------------------------------------------------------------------
class LLPolyMorphData
{
public:
	LLPolyMorphData(const std::string& morph_name);
	~LLPolyMorphData();

	BOOL			loadBinary(LLFILE* fp, LLPolyMeshSharedData *mesh);
	const std::string& getName() { return mName; }

public:
	std::string			mName;

	// morphology
	U32					mNumIndices;
	U32*				mVertexIndices;
	U32					mCurrentIndex;
	LLVector3*			mCoords;
	LLVector3*			mNormals;
	LLVector3*			mBinormals;
	LLVector2*			mTexCoords;

	F32					mTotalDistortion;	// vertex distortion summed over entire morph
	F32					mMaxDistortion;		// maximum single vertex distortion in a given morph
	LLVector3			mAvgDistortion;		// average vertex distortion, to infer directionality of the morph
	LLPolyMeshSharedData*	mMesh;
};

//-----------------------------------------------------------------------------
// LLPolyVertexMask()
//-----------------------------------------------------------------------------
class LLPolyVertexMask
{
public:
	LLPolyVertexMask(LLPolyMorphData* morph_data);
	~LLPolyVertexMask();

	void generateMask(U8 *maskData, S32 width, S32 height, S32 num_components, BOOL invert, LLVector4 *clothing_weights);
	F32* getMorphMaskWeights();


protected:
	F32*		mWeights;
	LLPolyMorphData *mMorphData;
	BOOL			mWeightsGenerated;

};

//-----------------------------------------------------------------------------
// LLPolyMorphTarget Data structs
//-----------------------------------------------------------------------------
struct LLPolyVolumeMorphInfo
{
	LLPolyVolumeMorphInfo(std::string &name, LLVector3 &scale, LLVector3 &pos)
		: mName(name), mScale(scale), mPos(pos) {};

	std::string						mName;
	LLVector3						mScale;
	LLVector3						mPos;
};

struct LLPolyVolumeMorph
{
	LLPolyVolumeMorph(LLViewerJointCollisionVolume* volume, LLVector3 scale, LLVector3 pos)
		: mVolume(volume), mScale(scale), mPos(pos) {};

	LLViewerJointCollisionVolume*	mVolume;
	LLVector3						mScale;
	LLVector3						mPos;
};

//-----------------------------------------------------------------------------
// LLPolyMorphTargetInfo
// Shared information for LLPolyMorphTargets
//-----------------------------------------------------------------------------
class LLPolyMorphTargetInfo : public LLViewerVisualParamInfo
{
	friend class LLPolyMorphTarget;
public:
	LLPolyMorphTargetInfo();
	/*virtual*/ ~LLPolyMorphTargetInfo() {};
	
	/*virtual*/ BOOL parseXml(LLXmlTreeNode* node);

protected:
	std::string		mMorphName;
	BOOL			mIsClothingMorph;
	typedef std::vector<LLPolyVolumeMorphInfo> volume_info_list_t;
	volume_info_list_t mVolumeInfoList;	
};

//-----------------------------------------------------------------------------
// LLPolyMorphTarget
// A set of vertex data associated with morph target.
// These morph targets must be topologically consistent with a given Polymesh
// (share face sets)
//-----------------------------------------------------------------------------
class LLPolyMorphTarget : public LLViewerVisualParam
{
public:
	LLPolyMorphTarget(LLPolyMesh *poly_mesh);
	~LLPolyMorphTarget();

	// Special: These functions are overridden by child classes
	LLPolyMorphTargetInfo*	getInfo() const { return (LLPolyMorphTargetInfo*)mInfo; }
	//   This sets mInfo and calls initialization functions
	BOOL					setInfo(LLPolyMorphTargetInfo *info);

	// LLVisualParam Virtual functions
	///*virtual*/ BOOL				parseData(LLXmlTreeNode* node);
	/*virtual*/ void				apply( ESex sex );
	
	// LLViewerVisualParam Virtual functions
	/*virtual*/ F32					getTotalDistortion();
	/*virtual*/ const LLVector3&	getAvgDistortion();
	/*virtual*/ F32					getMaxDistortion();
	/*virtual*/ LLVector3			getVertexDistortion(S32 index, LLPolyMesh *poly_mesh);
	/*virtual*/ const LLVector3*	getFirstDistortion(U32 *index, LLPolyMesh **poly_mesh);
	/*virtual*/ const LLVector3*	getNextDistortion(U32 *index, LLPolyMesh **poly_mesh);

	void	applyMask(U8 *maskData, S32 width, S32 height, S32 num_components, BOOL invert);
	void	addPendingMorphMask() { mNumMorphMasksPending++; }

protected:
	LLPolyMorphData*				mMorphData;
	LLPolyMesh*						mMesh;
	LLPolyVertexMask *				mVertMask;
	ESex							mLastSex;
	// number of morph masks that haven't been generated, must be 0 before this morph is applied
	BOOL							mNumMorphMasksPending;	

	typedef std::vector<LLPolyVolumeMorph> volume_list_t;
	volume_list_t 					mVolumeMorphs;

};

#endif // LL_LLPOLYMORPH_H
