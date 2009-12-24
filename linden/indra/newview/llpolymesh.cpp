/** 
 * @file llpolymesh.cpp
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

#include "llpolymesh.h"

#include "llviewercontrol.h"
#include "llxmltree.h"
#include "llvoavatar.h"
#include "lldir.h"
#include "llvolume.h"
#include "llendianswizzle.h"

#include "llfasttimer.h"

#define HEADER_ASCII "Linden Mesh 1.0"
#define HEADER_BINARY "Linden Binary Mesh 1.0"

extern LLControlGroup gSavedSettings;				// read only

//-----------------------------------------------------------------------------
// Global table of loaded LLPolyMeshes
//-----------------------------------------------------------------------------
LLPolyMesh::LLPolyMeshSharedDataTable LLPolyMesh::sGlobalSharedMeshList;

//-----------------------------------------------------------------------------
// LLPolyMeshSharedData()
//-----------------------------------------------------------------------------
LLPolyMeshSharedData::LLPolyMeshSharedData()
{
	mNumVertices = 0;
	mBaseCoords = NULL;
	mBaseNormals = NULL;
	mBaseBinormals = NULL;
	mTexCoords = NULL;
	mDetailTexCoords = NULL;
	mWeights = NULL;
	mHasWeights = FALSE;
	mHasDetailTexCoords = FALSE;

	mNumFaces = 0;
	mFaces = NULL;

	mNumJointNames = 0;
	mJointNames = NULL;

	mTriangleIndices = NULL;
	mNumTriangleIndices = 0;

	mReferenceData = NULL;

	mLastIndexOffset = -1;
}

//-----------------------------------------------------------------------------
// ~LLPolyMeshSharedData()
//-----------------------------------------------------------------------------
LLPolyMeshSharedData::~LLPolyMeshSharedData()
{
	freeMeshData();
	for_each(mMorphData.begin(), mMorphData.end(), DeletePointer());
	mMorphData.clear();
}

//-----------------------------------------------------------------------------
// setupLOD()
//-----------------------------------------------------------------------------
void LLPolyMeshSharedData::setupLOD(LLPolyMeshSharedData* reference_data)
{
	mReferenceData = reference_data;

	if (reference_data)
	{
		mBaseCoords = reference_data->mBaseCoords;
		mBaseNormals = reference_data->mBaseNormals;
		mBaseBinormals = reference_data->mBaseBinormals;
		mTexCoords = reference_data->mTexCoords;
		mDetailTexCoords = reference_data->mDetailTexCoords;
		mWeights = reference_data->mWeights;
		mHasWeights = reference_data->mHasWeights;
		mHasDetailTexCoords = reference_data->mHasDetailTexCoords;
	}
}

//-----------------------------------------------------------------------------
// LLPolyMeshSharedData::freeMeshData()
//-----------------------------------------------------------------------------
void LLPolyMeshSharedData::freeMeshData()
{
	if (!mReferenceData)
	{
		mNumVertices = 0;

		delete [] mBaseCoords;
		mBaseCoords = NULL;

		delete [] mBaseNormals;
		mBaseNormals = NULL;

		delete [] mBaseBinormals;
		mBaseBinormals = NULL;

		delete [] mTexCoords;
		mTexCoords = NULL;

		delete [] mDetailTexCoords;
		mDetailTexCoords = NULL;

		delete [] mWeights;
		mWeights = NULL;
	}

	mNumFaces = 0;
	delete [] mFaces;
	mFaces = NULL;

	mNumJointNames = 0;
	delete [] mJointNames;
	mJointNames = NULL;

	delete [] mTriangleIndices;
	mTriangleIndices = NULL;

//	mVertFaceMap.deleteAllData();
}

// compate_int is used by the qsort function to sort the index array
int compare_int(const void *a, const void *b);

//-----------------------------------------------------------------------------
// genIndices()
//-----------------------------------------------------------------------------
void LLPolyMeshSharedData::genIndices(S32 index_offset)
{
	if (index_offset == mLastIndexOffset)
	{
		return;
	}

	delete []mTriangleIndices;
	mTriangleIndices = new U32[mNumTriangleIndices];

	S32 cur_index = 0;
	for (S32 i = 0; i < mNumFaces; i++)
	{
		mTriangleIndices[cur_index] = mFaces[i][0] + index_offset;
		cur_index++;
		mTriangleIndices[cur_index] = mFaces[i][1] + index_offset;
		cur_index++;
		mTriangleIndices[cur_index] = mFaces[i][2] + index_offset;
		cur_index++;
	}

	mLastIndexOffset = index_offset;
}

//--------------------------------------------------------------------
// LLPolyMeshSharedData::getNumKB()
//--------------------------------------------------------------------
U32 LLPolyMeshSharedData::getNumKB()
{
	U32 num_kb = sizeof(LLPolyMesh);

	if (!isLOD())
	{
		num_kb += mNumVertices *
					( sizeof(LLVector3) +	// coords
					sizeof(LLVector3) +		// normals
					sizeof(LLVector2) );	// texCoords
	}

	if (mHasDetailTexCoords && !isLOD())
	{
		num_kb += mNumVertices * sizeof(LLVector2);	// detailTexCoords
	}

	if (mHasWeights && !isLOD())
	{
		num_kb += mNumVertices * sizeof(float);		// weights
	}

	num_kb += mNumFaces * sizeof(LLPolyFace);	// faces

	num_kb /= 1024;
	return num_kb;
}

//-----------------------------------------------------------------------------
// LLPolyMeshSharedData::allocateVertexData()
//-----------------------------------------------------------------------------
BOOL LLPolyMeshSharedData::allocateVertexData( U32 numVertices )
{
	U32 i;
	mBaseCoords = new LLVector3[ numVertices ];
	mBaseNormals = new LLVector3[ numVertices ];
	mBaseBinormals = new LLVector3[ numVertices ];
	mTexCoords = new LLVector2[ numVertices ];
	mDetailTexCoords = new LLVector2[ numVertices ];
	mWeights = new F32[ numVertices ];
	for (i = 0; i < numVertices; i++)
	{
		mWeights[i] = 0.f;
	}
	mNumVertices = numVertices;
	return TRUE;
}

//-----------------------------------------------------------------------------
// LLPolyMeshSharedData::allocateFaceData()
//-----------------------------------------------------------------------------
BOOL LLPolyMeshSharedData::allocateFaceData( U32 numFaces )
{
	mFaces = new LLPolyFace[ numFaces ];
	mNumFaces = numFaces;
	mNumTriangleIndices = mNumFaces * 3;
	return TRUE;
}

//-----------------------------------------------------------------------------
// LLPolyMeshSharedData::allocateJointNames()
//-----------------------------------------------------------------------------
BOOL LLPolyMeshSharedData::allocateJointNames( U32 numJointNames )
{
	mJointNames = new std::string[ numJointNames ];
	mNumJointNames = numJointNames;
	return TRUE;
}

//--------------------------------------------------------------------
// LLPolyMeshSharedData::loadMesh()
//--------------------------------------------------------------------
BOOL LLPolyMeshSharedData::loadMesh( const std::string& fileName )
{
	//-------------------------------------------------------------------------
	// Open the file
	//-------------------------------------------------------------------------
	if(fileName.empty())
	{
		llerrs << "Filename is Empty!" << llendl;
		return FALSE;
	}
	LLFILE* fp = LLFile::fopen(fileName, "rb");			/*Flawfinder: ignore*/
	if (!fp)
	{
		llerrs << "can't open: " << fileName << llendl;
		return FALSE;
	}

	//-------------------------------------------------------------------------
	// Read a chunk
	//-------------------------------------------------------------------------
	char header[128];		/*Flawfinder: ignore*/
	if (fread(header, sizeof(char), 128, fp) != 128)
	{
		llwarns << "Short read" << llendl;
	}

	//-------------------------------------------------------------------------
	// Check for proper binary header
	//-------------------------------------------------------------------------
	BOOL status = FALSE;
	if ( strncmp(header, HEADER_BINARY, strlen(HEADER_BINARY)) == 0 )	/*Flawfinder: ignore*/
	{
		lldebugs << "Loading " << fileName << llendl;

		//----------------------------------------------------------------
		// File Header (seek past it)
		//----------------------------------------------------------------
		fseek(fp, 24, SEEK_SET);

		//----------------------------------------------------------------
		// HasWeights
		//----------------------------------------------------------------
		U8 hasWeights;
		size_t numRead = fread(&hasWeights, sizeof(U8), 1, fp);
		if (numRead != 1)
		{
			llerrs << "can't read HasWeights flag from " << fileName << llendl;
			return FALSE;
		}
		if (!isLOD())
		{
			mHasWeights = (hasWeights==0) ? FALSE : TRUE;
		}

		//----------------------------------------------------------------
		// HasDetailTexCoords
		//----------------------------------------------------------------
		U8 hasDetailTexCoords;
		numRead = fread(&hasDetailTexCoords, sizeof(U8), 1, fp);
		if (numRead != 1)
		{
			llerrs << "can't read HasDetailTexCoords flag from " << fileName << llendl;
			return FALSE;
		}

		//----------------------------------------------------------------
		// Position
		//----------------------------------------------------------------
		LLVector3 position;
		numRead = fread(position.mV, sizeof(float), 3, fp);
		llendianswizzle(position.mV, sizeof(float), 3);
		if (numRead != 3)
		{
			llerrs << "can't read Position from " << fileName << llendl;
			return FALSE;
		}
		setPosition( position );

		//----------------------------------------------------------------
		// Rotation
		//----------------------------------------------------------------
		LLVector3 rotationAngles;
		numRead = fread(rotationAngles.mV, sizeof(float), 3, fp);
		llendianswizzle(rotationAngles.mV, sizeof(float), 3);
		if (numRead != 3)
		{
			llerrs << "can't read RotationAngles from " << fileName << llendl;
			return FALSE;
		}

		U8 rotationOrder;
		numRead = fread(&rotationOrder, sizeof(U8), 1, fp);

		if (numRead != 1)
		{
			llerrs << "can't read RotationOrder from " << fileName << llendl;
			return FALSE;
		}

		rotationOrder = 0;

		setRotation( mayaQ(	rotationAngles.mV[0],
							rotationAngles.mV[1],
							rotationAngles.mV[2],
							(LLQuaternion::Order)rotationOrder ) );

		//----------------------------------------------------------------
		// Scale
		//----------------------------------------------------------------
		LLVector3 scale;
		numRead = fread(scale.mV, sizeof(float), 3, fp);
		llendianswizzle(scale.mV, sizeof(float), 3);
		if (numRead != 3)
		{
			llerrs << "can't read Scale from " << fileName << llendl;
			return FALSE;
		}
		setScale( scale );

		//-------------------------------------------------------------------------
		// Release any existing mesh geometry
		//-------------------------------------------------------------------------
		freeMeshData();

		U16 numVertices = 0;

		//----------------------------------------------------------------
		// NumVertices
		//----------------------------------------------------------------
		if (!isLOD())
		{
			numRead = fread(&numVertices, sizeof(U16), 1, fp);
			llendianswizzle(&numVertices, sizeof(U16), 1);
			if (numRead != 1)
			{
				llerrs << "can't read NumVertices from " << fileName << llendl;
				return FALSE;
			}

			allocateVertexData( numVertices );	

			//----------------------------------------------------------------
			// Coords
			//----------------------------------------------------------------
			numRead = fread(mBaseCoords, 3*sizeof(float), numVertices, fp);
			llendianswizzle(mBaseCoords, sizeof(float), 3*numVertices);
			if (numRead != numVertices)
			{
				llerrs << "can't read Coordinates from " << fileName << llendl;
				return FALSE;
			}

			//----------------------------------------------------------------
			// Normals
			//----------------------------------------------------------------
			numRead = fread(mBaseNormals, 3*sizeof(float), numVertices, fp);
			llendianswizzle(mBaseNormals, sizeof(float), 3*numVertices);
			if (numRead != numVertices)
			{
				llerrs << " can't read Normals from " << fileName << llendl;
				return FALSE;
			}

			//----------------------------------------------------------------
			// Binormals
			//----------------------------------------------------------------
			numRead = fread(mBaseBinormals, 3*sizeof(float), numVertices, fp);
			llendianswizzle(mBaseBinormals, sizeof(float), 3*numVertices);
			if (numRead != numVertices)
			{
				llerrs << " can't read Binormals from " << fileName << llendl;
				return FALSE;
			}


			//----------------------------------------------------------------
			// TexCoords
			//----------------------------------------------------------------
			numRead = fread(mTexCoords, 2*sizeof(float), numVertices, fp);
			llendianswizzle(mTexCoords, sizeof(float), 2*numVertices);
			if (numRead != numVertices)
			{
				llerrs << "can't read TexCoords from " << fileName << llendl;
				return FALSE;
			}

			//----------------------------------------------------------------
			// DetailTexCoords
			//----------------------------------------------------------------
			if (mHasDetailTexCoords)
			{
				numRead = fread(mDetailTexCoords, 2*sizeof(float), numVertices, fp);
				llendianswizzle(mDetailTexCoords, sizeof(float), 2*numVertices);
				if (numRead != numVertices)
				{
					llerrs << "can't read DetailTexCoords from " << fileName << llendl;
					return FALSE;
				}
			}

			//----------------------------------------------------------------
			// Weights
			//----------------------------------------------------------------
			if (mHasWeights)
			{
				numRead = fread(mWeights, sizeof(float), numVertices, fp);
				llendianswizzle(mWeights, sizeof(float), numVertices);
				if (numRead != numVertices)
				{
					llerrs << "can't read Weights from " << fileName << llendl;
					return FALSE;
				}
			}
		}

		//----------------------------------------------------------------
		// NumFaces
		//----------------------------------------------------------------
		U16 numFaces;
		numRead = fread(&numFaces, sizeof(U16), 1, fp);
		llendianswizzle(&numFaces, sizeof(U16), 1);
		if (numRead != 1)
		{
			llerrs << "can't read NumFaces from " << fileName << llendl;
			return FALSE;
		}
		allocateFaceData( numFaces );


		//----------------------------------------------------------------
		// Faces
		//----------------------------------------------------------------
		U32 i;
		U32 numTris = 0;
		for (i = 0; i < numFaces; i++)
		{
			S16 face[3];
			numRead = fread(face, sizeof(U16), 3, fp);
			llendianswizzle(face, sizeof(U16), 3);
			if (numRead != 3)
			{
				llerrs << "can't read Face[" << i << "] from " << fileName << llendl;
				return FALSE;
			}
			if (mReferenceData)
			{
				llassert(face[0] < mReferenceData->mNumVertices);
				llassert(face[1] < mReferenceData->mNumVertices);
				llassert(face[2] < mReferenceData->mNumVertices);
			}
			
			if (isLOD())
			{
				// store largest index in case of LODs
				for (S32 j = 0; j < 3; j++)
				{
					if (face[j] > mNumVertices - 1)
					{
						mNumVertices = face[j] + 1;
					}
				}
			}
			mFaces[i][0] = face[0];
			mFaces[i][1] = face[1];
			mFaces[i][2] = face[2];

//			S32 j;
//			for(j = 0; j < 3; j++)
//			{
//				LLDynamicArray<S32> *face_list = mVertFaceMap.getIfThere(face[j]);
//				if (!face_list)
//				{
//					face_list = new LLDynamicArray<S32>;
//					mVertFaceMap.addData(face[j], face_list);
//				}
//				face_list->put(i);
//			}

			numTris++;
		}

		lldebugs << "verts: " << numVertices 
			<< ", faces: "   << numFaces
			<< ", tris: "    << numTris
			<< llendl;

		//----------------------------------------------------------------
		// NumSkinJoints
		//----------------------------------------------------------------
		if (!isLOD())
		{
			U16 numSkinJoints = 0;
			if ( mHasWeights )
			{
				numRead = fread(&numSkinJoints, sizeof(U16), 1, fp);
				llendianswizzle(&numSkinJoints, sizeof(U16), 1);
				if (numRead != 1)
				{
					llerrs << "can't read NumSkinJoints from " << fileName << llendl;
					return FALSE;
				}
				allocateJointNames( numSkinJoints );
			}

			//----------------------------------------------------------------
			// SkinJoints
			//----------------------------------------------------------------
			for (i=0; i < numSkinJoints; i++)
			{
				char jointName[64+1];
				numRead = fread(jointName, sizeof(jointName)-1, 1, fp);
				jointName[sizeof(jointName)-1] = '\0'; // ensure nul-termination
				if (numRead != 1)
				{
					llerrs << "can't read Skin[" << i << "].Name from " << fileName << llendl;
					return FALSE;
				}

				std::string *jn = &mJointNames[i];
				*jn = jointName;
			}

			//-------------------------------------------------------------------------
			// look for morph section
			//-------------------------------------------------------------------------
			char morphName[64+1];
			morphName[sizeof(morphName)-1] = '\0'; // ensure nul-termination
			while(fread(&morphName, sizeof(char), 64, fp) == 64)
			{
				if (!strcmp(morphName, "End Morphs"))
				{
					// we reached the end of the morphs
					break;
				}
				LLPolyMorphData* morph_data = new LLPolyMorphData(std::string(morphName));

				BOOL result = morph_data->loadBinary(fp, this);

				if (!result)
				{
					delete morph_data;
					continue;
				}

				mMorphData.insert(morph_data);
			}

			S32 numRemaps;
			if (fread(&numRemaps, sizeof(S32), 1, fp) == 1)
			{
				llendianswizzle(&numRemaps, sizeof(S32), 1);
				for (S32 i = 0; i < numRemaps; i++)
				{
					S32 remapSrc;
					S32 remapDst;
					if (fread(&remapSrc, sizeof(S32), 1, fp) != 1)
					{
						llerrs << "can't read source vertex in vertex remap data" << llendl;
						break;
					}
					if (fread(&remapDst, sizeof(S32), 1, fp) != 1)
					{
						llerrs << "can't read destination vertex in vertex remap data" << llendl;
						break;
					}
					llendianswizzle(&remapSrc, sizeof(S32), 1);
					llendianswizzle(&remapDst, sizeof(S32), 1);

					mSharedVerts[remapSrc] = remapDst;
				}
			}
		}

		status = TRUE;
	}
	else
	{
		llerrs << "invalid mesh file header: " << fileName << llendl;
		status = FALSE;
	}

	if (0 == mNumJointNames)
	{
		allocateJointNames(1);
	}

	fclose( fp );

	return status;
}

