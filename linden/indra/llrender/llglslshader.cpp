/** 
 * @file llglslshader.cpp
 * @brief GLSL helper functions and state.
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

#include "linden_common.h"

#include "llglslshader.h"

#include "llshadermgr.h"
#include "llfile.h"
#include "llrender.h"

#if LL_DARWIN
#include "OpenGL/OpenGL.h"
#endif

#ifdef LL_RELEASE_FOR_DOWNLOAD
#define UNIFORM_ERRS LL_WARNS_ONCE("Shader")
#else
#define UNIFORM_ERRS LL_ERRS("Shader")
#endif

// Lots of STL stuff in here, using namespace std to keep things more readable
using std::vector;
using std::pair;
using std::make_pair;
using std::string;

BOOL shouldChange(const LLVector4& v1, const LLVector4& v2)
{
	return v1 != v2;
}

LLShaderFeatures::LLShaderFeatures()
: calculatesLighting(false), isShiny(false), isFullbright(false), hasWaterFog(false),
hasTransport(false), hasSkinning(false), hasAtmospherics(false), isSpecular(false),
hasGamma(false), hasLighting(false), calculatesAtmospherics(false)
{
}

//===============================
// LLGLSL Shader implementation
//===============================
LLGLSLShader::LLGLSLShader()
: mProgramObject(0), mShaderLevel(0), mShaderGroup(SG_DEFAULT)
{
}

void LLGLSLShader::unload()
{
	stop_glerror();
	mAttribute.clear();
	mTexture.clear();
	mUniform.clear();
	mShaderFiles.clear();

	if (mProgramObject)
	{
		GLhandleARB obj[1024];
		GLsizei count;

		glGetAttachedObjectsARB(mProgramObject, 1024, &count, obj);
		for (GLsizei i = 0; i < count; i++)
		{
			glDeleteObjectARB(obj[i]);
		}

		glDeleteObjectARB(mProgramObject);

		mProgramObject = 0;
	}
	
	//hack to make apple not complain
	glGetError();
	
	stop_glerror();
}

BOOL LLGLSLShader::createShader(vector<string> * attributes,
								vector<string> * uniforms)
{
	llassert_always(!mShaderFiles.empty());
	BOOL success = TRUE;

	// Create program
	mProgramObject = glCreateProgramObjectARB();
	
	// Attach existing objects
	if (!LLShaderMgr::instance()->attachShaderFeatures(this))
	{
		return FALSE;
	}

	vector< pair<string,GLenum> >::iterator fileIter = mShaderFiles.begin();
	for ( ; fileIter != mShaderFiles.end(); fileIter++ )
	{
		GLhandleARB shaderhandle = LLShaderMgr::instance()->loadShaderFile((*fileIter).first, mShaderLevel, (*fileIter).second);
		LL_DEBUGS("ShaderLoading") << "SHADER FILE: " << (*fileIter).first << " mShaderLevel=" << mShaderLevel << LL_ENDL;
		if (mShaderLevel > 0)
		{
			attachObject(shaderhandle);
		}
		else
		{
			success = FALSE;
		}
	}

	// Map attributes and uniforms
	if (success)
	{
		success = mapAttributes(attributes);
	}
	if (success)
	{
		success = mapUniforms(uniforms);
	}
	if( !success )
	{
		LL_WARNS("ShaderLoading") << "Failed to link shader: " << mName << LL_ENDL;

		// Try again using a lower shader level;
		if (mShaderLevel > 0)
		{
			LL_WARNS("ShaderLoading") << "Failed to link using shader level " << mShaderLevel << " trying again using shader level " << (mShaderLevel - 1) << LL_ENDL;
			mShaderLevel--;
			return createShader(attributes,uniforms);
		}
	}
	return success;
}

BOOL LLGLSLShader::attachObject(std::string object)
{
	if (LLShaderMgr::instance()->mShaderObjects.count(object) > 0)
	{
		stop_glerror();
		glAttachObjectARB(mProgramObject, LLShaderMgr::instance()->mShaderObjects[object]);
		stop_glerror();
		return TRUE;
	}
	else
	{
		LL_WARNS("ShaderLoading") << "Attempting to attach shader object that hasn't been compiled: " << object << LL_ENDL;
		return FALSE;
	}
}

void LLGLSLShader::attachObject(GLhandleARB object)
{
	if (object != 0)
	{
		stop_glerror();
		glAttachObjectARB(mProgramObject, object);
		stop_glerror();
	}
	else
	{
		LL_WARNS("ShaderLoading") << "Attempting to attach non existing shader object. " << LL_ENDL;
	}
}

void LLGLSLShader::attachObjects(GLhandleARB* objects, S32 count)
{
	for (S32 i = 0; i < count; i++)
	{
		attachObject(objects[i]);
	}
}

BOOL LLGLSLShader::mapAttributes(const vector<string> * attributes)
{
	//link the program
	BOOL res = link();

	mAttribute.clear();
	U32 numAttributes = (attributes == NULL) ? 0 : attributes->size();
	mAttribute.resize(LLShaderMgr::instance()->mReservedAttribs.size() + numAttributes, -1);
	
	if (res)
	{ //read back channel locations

		//read back reserved channels first
		for (U32 i = 0; i < (S32) LLShaderMgr::instance()->mReservedAttribs.size(); i++)
		{
			const char* name = LLShaderMgr::instance()->mReservedAttribs[i].c_str();
			S32 index = glGetAttribLocationARB(mProgramObject, (const GLcharARB *)name);
			if (index != -1)
			{
				mAttribute[i] = index;
				LL_DEBUGS("ShaderLoading") << "Attribute " << name << " assigned to channel " << index << LL_ENDL;
			}
		}
		if (attributes != NULL)
		{
			for (U32 i = 0; i < numAttributes; i++)
			{
				const char* name = (*attributes)[i].c_str();
				S32 index = glGetAttribLocationARB(mProgramObject, name);
				if (index != -1)
				{
					mAttribute[LLShaderMgr::instance()->mReservedAttribs.size() + i] = index;
					LL_DEBUGS("ShaderLoading") << "Attribute " << name << " assigned to channel " << index << LL_ENDL;
				}
			}
		}

		return TRUE;
	}
	
	return FALSE;
}

void LLGLSLShader::mapUniform(GLint index, const vector<string> * uniforms)
{
	if (index == -1)
	{
		return;
	}

	GLenum type;
	GLsizei length;
	GLint size;
	char name[1024];		/* Flawfinder: ignore */
	name[0] = 0;

	glGetActiveUniformARB(mProgramObject, index, 1024, &length, &size, &type, (GLcharARB *)name);
	S32 location = glGetUniformLocationARB(mProgramObject, name);
	if (location != -1)
	{
		mUniformMap[name] = location;
		LL_DEBUGS("ShaderLoading") << "Uniform " << name << " is at location " << location << LL_ENDL;
	
		//find the index of this uniform
		for (S32 i = 0; i < (S32) LLShaderMgr::instance()->mReservedUniforms.size(); i++)
		{
			if ( (mUniform[i] == -1)
				&& (LLShaderMgr::instance()->mReservedUniforms[i].compare(0, length, name, LLShaderMgr::instance()->mReservedUniforms[i].length()) == 0))
			{
				//found it
				mUniform[i] = location;
				mTexture[i] = mapUniformTextureChannel(location, type);
				return;
			}
		}

		if (uniforms != NULL)
		{
			for (U32 i = 0; i < uniforms->size(); i++)
			{
				if ( (mUniform[i+LLShaderMgr::instance()->mReservedUniforms.size()] == -1)
					&& ((*uniforms)[i].compare(0, length, name, (*uniforms)[i].length()) == 0))
				{
					//found it
					mUniform[i+LLShaderMgr::instance()->mReservedUniforms.size()] = location;
					mTexture[i+LLShaderMgr::instance()->mReservedUniforms.size()] = mapUniformTextureChannel(location, type);
					return;
				}
			}
		}
	}
 }

