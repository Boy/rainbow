/** 
 * @file llrender.cpp
 * @brief LLRender implementation
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

#include "linden_common.h"

#include "llrender.h"

#include "llvertexbuffer.h"
#include "llcubemap.h"
#include "llimagegl.h"
#include "llrendertarget.h"

LLRender gGL;

// Handy copies of last good GL matrices
F64	gGLModelView[16];
F64	gGLLastModelView[16];
F64 gGLProjection[16];
S32	gGLViewport[4];

static const U32 LL_NUM_TEXTURE_LAYERS = 8; 

static GLenum sGLTextureType[] =
{
	GL_TEXTURE_2D,
	GL_TEXTURE_RECTANGLE_ARB,
	GL_TEXTURE_CUBE_MAP_ARB
};

static GLint sGLAddressMode[] =
{	
	GL_REPEAT,
	GL_MIRRORED_REPEAT,
	GL_CLAMP_TO_EDGE
};

static GLenum sGLCompareFunc[] =
{
	GL_NEVER,
	GL_ALWAYS,
	GL_LESS,
	GL_LEQUAL,
	GL_EQUAL,
	GL_NOTEQUAL,
	GL_GEQUAL,
	GL_GREATER
};

const U32 immediate_mask = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_COLOR | LLVertexBuffer::MAP_TEXCOORD;

static GLenum sGLBlendFactor[] =
{
	GL_ONE,
	GL_ZERO,
	GL_DST_COLOR,
	GL_SRC_COLOR,
	GL_ONE_MINUS_DST_COLOR,
	GL_ONE_MINUS_SRC_COLOR,
	GL_DST_ALPHA,
	GL_SRC_ALPHA,
	GL_ONE_MINUS_DST_ALPHA,
	GL_ONE_MINUS_SRC_ALPHA
};

LLTexUnit::LLTexUnit(S32 index)
: mCurrTexType(TT_NONE), mCurrBlendType(TB_MULT), 
mCurrColorOp(TBO_MULT), mCurrAlphaOp(TBO_MULT),
mCurrColorSrc1(TBS_TEX_COLOR), mCurrColorSrc2(TBS_PREV_COLOR),
mCurrAlphaSrc1(TBS_TEX_ALPHA), mCurrAlphaSrc2(TBS_PREV_ALPHA),
mCurrColorScale(1), mCurrAlphaScale(1), mCurrTexture(0)
{
	llassert_always(index < LL_NUM_TEXTURE_LAYERS);
	mIndex = index;
}

//static
U32 LLTexUnit::getInternalType(eTextureType type)
{
	return sGLTextureType[type];
}

void LLTexUnit::refreshState(void)
{
	// We set dirty to true so that the tex unit knows to ignore caching
	// and we reset the cached tex unit state

	glActiveTextureARB(GL_TEXTURE0_ARB + mIndex);
	if (mCurrTexType != TT_NONE)
	{
		glEnable(sGLTextureType[mCurrTexType]);
		glBindTexture(sGLTextureType[mCurrTexType], mCurrTexture);
	}
	else
	{
		glDisable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);	
	}

	if (mCurrBlendType != TB_COMBINE)
	{
		setTextureBlendType(mCurrBlendType);
	}
	else
	{
		setTextureCombiner(mCurrColorOp, mCurrColorSrc1, mCurrColorSrc2, false);
		setTextureCombiner(mCurrAlphaOp, mCurrAlphaSrc1, mCurrAlphaSrc2, true);
	}
}

void LLTexUnit::activate(void)
{
	if (mIndex < 0) return;

	if (gGL.mCurrTextureUnitIndex != mIndex || gGL.mDirty)
	{
		glActiveTextureARB(GL_TEXTURE0_ARB + mIndex);
		gGL.mCurrTextureUnitIndex = mIndex;
	}
}

void LLTexUnit::enable(eTextureType type)
{
	if (mIndex < 0) return;

	if ( (mCurrTexType != type || gGL.mDirty) && (type != TT_NONE) )
	{
		activate();
		if (mCurrTexType != TT_NONE && !gGL.mDirty)
		{
			disable(); // Force a disable of a previous texture type if it's enabled.
		}
		mCurrTexType = type;
		glEnable(sGLTextureType[type]);
	}
}

void LLTexUnit::disable(void)
{
	if (mIndex < 0) return;

	if (mCurrTexType != TT_NONE)
	{
		activate();
		unbind(mCurrTexType);
		glDisable(sGLTextureType[mCurrTexType]);
		mCurrTexType = TT_NONE;
	}
}

bool LLTexUnit::bind(const LLImageGL* texture, bool forceBind)
{
	if (mIndex < 0) return false;

	gGL.flush();

	if (texture == NULL)
	{
		llwarns << "NULL LLTexUnit::bind texture" << llendl;
		return false;
	}
	
	if (!texture->getTexName()) //if texture does not exist
	{
		return texture->bindDefaultImage(mIndex);
	}
	// Disabled caching of binding state.
	activate();
	enable(texture->getTarget());
	mCurrTexture = texture->getTexName();
	glBindTexture(sGLTextureType[texture->getTarget()], mCurrTexture);
	texture->updateBindStats();
	return true;
}

bool LLTexUnit::bind(LLCubeMap* cubeMap)
{
	if (mIndex < 0) return false;

	gGL.flush();

	// Disabled caching of binding state.
	if (cubeMap != NULL)
	{
		if (gGLManager.mHasCubeMap && LLCubeMap::sUseCubeMaps)
		{
			activate();
			enable(LLTexUnit::TT_CUBE_MAP);
			mCurrTexture = cubeMap->mImages[0]->getTexName();
			glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, mCurrTexture);
			cubeMap->mImages[0]->updateBindStats();
			cubeMap->mImages[0]->setMipFilterNearest (FALSE, FALSE);
			return true;
		}
		else
		{
			llwarns << "Using cube map without extension!" << llendl
		}
	}
	return false;
}

bool LLTexUnit::bind(LLRenderTarget* renderTarget, bool bindDepth)
{
	if (mIndex < 0) return false;

	gGL.flush();

	if (bindDepth)
	{
		bindManual(renderTarget->getUsage(), renderTarget->getDepth());
	}
	else
	{
		bindManual(renderTarget->getUsage(), renderTarget->getTexture());
	}

	return true;
}

bool LLTexUnit::bindManual(eTextureType type, U32 texture)
{
	if (mIndex < 0) return false;

	// Disabled caching of binding state.
	gGL.flush();
	
	activate();
	enable(type);
	mCurrTexture = texture;
	glBindTexture(sGLTextureType[type], texture);
	return true;
}

void LLTexUnit::unbind(eTextureType type)
{
	if (mIndex < 0) return;

	// Disabled caching of binding state.
	if (mCurrTexType == type)
	{
		gGL.flush();

		activate();
		mCurrTexture = 0;
		glBindTexture(sGLTextureType[type], 0);
	}
}

void LLTexUnit::setTextureAddressMode(eTextureAddressMode mode)
{
	if (mIndex < 0) return;

	if (true)
	{
		activate();

		glTexParameteri (sGLTextureType[mCurrTexType], GL_TEXTURE_WRAP_S, sGLAddressMode[mode]);
		glTexParameteri (sGLTextureType[mCurrTexType], GL_TEXTURE_WRAP_T, sGLAddressMode[mode]);
		if (mCurrTexType == TT_CUBE_MAP)
		{
			glTexParameteri (GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_R, sGLAddressMode[mode]);
		}
	}
}

void LLTexUnit::setTextureBlendType(eTextureBlendType type)
{
	if (mIndex < 0) return;

	// Do nothing if it's already correctly set.
	if (mCurrBlendType == type && !gGL.mDirty)
	{
		return;
	}

	activate();
	mCurrBlendType = type;
	S32 scale_amount = 1;
	switch (type) 
	{
		case TB_REPLACE:
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			break;
		case TB_ADD:
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
			break;
		case TB_MULT:
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			break;
		case TB_MULT_X2:
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			scale_amount = 2;
			break;
		case TB_ALPHA_BLEND:
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
			break;
		case TB_COMBINE:
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
			break;
		default:
			llerrs << "Unknown Texture Blend Type: " << type << llendl;
			break;
	}
	setColorScale(scale_amount);
	setAlphaScale(1);
}

GLint LLTexUnit::getTextureSource(eTextureBlendSrc src)
{
	switch(src)
	{
		// All four cases should return the same value.
		case TBS_PREV_COLOR:
		case TBS_PREV_ALPHA:
		case TBS_ONE_MINUS_PREV_COLOR:
		case TBS_ONE_MINUS_PREV_ALPHA:
			return GL_PREVIOUS_ARB;

		// All four cases should return the same value.
		case TBS_TEX_COLOR:
		case TBS_TEX_ALPHA:
		case TBS_ONE_MINUS_TEX_COLOR:
		case TBS_ONE_MINUS_TEX_ALPHA:
			return GL_TEXTURE;

		// All four cases should return the same value.
		case TBS_VERT_COLOR:
		case TBS_VERT_ALPHA:
		case TBS_ONE_MINUS_VERT_COLOR:
		case TBS_ONE_MINUS_VERT_ALPHA:
			return GL_PRIMARY_COLOR_ARB;

		// All four cases should return the same value.
		case TBS_CONST_COLOR:
		case TBS_CONST_ALPHA:
		case TBS_ONE_MINUS_CONST_COLOR:
		case TBS_ONE_MINUS_CONST_ALPHA:
			return GL_CONSTANT_ARB;

		default:
			llwarns << "Unknown eTextureBlendSrc: " << src << ".  Using Vertex Color instead." << llendl;
			return GL_PRIMARY_COLOR_ARB;
	}
}

GLint LLTexUnit::getTextureSourceType(eTextureBlendSrc src, bool isAlpha)
{
	switch(src)
	{
		// All four cases should return the same value.
		case TBS_PREV_COLOR:
		case TBS_TEX_COLOR:
		case TBS_VERT_COLOR:
		case TBS_CONST_COLOR:
			return (isAlpha) ? GL_SRC_ALPHA: GL_SRC_COLOR;

		// All four cases should return the same value.
		case TBS_PREV_ALPHA:
		case TBS_TEX_ALPHA:
		case TBS_VERT_ALPHA:
		case TBS_CONST_ALPHA:
			return GL_SRC_ALPHA;

		// All four cases should return the same value.
		case TBS_ONE_MINUS_PREV_COLOR:
		case TBS_ONE_MINUS_TEX_COLOR:
		case TBS_ONE_MINUS_VERT_COLOR:
		case TBS_ONE_MINUS_CONST_COLOR:
			return (isAlpha) ? GL_ONE_MINUS_SRC_ALPHA : GL_ONE_MINUS_SRC_COLOR;

		// All four cases should return the same value.
		case TBS_ONE_MINUS_PREV_ALPHA:
		case TBS_ONE_MINUS_TEX_ALPHA:
		case TBS_ONE_MINUS_VERT_ALPHA:
		case TBS_ONE_MINUS_CONST_ALPHA:
			return GL_ONE_MINUS_SRC_ALPHA;

		default:
			llwarns << "Unknown eTextureBlendSrc: " << src << ".  Using Source Color or Alpha instead." << llendl;
			return (isAlpha) ? GL_SRC_ALPHA: GL_SRC_COLOR;
	}
}

void LLTexUnit::setTextureCombiner(eTextureBlendOp op, eTextureBlendSrc src1, eTextureBlendSrc src2, bool isAlpha)
{
	if (mIndex < 0) return;

	activate();
	if (mCurrBlendType != TB_COMBINE || gGL.mDirty)
	{
		mCurrBlendType = TB_COMBINE;
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
	}

	// We want an early out, because this function does a LOT of stuff.
	if ( ( (isAlpha && (mCurrAlphaOp == op) && (mCurrAlphaSrc1 == src1) && (mCurrAlphaSrc2 == src2))
			|| (!isAlpha && (mCurrColorOp == op) && (mCurrColorSrc1 == src1) && (mCurrColorSrc2 == src2)) ) && !gGL.mDirty)
	{
		return;
	}

	// Get the gl source enums according to the eTextureBlendSrc sources passed in
	GLint source1 = getTextureSource(src1);
	GLint source2 = getTextureSource(src2);
	// Get the gl operand enums according to the eTextureBlendSrc sources passed in
	GLint operand1 = getTextureSourceType(src1, isAlpha);
	GLint operand2 = getTextureSourceType(src2, isAlpha);
	// Default the scale amount to 1
	S32 scale_amount = 1;
	GLenum comb_enum, src0_enum, src1_enum, src2_enum, operand0_enum, operand1_enum, operand2_enum;
	
	if (isAlpha)
	{
		// Set enums to ALPHA ones
		comb_enum = GL_COMBINE_ALPHA_ARB;
		src0_enum = GL_SOURCE0_ALPHA_ARB;
		src1_enum = GL_SOURCE1_ALPHA_ARB;
		src2_enum = GL_SOURCE2_ALPHA_ARB;
		operand0_enum = GL_OPERAND0_ALPHA_ARB;
		operand1_enum = GL_OPERAND1_ALPHA_ARB;
		operand2_enum = GL_OPERAND2_ALPHA_ARB;

		// cache current combiner
		mCurrAlphaOp = op;
		mCurrAlphaSrc1 = src1;
		mCurrAlphaSrc2 = src2;
	}
	else 
	{
		// Set enums to RGB ones
		comb_enum = GL_COMBINE_RGB_ARB;
		src0_enum = GL_SOURCE0_RGB_ARB;
		src1_enum = GL_SOURCE1_RGB_ARB;
		src2_enum = GL_SOURCE2_RGB_ARB;
		operand0_enum = GL_OPERAND0_RGB_ARB;
		operand1_enum = GL_OPERAND1_RGB_ARB;
		operand2_enum = GL_OPERAND2_RGB_ARB;

		// cache current combiner
		mCurrColorOp = op;
		mCurrColorSrc1 = src1;
		mCurrColorSrc2 = src2;
	}

	switch(op)
	{
		case TBO_REPLACE:
			// Slightly special syntax (no second sources), just set all and return.
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_ENV, src0_enum, source1);
			glTexEnvi(GL_TEXTURE_ENV, operand0_enum, operand1);
			(isAlpha) ? setAlphaScale(1) : setColorScale(1);
			return;

		case TBO_MULT:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_MODULATE);
			break;

		case TBO_MULT_X2:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_MODULATE);
			scale_amount = 2;
			break;

		case TBO_MULT_X4:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_MODULATE);
			scale_amount = 4;
			break;

		case TBO_ADD:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_ADD);
			break;

		case TBO_ADD_SIGNED:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_ADD_SIGNED_ARB);
			break;

		case TBO_SUBTRACT:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_SUBTRACT_ARB);
			break;

		case TBO_LERP_VERT_ALPHA:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_INTERPOLATE);
			glTexEnvi(GL_TEXTURE_ENV, src2_enum, GL_PRIMARY_COLOR_ARB);
			glTexEnvi(GL_TEXTURE_ENV, operand2_enum, GL_SRC_ALPHA);
			break;

		case TBO_LERP_TEX_ALPHA:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_INTERPOLATE);
			glTexEnvi(GL_TEXTURE_ENV, src2_enum, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, operand2_enum, GL_SRC_ALPHA);
			break;

		case TBO_LERP_PREV_ALPHA:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_INTERPOLATE);
			glTexEnvi(GL_TEXTURE_ENV, src2_enum, GL_PREVIOUS_ARB);
			glTexEnvi(GL_TEXTURE_ENV, operand2_enum, GL_SRC_ALPHA);
			break;

		case TBO_LERP_CONST_ALPHA:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_INTERPOLATE);
			glTexEnvi(GL_TEXTURE_ENV, src2_enum, GL_CONSTANT_ARB);
			glTexEnvi(GL_TEXTURE_ENV, operand2_enum, GL_SRC_ALPHA);
			break;

		case TBO_LERP_VERT_COLOR:
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_INTERPOLATE);
			glTexEnvi(GL_TEXTURE_ENV, src2_enum, GL_PRIMARY_COLOR_ARB);
			glTexEnvi(GL_TEXTURE_ENV, operand2_enum, (isAlpha) ? GL_SRC_ALPHA : GL_SRC_COLOR);
			break;

		default:
			llwarns << "Unknown eTextureBlendOp: " << op << ".  Setting op to replace." << llendl;
			// Slightly special syntax (no second sources), just set all and return.
			glTexEnvi(GL_TEXTURE_ENV, comb_enum, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_ENV, src0_enum, source1);
			glTexEnvi(GL_TEXTURE_ENV, operand0_enum, operand1);
			(isAlpha) ? setAlphaScale(1) : setColorScale(1);
			return;
	}

	// Set sources, operands, and scale accordingly
	glTexEnvi(GL_TEXTURE_ENV, src0_enum, source1);
	glTexEnvi(GL_TEXTURE_ENV, operand0_enum, operand1);
	glTexEnvi(GL_TEXTURE_ENV, src1_enum, source2);
	glTexEnvi(GL_TEXTURE_ENV, operand1_enum, operand2);
	(isAlpha) ? setAlphaScale(scale_amount) : setColorScale(scale_amount);
}

void LLTexUnit::setColorScale(S32 scale)
{
	if (mCurrColorScale != scale || gGL.mDirty)
	{
		mCurrColorScale = scale;
		glTexEnvi( GL_TEXTURE_ENV, GL_RGB_SCALE, scale );
	}
}

void LLTexUnit::setAlphaScale(S32 scale)
{
	if (mCurrAlphaScale != scale || gGL.mDirty)
	{
		mCurrAlphaScale = scale;
		glTexEnvi( GL_TEXTURE_ENV, GL_ALPHA_SCALE, scale );
	}
}

// Useful for debugging that you've manually assigned a texture operation to the correct 
// texture unit based on the currently set active texture in opengl.
void LLTexUnit::debugTextureUnit(void)
{
	if (mIndex < 0) return;

	GLint activeTexture;
	glGetIntegerv(GL_ACTIVE_TEXTURE_ARB, &activeTexture);
	if ((GL_TEXTURE0_ARB + mIndex) != activeTexture)
	{
		U32 set_unit = (activeTexture - GL_TEXTURE0_ARB);
		llwarns << "Incorrect Texture Unit!  Expected: " << set_unit << " Actual: " << mIndex << llendl;
	}
}


LLRender::LLRender()
: mDirty(false), mCount(0), mMode(LLRender::TRIANGLES)
{
	mBuffer = new LLVertexBuffer(immediate_mask, 0);
	mBuffer->allocateBuffer(4096, 0, TRUE);
	mBuffer->getVertexStrider(mVerticesp);
	mBuffer->getTexCoordStrider(mTexcoordsp);
	mBuffer->getColorStrider(mColorsp);
	
	mTexUnits.reserve(LL_NUM_TEXTURE_LAYERS);
	for (U32 i = 0; i < LL_NUM_TEXTURE_LAYERS; i++)
	{
		mTexUnits.push_back(new LLTexUnit(i));
	}
	mDummyTexUnit = new LLTexUnit(-1);

	for (U32 i = 0; i < 4; i++)
	{
		mCurrColorMask[i] = true;
	}

	mCurrAlphaFunc = CF_DEFAULT;
	mCurrAlphaFuncVal = 0.01f;
}

LLRender::~LLRender()
{
	shutdown();
}

void LLRender::shutdown()
{
	for (U32 i = 0; i < mTexUnits.size(); i++)
	{
		delete mTexUnits[i];
	}
	mTexUnits.clear();
	delete mDummyTexUnit;
	mDummyTexUnit = NULL;
}

void LLRender::refreshState(void)
{
	mDirty = true;

	U32 active_unit = mCurrTextureUnitIndex;

	for (U32 i = 0; i < mTexUnits.size(); i++)
	{
		mTexUnits[i]->refreshState();
	}
	
	mTexUnits[active_unit]->activate();

	setColorMask(mCurrColorMask[0], mCurrColorMask[1], mCurrColorMask[2], mCurrColorMask[3]);
	
	setAlphaRejectSettings(mCurrAlphaFunc, mCurrAlphaFuncVal);

	mDirty = false;
}

void LLRender::translatef(const GLfloat& x, const GLfloat& y, const GLfloat& z)
{
	flush();
	glTranslatef(x,y,z);
}

void LLRender::scalef(const GLfloat& x, const GLfloat& y, const GLfloat& z)
{
	flush();
	glScalef(x,y,z);
}

void LLRender::pushMatrix()
{
	flush();
	glPushMatrix();
}

void LLRender::popMatrix()
{
	flush();
	glPopMatrix();
}

void LLRender::setColorMask(bool writeColor, bool writeAlpha)
{
	setColorMask(writeColor, writeColor, writeColor, writeAlpha);
}

void LLRender::setColorMask(bool writeColorR, bool writeColorG, bool writeColorB, bool writeAlpha)
{
	flush();

	mCurrColorMask[0] = writeColorR;
	mCurrColorMask[1] = writeColorG;
	mCurrColorMask[2] = writeColorB;
	mCurrColorMask[3] = writeAlpha;

	glColorMask(writeColorR, writeColorG, writeColorB, writeAlpha);
}

void LLRender::setSceneBlendType(eBlendType type)
{
	flush();
	switch (type) 
	{
		case BT_ALPHA:
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			break;
		case BT_ADD:
			glBlendFunc(GL_ONE, GL_ONE);
			break;
		case BT_ADD_WITH_ALPHA:
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			break;
		case BT_MULT:
			glBlendFunc(GL_DST_COLOR, GL_ZERO);
			break;
		case BT_MULT_X2:
			glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
			break;
		case BT_REPLACE:
			glBlendFunc(GL_ONE, GL_ZERO);
			break;
		default:
			llerrs << "Unknown Scene Blend Type: " << type << llendl;
			break;
	}
}

void LLRender::setAlphaRejectSettings(eCompareFunc func, F32 value)
{
	flush();

	mCurrAlphaFunc = func;
	mCurrAlphaFuncVal = value;
	if (func == CF_DEFAULT)
	{
		glAlphaFunc(GL_GREATER, 0.01f);
	} 
	else
	{
		glAlphaFunc(sGLCompareFunc[func], value);
	}
}

void LLRender::blendFunc(eBlendFactor sfactor, eBlendFactor dfactor)
{
	flush();
	glBlendFunc(sGLBlendFactor[sfactor], sGLBlendFactor[dfactor]);
}

LLTexUnit* LLRender::getTexUnit(U32 index)
{
	if ((index >= 0) && (index < mTexUnits.size()))
	{
		return mTexUnits[index];
	}
	else 
	{
		lldebugs << "Non-existing texture unit layer requested: " << index << llendl;
		return mDummyTexUnit;
	}
}

bool LLRender::verifyTexUnitActive(U32 unitToVerify)
{
	if (mCurrTextureUnitIndex == unitToVerify)
	{
		return true;
	}
	else 
	{
		llwarns << "TexUnit currently active: " << mCurrTextureUnitIndex << " (expecting " << unitToVerify << ")" << llendl;
		return false;
	}
}

void LLRender::begin(const GLuint& mode)
{
	if (mode != mMode)
	{
		if (mMode == LLRender::QUADS ||
			mMode == LLRender::LINES ||
			mMode == LLRender::TRIANGLES ||
			mMode == LLRender::POINTS)
		{
			flush();
		}
		else if (mCount != 0)
		{
			llerrs << "gGL.begin() called redundantly." << llendl;
		}
		
		mMode = mode;
	}
}

void LLRender::end()
{ 
	if (mCount == 0)
	{
		return;
		//IMM_ERRS << "GL begin and end called with no vertices specified." << llendl;
	}

	if ((mMode != LLRender::QUADS && 
		mMode != LLRender::LINES &&
		mMode != LLRender::TRIANGLES &&
		mMode != LLRender::POINTS) ||
		mCount > 2048)
	{
		flush();
	}
}
void LLRender::flush()
{
	if (mCount > 0)
	{
#if 0
		if (!glIsEnabled(GL_VERTEX_ARRAY))
		{
			llerrs << "foo 1" << llendl;
		}

		if (!glIsEnabled(GL_COLOR_ARRAY))
		{
			llerrs << "foo 2" << llendl;
		}

		if (!glIsEnabled(GL_TEXTURE_COORD_ARRAY))
		{
			llerrs << "foo 3" << llendl;
		}

		if (glIsEnabled(GL_NORMAL_ARRAY))
		{
			llerrs << "foo 7" << llendl;
		}

		GLvoid* pointer;

		glGetPointerv(GL_VERTEX_ARRAY_POINTER, &pointer);
		if (pointer != &(mBuffer[0].v))
		{
			llerrs << "foo 4" << llendl;
		}

		glGetPointerv(GL_COLOR_ARRAY_POINTER, &pointer);
		if (pointer != &(mBuffer[0].c))
		{
			llerrs << "foo 5" << llendl;
		}

		glGetPointerv(GL_TEXTURE_COORD_ARRAY_POINTER, &pointer);
		if (pointer != &(mBuffer[0].uv))
		{
			llerrs << "foo 6" << llendl;
		}
#endif
				
		mBuffer->setBuffer(immediate_mask);
		mBuffer->drawArrays(mMode, 0, mCount);

		mVerticesp[0] = mVerticesp[mCount];
		mTexcoordsp[0] = mTexcoordsp[mCount];
		mColorsp[0] = mColorsp[mCount];
		mCount = 0;
	}
}
void LLRender::vertex3f(const GLfloat& x, const GLfloat& y, const GLfloat& z)
{ 
	//the range of mVerticesp, mColorsp and mTexcoordsp is [0, 4095]
	if (mCount > 4094)
	{
	//	llwarns << "GL immediate mode overflow.  Some geometry not drawn." << llendl;
		return;
	}

	mVerticesp[mCount] = LLVector3(x,y,z);
	mCount++;
	if (mCount < 4096)
	{
		mVerticesp[mCount] = mVerticesp[mCount-1];
		mColorsp[mCount] = mColorsp[mCount-1];
		mTexcoordsp[mCount] = mTexcoordsp[mCount-1];
	}
}
void LLRender::vertex2i(const GLint& x, const GLint& y)
{
	vertex3f((GLfloat) x, (GLfloat) y, 0);	
}

void LLRender::vertex2f(const GLfloat& x, const GLfloat& y)
{ 
	vertex3f(x,y,0);
}

void LLRender::vertex2fv(const GLfloat* v)
{ 
	vertex3f(v[0], v[1], 0);
}

void LLRender::vertex3fv(const GLfloat* v)
{
	vertex3f(v[0], v[1], v[2]);
}

void LLRender::texCoord2f(const GLfloat& x, const GLfloat& y)
{ 
	mTexcoordsp[mCount] = LLVector2(x,y);
}

void LLRender::texCoord2i(const GLint& x, const GLint& y)
{ 
	texCoord2f((GLfloat) x, (GLfloat) y);
}

void LLRender::texCoord2fv(const GLfloat* tc)
{ 
	texCoord2f(tc[0], tc[1]);
}

void LLRender::color4ub(const GLubyte& r, const GLubyte& g, const GLubyte& b, const GLubyte& a)
{
	mColorsp[mCount] = LLColor4U(r,g,b,a);
}
void LLRender::color4ubv(const GLubyte* c)
{
	color4ub(c[0], c[1], c[2], c[3]);
}

void LLRender::color4f(const GLfloat& r, const GLfloat& g, const GLfloat& b, const GLfloat& a)
{
	color4ub((GLubyte) (llclamp(r, 0.f, 1.f)*255),
		(GLubyte) (llclamp(g, 0.f, 1.f)*255),
		(GLubyte) (llclamp(b, 0.f, 1.f)*255),
		(GLubyte) (llclamp(a, 0.f, 1.f)*255));
}

void LLRender::color4fv(const GLfloat* c)
{ 
	color4f(c[0],c[1],c[2],c[3]);
}

void LLRender::color3f(const GLfloat& r, const GLfloat& g, const GLfloat& b)
{ 
	color4f(r,g,b,1);
}

void LLRender::color3fv(const GLfloat* c)
{ 
	color4f(c[0],c[1],c[2],1);
}

void LLRender::debugTexUnits(void)
{
	LL_INFOS("TextureUnit") << "Active TexUnit: " << mCurrTextureUnitIndex << LL_ENDL;
	std::string active_enabled = "false";
	for (U32 i = 0; i < mTexUnits.size(); i++)
	{
		if (getTexUnit(i)->mCurrTexType != LLTexUnit::TT_NONE)
		{
			if (i == mCurrTextureUnitIndex) active_enabled = "true";
			LL_INFOS("TextureUnit") << "TexUnit: " << i << " Enabled" << LL_ENDL;
			LL_INFOS("TextureUnit") << "Enabled As: " ;
			switch (getTexUnit(i)->mCurrTexType)
			{
				case LLTexUnit::TT_TEXTURE:
					LL_CONT << "Texture 2D";
					break;
				case LLTexUnit::TT_RECT_TEXTURE:
					LL_CONT << "Texture Rectangle";
					break;
				case LLTexUnit::TT_CUBE_MAP:
					LL_CONT << "Cube Map";
					break;
				default:
					LL_CONT << "ARGH!!! NONE!";
					break;
			}
			LL_CONT << ", Texture Bound: " << getTexUnit(i)->mCurrTexture << LL_ENDL;
		}
	}
	LL_INFOS("TextureUnit") << "Active TexUnit Enabled : " << active_enabled << LL_ENDL;
}

