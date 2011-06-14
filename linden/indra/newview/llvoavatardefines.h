/** 
 * @file llvoavatar.h
 * @brief Declaration of LLVOAvatar class which is a derivation fo
 * LLViewerObject
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2010, Linden Research, Inc.
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
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
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

#ifndef LLVOAVATAR_DEFINES_H
#define LLVOAVATAR_DEFINES_H

#include <vector>
#include "llwearable.h"
#include "llviewerjoint.h"

namespace LLVOAvatarDefines
{

extern const S32 SCRATCH_TEX_WIDTH;
extern const S32 SCRATCH_TEX_HEIGHT;
extern const S32 IMPOSTOR_PERIOD;

//--------------------------------------------------------------------
// texture entry assignment
//--------------------------------------------------------------------
enum ETextureIndex
{
	TEX_HEAD_BODYPAINT = 0,
	TEX_UPPER_SHIRT,
	TEX_LOWER_PANTS,
	TEX_EYES_IRIS,
	TEX_HAIR,
	TEX_UPPER_BODYPAINT,
	TEX_LOWER_BODYPAINT,
	TEX_LOWER_SHOES,
	TEX_HEAD_BAKED,			// Pre-composited
	TEX_UPPER_BAKED,		// Pre-composited
	TEX_LOWER_BAKED,		// Pre-composited
	TEX_EYES_BAKED,		// Pre-composited
	TEX_LOWER_SOCKS,
	TEX_UPPER_JACKET,
	TEX_LOWER_JACKET,
	TEX_UPPER_GLOVES,
	TEX_UPPER_UNDERSHIRT,
	TEX_LOWER_UNDERPANTS,
	TEX_SKIRT,
	TEX_SKIRT_BAKED,		// Pre-composited
	TEX_HAIR_BAKED,     // Pre-composited
	TEX_LOWER_ALPHA,
	TEX_UPPER_ALPHA,
	TEX_HEAD_ALPHA,
	TEX_EYES_ALPHA,
	TEX_HAIR_ALPHA,
	TEX_HEAD_TATTOO,
	TEX_UPPER_TATTOO,
	TEX_LOWER_TATTOO,
	TEX_NUM_INDICES
};

typedef std::vector<ETextureIndex> texture_vec_t;
	
enum EBakedTextureIndex
{
	BAKED_HEAD = 0,
	BAKED_UPPER,
	BAKED_LOWER,
	BAKED_EYES,
	BAKED_SKIRT,
	BAKED_HAIR,
	BAKED_NUM_INDICES
};
typedef std::vector<EBakedTextureIndex> bakedtexture_vec_t;

// Reference IDs for each mesh. Used as indices for vector of joints
enum EMeshIndex
{
	MESH_ID_HAIR = 0,
	MESH_ID_HEAD,
	MESH_ID_EYELASH,
	MESH_ID_UPPER_BODY,
	MESH_ID_LOWER_BODY,
	MESH_ID_EYEBALL_LEFT,
	MESH_ID_EYEBALL_RIGHT,
	MESH_ID_SKIRT,
	MESH_ID_NUM_INDICES
};
typedef std::vector<EMeshIndex> mesh_vec_t;

typedef std::vector<EWearableType> wearables_vec_t;

//--------------------------------------------------------------------------------
// Convenience Functions
//--------------------------------------------------------------------------------

// Convert from baked texture to associated texture; e.g. BAKED_HEAD -> TEX_HEAD_BAKED
ETextureIndex getTextureIndex(EBakedTextureIndex t);



//------------------------------------------------------------------------
// LLVOAvatarDictionary
// 
// Holds dictionary static entries for textures, baked textures, meshes, etc.; i.e.
// information that is common to all avatars.
// 
// This holds const data - it is initialized once and the contents never change after that.
//------------------------------------------------------------------------
class LLVOAvatarDictionary : public LLSingleton<LLVOAvatarDictionary>
{
public:
	LLVOAvatarDictionary();
	virtual ~LLVOAvatarDictionary();
	
	struct TextureDictionaryEntry
	{
		TextureDictionaryEntry(const std::string &name, 
							   bool is_local_texture, 
							   EBakedTextureIndex baked_texture_index = BAKED_NUM_INDICES,
							   const std::string &default_image_name = "",
							   EWearableType wearable_type = WT_INVALID);
		const std::string mName;
		const std::string mDefaultImageName;
		const EWearableType mWearableType;
		// It's either a local texture xor baked
		BOOL mIsLocalTexture;
		BOOL mIsBakedTexture;
		// If it's a local texture, it may be used by a baked texture
		BOOL mIsUsedByBakedTexture;
		EBakedTextureIndex mBakedTextureIndex;
	};
	
	struct MeshDictionaryEntry
	{
		MeshDictionaryEntry(EBakedTextureIndex baked_index, 
							const std::string &name, 
							U8 level,
							LLViewerJoint::PickName pick);
		const std::string mName; // names of mesh types as they are used in avatar_lad.xml
		// Levels of Detail for each mesh.  Must match levels of detail present in avatar_lad.xml
        // Otherwise meshes will be unable to be found, or levels of detail will be ignored
		const U8 mLOD;
		const EBakedTextureIndex mBakedID;
		const LLViewerJoint::PickName mPickName;
	};

	struct BakedDictionaryEntry
	{
		BakedDictionaryEntry(ETextureIndex tex_index, 
							 const std::string &name, 
							 U32 num_local_textures, ... );
		const ETextureIndex mTextureIndex;
		const std::string mName;
		texture_vec_t mLocalTextures;
	};
	
	struct WearableDictionaryEntry
	{
		WearableDictionaryEntry(const std::string &hash_name,
								U32 num_wearables, ... );
		const LLUUID mHashID;
		wearables_vec_t mWearablesVec;
	};

	typedef std::map<EBakedTextureIndex, BakedDictionaryEntry*> baked_map_t;
	typedef std::map<ETextureIndex, TextureDictionaryEntry*> texture_map_t;
	typedef std::map<EMeshIndex, MeshDictionaryEntry*> mesh_map_t;
	typedef std::map<EBakedTextureIndex, WearableDictionaryEntry*> wearable_map_t;

	const MeshDictionaryEntry *getMesh(EMeshIndex index) const;
	const BakedDictionaryEntry *getBakedTexture(EBakedTextureIndex index) const;
	const TextureDictionaryEntry *getTexture(ETextureIndex index) const;
	const WearableDictionaryEntry *getWearable(EBakedTextureIndex index) const;

	const texture_map_t &getTextures() const { return mTextureMap; }
	const baked_map_t &getBakedTextures() const { return mBakedTextureMap; }
	const mesh_map_t &getMeshes() const { return mMeshMap; }
	const wearable_map_t &getWearables() const { return mWearableMap; }
	
private:
	void initData();
	void createAssociations();

	texture_map_t mTextureMap;
	baked_map_t mBakedTextureMap;
	mesh_map_t mMeshMap;
	wearable_map_t mWearableMap;

}; // End LLVOAvatarDictionary

} // End namespace LLVOAvatarDefines

#endif