GLint LLGLSLShader::mapUniformTextureChannel(GLint location, GLenum type)
{
	if (type >= GL_SAMPLER_1D_ARB && type <= GL_SAMPLER_2D_RECT_SHADOW_ARB)
	{	//this here is a texture
		glUniform1iARB(location, mActiveTextureChannels);
		LL_DEBUGS("ShaderLoading") << "Assigned to texture channel " << mActiveTextureChannels << LL_ENDL;
		return mActiveTextureChannels++;
	}
	return -1;
}

BOOL LLGLSLShader::mapUniforms(const vector<string> * uniforms)
{
	BOOL res = TRUE;
	
	mActiveTextureChannels = 0;
	mUniform.clear();
	mUniformMap.clear();
	mTexture.clear();
	mValue.clear();
	//initialize arrays
	U32 numUniforms = (uniforms == NULL) ? 0 : uniforms->size();
	mUniform.resize(numUniforms + LLShaderMgr::instance()->mReservedUniforms.size(), -1);
	mTexture.resize(numUniforms + LLShaderMgr::instance()->mReservedUniforms.size(), -1);
	
	bind();

	//get the number of active uniforms
	GLint activeCount;
	glGetObjectParameterivARB(mProgramObject, GL_OBJECT_ACTIVE_UNIFORMS_ARB, &activeCount);

	for (S32 i = 0; i < activeCount; i++)
	{
		mapUniform(i, uniforms);
	}

	unbind();

	return res;
}

