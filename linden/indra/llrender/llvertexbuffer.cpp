/** 
 * @file llvertexbuffer.cpp
 * @brief LLVertexBuffer implementation
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 * 
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#include <boost/static_assert.hpp>

#include "llvertexbuffer.h"
// #include "llrender.h"
#include "llglheaders.h"
#include "llmemory.h"
#include "llmemtype.h"
#include "llrender.h"

//============================================================================

//static
LLVBOPool LLVertexBuffer::sStreamVBOPool;
LLVBOPool LLVertexBuffer::sDynamicVBOPool;
LLVBOPool LLVertexBuffer::sStreamIBOPool;
LLVBOPool LLVertexBuffer::sDynamicIBOPool;

U32 LLVertexBuffer::sBindCount = 0;
U32 LLVertexBuffer::sSetCount = 0;
S32 LLVertexBuffer::sCount = 0;
S32 LLVertexBuffer::sGLCount = 0;
S32 LLVertexBuffer::sMappedCount = 0;
BOOL LLVertexBuffer::sEnableVBOs = TRUE;
U32 LLVertexBuffer::sGLRenderBuffer = 0;
U32 LLVertexBuffer::sGLRenderIndices = 0;
U32 LLVertexBuffer::sLastMask = 0;
BOOL LLVertexBuffer::sVBOActive = FALSE;
BOOL LLVertexBuffer::sIBOActive = FALSE;
U32 LLVertexBuffer::sAllocatedBytes = 0;
BOOL LLVertexBuffer::sMapped = FALSE;

std::vector<U32> LLVertexBuffer::sDeleteList;

S32 LLVertexBuffer::sTypeOffsets[LLVertexBuffer::TYPE_MAX] =
{
	sizeof(LLVector3), // TYPE_VERTEX,
	sizeof(LLVector3), // TYPE_NORMAL,
	sizeof(LLVector2), // TYPE_TEXCOORD,
	sizeof(LLVector2), // TYPE_TEXCOORD2,
	sizeof(LLColor4U), // TYPE_COLOR,
	sizeof(LLVector3), // TYPE_BINORMAL,
	sizeof(F32),	   // TYPE_WEIGHT,
	sizeof(LLVector4), // TYPE_CLOTHWEIGHT,
};

U32 LLVertexBuffer::sGLMode[LLRender::NUM_MODES] = 
{
	GL_TRIANGLES,
	GL_TRIANGLE_STRIP,
	GL_TRIANGLE_FAN,
	GL_POINTS,
	GL_LINES,
	GL_LINE_STRIP,
	GL_QUADS,
	GL_LINE_LOOP,
};

//static
void LLVertexBuffer::setupClientArrays(U32 data_mask)
{
	/*if (LLGLImmediate::sStarted)
	{
		llerrs << "Cannot use LLGLImmediate and LLVertexBuffer simultaneously!" << llendl;
	}*/

	if (sLastMask != data_mask)
	{
		U32 mask[] =
		{
			MAP_VERTEX,
			MAP_NORMAL,
			MAP_TEXCOORD,
			MAP_COLOR
		};
		
		GLenum array[] =
		{
			GL_VERTEX_ARRAY,
			GL_NORMAL_ARRAY,
			GL_TEXTURE_COORD_ARRAY,
			GL_COLOR_ARRAY
		};

		for (U32 i = 0; i < 4; ++i)
		{
			if (sLastMask & mask[i])
			{ //was enabled
				if (!(data_mask & mask[i]) && i > 0)
				{ //needs to be disabled
					glDisableClientState(array[i]);
				}
				else
				{ //needs to be enabled, make sure it was (DEBUG TEMPORARY)
					if (i > 0 && !glIsEnabled(array[i]))
					{
						llerrs << "Bad client state! " << array[i] << " disabled." << llendl;
					}
				}
			}
			else 
			{	//was disabled
				if (data_mask & mask[i])
				{ //needs to be enabled
					glEnableClientState(array[i]);
				}
				else if (glIsEnabled(array[i]))
				{ //needs to be disabled, make sure it was (DEBUG TEMPORARY)
					llerrs << "Bad client state! " << array[i] << " enabled." << llendl;
				}
			}
		}

		if (sLastMask & MAP_TEXCOORD2)
		{
			if (!(data_mask & MAP_TEXCOORD2))
			{
				glClientActiveTextureARB(GL_TEXTURE1_ARB);
				glDisableClientState(GL_TEXTURE_COORD_ARRAY);
				glClientActiveTextureARB(GL_TEXTURE0_ARB);
			}
		}
		else if (data_mask & MAP_TEXCOORD2)
		{
			glClientActiveTextureARB(GL_TEXTURE1_ARB);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glClientActiveTextureARB(GL_TEXTURE0_ARB);
		}

		sLastMask = data_mask;
	}
}