//-----------------------------------------------------------------------------
// getSharedVert()
//-----------------------------------------------------------------------------
const S32 *LLPolyMeshSharedData::getSharedVert(S32 vert)
{
	if (mSharedVerts.count(vert) > 0)
	{
		return &mSharedVerts[vert];
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// getUV()
//-----------------------------------------------------------------------------
const LLVector2 &LLPolyMeshSharedData::getUVs(U32 index)
{
	// TODO: convert all index variables to S32
	llassert((S32)index < mNumVertices);

	return mTexCoords[index];
}

//-----------------------------------------------------------------------------
// LLPolyMesh()
//-----------------------------------------------------------------------------
LLPolyMesh::LLPolyMesh(LLPolyMeshSharedData *shared_data, LLPolyMesh *reference_mesh)
{	
	LLMemType mt(LLMemType::MTYPE_AVATAR_MESH);

	llassert(shared_data);

	mSharedData = shared_data;
	mReferenceMesh = reference_mesh;
	mAvatarp = NULL;
	mVertexData = NULL;

	mCurVertexCount = 0;
	mFaceIndexCount = 0;
	mFaceIndexOffset = 0;
	mFaceVertexCount = 0;
	mFaceVertexOffset = 0;

	if (shared_data->isLOD() && reference_mesh)
	{
		mCoords = reference_mesh->mCoords;
		mNormals = reference_mesh->mNormals;
		mScaledNormals = reference_mesh->mScaledNormals;
		mBinormals = reference_mesh->mBinormals;
		mScaledBinormals = reference_mesh->mScaledBinormals;
		mTexCoords = reference_mesh->mTexCoords;
		mClothingWeights = reference_mesh->mClothingWeights;
	}
	else
	{
#if 1	// Allocate memory without initializing every vector
		// NOTE: This makes asusmptions about the size of LLVector[234]
		int nverts = mSharedData->mNumVertices;
		int nfloats = nverts * (3*5 + 2 + 4);
		mVertexData = new F32[nfloats];
		int offset = 0;
		mCoords = 				(LLVector3*)(mVertexData + offset); offset += 3*nverts;
		mNormals = 				(LLVector3*)(mVertexData + offset); offset += 3*nverts;
		mScaledNormals = 		(LLVector3*)(mVertexData + offset); offset += 3*nverts;
		mBinormals = 			(LLVector3*)(mVertexData + offset); offset += 3*nverts;
		mScaledBinormals = 		(LLVector3*)(mVertexData + offset); offset += 3*nverts;
		mTexCoords = 			(LLVector2*)(mVertexData + offset); offset += 2*nverts;
		mClothingWeights = 	(LLVector4*)(mVertexData + offset); offset += 4*nverts;
#else
		mCoords = new LLVector3[mSharedData->mNumVertices];
		mNormals = new LLVector3[mSharedData->mNumVertices];
		mScaledNormals = new LLVector3[mSharedData->mNumVertices];
		mBinormals = new LLVector3[mSharedData->mNumVertices];
		mScaledBinormals = new LLVector3[mSharedData->mNumVertices];
		mTexCoords = new LLVector2[mSharedData->mNumVertices];
		mClothingWeights = new LLVector4[mSharedData->mNumVertices];
		memset(mClothingWeights, 0, sizeof(LLVector4) * mSharedData->mNumVertices);
#endif
		initializeForMorph();
	}
}


//-----------------------------------------------------------------------------
// ~LLPolyMesh()
//-----------------------------------------------------------------------------
LLPolyMesh::~LLPolyMesh()
{
	S32 i;
	for (i = 0; i < mJointRenderData.count(); i++)
	{
		delete mJointRenderData[i];
		mJointRenderData[i] = NULL;
	}
#if 0 // These are now allocated as one big uninitialized chunk
	delete [] mCoords;
	delete [] mNormals;
	delete [] mScaledNormals;
	delete [] mBinormals;
	delete [] mScaledBinormals;
	delete [] mClothingWeights;
	delete [] mTexCoords;
#else
	delete [] mVertexData;
#endif
}


//-----------------------------------------------------------------------------
// LLPolyMesh::getMesh()
//-----------------------------------------------------------------------------
LLPolyMesh *LLPolyMesh::getMesh(const std::string &name, LLPolyMesh* reference_mesh)
{
	//-------------------------------------------------------------------------
	// search for an existing mesh by this name
	//-------------------------------------------------------------------------
	LLPolyMeshSharedData* meshSharedData = get_if_there(sGlobalSharedMeshList, name, (LLPolyMeshSharedData*)NULL);
	if (meshSharedData)
	{
//		llinfos << "Polymesh " << name << " found in global mesh table." << llendl;
		LLPolyMesh *poly_mesh = new LLPolyMesh(meshSharedData, reference_mesh);
		return poly_mesh;
	}

	//-------------------------------------------------------------------------
	// if not found, create a new one, add it to the list
	//-------------------------------------------------------------------------
	std::string full_path;
	full_path = gDirUtilp->getExpandedFilename(LL_PATH_CHARACTER,name);

	LLPolyMeshSharedData *mesh_data = new LLPolyMeshSharedData();
	if (reference_mesh)
	{
		mesh_data->setupLOD(reference_mesh->getSharedData());
	}
	if ( ! mesh_data->loadMesh( full_path ) )
	{
		delete mesh_data;
		return NULL;
	}

	LLPolyMesh *poly_mesh = new LLPolyMesh(mesh_data, reference_mesh);

//	llinfos << "Polymesh " << name << " added to global mesh table." << llendl;
	sGlobalSharedMeshList[name] = poly_mesh->mSharedData;

	return poly_mesh;
}

//-----------------------------------------------------------------------------
// LLPolyMesh::freeAllMeshes()
//-----------------------------------------------------------------------------
void LLPolyMesh::freeAllMeshes()
{
	// delete each item in the global lists
	for_each(sGlobalSharedMeshList.begin(), sGlobalSharedMeshList.end(), DeletePairedPointer());
	sGlobalSharedMeshList.clear();
}

LLPolyMeshSharedData *LLPolyMesh::getSharedData() const
{
	return mSharedData;
}


//--------------------------------------------------------------------
// LLPolyMesh::dumpDiagInfo()
//--------------------------------------------------------------------
void LLPolyMesh::dumpDiagInfo()
{
	// keep track of totals
	U32 total_verts = 0;
	U32 total_faces = 0;
	U32 total_kb = 0;

	std::string buf;

	llinfos << "-----------------------------------------------------" << llendl;
	llinfos << "       Global PolyMesh Table (DEBUG only)" << llendl;
	llinfos << "   Verts    Faces  Mem(KB) Name" << llendl;
	llinfos << "-----------------------------------------------------" << llendl;

	// print each loaded mesh, and it's memory usage
	for(LLPolyMeshSharedDataTable::iterator iter = sGlobalSharedMeshList.begin();
		iter != sGlobalSharedMeshList.end(); ++iter)
	{
		const std::string& mesh_name = iter->first;
		LLPolyMeshSharedData* mesh = iter->second;

		S32 num_verts = mesh->mNumVertices;
		S32 num_faces = mesh->mNumFaces;
		U32 num_kb = mesh->getNumKB();

		buf = llformat("%8d %8d %8d %s", num_verts, num_faces, num_kb, mesh_name.c_str());
		llinfos << buf << llendl;

		total_verts += num_verts;
		total_faces += num_faces;
		total_kb += num_kb;
	}

	llinfos << "-----------------------------------------------------" << llendl;
	buf = llformat("%8d %8d %8d TOTAL", total_verts, total_faces, total_kb );
	llinfos << buf << llendl;
	llinfos << "-----------------------------------------------------" << llendl;
}

//-----------------------------------------------------------------------------
// getWritableCoords()
//-----------------------------------------------------------------------------
LLVector3 *LLPolyMesh::getWritableCoords()
{
	return mCoords;
}

//-----------------------------------------------------------------------------
// getWritableNormals()
//-----------------------------------------------------------------------------
LLVector3 *LLPolyMesh::getWritableNormals()
{
	return mNormals;
}

//-----------------------------------------------------------------------------
// getWritableBinormals()
//-----------------------------------------------------------------------------
LLVector3 *LLPolyMesh::getWritableBinormals()
{
	return mBinormals;
}


//-----------------------------------------------------------------------------
// getWritableClothingWeights()
//-----------------------------------------------------------------------------
LLVector4	*LLPolyMesh::getWritableClothingWeights()
{
	return mClothingWeights;
}

//-----------------------------------------------------------------------------
// getWritableTexCoords()
//-----------------------------------------------------------------------------
LLVector2	*LLPolyMesh::getWritableTexCoords()
{
	return mTexCoords;
}

//-----------------------------------------------------------------------------
// getScaledNormals()
//-----------------------------------------------------------------------------
LLVector3 *LLPolyMesh::getScaledNormals()
{
	return mScaledNormals;
}

//-----------------------------------------------------------------------------
// getScaledBinormals()
//-----------------------------------------------------------------------------
LLVector3 *LLPolyMesh::getScaledBinormals()
{
	return mScaledBinormals;
}


//-----------------------------------------------------------------------------
// initializeForMorph()
//-----------------------------------------------------------------------------
void LLPolyMesh::initializeForMorph()
{
	if (!mSharedData)
		return;

	memcpy(mCoords, mSharedData->mBaseCoords, sizeof(LLVector3) * mSharedData->mNumVertices);	/*Flawfinder: ignore*/
	memcpy(mNormals, mSharedData->mBaseNormals, sizeof(LLVector3) * mSharedData->mNumVertices);	/*Flawfinder: ignore*/
	memcpy(mScaledNormals, mSharedData->mBaseNormals, sizeof(LLVector3) * mSharedData->mNumVertices);	/*Flawfinder: ignore*/
	memcpy(mBinormals, mSharedData->mBaseBinormals, sizeof(LLVector3) * mSharedData->mNumVertices);	/*Flawfinder: ignore*/
	memcpy(mScaledBinormals, mSharedData->mBaseBinormals, sizeof(LLVector3) * mSharedData->mNumVertices);		/*Flawfinder: ignore*/
	memcpy(mTexCoords, mSharedData->mTexCoords, sizeof(LLVector2) * mSharedData->mNumVertices);		/*Flawfinder: ignore*/
	memset(mClothingWeights, 0, sizeof(LLVector4) * mSharedData->mNumVertices);
}

//-----------------------------------------------------------------------------
// getMorphData()
//-----------------------------------------------------------------------------
LLPolyMorphData*	LLPolyMesh::getMorphData(const std::string& morph_name)
{
	if (!mSharedData)
		return NULL;
	for (LLPolyMeshSharedData::morphdata_list_t::iterator iter = mSharedData->mMorphData.begin();
		 iter != mSharedData->mMorphData.end(); ++iter)
	{
		LLPolyMorphData *morph_data = *iter;
		if (morph_data->getName() == morph_name)
		{
			return morph_data;
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// removeMorphData()
//-----------------------------------------------------------------------------
// // erasing but not deleting seems bad, but fortunately we don't actually use this...
// void	LLPolyMesh::removeMorphData(LLPolyMorphData *morph_target)
// {
// 	if (!mSharedData)
// 		return;
// 	mSharedData->mMorphData.erase(morph_target);
// }

//-----------------------------------------------------------------------------
// deleteAllMorphData()
//-----------------------------------------------------------------------------
// void	LLPolyMesh::deleteAllMorphData()
// {
// 	if (!mSharedData)
// 		return;

// 	for_each(mSharedData->mMorphData.begin(), mSharedData->mMorphData.end(), DeletePointer());
// 	mSharedData->mMorphData.clear();
// }

//-----------------------------------------------------------------------------
// getWritableWeights()
//-----------------------------------------------------------------------------
F32*	LLPolyMesh::getWritableWeights() const
{
	return mSharedData->mWeights;
}

//-----------------------------------------------------------------------------
// LLPolySkeletalDistortionInfo()
//-----------------------------------------------------------------------------
LLPolySkeletalDistortionInfo::LLPolySkeletalDistortionInfo()
{
}

BOOL LLPolySkeletalDistortionInfo::parseXml(LLXmlTreeNode* node)
{
	llassert( node->hasName( "param" ) && node->getChildByName( "param_skeleton" ) );
	
	if (!LLViewerVisualParamInfo::parseXml(node))
		return FALSE;

	LLXmlTreeNode* skeletalParam = node->getChildByName("param_skeleton");

	if (NULL == skeletalParam)
	{
		llwarns << "Failed to getChildByName(\"param_skeleton\")"
			<< llendl;
		return FALSE;
	}

	for( LLXmlTreeNode* bone = skeletalParam->getFirstChild(); bone; bone = skeletalParam->getNextChild() )
	{
		if (bone->hasName("bone"))
		{
			std::string name;
			LLVector3 scale;
			LLVector3 pos;
			BOOL haspos = FALSE;
			
			static LLStdStringHandle name_string = LLXmlTree::addAttributeString("name");
			if (!bone->getFastAttributeString(name_string, name))
			{
				llwarns << "No bone name specified for skeletal param." << llendl;
				continue;
			}

			static LLStdStringHandle scale_string = LLXmlTree::addAttributeString("scale");
			if (!bone->getFastAttributeVector3(scale_string, scale))
			{
				llwarns << "No scale specified for bone " << name << "." << llendl;
				continue;
			}

			// optional offset deformation (translation)
			static LLStdStringHandle offset_string = LLXmlTree::addAttributeString("offset");
			if (bone->getFastAttributeVector3(offset_string, pos))
			{
				haspos = TRUE;
			}
			mBoneInfoList.push_back(LLPolySkeletalBoneInfo(name, scale, pos, haspos));
		}
		else
		{
			llwarns << "Unrecognized element " << bone->getName() << " in skeletal distortion" << llendl;
			continue;
		}
	}
	return TRUE;
}

//-----------------------------------------------------------------------------
// LLPolySkeletalDistortion()
//-----------------------------------------------------------------------------
LLPolySkeletalDistortion::LLPolySkeletalDistortion(LLVOAvatar *avatarp)
{
	mAvatar = avatarp;
	mDefaultVec.setVec(0.001f, 0.001f, 0.001f);
}

//-----------------------------------------------------------------------------
// ~LLPolySkeletalDistortion()
//-----------------------------------------------------------------------------
LLPolySkeletalDistortion::~LLPolySkeletalDistortion()
{
}

BOOL LLPolySkeletalDistortion::setInfo(LLPolySkeletalDistortionInfo *info)
{
	llassert(mInfo == NULL);
	if (info->mID < 0)
		return FALSE;
	mInfo = info;
	mID = info->mID;
	setWeight(getDefaultWeight(), FALSE );

	LLPolySkeletalDistortionInfo::bone_info_list_t::iterator iter;
	for (iter = getInfo()->mBoneInfoList.begin(); iter != getInfo()->mBoneInfoList.end(); iter++)
	{
		LLPolySkeletalBoneInfo *bone_info = &(*iter);
		LLJoint* joint = mAvatar->getJoint(bone_info->mBoneName);
		if (!joint)
		{
			llwarns << "Joint " << bone_info->mBoneName << " not found." << llendl;
			continue;
		}

		if (mJointScales.find(joint) != mJointScales.end())
		{
			llwarns << "Scale deformation already supplied for joint " << joint->getName() << "." << llendl;
		}

		// store it
		mJointScales[joint] = bone_info->mScaleDeformation;

		// apply to children that need to inherit it
		for (LLJoint::child_list_t::iterator iter = joint->mChildren.begin();
			 iter != joint->mChildren.end(); ++iter)
		{
			LLViewerJoint* child_joint = (LLViewerJoint*)(*iter);
			if (child_joint->inheritScale())
			{
				LLVector3 childDeformation = LLVector3(child_joint->getScale());
				childDeformation.scaleVec(bone_info->mScaleDeformation);
				mJointScales[child_joint] = childDeformation;
			}
		}

		if (bone_info->mHasPositionDeformation)
		{
			if (mJointOffsets.find(joint) != mJointOffsets.end())
			{
				llwarns << "Offset deformation already supplied for joint " << joint->getName() << "." << llendl;
			}
			mJointOffsets[joint] = bone_info->mPositionDeformation;
		}
	}
	return TRUE;
}

//-----------------------------------------------------------------------------
// apply()
//-----------------------------------------------------------------------------
void LLPolySkeletalDistortion::apply( ESex avatar_sex )
{
	F32 effective_weight = ( getSex() & avatar_sex ) ? mCurWeight : getDefaultWeight();

	LLJoint* joint;
	joint_vec_map_t::iterator iter;

	for (iter = mJointScales.begin();
		 iter != mJointScales.end();
		 iter++)
	{
		joint = iter->first;
		LLVector3 newScale = joint->getScale();
		LLVector3 scaleDelta = iter->second;
		newScale = newScale + (effective_weight * scaleDelta) - (mLastWeight * scaleDelta);
		joint->setScale(newScale);
	}

	for (iter = mJointOffsets.begin();
		 iter != mJointOffsets.end();
		 iter++)
	{
		joint = iter->first;
		LLVector3 newPosition = joint->getPosition();
		LLVector3 positionDelta = iter->second;
		newPosition = newPosition + (effective_weight * positionDelta) - (mLastWeight * positionDelta);
		joint->setPosition(newPosition);
	}

	if (mLastWeight != mCurWeight && !mIsAnimating)
	{
		mAvatar->setSkeletonSerialNum(mAvatar->getSkeletonSerialNum() + 1);
	}
	mLastWeight = mCurWeight;
}

// End