BOOL LLGLSLShader::link(BOOL suppress_errors)
{
	return LLShaderMgr::instance()->linkProgramObject(mProgramObject, suppress_errors);
}

void LLGLSLShader::bind()
{
	if (gGLManager.mHasShaderObjects)
	{
		glUseProgramObjectARB(mProgramObject);

		if (mUniformsDirty)
		{
			LLShaderMgr::instance()->updateShaderUniforms(this);
			mUniformsDirty = FALSE;
		}
	}
}

void LLGLSLShader::unbind()
{
	if (gGLManager.mHasShaderObjects)
	{
		for (U32 i = 0; i < mAttribute.size(); ++i)
		{
			vertexAttrib4f(i, 0,0,0,1);
		}
		glUseProgramObjectARB(0);
	}
}

void LLGLSLShader::bindNoShader(void)
{
	glUseProgramObjectARB(0);
}

S32 LLGLSLShader::enableTexture(S32 uniform, LLTexUnit::eTextureType mode)
{
	if (uniform < 0 || uniform >= (S32)mTexture.size())
	{
		UNIFORM_ERRS << "Uniform out of range: " << uniform << LL_ENDL;
		return -1;
	}
	S32 index = mTexture[uniform];
	if (index != -1)
	{
		gGL.getTexUnit(index)->activate();
		gGL.getTexUnit(index)->enable(mode);
	}
	return index;
}

S32 LLGLSLShader::disableTexture(S32 uniform, LLTexUnit::eTextureType mode)
{
	if (uniform < 0 || uniform >= (S32)mTexture.size())
	{
		UNIFORM_ERRS << "Uniform out of range: " << uniform << LL_ENDL;
		return -1;
	}
	S32 index = mTexture[uniform];
	if (index != -1)
	{
		gGL.getTexUnit(index)->activate();
		gGL.getTexUnit(index)->disable();
	}
	return index;
}

void LLGLSLShader::uniform1f(U32 index, GLfloat x)
{
	if (mProgramObject > 0)
	{	
		if (mUniform.size() <= index)
		{
			UNIFORM_ERRS << "Uniform index out of bounds." << LL_ENDL;
			return;
		}

		if (mUniform[index] >= 0)
		{
			std::map<GLint, LLVector4>::iterator iter = mValue.find(mUniform[index]);
			if (iter == mValue.end() || iter->second.mV[0] != x)
			{
				glUniform1fARB(mUniform[index], x);
				mValue[mUniform[index]] = LLVector4(x,0.f,0.f,0.f);
			}
		}
	}
}

