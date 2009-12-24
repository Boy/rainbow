/** 
 * @file llpolymorph.cpp
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

//-----------------------------------------------------------------------------
// Header Files
//-----------------------------------------------------------------------------
#include "llviewerprecompiledheaders.h"

#include "llpolymorph.h"
#include "llvoavatar.h"
#include "llxmltree.h"
#include "llendianswizzle.h"

//#include "../tools/imdebug/imdebug.h"

const F32 NORMAL_SOFTEN_FACTOR = 0.65f;

//-----------------------------------------------------------------------------
// LLPolyMorphData()
//-----------------------------------------------------------------------------
LLPolyMorphData::LLPolyMorphData(const std::string& morph_name)
	: mName(morph_name)
{
	mNumIndices = 0;
	mCurrentIndex = 0;
	mTotalDistortion = 0.f;
	mAvgDistortion.zeroVec();
	mMaxDistortion = 0.f;
	mVertexIndices = NULL;
	mCoords = NULL;
	mNormals = NULL;
	mBinormals = NULL;
	mTexCoords = NULL;

	mMesh = NULL;
}

//-----------------------------------------------------------------------------
// ~LLPolyMorphData()
//-----------------------------------------------------------------------------
LLPolyMorphData::~LLPolyMorphData()
{
	delete [] mVertexIndices;
	delete [] mCoords;
	delete [] mNormals;
	delete [] mBinormals;
	delete [] mTexCoords;
}

//-----------------------------------------------------------------------------
// loadBinary()
//-----------------------------------------------------------------------------
BOOL LLPolyMorphData::loadBinary(LLFILE *fp, LLPolyMeshSharedData *mesh)
{
	S32 numVertices;
	S32 numRead;

	numRead = fread(&numVertices, sizeof(S32), 1, fp);
	llendianswizzle(&numVertices, sizeof(S32), 1);
	if (numRead != 1)
	{
		llwarns << "Can't read number of morph target vertices" << llendl;
		return FALSE;
	}

	//-------------------------------------------------------------------------
	// allocate vertices
	//-------------------------------------------------------------------------
	mCoords = new LLVector3[numVertices];
	mNormals = new LLVector3[numVertices];
	mBinormals = new LLVector3[numVertices];
	mTexCoords = new LLVector2[numVertices];
	// Actually, we are allocating more space than we need for the skiplist
	mVertexIndices = new U32[numVertices];
	mNumIndices = 0;
	mTotalDistortion = 0.f;
	mMaxDistortion = 0.f;
	mAvgDistortion.zeroVec();
	mMesh = mesh;

	//-------------------------------------------------------------------------
	// read vertices
	//-------------------------------------------------------------------------
	for(S32 v = 0; v < numVertices; v++)
	{
		numRead = fread(&mVertexIndices[v], sizeof(U32), 1, fp);
		llendianswizzle(&mVertexIndices[v], sizeof(U32), 1);
		if (numRead != 1)
		{
			llwarns << "Can't read morph target vertex number" << llendl;
			return FALSE;
		}

		if (mVertexIndices[v] > 10000)
		{
			llerrs << "Bad morph index: " << mVertexIndices[v] << llendl;
		}


		numRead = fread(&mCoords[v].mV, sizeof(F32), 3, fp);
		llendianswizzle(&mCoords[v].mV, sizeof(F32), 3);
		if (numRead != 3)
		{
			llwarns << "Can't read morph target vertex coordinates" << llendl;
			return FALSE;
		}

		F32 magnitude = mCoords[v].magVec();
		
		mTotalDistortion += magnitude;
		mAvgDistortion.mV[VX] += fabs(mCoords[v].mV[VX]);
		mAvgDistortion.mV[VY] += fabs(mCoords[v].mV[VY]);
		mAvgDistortion.mV[VZ] += fabs(mCoords[v].mV[VZ]);
		
		if (magnitude > mMaxDistortion)
		{
			mMaxDistortion = magnitude;
		}

		numRead = fread(&mNormals[v].mV, sizeof(F32), 3, fp);
		llendianswizzle(&mNormals[v].mV, sizeof(F32), 3);
		if (numRead != 3)
		{
			llwarns << "Can't read morph target normal" << llendl;
			return FALSE;
		}

		numRead = fread(&mBinormals[v].mV, sizeof(F32), 3, fp);
		llendianswizzle(&mBinormals[v].mV, sizeof(F32), 3);
		if (numRead != 3)
		{
			llwarns << "Can't read morph target binormal" << llendl;
			return FALSE;
		}


		numRead = fread(&mTexCoords[v].mV, sizeof(F32), 2, fp);
		llendianswizzle(&mTexCoords[v].mV, sizeof(F32), 2);
		if (numRead != 2)
		{
			llwarns << "Can't read morph target uv" << llendl;
			return FALSE;
		}

		mNumIndices++;
	}

	mAvgDistortion = mAvgDistortion * (1.f/(F32)mNumIndices);
	mAvgDistortion.normVec();

	return TRUE;
}

//-----------------------------------------------------------------------------
// LLPolyMorphTargetInfo()
//-----------------------------------------------------------------------------
LLPolyMorphTargetInfo::LLPolyMorphTargetInfo()
	: mIsClothingMorph(FALSE)
{
}

BOOL LLPolyMorphTargetInfo::parseXml(LLXmlTreeNode* node)
{
	llassert( node->hasName( "param" ) && node->getChildByName( "param_morph" ) );

	if (!LLViewerVisualParamInfo::parseXml(node))
		return FALSE;

	// Get mixed-case name
	static LLStdStringHandle name_string = LLXmlTree::addAttributeString("name");
	if( !node->getFastAttributeString( name_string, mMorphName ) )
	{
		llwarns << "Avatar file: <param> is missing name attribute" << llendl;
		return FALSE;  // Continue, ignoring this tag
	}

	static LLStdStringHandle clothing_morph_string = LLXmlTree::addAttributeString("clothing_morph");
	node->getFastAttributeBOOL(clothing_morph_string, mIsClothingMorph);

	LLXmlTreeNode *paramNode = node->getChildByName("param_morph");

        if (NULL == paramNode)
        {
                llwarns << "Failed to getChildByName(\"param_morph\")"
                        << llendl;
                return FALSE;
        }

	for (LLXmlTreeNode* child_node = paramNode->getFirstChild();
		 child_node;
		 child_node = paramNode->getNextChild())
	{
		static LLStdStringHandle name_string = LLXmlTree::addAttributeString("name");
		if (child_node->hasName("volume_morph"))
		{
			std::string volume_name;
			if (child_node->getFastAttributeString(name_string, volume_name))
			{
				LLVector3 scale;
				static LLStdStringHandle scale_string = LLXmlTree::addAttributeString("scale");
				child_node->getFastAttributeVector3(scale_string, scale);
				
				LLVector3 pos;
				static LLStdStringHandle pos_string = LLXmlTree::addAttributeString("pos");
				child_node->getFastAttributeVector3(pos_string, pos);

				mVolumeInfoList.push_back(LLPolyVolumeMorphInfo(volume_name,scale,pos));
			}
		}
	}
	
	return TRUE;
}

//-----------------------------------------------------------------------------
// LLPolyMorphTarget()
//-----------------------------------------------------------------------------
LLPolyMorphTarget::LLPolyMorphTarget(LLPolyMesh *poly_mesh)
	: mMorphData(NULL), mMesh(poly_mesh),
	  mVertMask(NULL),
	  mLastSex(SEX_FEMALE),
	  mNumMorphMasksPending(0)
{
}

//-----------------------------------------------------------------------------
// ~LLPolyMorphTarget()
//-----------------------------------------------------------------------------
LLPolyMorphTarget::~LLPolyMorphTarget()
{
	if (mVertMask)
	{
		delete mVertMask;
	}
}

//-----------------------------------------------------------------------------
// setInfo()
//-----------------------------------------------------------------------------
BOOL LLPolyMorphTarget::setInfo(LLPolyMorphTargetInfo* info)
{
	llassert(mInfo == NULL);
	if (info->mID < 0)
		return FALSE;
	mInfo = info;
	mID = info->mID;
	setWeight(getDefaultWeight(), FALSE );

	LLVOAvatar* avatarp = mMesh->getAvatar();
	LLPolyMorphTargetInfo::volume_info_list_t::iterator iter;
	for (iter = getInfo()->mVolumeInfoList.begin(); iter != getInfo()->mVolumeInfoList.end(); iter++)
	{
		LLPolyVolumeMorphInfo *volume_info = &(*iter);
		for (S32 i = 0; i < avatarp->mNumCollisionVolumes; i++)
		{
			if (avatarp->mCollisionVolumes[i].getName() == volume_info->mName)
			{
				mVolumeMorphs.push_back(LLPolyVolumeMorph(&avatarp->mCollisionVolumes[i],
														  volume_info->mScale,
														  volume_info->mPos));
				break;
			}
		}
	}

	mMorphData = mMesh->getMorphData(getInfo()->mMorphName);
	if (!mMorphData)
	{
		llwarns << "No morph target named " << getInfo()->mMorphName << " found in mesh." << llendl;
		return FALSE;  // Continue, ignoring this tag
	}
	return TRUE;
}

#if 0 // obsolete
//-----------------------------------------------------------------------------
// parseData()
//-----------------------------------------------------------------------------
BOOL LLPolyMorphTarget::parseData(LLXmlTreeNode* node)
{
	LLPolyMorphTargetInfo* info = new LLPolyMorphTargetInfo;

	info->parseXml(node);
	if (!setInfo(info))
	{
		delete info;
		return FALSE;
	}
	return TRUE;
}
#endif

//-----------------------------------------------------------------------------
// getVertexDistortion()
//-----------------------------------------------------------------------------
LLVector3 LLPolyMorphTarget::getVertexDistortion(S32 requested_index, LLPolyMesh *mesh)
{
	if (!mMorphData || mMesh != mesh) return LLVector3::zero;

	for(U32 index = 0; index < mMorphData->mNumIndices; index++)
	{
		if (mMorphData->mVertexIndices[index] == (U32)requested_index)
		{
			return mMorphData->mCoords[index];
		}
	}

	return LLVector3::zero;
}

//-----------------------------------------------------------------------------
// getFirstDistortion()
//-----------------------------------------------------------------------------
const LLVector3 *LLPolyMorphTarget::getFirstDistortion(U32 *index, LLPolyMesh **poly_mesh)
{
	if (!mMorphData) return &LLVector3::zero;

	LLVector3* resultVec;
	mMorphData->mCurrentIndex = 0;
	if (mMorphData->mNumIndices)
	{
		resultVec = &mMorphData->mCoords[mMorphData->mCurrentIndex];
		if (index != NULL)
		{
			*index = mMorphData->mVertexIndices[mMorphData->mCurrentIndex];
		}
		if (poly_mesh != NULL)
		{
			*poly_mesh = mMesh;
		}

		return resultVec;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// getNextDistortion()
//-----------------------------------------------------------------------------
const LLVector3 *LLPolyMorphTarget::getNextDistortion(U32 *index, LLPolyMesh **poly_mesh)
{
	if (!mMorphData) return &LLVector3::zero;

	LLVector3* resultVec;
	mMorphData->mCurrentIndex++;
	if (mMorphData->mCurrentIndex < mMorphData->mNumIndices)
	{
		resultVec = &mMorphData->mCoords[mMorphData->mCurrentIndex];
		if (index != NULL)
		{
			*index = mMorphData->mVertexIndices[mMorphData->mCurrentIndex];
		}
		if (poly_mesh != NULL)
		{
			*poly_mesh = mMesh;
		}
		return resultVec;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// getTotalDistortion()
//-----------------------------------------------------------------------------
F32	LLPolyMorphTarget::getTotalDistortion() 
{ 
	if (mMorphData) 
	{
		return mMorphData->mTotalDistortion; 
	}
	else 
	{
		return 0.f;
	}
}

//-----------------------------------------------------------------------------
// getAvgDistortion()
//-----------------------------------------------------------------------------
const LLVector3& LLPolyMorphTarget::getAvgDistortion()	
{
	if (mMorphData) 
	{
		return mMorphData->mAvgDistortion; 
	}
	else 
	{
		return LLVector3::zero;
	}
}

//-----------------------------------------------------------------------------
// getMaxDistortion()
//-----------------------------------------------------------------------------
F32	LLPolyMorphTarget::getMaxDistortion() 
{
	if (mMorphData) 
	{
		return mMorphData->mMaxDistortion; 
	}
	else
	{
		return 0.f;
	}
}

//-----------------------------------------------------------------------------
// apply()
//-----------------------------------------------------------------------------
void LLPolyMorphTarget::apply( ESex avatar_sex )
{
	if (!mMorphData || mNumMorphMasksPending > 0)
	{
		return;
	}

	mLastSex = avatar_sex;

	// perform differential update of morph
	F32 delta_weight = ( getSex() & avatar_sex ) ? (mCurWeight - mLastWeight) : (getDefaultWeight() - mLastWeight);
	// store last weight
	mLastWeight += delta_weight;

	if (delta_weight != 0.f)
	{
		llassert(!mMesh->isLOD());
		LLVector3 *coords = mMesh->getWritableCoords();

		LLVector3 *scaled_normals = mMesh->getScaledNormals();
		LLVector3 *normals = mMesh->getWritableNormals();

		LLVector3 *scaled_binormals = mMesh->getScaledBinormals();
		LLVector3 *binormals = mMesh->getWritableBinormals();

		LLVector4 *clothing_weights = mMesh->getWritableClothingWeights();
		LLVector2 *tex_coords = mMesh->getWritableTexCoords();

		F32 *maskWeightArray = (mVertMask) ? mVertMask->getMorphMaskWeights() : NULL;

		for(U32 vert_index_morph = 0; vert_index_morph < mMorphData->mNumIndices; vert_index_morph++)
		{
			S32 vert_index_mesh = mMorphData->mVertexIndices[vert_index_morph];

			F32 maskWeight = 1.f;
			if (maskWeightArray)
			{
				maskWeight = maskWeightArray[vert_index_morph];
			}

			coords[vert_index_mesh] += mMorphData->mCoords[vert_index_morph] * delta_weight * maskWeight;
			if (getInfo()->mIsClothingMorph && clothing_weights)
			{
				LLVector3 clothing_offset = mMorphData->mCoords[vert_index_morph] * delta_weight * maskWeight;
				LLVector4* clothing_weight = &clothing_weights[vert_index_mesh];
				clothing_weight->mV[VX] += clothing_offset.mV[VX];
				clothing_weight->mV[VY] += clothing_offset.mV[VY];
				clothing_weight->mV[VZ] += clothing_offset.mV[VZ];
				clothing_weight->mV[VW] = maskWeight;
			}

			// calculate new normals based on half angles
			scaled_normals[vert_index_mesh] += mMorphData->mNormals[vert_index_morph] * delta_weight * maskWeight * NORMAL_SOFTEN_FACTOR;
			LLVector3 normalized_normal = scaled_normals[vert_index_mesh];
			normalized_normal.normVec();
			normals[vert_index_mesh] = normalized_normal;

			// calculate new binormals
			scaled_binormals[vert_index_mesh] += mMorphData->mBinormals[vert_index_morph] * delta_weight * maskWeight * NORMAL_SOFTEN_FACTOR;
			LLVector3 tangent = scaled_binormals[vert_index_mesh] % normalized_normal;
			LLVector3 normalized_binormal = normalized_normal % tangent; 
			normalized_binormal.normVec();
			binormals[vert_index_mesh] = normalized_binormal;

			tex_coords[vert_index_mesh] += mMorphData->mTexCoords[vert_index_morph] * delta_weight * maskWeight;
		}

		// now apply volume changes
		for( volume_list_t::iterator iter = mVolumeMorphs.begin(); iter != mVolumeMorphs.end(); iter++ )
		{
			LLPolyVolumeMorph* volume_morph = &(*iter);
			LLVector3 scale_delta = volume_morph->mScale * delta_weight;
			LLVector3 pos_delta = volume_morph->mPos * delta_weight;
			
			volume_morph->mVolume->setScale(volume_morph->mVolume->getScale() + scale_delta);
			volume_morph->mVolume->setPosition(volume_morph->mVolume->getPosition() + pos_delta);
		}
	}

	if (mNext)
	{
		mNext->apply(avatar_sex);
	}
}

//-----------------------------------------------------------------------------
// applyMask()
//-----------------------------------------------------------------------------
void	LLPolyMorphTarget::applyMask(U8 *maskTextureData, S32 width, S32 height, S32 num_components, BOOL invert)
{
	LLVector4 *clothing_weights = getInfo()->mIsClothingMorph ? mMesh->getWritableClothingWeights() : NULL;

	if (!mVertMask)
	{
		mVertMask = new LLPolyVertexMask(mMorphData);
		mNumMorphMasksPending--;
	}
	else
	{
		// remove effect of previous mask
		F32 *maskWeights = (mVertMask) ? mVertMask->getMorphMaskWeights() : NULL;

		if (maskWeights)
		{
			LLVector3 *coords = mMesh->getWritableCoords();
			LLVector3 *scaled_normals = mMesh->getScaledNormals();
			LLVector3 *scaled_binormals = mMesh->getScaledBinormals();
			LLVector2 *tex_coords = mMesh->getWritableTexCoords();

			for(U32 vert = 0; vert < mMorphData->mNumIndices; vert++)
			{
				F32 lastMaskWeight = mLastWeight * maskWeights[vert];
				S32 out_vert = mMorphData->mVertexIndices[vert];

				// remove effect of existing masked morph
				coords[out_vert] -= mMorphData->mCoords[vert] * lastMaskWeight;
				scaled_normals[out_vert] -= mMorphData->mNormals[vert] * lastMaskWeight * NORMAL_SOFTEN_FACTOR;
				scaled_binormals[out_vert] -= mMorphData->mBinormals[vert] * lastMaskWeight * NORMAL_SOFTEN_FACTOR;
				tex_coords[out_vert] -= mMorphData->mTexCoords[vert] * lastMaskWeight;

				if (clothing_weights)
				{
					LLVector3 clothing_offset = mMorphData->mCoords[vert] * lastMaskWeight;
					LLVector4* clothing_weight = &clothing_weights[out_vert];
					clothing_weight->mV[VX] -= clothing_offset.mV[VX];
					clothing_weight->mV[VY] -= clothing_offset.mV[VY];
					clothing_weight->mV[VZ] -= clothing_offset.mV[VZ];
				}
			}
		}
	}

	// set last weight to 0, since we've removed the effect of this morph
	mLastWeight = 0.f;

	mVertMask->generateMask(maskTextureData, width, height, num_components, invert, clothing_weights);

	apply(mLastSex);
}


//-----------------------------------------------------------------------------
// LLPolyVertexMask()
//-----------------------------------------------------------------------------
LLPolyVertexMask::LLPolyVertexMask(LLPolyMorphData* morph_data)
{
	mWeights = new F32[morph_data->mNumIndices];
	mMorphData = morph_data;
	mWeightsGenerated = FALSE;
}

//-----------------------------------------------------------------------------
// ~LLPolyVertexMask()
//-----------------------------------------------------------------------------
LLPolyVertexMask::~LLPolyVertexMask()
{
	delete[] mWeights;
}

//-----------------------------------------------------------------------------
// generateMask()
//-----------------------------------------------------------------------------
void LLPolyVertexMask::generateMask(U8 *maskTextureData, S32 width, S32 height, S32 num_components, BOOL invert, LLVector4 *clothing_weights)
{
// RN debug output that uses Image Debugger (http://www.cs.unc.edu/~baxter/projects/imdebug/)
//	BOOL debugImg = FALSE; 
//	if (debugImg)
//	{
//		if (invert)
//		{
//			imdebug("lum rbga=rgba b=8 w=%d h=%d *-1 %p", width, height, maskTextureData);
//		}
//		else
//		{
//			imdebug("lum rbga=rgba b=8 w=%d h=%d %p", width, height, maskTextureData);
//		}
//	}
	for (U32 index = 0; index < mMorphData->mNumIndices; index++)
	{
		S32 vertIndex = mMorphData->mVertexIndices[index];
		const S32 *sharedVertIndex = mMorphData->mMesh->getSharedVert(vertIndex);
		LLVector2 uvCoords;

		if (sharedVertIndex)
		{
			uvCoords = mMorphData->mMesh->getUVs(*sharedVertIndex);
		}
		else
		{
			uvCoords = mMorphData->mMesh->getUVs(vertIndex);
		}
		U32 s = llclamp((U32)(uvCoords.mV[VX] * (F32)(width - 1)), (U32)0, (U32)width - 1);
		U32 t = llclamp((U32)(uvCoords.mV[VY] * (F32)(height - 1)), (U32)0, (U32)height - 1);
		
		mWeights[index] = ((F32) maskTextureData[((t * width + s) * num_components) + (num_components - 1)]) / 255.f;
		
		if (invert) 
		{
			mWeights[index] = 1.f - mWeights[index];
		}

		// now apply step function
		// mWeights[index] = mWeights[index] > 0.95f ? 1.f : 0.f;

		if (clothing_weights)
		{
			clothing_weights[vertIndex].mV[VW] = mWeights[index];
		}
	}
	mWeightsGenerated = TRUE;
}

//-----------------------------------------------------------------------------
// getMaskForMorphIndex()
//-----------------------------------------------------------------------------
F32* LLPolyVertexMask::getMorphMaskWeights()
{
	if (!mWeightsGenerated)
	{
		return NULL;
	}
	
	return mWeights;
}