void LLVertexBuffer::drawRange(U32 mode, U32 start, U32 end, U32 count, U32 indices_offset) const
{
	if (start >= (U32) mRequestedNumVerts ||
		end >= (U32) mRequestedNumVerts)
	{
		llerrs << "Bad vertex buffer draw range: [" << start << ", " << end << "]" << llendl;
	}

	if (indices_offset >= (U32) mRequestedNumIndices ||
		indices_offset + count > (U32) mRequestedNumIndices)
	{
		llerrs << "Bad index buffer draw range: [" << indices_offset << ", " << indices_offset+count << "]" << llendl;
	}

	if (mGLIndices != sGLRenderIndices)
	{
		llerrs << "Wrong index buffer bound." << llendl;
	}

	if (mGLBuffer != sGLRenderBuffer)
	{
		llerrs << "Wrong vertex buffer bound." << llendl;
	}

	if (mode > LLRender::NUM_MODES)
	{
		llerrs << "Invalid draw mode: " << mode << llendl;
		return;
	}

	glDrawRangeElements(sGLMode[mode], start, end, count, GL_UNSIGNED_SHORT, 
		((U16*) getIndicesPointer()) + indices_offset);
	stop_glerror();
}

void LLVertexBuffer::draw(U32 mode, U32 count, U32 indices_offset) const
{
	if (indices_offset >= (U32) mRequestedNumIndices ||
		indices_offset + count > (U32) mRequestedNumIndices)
	{
		llerrs << "Bad index buffer draw range: [" << indices_offset << ", " << indices_offset+count << "]" << llendl;
	}

	if (mGLIndices != sGLRenderIndices)
	{
		llerrs << "Wrong index buffer bound." << llendl;
	}

	if (mGLBuffer != sGLRenderBuffer)
	{
		llerrs << "Wrong vertex buffer bound." << llendl;
	}

	if (mode > LLRender::NUM_MODES)
	{
		llerrs << "Invalid draw mode: " << mode << llendl;
		return;
	}

	glDrawElements(sGLMode[mode], count, GL_UNSIGNED_SHORT,
		((U16*) getIndicesPointer()) + indices_offset);
}

void LLVertexBuffer::drawArrays(U32 mode, U32 first, U32 count) const
{
	
	if (first >= (U32) mRequestedNumVerts ||
		first + count > (U32) mRequestedNumVerts)
	{
		llerrs << "Bad vertex buffer draw range: [" << first << ", " << first+count << "]" << llendl;
	}

	if (mGLBuffer != sGLRenderBuffer || useVBOs() != sVBOActive)
	{
		llerrs << "Wrong vertex buffer bound." << llendl;
	}

	if (mode > LLRender::NUM_MODES)
	{
		llerrs << "Invalid draw mode: " << mode << llendl;
		return;
	}

	glDrawArrays(sGLMode[mode], first, count);
	stop_glerror();
}

//static
void LLVertexBuffer::initClass(bool use_vbo)
{
	sEnableVBOs = use_vbo;
	LLGLNamePool::registerPool(&sDynamicVBOPool);
	LLGLNamePool::registerPool(&sDynamicIBOPool);
	LLGLNamePool::registerPool(&sStreamVBOPool);
	LLGLNamePool::registerPool(&sStreamIBOPool);
}