void LLGLSLShader::uniform2f(U32 index, GLfloat x, GLfloat y)
{
	if (mProgramObject > 0)
	{	
		if (mUniform.size() <= index)
		{
			UNIFORM_ERRS << "Uniform index out of bounds." << LL_ENDL;
			return;
		}

		if (mUniform[index] >= 0)
		{
			std::map<GLint, LLVector4>::iterator iter = mValue.find(mUniform[index]);
			LLVector4 vec(x,y,0.f,0.f);
			if (iter == mValue.end() || shouldChange(iter->second,vec))
			{
				glUniform2fARB(mUniform[index], x, y);
				mValue[mUniform[index]] = vec;
			}
		}
	}
}

void LLGLSLShader::uniform3f(U32 index, GLfloat x, GLfloat y, GLfloat z)
{
	if (mProgramObject > 0)
	{	
		if (mUniform.size() <= index)
		{
			UNIFORM_ERRS << "Uniform index out of bounds." << LL_ENDL;
			return;
		}

		if (mUniform[index] >= 0)
		{
			std::map<GLint, LLVector4>::iterator iter = mValue.find(mUniform[index]);
			LLVector4 vec(x,y,z,0.f);
			if (iter == mValue.end() || shouldChange(iter->second,vec))
			{
				glUniform3fARB(mUniform[index], x, y, z);
				mValue[mUniform[index]] = vec;
			}
		}
	}
}

void LLGLSLShader::uniform4f(U32 index, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	if (mProgramObject > 0)
	{	
		if (mUniform.size() <= index)
		{
			UNIFORM_ERRS << "Uniform index out of bounds." << LL_ENDL;
			return;
		}

		if (mUniform[index] >= 0)
		{
			std::map<GLint, LLVector4>::iterator iter = mValue.find(mUniform[index]);
			LLVector4 vec(x,y,z,w);
			if (iter == mValue.end() || shouldChange(iter->second,vec))
			{
				glUniform4fARB(mUniform[index], x, y, z, w);
				mValue[mUniform[index]] = vec;
			}
		}
	}
}

void LLGLSLShader::uniform1fv(U32 index, U32 count, const GLfloat* v)
{
	if (mProgramObject > 0)
	{	
		if (mUniform.size() <= index)
		{
			UNIFORM_ERRS << "Uniform index out of bounds." << LL_ENDL;
			return;
		}

		if (mUniform[index] >= 0)
		{
			std::map<GLint, LLVector4>::iterator iter = mValue.find(mUniform[index]);
			LLVector4 vec(v[0],0.f,0.f,0.f);
			if (iter == mValue.end() || shouldChange(iter->second,vec) || count != 1)
			{
				glUniform1fvARB(mUniform[index], count, v);
				mValue[mUniform[index]] = vec;
			}
		}
	}
}

void LLGLSLShader::uniform2fv(U32 index, U32 count, const GLfloat* v)
{
	if (mProgramObject > 0)
	{	
		if (mUniform.size() <= index)
		{
			UNIFORM_ERRS << "Uniform index out of bounds." << LL_ENDL;
			return;
		}

		if (mUniform[index] >= 0)
		{
			std::map<GLint, LLVector4>::iterator iter = mValue.find(mUniform[index]);
			LLVector4 vec(v[0],v[1],0.f,0.f);
			if (iter == mValue.end() || shouldChange(iter->second,vec) || count != 1)
			{
				glUniform2fvARB(mUniform[index], count, v);
				mValue[mUniform[index]] = vec;
			}
		}
	}
}

void LLGLSLShader::uniform3fv(U32 index, U32 count, const GLfloat* v)
{
	if (mProgramObject > 0)
	{	
		if (mUniform.size() <= index)
		{
			UNIFORM_ERRS << "Uniform index out of bounds." << LL_ENDL;
			return;
		}

		if (mUniform[index] >= 0)
		{
			std::map<GLint, LLVector4>::iterator iter = mValue.find(mUniform[index]);
			LLVector4 vec(v[0],v[1],v[2],0.f);
			if (iter == mValue.end() || shouldChange(iter->second,vec) || count != 1)
			{
				glUniform3fvARB(mUniform[index], count, v);
				mValue[mUniform[index]] = vec;
			}
		}
	}
}

