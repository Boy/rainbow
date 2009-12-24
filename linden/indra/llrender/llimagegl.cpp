/** 
 * @file llimagegl.cpp
 * @brief Generic GL image handler
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


// TODO: create 2 classes for images w/ and w/o discard levels?

#include "linden_common.h"

#include "llimagegl.h"

#include "llerror.h"
#include "llimage.h"

#include "llmath.h"
#include "llgl.h"
#include "llrender.h"


//----------------------------------------------------------------------------

const F32 MIN_TEXTURE_LIFETIME = 10.f;

//statics
LLGLuint LLImageGL::sCurrentBoundTextures[MAX_GL_TEXTURE_UNITS] = { 0 };

U32 LLImageGL::sUniqueCount				= 0;
U32 LLImageGL::sBindCount				= 0;
S32 LLImageGL::sGlobalTextureMemory		= 0;
S32 LLImageGL::sBoundTextureMemory		= 0;
S32 LLImageGL::sCurBoundTextureMemory	= 0;
S32 LLImageGL::sCount					= 0;

BOOL LLImageGL::sGlobalUseAnisotropic	= FALSE;
F32 LLImageGL::sLastFrameTime			= 0.f;

std::set<LLImageGL*> LLImageGL::sImageList;

//**************************************************************************************
//below are functions for debug use
//do not delete them even though they are not currently being used.
void check_all_images()
{
	for (std::set<LLImageGL*>::iterator iter = LLImageGL::sImageList.begin();
		 iter != LLImageGL::sImageList.end(); iter++)
	{
		LLImageGL* glimage = *iter;
		if (glimage->getTexName() && glimage->isGLTextureCreated())
		{
			gGL.getTexUnit(0)->bind(glimage) ;
			glimage->checkTexSize() ;
			gGL.getTexUnit(0)->unbind(glimage->getTarget()) ;
		}
	}
}

void LLImageGL::checkTexSize() const
{
	if (gDebugGL && mTarget == GL_TEXTURE_2D)
	{
		GLint texname;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &texname);
		if (texname != mTexName)
		{
			llerrs << "Invalid texture bound!" << llendl;
		}
		stop_glerror() ;
		LLGLint x = 0, y = 0 ;
		glGetTexLevelParameteriv(mTarget, 0, GL_TEXTURE_WIDTH, (GLint*)&x);
		glGetTexLevelParameteriv(mTarget, 0, GL_TEXTURE_HEIGHT, (GLint*)&y) ;
		stop_glerror() ;
		if(!x || !y)
		{
			return ;
		}
		if(x != (mWidth >> mCurrentDiscardLevel) || y != (mHeight >> mCurrentDiscardLevel))
		{
			llerrs << "wrong texture size and discard level!" << llendl ;
		}
	}
}
//end of debug functions
//**************************************************************************************

//----------------------------------------------------------------------------

//static
S32 LLImageGL::dataFormatBits(S32 dataformat)
{
	switch (dataformat)
	{
	  case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:	return 4;
	  case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:	return 8;
	  case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:	return 8;
	  case GL_LUMINANCE:						return 8;
	  case GL_ALPHA:							return 8;
	  case GL_COLOR_INDEX:						return 8;
	  case GL_LUMINANCE_ALPHA:					return 16;
	  case GL_RGB:								return 24;
	  case GL_RGB8:								return 24;
	  case GL_RGBA:								return 32;
	  case GL_BGRA:								return 32;		// Used for QuickTime media textures on the Mac
	  default:
		llerrs << "LLImageGL::Unknown format: " << dataformat << llendl;
		return 0;
	}
}

//static
S32 LLImageGL::dataFormatBytes(S32 dataformat, S32 width, S32 height)
{
	if (dataformat >= GL_COMPRESSED_RGB_S3TC_DXT1_EXT &&
		dataformat <= GL_COMPRESSED_RGBA_S3TC_DXT5_EXT)
	{
		if (width < 4) width = 4;
		if (height < 4) height = 4;
	}
	S32 bytes ((width*height*dataFormatBits(dataformat)+7)>>3);
	S32 aligned = (bytes+3)&~3;
	return aligned;
}

//static
S32 LLImageGL::dataFormatComponents(S32 dataformat)
{
	switch (dataformat)
	{
	  case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:	return 3;
	  case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:	return 4;
	  case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:	return 4;
	  case GL_LUMINANCE:						return 1;
	  case GL_ALPHA:							return 1;
	  case GL_COLOR_INDEX:						return 1;
	  case GL_LUMINANCE_ALPHA:					return 2;
	  case GL_RGB:								return 3;
	  case GL_RGBA:								return 4;
	  case GL_BGRA:								return 4;		// Used for QuickTime media textures on the Mac
	  default:
		llerrs << "LLImageGL::Unknown format: " << dataformat << llendl;
		return 0;
	}
}

//----------------------------------------------------------------------------

// static
void LLImageGL::updateStats(F32 current_time)
{
	sLastFrameTime = current_time;
	sBoundTextureMemory = sCurBoundTextureMemory;
	sCurBoundTextureMemory = 0;
}

//static
S32 LLImageGL::updateBoundTexMem(const S32 delta)
{
	LLImageGL::sCurBoundTextureMemory += delta;
	return LLImageGL::sCurBoundTextureMemory;
}

//----------------------------------------------------------------------------

//static 
void LLImageGL::destroyGL(BOOL save_state)
{
	for (S32 stage = 0; stage < gGLManager.mNumTextureUnits; stage++)
	{
		gGL.getTexUnit(stage)->unbind(LLTexUnit::TT_TEXTURE);
	}
	
	for (std::set<LLImageGL*>::iterator iter = sImageList.begin();
		 iter != sImageList.end(); iter++)
	{
		LLImageGL* glimage = *iter;
		if (glimage->mTexName)
		{
			if (save_state && glimage->isGLTextureCreated() && glimage->mComponents)
			{
				glimage->mSaveData = new LLImageRaw;
				if(!glimage->readBackRaw(glimage->mCurrentDiscardLevel, glimage->mSaveData, false))
				{
					glimage->mSaveData = NULL ;
				}
			}

			glimage->destroyGLTexture();
			stop_glerror();
		}
	}
}

//static 
void LLImageGL::restoreGL()
{
	for (std::set<LLImageGL*>::iterator iter = sImageList.begin();
		 iter != sImageList.end(); iter++)
	{
		LLImageGL* glimage = *iter;
		if(glimage->getTexName())
		{
			llerrs << "tex name is not 0." << llendl ;
		}
		if (glimage->mSaveData.notNull())
		{
			if (glimage->getComponents() && glimage->mSaveData->getComponents())
			{
				glimage->createGLTexture(glimage->mCurrentDiscardLevel, glimage->mSaveData);
				stop_glerror();
			}
			glimage->mSaveData = NULL; // deletes data
		}
	}
}

//----------------------------------------------------------------------------

//static 
BOOL LLImageGL::create(LLPointer<LLImageGL>& dest, BOOL usemipmaps)
{
	dest = new LLImageGL(usemipmaps);
	return TRUE;
}

BOOL LLImageGL::create(LLPointer<LLImageGL>& dest, U32 width, U32 height, U8 components, BOOL usemipmaps)
{
	dest = new LLImageGL(width, height, components, usemipmaps);
	return TRUE;
}

BOOL LLImageGL::create(LLPointer<LLImageGL>& dest, const LLImageRaw* imageraw, BOOL usemipmaps)
{
	dest = new LLImageGL(imageraw, usemipmaps);
	return TRUE;
}

//----------------------------------------------------------------------------

LLImageGL::LLImageGL(BOOL usemipmaps)
	: mSaveData(0)
{
	init(usemipmaps);
	setSize(0, 0, 0);
	sImageList.insert(this);
	sCount++;
}

LLImageGL::LLImageGL(U32 width, U32 height, U8 components, BOOL usemipmaps)
	: mSaveData(0)
{
	llassert( components <= 4 );
	init(usemipmaps);
	setSize(width, height, components);
	sImageList.insert(this);
	sCount++;
}

LLImageGL::LLImageGL(const LLImageRaw* imageraw, BOOL usemipmaps)
	: mSaveData(0)
{
	init(usemipmaps);
	setSize(0, 0, 0);
	sImageList.insert(this);
	sCount++;

	createGLTexture(0, imageraw); 
}

LLImageGL::~LLImageGL()
{
	LLImageGL::cleanup();
	sImageList.erase(this);
	delete [] mPickMask;
	mPickMask = NULL;
	sCount--;
}

void LLImageGL::init(BOOL usemipmaps)
{
#ifdef DEBUG_MISS
	mMissed				= FALSE;
#endif

	mPickMask		  = NULL;
	mTextureMemory    = 0;
	mLastBindTime     = 0.f;

	mTarget			  = GL_TEXTURE_2D;
	mBindTarget		  = LLTexUnit::TT_TEXTURE;
	mUseMipMaps		  = usemipmaps;
	mHasMipMaps		  = FALSE;
	mAutoGenMips	  = FALSE;
	mTexName          = 0;
	mIsResident       = 0;
	mClampS			  = FALSE;
	mClampT			  = FALSE;
	mClampR			  = FALSE;
	mMagFilterNearest  = FALSE;
	mMinFilterNearest  = FALSE;
	mWidth				= 0;
	mHeight				= 0;
	mComponents			= 0;
	
	mMaxDiscardLevel = MAX_DISCARD_LEVEL;
	mCurrentDiscardLevel = -1;
	mDontDiscard = FALSE;
	
	mFormatInternal = -1;
	mFormatPrimary = (LLGLenum) 0;
	mFormatType = GL_UNSIGNED_BYTE;
	mFormatSwapBytes = FALSE;
	mHasExplicitFormat = FALSE;

	mGLTextureCreated = FALSE ;
}

void LLImageGL::cleanup()
{
	if (!gGLManager.mIsDisabled)
	{
		destroyGLTexture();
	}
	mSaveData = NULL; // deletes data
}

//----------------------------------------------------------------------------

//this function is used to check the size of a texture image.
//so dim should be a positive number
static bool check_power_of_two(S32 dim)
{
	if(dim < 0)
	{
		return false ;
	}
	if(!dim)//0 is a power-of-two number
	{
		return true ;
	}
	return !(dim & (dim - 1)) ;
}

//static
bool LLImageGL::checkSize(S32 width, S32 height)
{
	return check_power_of_two(width) && check_power_of_two(height);
}

void LLImageGL::setSize(S32 width, S32 height, S32 ncomponents)
{
	if (width != mWidth || height != mHeight || ncomponents != mComponents)
	{
		// Check if dimensions are a power of two!
		if (!checkSize(width,height))
		{
			llerrs << llformat("Texture has non power of two dimention: %dx%d",width,height) << llendl;
		}
		
		if (mTexName)
		{
// 			llwarns << "Setting Size of LLImageGL with existing mTexName = " << mTexName << llendl;
			destroyGLTexture();
		}
		
		mWidth = width;
		mHeight = height;
		mComponents = ncomponents;
		if (ncomponents > 0)
		{
			mMaxDiscardLevel = 0;
			while (width > 1 && height > 1 && mMaxDiscardLevel < MAX_DISCARD_LEVEL)
			{
				mMaxDiscardLevel++;
				width >>= 1;
				height >>= 1;
			}
		}
		else
		{
			mMaxDiscardLevel = MAX_DISCARD_LEVEL;
		}
	}
}

//----------------------------------------------------------------------------

// virtual
void LLImageGL::dump()
{
	llinfos << "mMaxDiscardLevel " << S32(mMaxDiscardLevel)
			<< " mLastBindTime " << mLastBindTime
			<< " mTarget " << S32(mTarget)
			<< " mBindTarget " << S32(mBindTarget)
			<< " mUseMipMaps " << S32(mUseMipMaps)
			<< " mHasMipMaps " << S32(mHasMipMaps)
			<< " mCurrentDiscardLevel " << S32(mCurrentDiscardLevel)
			<< " mFormatInternal " << S32(mFormatInternal)
			<< " mFormatPrimary " << S32(mFormatPrimary)
			<< " mFormatType " << S32(mFormatType)
			<< " mFormatSwapBytes " << S32(mFormatSwapBytes)
			<< " mHasExplicitFormat " << S32(mHasExplicitFormat)
#if DEBUG_MISS
			<< " mMissed " << mMissed
#endif
			<< llendl;

	llinfos << " mTextureMemory " << mTextureMemory
			<< " mTexNames " << mTexName
			<< " mIsResident " << S32(mIsResident)
			<< llendl;
}

//----------------------------------------------------------------------------

void LLImageGL::updateBindStats(void) const
{	
	if (mTexName != 0)
	{
#ifdef DEBUG_MISS
		mMissed = ! getIsResident(TRUE);
#endif
		sBindCount++;
		if (mLastBindTime != sLastFrameTime)
		{
			// we haven't accounted for this texture yet this frame
			sUniqueCount++;
			updateBoundTexMem(mTextureMemory);
			mLastBindTime = sLastFrameTime;
		}
	}
}

//virtual
bool LLImageGL::bindError(const S32 stage) const
{
	return false;
}

//virtual
bool LLImageGL::bindDefaultImage(const S32 stage) const
{
	return false;
}


void LLImageGL::setExplicitFormat( LLGLint internal_format, LLGLenum primary_format, LLGLenum type_format, BOOL swap_bytes )
{
	// Note: must be called before createTexture()
	// Note: it's up to the caller to ensure that the format matches the number of components.
	mHasExplicitFormat = TRUE;
	mFormatInternal = internal_format;
	mFormatPrimary = primary_format;
	if(type_format == 0)
		mFormatType = GL_UNSIGNED_BYTE;
	else
		mFormatType = type_format;
	mFormatSwapBytes = swap_bytes;
}

//----------------------------------------------------------------------------

void LLImageGL::setImage(const LLImageRaw* imageraw)
{
	llassert((imageraw->getWidth() == getWidth(mCurrentDiscardLevel)) &&
			 (imageraw->getHeight() == getHeight(mCurrentDiscardLevel)) &&
			 (imageraw->getComponents() == getComponents()));
	const U8* rawdata = imageraw->getData();
	setImage(rawdata, FALSE);
}

void LLImageGL::setImage(const U8* data_in, BOOL data_hasmips)
{
// 	LLFastTimer t1(LLFastTimer::FTM_TEMP1);
	
	bool is_compressed = false;
	if (mFormatPrimary >= GL_COMPRESSED_RGBA_S3TC_DXT1_EXT && mFormatPrimary <= GL_COMPRESSED_RGBA_S3TC_DXT5_EXT)
	{
		is_compressed = true;
	}

	{
// 		LLFastTimer t2(LLFastTimer::FTM_TEMP2);
		llverify(gGL.getTexUnit(0)->bind(this));
	}
	
	if (mUseMipMaps)
	{
// 		LLFastTimer t2(LLFastTimer::FTM_TEMP3);
		if (data_hasmips)
		{
			// NOTE: data_in points to largest image; smaller images
			// are stored BEFORE the largest image
			for (S32 d=mCurrentDiscardLevel; d<=mMaxDiscardLevel; d++)
			{
				S32 w = getWidth(d);
				S32 h = getHeight(d);
				S32 gl_level = d-mCurrentDiscardLevel;
				if (d > mCurrentDiscardLevel)
				{
					data_in -= dataFormatBytes(mFormatPrimary, w, h); // see above comment
				}
				if (is_compressed)
				{
// 					LLFastTimer t2(LLFastTimer::FTM_TEMP4);
					S32 tex_size = dataFormatBytes(mFormatPrimary, w, h);
					glCompressedTexImage2DARB(mTarget, gl_level, mFormatPrimary, w, h, 0, tex_size, (GLvoid *)data_in);
					stop_glerror();
				}
				else
				{
// 					LLFastTimer t2(LLFastTimer::FTM_TEMP4);

					if(mFormatSwapBytes)
					{
						glPixelStorei(GL_UNPACK_SWAP_BYTES, 1);
						stop_glerror();
					}
						
					glTexImage2D(mTarget, gl_level, mFormatInternal, w, h, 0, mFormatPrimary, GL_UNSIGNED_BYTE, (GLvoid*)data_in);
					updatePickMask(w, h, data_in);

					if(mFormatSwapBytes)
					{
						glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
						stop_glerror();
					}
						
					stop_glerror();
				}
				stop_glerror();
			}			
		}
		else if (!is_compressed)
		{
			if (mAutoGenMips)
			{
				glTexParameteri(LLTexUnit::getInternalType(mBindTarget), GL_GENERATE_MIPMAP_SGIS, TRUE);
				stop_glerror();
				{
// 					LLFastTimer t2(LLFastTimer::FTM_TEMP4);

					if(mFormatSwapBytes)
					{
						glPixelStorei(GL_UNPACK_SWAP_BYTES, 1);
						stop_glerror();
					}

					S32 w = getWidth(mCurrentDiscardLevel);
					S32 h = getHeight(mCurrentDiscardLevel);

					glTexImage2D(mTarget, 0, mFormatInternal,
								 w, h, 0,
								 mFormatPrimary, mFormatType,
								 data_in);
					stop_glerror();

					updatePickMask(w, h, data_in);

					if(mFormatSwapBytes)
					{
						glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
						stop_glerror();
					}
				}
			}
			else
			{
				// Create mips by hand
				// about 30% faster than autogen on ATI 9800, 50% slower on nVidia 4800
				// ~4x faster than gluBuild2DMipmaps
				S32 width = getWidth(mCurrentDiscardLevel);
				S32 height = getHeight(mCurrentDiscardLevel);
				S32 nummips = mMaxDiscardLevel - mCurrentDiscardLevel + 1;
				S32 w = width, h = height;
				const U8* prev_mip_data = 0;
				const U8* cur_mip_data = 0;
				S32 prev_mip_size = 0;
				S32 cur_mip_size = 0;
				for (int m=0; m<nummips; m++)
				{
					if (m==0)
					{
						cur_mip_data = data_in;
						cur_mip_size = width * height * mComponents; 
					}
					else
					{
						S32 bytes = w * h * mComponents;
						llassert(prev_mip_data);
						llassert(prev_mip_size == bytes*4);
						U8* new_data = new U8[bytes];
						llassert_always(new_data);
						LLImageBase::generateMip(prev_mip_data, new_data, w, h, mComponents);
						cur_mip_data = new_data;
						cur_mip_size = bytes; 
					}
					llassert(w > 0 && h > 0 && cur_mip_data);
					{
// 						LLFastTimer t1(LLFastTimer::FTM_TEMP4);
						if(mFormatSwapBytes)
						{
							glPixelStorei(GL_UNPACK_SWAP_BYTES, 1);
							stop_glerror();
						}

						glTexImage2D(mTarget, m, mFormatInternal, w, h, 0, mFormatPrimary, mFormatType, cur_mip_data);
						stop_glerror();
						if (m == 0)
						{
							updatePickMask(w, h, cur_mip_data);
						}

						if(mFormatSwapBytes)
						{
							glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
							stop_glerror();
						}
					}
					if (prev_mip_data && prev_mip_data != data_in)
					{
						delete[] prev_mip_data;
					}
					prev_mip_data = cur_mip_data;
					prev_mip_size = cur_mip_size;
					w >>= 1;
					h >>= 1;
				}
				if (prev_mip_data && prev_mip_data != data_in)
				{
					delete[] prev_mip_data;
					prev_mip_data = NULL;
				}
			}
		}
		else
		{
			llerrs << "Compressed Image has mipmaps but data does not (can not auto generate compressed mips)" << llendl;
		}
		mHasMipMaps = TRUE;
	}
	else
	{
// 		LLFastTimer t2(LLFastTimer::FTM_TEMP5);
		S32 w = getWidth();
		S32 h = getHeight();
		if (is_compressed)
		{
			S32 tex_size = dataFormatBytes(mFormatPrimary, w, h);
			glCompressedTexImage2DARB(mTarget, 0, mFormatPrimary, w, h, 0, tex_size, (GLvoid *)data_in);
			stop_glerror();
		}
		else
		{
			if(mFormatSwapBytes)
			{
				glPixelStorei(GL_UNPACK_SWAP_BYTES, 1);
				stop_glerror();
			}

			glTexImage2D(mTarget, 0, mFormatInternal, w, h, 0,
						 mFormatPrimary, mFormatType, (GLvoid *)data_in);
			updatePickMask(w, h, data_in);

			stop_glerror();

			if(mFormatSwapBytes)
			{
				glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
				stop_glerror();
			}

		}
		mHasMipMaps = FALSE;
	}
	stop_glerror();
	mGLTextureCreated = true;
}

BOOL LLImageGL::setSubImage(const U8* datap, S32 data_width, S32 data_height, S32 x_pos, S32 y_pos, S32 width, S32 height)
{
	if (!width || !height)
	{
		return TRUE;
	}
	if (mTexName == 0)
	{
		llwarns << "Setting subimage on image without GL texture" << llendl;
		return FALSE;
	}
	if (datap == NULL)
	{
		llwarns << "Setting subimage on image with NULL datap" << llendl;
		return FALSE;
	}
	
	if (x_pos == 0 && y_pos == 0 && width == getWidth() && height == getHeight() && data_width == width && data_height == height)
	{
		setImage(datap, FALSE);
	}
	else
	{
		if (mUseMipMaps)
		{
			dump();
			llerrs << "setSubImage called with mipmapped image (not supported)" << llendl;
		}
		llassert_always(mCurrentDiscardLevel == 0);
		llassert_always(x_pos >= 0 && y_pos >= 0);
		
		if (((x_pos + width) > getWidth()) || 
			(y_pos + height) > getHeight())
		{
			dump();
			llerrs << "Subimage not wholly in target image!" 
				   << " x_pos " << x_pos
				   << " y_pos " << y_pos
				   << " width " << width
				   << " height " << height
				   << " getWidth() " << getWidth()
				   << " getHeight() " << getHeight()
				   << llendl;
		}

		if ((x_pos + width) > data_width || 
			(y_pos + height) > data_height)
		{
			dump();
			llerrs << "Subimage not wholly in source image!" 
				   << " x_pos " << x_pos
				   << " y_pos " << y_pos
				   << " width " << width
				   << " height " << height
				   << " source_width " << data_width
				   << " source_height " << data_height
				   << llendl;
		}


		glPixelStorei(GL_UNPACK_ROW_LENGTH, data_width);
		stop_glerror();

		if(mFormatSwapBytes)
		{
			glPixelStorei(GL_UNPACK_SWAP_BYTES, 1);
			stop_glerror();
		}

		datap += (y_pos * data_width + x_pos) * getComponents();
		// Update the GL texture
		BOOL res = gGL.getTexUnit(0)->bindManual(mBindTarget, mTexName);
		if (!res) llerrs << "LLImageGL::setSubImage(): bindTexture failed" << llendl;
		stop_glerror();

		glTexSubImage2D(mTarget, 0, x_pos, y_pos, 
						width, height, mFormatPrimary, mFormatType, datap);
		stop_glerror();

		if(mFormatSwapBytes)
		{
			glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
			stop_glerror();
		}

		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		stop_glerror();
		mGLTextureCreated = true;
	}
	return TRUE;
}

BOOL LLImageGL::setSubImage(const LLImageRaw* imageraw, S32 x_pos, S32 y_pos, S32 width, S32 height)
{
	return setSubImage(imageraw->getData(), imageraw->getWidth(), imageraw->getHeight(), x_pos, y_pos, width, height);
}

// Copy sub image from frame buffer
BOOL LLImageGL::setSubImageFromFrameBuffer(S32 fb_x, S32 fb_y, S32 x_pos, S32 y_pos, S32 width, S32 height)
{
	if (gGL.getTexUnit(0)->bind(this, true))
	{
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, fb_x, fb_y, x_pos, y_pos, width, height);
		mGLTextureCreated = true;
		stop_glerror();
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

//create an empty GL texture: just create a texture name
//the texture is assiciate with some image by calling glTexImage outside LLImageGL
BOOL LLImageGL::createGLTexture()
{
	if (gGLManager.mIsDisabled)
	{
		llwarns << "Trying to create a texture while GL is disabled!" << llendl;
		return FALSE;
	}
	
	mGLTextureCreated = false ; //do not save this texture when gl is destroyed.

	llassert(gGLManager.mInited);
	stop_glerror();

	if(mTexName)
	{
		glDeleteTextures(1, (reinterpret_cast<GLuint*>(&mTexName))) ;
	}
	
	glGenTextures(1, (GLuint*)&mTexName);
	stop_glerror();
	if (!mTexName)
	{
		llerrs << "LLImageGL::createGLTexture failed to make an empty texture" << llendl;
	}

	return TRUE ;
}

BOOL LLImageGL::createGLTexture(S32 discard_level, const LLImageRaw* imageraw, S32 usename/*=0*/)
{
	if (gGLManager.mIsDisabled)
	{
		llwarns << "Trying to create a texture while GL is disabled!" << llendl;
		return FALSE;
	}
	mGLTextureCreated = false ;
	llassert(gGLManager.mInited);
	stop_glerror();

	if (discard_level < 0)
	{
		llassert(mCurrentDiscardLevel >= 0);
		discard_level = mCurrentDiscardLevel;
	}
	discard_level = llclamp(discard_level, 0, (S32)mMaxDiscardLevel);

	// Actual image width/height = raw image width/height * 2^discard_level
	S32 w = imageraw->getWidth() << discard_level;
	S32 h = imageraw->getHeight() << discard_level;

	// setSize may call destroyGLTexture if the size does not match
	setSize(w, h, imageraw->getComponents());

	if( !mHasExplicitFormat )
	{
		switch (mComponents)
		{
		  case 1:
			// Use luminance alpha (for fonts)
			mFormatInternal = GL_LUMINANCE8;
			mFormatPrimary = GL_LUMINANCE;
			mFormatType = GL_UNSIGNED_BYTE;
			break;
		  case 2:
			// Use luminance alpha (for fonts)
			mFormatInternal = GL_LUMINANCE8_ALPHA8;
			mFormatPrimary = GL_LUMINANCE_ALPHA;
			mFormatType = GL_UNSIGNED_BYTE;
			break;
		  case 3:
			mFormatInternal = GL_RGB8;
			mFormatPrimary = GL_RGB;
			mFormatType = GL_UNSIGNED_BYTE;
			break;
		  case 4:
			mFormatInternal = GL_RGBA8;
			mFormatPrimary = GL_RGBA;
			mFormatType = GL_UNSIGNED_BYTE;
			break;
		  default:
			llerrs << "Bad number of components for texture: " << (U32)getComponents() << llendl;
		}
	}

 	const U8* rawdata = imageraw->getData();
	return createGLTexture(discard_level, rawdata, FALSE, usename);
}