//static 
void LLVertexBuffer::unbind()
{
	if (sVBOActive)
	{
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		sVBOActive = FALSE;
	}
	if (sIBOActive)
	{
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
		sIBOActive = FALSE;
	}

	sGLRenderBuffer = 0;
	sGLRenderIndices = 0;

	setupClientArrays(0);
}

//static
void LLVertexBuffer::cleanupClass()
{
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);
	unbind();
	clientCopy(); // deletes GL buffers
}

void LLVertexBuffer::clientCopy(F64 max_time)
{
	if (!sDeleteList.empty())
	{
		glDeleteBuffersARB(sDeleteList.size(), (GLuint*) &(sDeleteList[0]));
		sDeleteList.clear();
	}
}

//----------------------------------------------------------------------------

LLVertexBuffer::LLVertexBuffer(U32 typemask, S32 usage) :
	LLRefCount(),
	mNumVerts(0), mNumIndices(0), mUsage(usage), mGLBuffer(0), mGLIndices(0), 
	mMappedData(NULL),
	mMappedIndexData(NULL), mLocked(FALSE),
	mFinal(FALSE),
	mFilthy(FALSE),
	mEmpty(TRUE),
	mResized(FALSE),
	mDynamicSize(FALSE)
{
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);
	if (!sEnableVBOs)
	{
		mUsage = 0 ; 
	}
	
	S32 stride = calcStride(typemask, mOffsets);

	mTypeMask = typemask;
	mStride = stride;
	sCount++;
}

//static
S32 LLVertexBuffer::calcStride(const U32& typemask, S32* offsets)
{
	S32 stride = 0;
	for (S32 i=0; i<TYPE_MAX; i++)
	{
		U32 mask = 1<<i;
		if (typemask & mask)
		{
			if (offsets)
			{
				offsets[i] = stride;
			}
			stride += sTypeOffsets[i];
		}
	}

	return stride;
}

// protected, use unref()
//virtual
LLVertexBuffer::~LLVertexBuffer()
{
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);
	destroyGLBuffer();
	destroyGLIndices();
	sCount--;
};

//----------------------------------------------------------------------------

void LLVertexBuffer::genBuffer()
{
	if (mUsage == GL_STREAM_DRAW_ARB)
	{
		mGLBuffer = sStreamVBOPool.allocate();
	}
	else if (mUsage == GL_DYNAMIC_DRAW_ARB)
	{
		mGLBuffer = sDynamicVBOPool.allocate();
	}
	else
	{
		BOOST_STATIC_ASSERT(sizeof(mGLBuffer) == sizeof(GLuint));
		glGenBuffersARB(1, (GLuint*)&mGLBuffer);
	}
	sGLCount++;
}

void LLVertexBuffer::genIndices()
{
	if (mUsage == GL_STREAM_DRAW_ARB)
	{
		mGLIndices = sStreamIBOPool.allocate();
	}
	else if (mUsage == GL_DYNAMIC_DRAW_ARB)
	{
		mGLIndices = sDynamicIBOPool.allocate();
	}
	else
	{
		BOOST_STATIC_ASSERT(sizeof(mGLBuffer) == sizeof(GLuint));
		glGenBuffersARB(1, (GLuint*)&mGLIndices);
	}
	sGLCount++;
}

void LLVertexBuffer::releaseBuffer()
{
	if (mUsage == GL_STREAM_DRAW_ARB)
	{
		sStreamVBOPool.release(mGLBuffer);
	}
	else if (mUsage == GL_DYNAMIC_DRAW_ARB)
	{
		sDynamicVBOPool.release(mGLBuffer);
	}
	else
	{
		sDeleteList.push_back(mGLBuffer);
	}
	sGLCount--;
}