void LLGLSLShader::uniform4fv(U32 index, U32 count, const GLfloat* v)
{
	if (mProgramObject > 0)
	{	
		if (mUniform.size() <= index)
		{
			UNIFORM_ERRS << "Uniform index out of bounds." << LL_ENDL;
			return;
		}

		if (mUniform[index] >= 0)
		{
			std::map<GLint, LLVector4>::iterator iter = mValue.find(mUniform[index]);
			LLVector4 vec(v[0],v[1],v[2],v[3]);
			if (iter == mValue.end() || shouldChange(iter->second,vec) || count != 1)
			{
				glUniform4fvARB(mUniform[index], count, v);
				mValue[mUniform[index]] = vec;
			}
		}
	}
}

void LLGLSLShader::uniformMatrix2fv(U32 index, U32 count, GLboolean transpose, const GLfloat *v)
{
	if (mProgramObject > 0)
	{	
		if (mUniform.size() <= index)
		{
			UNIFORM_ERRS << "Uniform index out of bounds." << LL_ENDL;
			return;
		}

		if (mUniform[index] >= 0)
		{
			glUniformMatrix2fvARB(mUniform[index], count, transpose, v);
		}
	}
}

void LLGLSLShader::uniformMatrix3fv(U32 index, U32 count, GLboolean transpose, const GLfloat *v)
{
	if (mProgramObject > 0)
	{	
		if (mUniform.size() <= index)
		{
			UNIFORM_ERRS << "Uniform index out of bounds." << LL_ENDL;
			return;
		}

		if (mUniform[index] >= 0)
		{
			glUniformMatrix3fvARB(mUniform[index], count, transpose, v);
		}
	}
}

void LLGLSLShader::uniformMatrix4fv(U32 index, U32 count, GLboolean transpose, const GLfloat *v)
{
	if (mProgramObject > 0)
	{	
		if (mUniform.size() <= index)
		{
			UNIFORM_ERRS << "Uniform index out of bounds." << LL_ENDL;
			return;
		}

		if (mUniform[index] >= 0)
		{
			glUniformMatrix4fvARB(mUniform[index], count, transpose, v);
		}
	}
}

GLint LLGLSLShader::getUniformLocation(const string& uniform)
{
	if (mProgramObject > 0)
	{
		std::map<string, GLint>::iterator iter = mUniformMap.find(uniform);
		if (iter != mUniformMap.end())
		{
			llassert(iter->second == glGetUniformLocationARB(mProgramObject, uniform.c_str()));
			return iter->second;
		}
	}

	return -1;
}

void LLGLSLShader::uniform1f(const string& uniform, GLfloat v)
{
	GLint location = getUniformLocation(uniform);
				
	if (location >= 0)
	{
		std::map<GLint, LLVector4>::iterator iter = mValue.find(location);
		LLVector4 vec(v,0.f,0.f,0.f);
		if (iter == mValue.end() || shouldChange(iter->second,vec))
		{
			glUniform1fARB(location, v);
			mValue[location] = vec;
		}
	}
}

void LLGLSLShader::uniform2f(const string& uniform, GLfloat x, GLfloat y)
{
	GLint location = getUniformLocation(uniform);
				
	if (location >= 0)
	{
		std::map<GLint, LLVector4>::iterator iter = mValue.find(location);
		LLVector4 vec(x,y,0.f,0.f);
		if (iter == mValue.end() || shouldChange(iter->second,vec))
		{
			glUniform2fARB(location, x,y);
			mValue[location] = vec;
		}
	}

}

void LLGLSLShader::uniform3f(const string& uniform, GLfloat x, GLfloat y, GLfloat z)
{
	GLint location = getUniformLocation(uniform);
				
	if (location >= 0)
	{
		std::map<GLint, LLVector4>::iterator iter = mValue.find(location);
		LLVector4 vec(x,y,z,0.f);
		if (iter == mValue.end() || shouldChange(iter->second,vec))
		{
			glUniform3fARB(location, x,y,z);
			mValue[location] = vec;
		}
	}
}