BOOL LLImageGL::createGLTexture(S32 discard_level, const U8* data_in, BOOL data_hasmips, S32 usename)
{
	llassert(data_in);

	if (discard_level < 0)
	{
		llassert(mCurrentDiscardLevel >= 0);
		discard_level = mCurrentDiscardLevel;
	}
	discard_level = llclamp(discard_level, 0, (S32)mMaxDiscardLevel);

	if (mTexName != 0 && discard_level == mCurrentDiscardLevel)
	{
		// This will only be true if the size has not changed
		setImage(data_in, data_hasmips);
		return TRUE;
	}
	
	GLuint old_name = mTexName;
// 	S32 old_discard = mCurrentDiscardLevel;
	
	if (usename != 0)
	{
		mTexName = usename;
	}
	else
	{
		glGenTextures(1, (GLuint*)&mTexName);
		stop_glerror();
		{
// 			LLFastTimer t1(LLFastTimer::FTM_TEMP6);
			llverify(gGL.getTexUnit(0)->bind(this));
			glTexParameteri(LLTexUnit::getInternalType(mBindTarget), GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(LLTexUnit::getInternalType(mBindTarget), GL_TEXTURE_MAX_LEVEL,  mMaxDiscardLevel-discard_level);
		}
	}
	if (!mTexName)
	{
		llerrs << "LLImageGL::createGLTexture failed to make texture" << llendl;
	}

	if (mUseMipMaps)
	{
		mAutoGenMips = gGLManager.mHasMipMapGeneration;
#if LL_DARWIN
		// On the Mac GF2 and GF4MX drivers, auto mipmap generation doesn't work right with alpha-only textures.
		if(gGLManager.mIsGF2or4MX && (mFormatInternal == GL_ALPHA8) && (mFormatPrimary == GL_ALPHA))
		{
			mAutoGenMips = FALSE;
		}
#endif
	}

	mCurrentDiscardLevel = discard_level;	

	setImage(data_in, data_hasmips);

	setClamp(mClampS, mClampT);
	setMipFilterNearest(mMagFilterNearest);
	
	// things will break if we don't unbind after creation
	gGL.getTexUnit(0)->unbind(mBindTarget);
	stop_glerror();

	if (old_name != 0)
	{
		sGlobalTextureMemory -= mTextureMemory;
		glDeleteTextures(1, &old_name);
		stop_glerror();
	}

	mTextureMemory = getMipBytes(discard_level);
	sGlobalTextureMemory += mTextureMemory;
		
	// mark this as bound at this point, so we don't throw it out immediately
	mLastBindTime = sLastFrameTime;

	return TRUE;
}

BOOL LLImageGL::setDiscardLevel(S32 discard_level)
{
	llassert(discard_level >= 0);
	llassert(mCurrentDiscardLevel >= 0);

	discard_level = llclamp(discard_level, 0, (S32)mMaxDiscardLevel);	

	if (mDontDiscard)
	{
		// don't discard!
		return FALSE;
	}
	else if (discard_level == mCurrentDiscardLevel)
	{
		// nothing to do
		return FALSE;
	}
	else if (discard_level < mCurrentDiscardLevel)
	{
		// larger image
		dump();
		llerrs << "LLImageGL::setDiscardLevel() called with larger discard level; use createGLTexture()" << llendl;
		return FALSE;
	}
	else if (mUseMipMaps)
	{
		LLPointer<LLImageRaw> imageraw = new LLImageRaw;
		while(discard_level > mCurrentDiscardLevel)
		{
			if (readBackRaw(discard_level, imageraw, false))
			{
				break;
			}
			discard_level--;
		}
		if (discard_level == mCurrentDiscardLevel)
		{
			// unable to increase the discard level
			return FALSE;
		}
		return createGLTexture(discard_level, imageraw);
	}
	else
	{
#if !LL_LINUX && !LL_SOLARIS
		 // *FIX: This should not be skipped for the linux client.
		llerrs << "LLImageGL::setDiscardLevel() called on image without mipmaps" << llendl;
#endif
		return FALSE;
	}
}

BOOL LLImageGL::isValidForSculpt(S32 discard_level, S32 image_width, S32 image_height, S32 ncomponents)
{
	assert_glerror();
	S32 gl_discard = discard_level - mCurrentDiscardLevel;
	LLGLint glwidth = 0;
	glGetTexLevelParameteriv(mTarget, gl_discard, GL_TEXTURE_WIDTH, (GLint*)&glwidth);
	LLGLint glheight = 0;
	glGetTexLevelParameteriv(mTarget, gl_discard, GL_TEXTURE_HEIGHT, (GLint*)&glheight);
	LLGLint glcomponents = 0 ;
	glGetTexLevelParameteriv(mTarget, gl_discard, GL_TEXTURE_INTERNAL_FORMAT, (GLint*)&glcomponents);
	assert_glerror();

	return glwidth >= image_width && glheight >= image_height && (GL_RGB8 == glcomponents || GL_RGBA8 == glcomponents) ;
}

BOOL LLImageGL::readBackRaw(S32 discard_level, LLImageRaw* imageraw, bool compressed_ok)
{
	if (discard_level < 0)
	{
		discard_level = mCurrentDiscardLevel;
	}
	
	if (mTexName == 0 || discard_level < mCurrentDiscardLevel || discard_level > mMaxDiscardLevel )
	{
		return FALSE;
	}

	S32 gl_discard = discard_level - mCurrentDiscardLevel;

	//explicitly unbind texture 
	gGL.getTexUnit(0)->unbind(mBindTarget);
	llverify(gGL.getTexUnit(0)->bind(this));	

	//debug code, leave it there commented.
	//checkTexSize() ;

	LLGLint glwidth = 0;
	glGetTexLevelParameteriv(mTarget, gl_discard, GL_TEXTURE_WIDTH, (GLint*)&glwidth);
	if (glwidth == 0)
	{
		// No mip data smaller than current discard level
		return FALSE;
	}
	
	S32 width = getWidth(discard_level);
	S32 height = getHeight(discard_level);
	S32 ncomponents = getComponents();
	if (ncomponents == 0)
	{
		return FALSE;
	}
	if(width < glwidth)
	{
		llwarns << "texture size is smaller than it should be." << llendl ;
		llwarns << "width: " << width << " glwidth: " << glwidth << " mWidth: " << mWidth << 
			" mCurrentDiscardLevel: " << (S32)mCurrentDiscardLevel << " discard_level: " << (S32)discard_level << llendl ;
		return FALSE ;
	}

	if (width <= 0 || width > 2048 || height <= 0 || height > 2048 || ncomponents < 1 || ncomponents > 4)
	{
		llerrs << llformat("LLImageGL::readBackRaw: bogus params: %d x %d x %d",width,height,ncomponents) << llendl;
	}
	
	LLGLint is_compressed = 0;
	if (compressed_ok)
	{
		glGetTexLevelParameteriv(mTarget, is_compressed, GL_TEXTURE_COMPRESSED, (GLint*)&is_compressed);
	}
	
	//-----------------------------------------------------------------------------------------------
	GLenum error ;
	while((error = glGetError()) != GL_NO_ERROR)
	{
		llwarns << "GL Error happens before reading back texture. Error code: " << error << llendl ;
	}
	//-----------------------------------------------------------------------------------------------

	if (is_compressed)
	{
		LLGLint glbytes;
		glGetTexLevelParameteriv(mTarget, gl_discard, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, (GLint*)&glbytes);
		if(!imageraw->allocateDataSize(width, height, ncomponents, glbytes))
		{
			llwarns << "Memory allocation failed for reading back texture. Size is: " << glbytes << llendl ;
			llwarns << "width: " << width << "height: " << height << "components: " << ncomponents << llendl ;
			return FALSE ;
		}

		glGetCompressedTexImageARB(mTarget, gl_discard, (GLvoid*)(imageraw->getData()));		
		//stop_glerror();
	}
	else
	{
		if(!imageraw->allocateDataSize(width, height, ncomponents))
		{
			llwarns << "Memory allocation failed for reading back texture." << llendl ;
			llwarns << "width: " << width << "height: " << height << "components: " << ncomponents << llendl ;
			return FALSE ;
		}
		
		glGetTexImage(GL_TEXTURE_2D, gl_discard, mFormatPrimary, mFormatType, (GLvoid*)(imageraw->getData()));		
		//stop_glerror();
	}
		
	//-----------------------------------------------------------------------------------------------
	if((error = glGetError()) != GL_NO_ERROR)
	{
		llwarns << "GL Error happens after reading back texture. Error code: " << error << llendl ;
		imageraw->deleteData() ;

		while((error = glGetError()) != GL_NO_ERROR)
		{
			llwarns << "GL Error happens after reading back texture. Error code: " << error << llendl ;
		}

		return FALSE ;
	}
	//-----------------------------------------------------------------------------------------------
	
	return TRUE ;
}

void LLImageGL::destroyGLTexture()
{
	if (mTexName != 0)
	{
		stop_glerror();

		for (int i = 0; i < gGLManager.mNumTextureUnits; i++)
		{
			if (sCurrentBoundTextures[i] == mTexName)
			{
				gGL.getTexUnit(i)->unbind(LLTexUnit::TT_TEXTURE);
				stop_glerror();
			}
		}

		sGlobalTextureMemory -= mTextureMemory;
		mTextureMemory = 0;

		glDeleteTextures(1, (GLuint*)&mTexName);
		mTexName = 0;
		mGLTextureCreated = FALSE ;
		stop_glerror();
	}
}

//----------------------------------------------------------------------------

void LLImageGL::glClampCubemap (BOOL clamps, BOOL clampt, BOOL clampr)
{
	glTexParameteri (GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_S, clamps ? GL_CLAMP_TO_EDGE : GL_REPEAT);
	glTexParameteri (GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_T, clamps ? GL_CLAMP_TO_EDGE : GL_REPEAT);
	glTexParameteri (GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_R, clamps ? GL_CLAMP_TO_EDGE : GL_REPEAT);
}

void LLImageGL::glClamp (BOOL clamps, BOOL clampt)
{
	if (mTexName != 0)
	{
		glTexParameteri (LLTexUnit::getInternalType(mBindTarget), GL_TEXTURE_WRAP_S, clamps ? GL_CLAMP_TO_EDGE : GL_REPEAT);
		glTexParameteri (LLTexUnit::getInternalType(mBindTarget), GL_TEXTURE_WRAP_T, clampt ? GL_CLAMP_TO_EDGE : GL_REPEAT);
	}
}

void LLImageGL::setClampCubemap (BOOL clamps, BOOL clampt, BOOL clampr)
{
	mClampS = clamps;
	mClampT = clampt;
	mClampR = clampr;
	glClampCubemap (clamps, clampt, clampr);
}

void LLImageGL::setClamp(BOOL clamps, BOOL clampt)
{
	mClampS = clamps;
	mClampT = clampt;
	glClamp (clamps, clampt);
}

void LLImageGL::overrideClamp (BOOL clamps, BOOL clampt)
{
	glClamp (clamps, clampt);
}

void LLImageGL::restoreClamp (void)
{
	glClamp (mClampS, mClampT);
}

void LLImageGL::setMipFilterNearest(BOOL mag_nearest, BOOL min_nearest)
{
	mMagFilterNearest = mag_nearest;
	mMinFilterNearest = min_nearest;

	if (mTexName != 0)
	{
		if (mMinFilterNearest)
		{
			glTexParameteri(LLTexUnit::getInternalType(mBindTarget), GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		}
		else if (mHasMipMaps)
		{
			glTexParameteri(LLTexUnit::getInternalType(mBindTarget), GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		}
		else
		{
			glTexParameteri(LLTexUnit::getInternalType(mBindTarget), GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		}
		if (mMagFilterNearest)
		{
			glTexParameteri(LLTexUnit::getInternalType(mBindTarget), GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}
		else 
		{
			glTexParameteri(LLTexUnit::getInternalType(mBindTarget), GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		if (gGLManager.mHasAnisotropic)
		{
			if (sGlobalUseAnisotropic && !mMagFilterNearest)
			{
				F32 largest_anisotropy;
				glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &largest_anisotropy);
				glTexParameterf(LLTexUnit::getInternalType(mBindTarget), GL_TEXTURE_MAX_ANISOTROPY_EXT, largest_anisotropy);
			}
			else
			{
				glTexParameterf(LLTexUnit::getInternalType(mBindTarget), GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.f);
			}
		}
		stop_glerror();
	}	
}

BOOL LLImageGL::getIsResident(BOOL test_now)
{
	if (test_now)
	{
		if (mTexName != 0)
		{
			glAreTexturesResident(1, (GLuint*)&mTexName, &mIsResident);
		}
		else
		{
			mIsResident = FALSE;
		}
	}

	return mIsResident;
}

S32 LLImageGL::getHeight(S32 discard_level) const
{
	if (discard_level < 0)
	{
		discard_level = mCurrentDiscardLevel;
	}
	S32 height = mHeight >> discard_level;
	if (height < 1) height = 1;
	return height;
}

S32 LLImageGL::getWidth(S32 discard_level) const
{
	if (discard_level < 0)
	{
		discard_level = mCurrentDiscardLevel;
	}
	S32 width = mWidth >> discard_level;
	if (width < 1) width = 1;
	return width;
}

S32 LLImageGL::getBytes(S32 discard_level) const
{
	if (discard_level < 0)
	{
		discard_level = mCurrentDiscardLevel;
	}
	S32 w = mWidth>>discard_level;
	S32 h = mHeight>>discard_level;
	if (w == 0) w = 1;
	if (h == 0) h = 1;
	return dataFormatBytes(mFormatPrimary, w, h);
}

S32 LLImageGL::getMipBytes(S32 discard_level) const
{
	if (discard_level < 0)
	{
		discard_level = mCurrentDiscardLevel;
	}
	S32 w = mWidth>>discard_level;
	S32 h = mHeight>>discard_level;
	S32 res = dataFormatBytes(mFormatPrimary, w, h);
	if (mUseMipMaps)
	{
		while (w > 1 && h > 1)
		{
			w >>= 1; if (w == 0) w = 1;
			h >>= 1; if (h == 0) h = 1;
			res += dataFormatBytes(mFormatPrimary, w, h);
		}
	}
	return res;
}

BOOL LLImageGL::getBoundRecently() const
{
	return (BOOL)(sLastFrameTime - mLastBindTime < MIN_TEXTURE_LIFETIME);
}

void LLImageGL::setTarget(const LLGLenum target, const LLTexUnit::eTextureType bind_target)
{
	mTarget = target;
	mBindTarget = bind_target;
}

void LLImageGL::updatePickMask(S32 width, S32 height, const U8* data_in)
{
	if (mFormatType != GL_UNSIGNED_BYTE ||
		mFormatPrimary != GL_RGBA)
	{
		//cannot generate a pick mask for this texture
		delete [] mPickMask;
		mPickMask = NULL;
		return;
	}

	U32 pick_width = width/2;
	U32 pick_height = height/2;

	U32 size = llmax(pick_width, (U32) 1) * llmax(pick_height, (U32) 1);

	size = size/8 + 1;

	delete[] mPickMask;
	mPickMask = new U8[size];

	memset(mPickMask, 0, sizeof(U8) * size);

	U32 pick_bit = 0;
	
	for (S32 y = 0; y < height; y += 2)
	{
		for (S32 x = 0; x < width; x += 2)
		{
			U8 alpha = data_in[(y*width+x)*4+3];

			if (alpha > 32)
			{
				U32 pick_idx = pick_bit/8;
				U32 pick_offset = pick_bit%8;
				if (pick_idx >= size)
				{
					llerrs << "WTF?" << llendl;
				}

				mPickMask[pick_idx] |= 1 << pick_offset;
			}
			
			++pick_bit;
		}
	}
}

BOOL LLImageGL::getMask(const LLVector2 &tc)
{
	BOOL res = TRUE;

	if (mPickMask)
	{
		S32 width = getWidth()/2;
		S32 height = getHeight()/2;

		F32 u = tc.mV[0] - floorf(tc.mV[0]);
		F32 v = tc.mV[1] - floorf(tc.mV[1]);

		if (u < 0.f || u > 1.f ||
		    v < 0.f || v > 1.f)
		{
			llerrs << "WTF?" << llendl;
		}
		
		S32 x = (S32)(u * width);
		S32 y = (S32)(v * height);

		S32 idx = y*width+x;
		S32 offset = idx%8;

		res = mPickMask[idx/8] & (1 << offset) ? TRUE : FALSE;
	}
	
	return res;
}

//----------------------------------------------------------------------------


// Manual Mip Generation
/*
		S32 width = getWidth(discard_level);
		S32 height = getHeight(discard_level);
		S32 w = width, h = height;
		S32 nummips = 1;
		while (w > 4 && h > 4)
		{
			w >>= 1; h >>= 1;
			nummips++;
		}
		stop_glerror();
		w = width, h = height;
		const U8* prev_mip_data = 0;
		const U8* cur_mip_data = 0;
		for (int m=0; m<nummips; m++)
		{
			if (m==0)
			{
				cur_mip_data = rawdata;
			}
			else
			{
				S32 bytes = w * h * mComponents;
				U8* new_data = new U8[bytes];
				LLImageBase::generateMip(prev_mip_data, new_data, w, h, mComponents);
				cur_mip_data = new_data;
			}
			llassert(w > 0 && h > 0 && cur_mip_data);
			U8 test = cur_mip_data[w*h*mComponents-1];
			{
				glTexImage2D(mTarget, m, mFormatInternal, w, h, 0, mFormatPrimary, mFormatType, cur_mip_data);
				stop_glerror();
			}
			if (prev_mip_data && prev_mip_data != rawdata)
			{
				delete prev_mip_data;
			}
			prev_mip_data = cur_mip_data;
			w >>= 1;
			h >>= 1;
		}
		if (prev_mip_data && prev_mip_data != rawdata)
		{
			delete prev_mip_data;
		}
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL,  nummips);
*/  