void LLVertexBuffer::releaseIndices()
{
	if (mUsage == GL_STREAM_DRAW_ARB)
	{
		sStreamIBOPool.release(mGLIndices);
	}
	else if (mUsage == GL_DYNAMIC_DRAW_ARB)
	{
		sDynamicIBOPool.release(mGLIndices);
	}
	else
	{
		sDeleteList.push_back(mGLIndices);
	}
	sGLCount--;
}

void LLVertexBuffer::createGLBuffer()
{
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);

	U32 size = getSize();
	if (mGLBuffer)
	{
		destroyGLBuffer();
	}

	if (size == 0)
	{
		return;
	}

	mEmpty = TRUE;

	if (useVBOs())
	{
		mMappedData = NULL;
		genBuffer();
		mResized = TRUE;
	}
	else
	{
		static int gl_buffer_idx = 0;
		mGLBuffer = ++gl_buffer_idx;
		mMappedData = new U8[size];
		memset(mMappedData, 0, size);
	}
}

void LLVertexBuffer::createGLIndices()
{
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);
	U32 size = getIndicesSize();

	if (mGLIndices)
	{
		destroyGLIndices();
	}
	
	if (size == 0)
	{
		return;
	}

	mEmpty = TRUE;

	if (useVBOs())
	{
		mMappedIndexData = NULL;
		genIndices();
		mResized = TRUE;
	}
	else
	{
		mMappedIndexData = new U8[size];
		memset(mMappedIndexData, 0, size);
		static int gl_buffer_idx = 0;
		mGLIndices = ++gl_buffer_idx;
	}
}

void LLVertexBuffer::destroyGLBuffer()
{
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);
	if (mGLBuffer)
	{
		if (useVBOs())
		{
			if (mMappedData || mMappedIndexData)
			{
				llerrs << "Vertex buffer destroyed while mapped!" << llendl;
			}
			releaseBuffer();
		}
		else
		{
			delete [] mMappedData;
			mMappedData = NULL;
			mEmpty = TRUE;
		}

		sAllocatedBytes -= getSize();
	}
	
	mGLBuffer = 0;
	unbind();
}

void LLVertexBuffer::destroyGLIndices()
{
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);
	if (mGLIndices)
	{
		if (useVBOs())
		{
			if (mMappedData || mMappedIndexData)
			{
				llerrs << "Vertex buffer destroyed while mapped." << llendl;
			}
			releaseIndices();
		}
		else
		{
			delete [] mMappedIndexData;
			mMappedIndexData = NULL;
			mEmpty = TRUE;
		}

		sAllocatedBytes -= getIndicesSize();
	}

	mGLIndices = 0;
	unbind();
}

void LLVertexBuffer::updateNumVerts(S32 nverts)
{
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);

	if (nverts >= 65535)
	{
		llwarns << "Vertex buffer overflow!" << llendl;
		nverts = 65535;
	}

	mRequestedNumVerts = nverts;
	
	if (!mDynamicSize)
	{
		mNumVerts = nverts;
	}
	else if (mUsage == GL_STATIC_DRAW_ARB ||
		nverts > mNumVerts ||
		nverts < mNumVerts/2)
	{
		if (mUsage != GL_STATIC_DRAW_ARB && nverts + nverts/4 <= 65535)
		{
			nverts += nverts/4;
		}
		mNumVerts = nverts;
	}

}

void LLVertexBuffer::updateNumIndices(S32 nindices)
{
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);
	mRequestedNumIndices = nindices;
	if (!mDynamicSize)
	{
		mNumIndices = nindices;
	}
	else if (mUsage == GL_STATIC_DRAW_ARB ||
		nindices > mNumIndices ||
		nindices < mNumIndices/2)
	{
		if (mUsage != GL_STATIC_DRAW_ARB)
		{
			nindices += nindices/4;
		}

		mNumIndices = nindices;
	}
}

void LLVertexBuffer::allocateBuffer(S32 nverts, S32 nindices, bool create)
{
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);
		
	updateNumVerts(nverts);
	updateNumIndices(nindices);
	
	if (mMappedData)
	{
		llerrs << "LLVertexBuffer::allocateBuffer() called redundantly." << llendl;
	}
	if (create && (nverts || nindices))
	{
		createGLBuffer();
		createGLIndices();
	}
	
	sAllocatedBytes += getSize() + getIndicesSize();
}