void LLGLSLShader::uniform4f(const string& uniform, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	GLint location = getUniformLocation(uniform);

	if (location >= 0)
	{
		std::map<GLint, LLVector4>::iterator iter = mValue.find(location);
		LLVector4 vec(x,y,z,w);
		if (iter == mValue.end() || shouldChange(iter->second,vec))
		{
			glUniform4fARB(location, x,y,z,w);
			mValue[location] = vec;
		}
	}
}

void LLGLSLShader::uniform1fv(const string& uniform, U32 count, const GLfloat* v)
{
	GLint location = getUniformLocation(uniform);

	if (location >= 0)
	{
		std::map<GLint, LLVector4>::iterator iter = mValue.find(location);
		LLVector4 vec(v[0],0.f,0.f,0.f);
		if (iter == mValue.end() || shouldChange(iter->second,vec) || count != 1)
		{
			glUniform1fvARB(location, count, v);
			mValue[location] = vec;
		}
	}
}

void LLGLSLShader::uniform2fv(const string& uniform, U32 count, const GLfloat* v)
{
	GLint location = getUniformLocation(uniform);
				
	if (location >= 0)
	{
		std::map<GLint, LLVector4>::iterator iter = mValue.find(location);
		LLVector4 vec(v[0],v[1],0.f,0.f);
		if (iter == mValue.end() || shouldChange(iter->second,vec) || count != 1)
		{
			glUniform2fvARB(location, count, v);
			mValue[location] = vec;
		}
	}
}

void LLGLSLShader::uniform3fv(const string& uniform, U32 count, const GLfloat* v)
{
	GLint location = getUniformLocation(uniform);
				
	if (location >= 0)
	{
		std::map<GLint, LLVector4>::iterator iter = mValue.find(location);
		LLVector4 vec(v[0],v[1],v[2],0.f);
		if (iter == mValue.end() || shouldChange(iter->second,vec) || count != 1)
		{
			glUniform3fvARB(location, count, v);
			mValue[location] = vec;
		}
	}
}

void LLGLSLShader::uniform4fv(const string& uniform, U32 count, const GLfloat* v)
{
	GLint location = getUniformLocation(uniform);

	if (location >= 0)
	{
		LLVector4 vec(v);
		std::map<GLint, LLVector4>::iterator iter = mValue.find(location);
		if (iter == mValue.end() || shouldChange(iter->second,vec) || count != 1)
		{
			glUniform4fvARB(location, count, v);
			mValue[location] = vec;
		}
	}
}

void LLGLSLShader::uniformMatrix2fv(const string& uniform, U32 count, GLboolean transpose, const GLfloat* v)
{
	GLint location = getUniformLocation(uniform);
				
	if (location >= 0)
	{
		glUniformMatrix2fvARB(location, count, transpose, v);
	}
}

void LLGLSLShader::uniformMatrix3fv(const string& uniform, U32 count, GLboolean transpose, const GLfloat* v)
{
	GLint location = getUniformLocation(uniform);
				
	if (location >= 0)
	{
		glUniformMatrix3fvARB(location, count, transpose, v);
	}
}

void LLGLSLShader::uniformMatrix4fv(const string& uniform, U32 count, GLboolean transpose, const GLfloat* v)
{
	GLint location = getUniformLocation(uniform);
				
	if (location >= 0)
	{
		glUniformMatrix4fvARB(location, count, transpose, v);
	}
}


void LLGLSLShader::vertexAttrib4f(U32 index, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	if (mAttribute[index] > 0)
	{
		glVertexAttrib4fARB(mAttribute[index], x, y, z, w);
	}
}

void LLGLSLShader::vertexAttrib4fv(U32 index, GLfloat* v)
{
	if (mAttribute[index] > 0)
	{
		glVertexAttrib4fvARB(mAttribute[index], v);
	}
}