void LLVertexBuffer::resizeBuffer(S32 newnverts, S32 newnindices)
{
	mRequestedNumVerts = newnverts;
	mRequestedNumIndices = newnindices;

	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);
	mDynamicSize = TRUE;
	if (mUsage == GL_STATIC_DRAW_ARB)
	{ //always delete/allocate static buffers on resize
		destroyGLBuffer();
		destroyGLIndices();
		allocateBuffer(newnverts, newnindices, TRUE);
		mFinal = FALSE;
	}
	else if (newnverts > mNumVerts || newnindices > mNumIndices ||
			 newnverts < mNumVerts/2 || newnindices < mNumIndices/2)
	{
		sAllocatedBytes -= getSize() + getIndicesSize();
		
		S32 oldsize = getSize();
		S32 old_index_size = getIndicesSize();

		updateNumVerts(newnverts);		
		updateNumIndices(newnindices);
		
		S32 newsize = getSize();
		S32 new_index_size = getIndicesSize();

		sAllocatedBytes += newsize + new_index_size;

		if (newsize)
		{
			if (!mGLBuffer)
			{ //no buffer exists, create a new one
				createGLBuffer();
			}
			else
			{
				//delete old buffer, keep GL buffer for now
				if (!useVBOs())
				{
					U8* old = mMappedData;
					mMappedData = new U8[newsize];
					if (old)
					{	
						memcpy(mMappedData, old, llmin(newsize, oldsize));
						if (newsize > oldsize)
						{
							memset(mMappedData+oldsize, 0, newsize-oldsize);
						}

						delete [] old;
					}
					else
					{
						memset(mMappedData, 0, newsize);
						mEmpty = TRUE;
					}
				}
				mResized = TRUE;
			}
		}
		else if (mGLBuffer)
		{
			destroyGLBuffer();
		}
		
		if (new_index_size)
		{
			if (!mGLIndices)
			{
				createGLIndices();
			}
			else
			{
				if (!useVBOs())
				{
					//delete old buffer, keep GL buffer for now
					U8* old = mMappedIndexData;
					mMappedIndexData = new U8[new_index_size];
					
					if (old)
					{	
						memcpy(mMappedIndexData, old, llmin(new_index_size, old_index_size));
						if (new_index_size > old_index_size)
						{
							memset(mMappedIndexData+old_index_size, 0, new_index_size - old_index_size);
						}
						delete [] old;
					}
					else
					{
						memset(mMappedIndexData, 0, new_index_size);
						mEmpty = TRUE;
					}
				}
				mResized = TRUE;
			}
		}
		else if (mGLIndices)
		{
			destroyGLIndices();
		}
	}

	if (mResized && useVBOs())
	{
		setBuffer(0);
	}
}

BOOL LLVertexBuffer::useVBOs() const
{
	//it's generally ineffective to use VBO for things that are streaming on apple
		
#if LL_DARWIN
	if (!mUsage || mUsage == GL_STREAM_DRAW_ARB)
	{
		return FALSE;
	}
#else
	if (!mUsage)
	{
		return FALSE;
	}
#endif
	return sEnableVBOs;
}

//----------------------------------------------------------------------------

// Map for data access
U8* LLVertexBuffer::mapBuffer(S32 access)
{
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);
	if (mFinal)
	{
		llerrs << "LLVertexBuffer::mapBuffer() called on a finalized buffer." << llendl;
	}
	if (!useVBOs() && !mMappedData && !mMappedIndexData)
	{
		llerrs << "LLVertexBuffer::mapBuffer() called on unallocated buffer." << llendl;
	}
		
	if (!mLocked && useVBOs())
	{
		setBuffer(0);
		mLocked = TRUE;
		stop_glerror();
		mMappedData = (U8*) glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
		stop_glerror();
		mMappedIndexData = (U8*) glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
		stop_glerror();
		/*if (sMapped)
		{
			llerrs << "Mapped two VBOs at the same time!" << llendl;
		}
		sMapped = TRUE;*/		
		if (!mMappedData)
		{
			//--------------------
			//print out more debug info before crash
			llinfos << "vertex buffer size: (num verts : num indices) = " << getNumVerts() << " : " << getNumIndices() << llendl ;
			GLint size ;
			glGetBufferParameterivARB(GL_ARRAY_BUFFER_ARB, GL_BUFFER_SIZE_ARB, &size) ;
			llinfos << "GL_ARRAY_BUFFER_ARB size is " << size << llendl ;
			//--------------------

			GLint buff;
			glGetIntegerv(GL_ARRAY_BUFFER_BINDING_ARB, &buff);
			if (buff != mGLBuffer)
			{
				llerrs << "Invalid GL vertex buffer bound: " << buff << llendl;
			}

			
			llerrs << "glMapBuffer returned NULL (no vertex data)" << llendl;
		}

		if (!mMappedIndexData)
		{
			GLint buff;
			glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB, &buff);
			if (buff != mGLIndices)
			{
				llerrs << "Invalid GL index buffer bound: " << buff << llendl;
			}

			llerrs << "glMapBuffer returned NULL (no index data)" << llendl;
		}

		sMappedCount++;
	}
	
	return mMappedData;
}

void LLVertexBuffer::unmapBuffer()
{
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);
	if (mMappedData || mMappedIndexData)
	{
		if (useVBOs() && mLocked)
		{
			stop_glerror();
			glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
			stop_glerror();
			glUnmapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB);
			stop_glerror();

			/*if (!sMapped)
			{
				llerrs << "Redundantly unmapped VBO!" << llendl;
			}
			sMapped = FALSE;*/
			sMappedCount--;

			if (mUsage == GL_STATIC_DRAW_ARB)
			{ //static draw buffers can only be mapped a single time
				//throw out client data (we won't be using it again)
				mEmpty = TRUE;
				mFinal = TRUE;
			}
			else
			{
				mEmpty = FALSE;
			}

			mMappedIndexData = NULL;
			mMappedData = NULL;
			
			mLocked = FALSE;
		}
	}
}

//----------------------------------------------------------------------------

template <class T,S32 type> struct VertexBufferStrider
{
	typedef LLStrider<T> strider_t;
	static bool get(LLVertexBuffer& vbo, 
					strider_t& strider, 
					S32 index)
	{
		if (vbo.mapBuffer() == NULL)
		{
			llwarns << "mapBuffer failed!" << llendl;
			return FALSE;
		}

		if (type == LLVertexBuffer::TYPE_INDEX)
		{
			S32 stride = sizeof(T);
			strider = (T*)(vbo.getMappedIndices() + index*stride);
			strider.setStride(0);
			return TRUE;
		}
		else if (vbo.hasDataType(type))
		{
			S32 stride = vbo.getStride();
			strider = (T*)(vbo.getMappedData() + vbo.getOffset(type) + index*stride);
			strider.setStride(stride);
			return TRUE;
		}
		else
		{
			llerrs << "VertexBufferStrider could not find valid vertex data." << llendl;
		}
		return FALSE;
	}
};


bool LLVertexBuffer::getVertexStrider(LLStrider<LLVector3>& strider, S32 index)
{
	return VertexBufferStrider<LLVector3,TYPE_VERTEX>::get(*this, strider, index);
}
bool LLVertexBuffer::getIndexStrider(LLStrider<U16>& strider, S32 index)
{
	return VertexBufferStrider<U16,TYPE_INDEX>::get(*this, strider, index);
}
bool LLVertexBuffer::getTexCoordStrider(LLStrider<LLVector2>& strider, S32 index)
{
	return VertexBufferStrider<LLVector2,TYPE_TEXCOORD>::get(*this, strider, index);
}
bool LLVertexBuffer::getTexCoord2Strider(LLStrider<LLVector2>& strider, S32 index)
{
	return VertexBufferStrider<LLVector2,TYPE_TEXCOORD2>::get(*this, strider, index);
}
bool LLVertexBuffer::getNormalStrider(LLStrider<LLVector3>& strider, S32 index)
{
	return VertexBufferStrider<LLVector3,TYPE_NORMAL>::get(*this, strider, index);
}
bool LLVertexBuffer::getBinormalStrider(LLStrider<LLVector3>& strider, S32 index)
{
	return VertexBufferStrider<LLVector3,TYPE_BINORMAL>::get(*this, strider, index);
}
bool LLVertexBuffer::getColorStrider(LLStrider<LLColor4U>& strider, S32 index)
{
	return VertexBufferStrider<LLColor4U,TYPE_COLOR>::get(*this, strider, index);
}
bool LLVertexBuffer::getWeightStrider(LLStrider<F32>& strider, S32 index)
{
	return VertexBufferStrider<F32,TYPE_WEIGHT>::get(*this, strider, index);
}
bool LLVertexBuffer::getClothWeightStrider(LLStrider<LLVector4>& strider, S32 index)
{
	return VertexBufferStrider<LLVector4,TYPE_CLOTHWEIGHT>::get(*this, strider, index);
}

void LLVertexBuffer::setStride(S32 type, S32 new_stride)
{
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);
	if (mNumVerts)
	{
		llerrs << "LLVertexBuffer::setOffset called with mNumVerts = " << mNumVerts << llendl;
	}
	// This code assumes that setStride() will only be called once per VBO per type.
	S32 delta = new_stride - sTypeOffsets[type];
	for (S32 i=type+1; i<TYPE_MAX; i++)
	{
		if (mTypeMask & (1<<i))
		{
			mOffsets[i] += delta;
		}
	}
	mStride += delta;
}

//----------------------------------------------------------------------------

// Set for rendering
void LLVertexBuffer::setBuffer(U32 data_mask)
{
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);
	//set up pointers if the data mask is different ...
	BOOL setup = (sLastMask != data_mask);

	if (useVBOs())
	{
		if (mGLBuffer && (mGLBuffer != sGLRenderBuffer || !sVBOActive))
		{
			/*if (sMapped)
			{
				llerrs << "VBO bound while another VBO mapped!" << llendl;
			}*/
			stop_glerror();
			glBindBufferARB(GL_ARRAY_BUFFER_ARB, mGLBuffer);
			stop_glerror();
			sBindCount++;
			sVBOActive = TRUE;
			setup = TRUE; // ... or the bound buffer changed
		}
		if (mGLIndices && (mGLIndices != sGLRenderIndices || !sIBOActive))
		{
			/*if (sMapped)
			{
				llerrs << "VBO bound while another VBO mapped!" << llendl;
			}*/
			stop_glerror();
			glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, mGLIndices);
			stop_glerror();
			sBindCount++;
			sIBOActive = TRUE;
		}
		
		if (gDebugGL)
		{
			GLint buff;
			glGetIntegerv(GL_ARRAY_BUFFER_BINDING_ARB, &buff);
			if (buff != mGLBuffer)
			{
				llerrs << "Invalid GL vertex buffer bound: " << buff << llendl;
			}

			glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB, &buff);
			if (buff != mGLIndices)
			{
				llerrs << "Invalid GL index buffer bound: " << buff << llendl;
			}
		}

		if (mResized)
		{
			if (gDebugGL)
			{
				GLint buff;
				glGetIntegerv(GL_ARRAY_BUFFER_BINDING_ARB, &buff);
				if (buff != mGLBuffer)
				{
					llerrs << "Invalid GL vertex buffer bound: " << buff << llendl;
				}

				glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB, &buff);
				if (buff != mGLIndices)
				{
					llerrs << "Invalid GL index buffer bound: " << buff << llendl;
				}
			}

			if (mGLBuffer)
			{
				stop_glerror();
				glBufferDataARB(GL_ARRAY_BUFFER_ARB, getSize(), NULL, mUsage);
				stop_glerror();
			}
			if (mGLIndices)
			{
				stop_glerror();
				glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, getIndicesSize(), NULL, mUsage);
				stop_glerror();
			}

			mEmpty = TRUE;
			mResized = FALSE;

			if (data_mask != 0)
			{
				llerrs << "Buffer set for rendering before being filled after resize." << llendl;
			}
		}

		unmapBuffer();
	}
	else
	{		
		if (mGLBuffer)
		{
			if (sEnableVBOs && sVBOActive)
			{
				glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
				sBindCount++;
				sVBOActive = FALSE;
				setup = TRUE; // ... or a VBO is deactivated
			}
			if (sGLRenderBuffer != mGLBuffer)
			{
				setup = TRUE; // ... or a client memory pointer changed
			}
		}
		if (sEnableVBOs && mGLIndices && sIBOActive)
		{
			/*if (sMapped)
			{
				llerrs << "VBO unbound while potentially mapped!" << llendl;
			}*/
			glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
			sBindCount++;
			sIBOActive = FALSE;
		}
	}

	setupClientArrays(data_mask);
	
	if (mGLIndices)
	{
		sGLRenderIndices = mGLIndices;
	}
	if (mGLBuffer)
	{
		sGLRenderBuffer = mGLBuffer;
		if (data_mask && setup)
		{
			setupVertexBuffer(data_mask); // subclass specific setup (virtual function)
			sSetCount++;
		}
	}
}

// virtual (default)
void LLVertexBuffer::setupVertexBuffer(U32 data_mask) const
{
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);
	stop_glerror();
	U8* base = useVBOs() ? NULL : mMappedData;
	S32 stride = mStride;

	if ((data_mask & mTypeMask) != data_mask)
	{
		llerrs << "LLVertexBuffer::setupVertexBuffer missing required components for supplied data mask." << llendl;
	}

	if (data_mask & MAP_NORMAL)
	{
		glNormalPointer(GL_FLOAT, stride, (void*)(base + mOffsets[TYPE_NORMAL]));
	}
	if (data_mask & MAP_TEXCOORD2)
	{
		glClientActiveTextureARB(GL_TEXTURE1_ARB);
		glTexCoordPointer(2,GL_FLOAT, stride, (void*)(base + mOffsets[TYPE_TEXCOORD2]));
		glClientActiveTextureARB(GL_TEXTURE0_ARB);
	}
	if (data_mask & MAP_TEXCOORD)
	{
		glTexCoordPointer(2,GL_FLOAT, stride, (void*)(base + mOffsets[TYPE_TEXCOORD]));
	}
	if (data_mask & MAP_COLOR)
	{
		glColorPointer(4, GL_UNSIGNED_BYTE, stride, (void*)(base + mOffsets[TYPE_COLOR]));
	}
	if (data_mask & MAP_BINORMAL)
	{
		glVertexAttribPointerARB(6, 3, GL_FLOAT, FALSE,  stride, (void*)(base + mOffsets[TYPE_BINORMAL]));
	}
	if (data_mask & MAP_WEIGHT)
	{
		glVertexAttribPointerARB(1, 1, GL_FLOAT, FALSE, stride, (void*)(base + mOffsets[TYPE_WEIGHT]));
	}
	if (data_mask & MAP_CLOTHWEIGHT)
	{
		glVertexAttribPointerARB(4, 4, GL_FLOAT, TRUE,  stride, (void*)(base + mOffsets[TYPE_CLOTHWEIGHT]));
	}
	if (data_mask & MAP_VERTEX)
	{
		glVertexPointer(3,GL_FLOAT, stride, (void*)(base + 0));
	}

	llglassertok();
}

void LLVertexBuffer::markDirty(U32 vert_index, U32 vert_count, U32 indices_index, U32 indices_count)
{
	// TODO: use GL_APPLE_flush_buffer_range here
	/*if (useVBOs() && !mFilthy)
	{
	
	}*/
}
