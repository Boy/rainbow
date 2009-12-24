/** 
 * @file llglheaders.h
 * @brief LLGL definitions
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

#ifndef LL_LLGLHEADERS_H
#define LL_LLGLHEADERS_H

#if LL_MESA
//----------------------------------------------------------------------------
// MESA headers
// quotes so we get libraries/.../GL/ version
#define GL_GLEXT_PROTOTYPES
#include "GL/gl.h"
#include "GL/glext.h"
#include "GL/glu.h"

// The __APPLE__ kludge is to make glh_extensions.h not symbol-clash horribly
# define __APPLE__
# include "GL/glh_extensions.h"
# undef __APPLE__

#elif LL_LINUX
//----------------------------------------------------------------------------
// Linux, MESA headers, but not necessarily assuming MESA runtime.
// quotes so we get libraries/.../GL/ version
#include "GL/gl.h"
#include "GL/glext.h"
#include "GL/glu.h"


#if LL_LINUX && !LL_MESA_HEADLESS
// The __APPLE__ kludge is to make glh_extensions.h not symbol-clash horribly
# define __APPLE__
# include "GL/glh_extensions.h"
# undef __APPLE__

/* Although SDL very likely ends up calling glXGetProcAddress() itself,
   if we use SDL_GL_GetProcAddress() then we get bogus addresses back on
   some systems.  Weird. */
/*# include "SDL/SDL.h"
  # define GLH_EXT_GET_PROC_ADDRESS(p) SDL_GL_GetProcAddress(p) */
#define GLX_GLXEXT_PROTOTYPES 1
# include "GL/glx.h"
# include "GL/glxext.h"
// Use glXGetProcAddressARB instead of glXGetProcAddress - the ARB symbol
// is considered 'legacy' but works on more machines.
# define GLH_EXT_GET_PROC_ADDRESS(p) glXGetProcAddressARB((const GLubyte*)(p))
// Whee, the X headers define 'Status'.  Undefine to avoid confusion.
#undef Status
#endif // LL_LINUX && !LL_MESA_HEADLESS


// GL_ARB_vertex_buffer_object
extern PFNGLBINDBUFFERARBPROC		glBindBufferARB;
extern PFNGLDELETEBUFFERSARBPROC	glDeleteBuffersARB;
extern PFNGLGENBUFFERSARBPROC		glGenBuffersARB;
extern PFNGLISBUFFERARBPROC			glIsBufferARB;
extern PFNGLBUFFERDATAARBPROC		glBufferDataARB;
extern PFNGLBUFFERSUBDATAARBPROC	glBufferSubDataARB;
extern PFNGLGETBUFFERSUBDATAARBPROC glGetBufferSubDataARB;
extern PFNGLMAPBUFFERARBPROC		glMapBufferARB;
extern PFNGLUNMAPBUFFERARBPROC		glUnmapBufferARB;
extern PFNGLGETBUFFERPARAMETERIVARBPROC	glGetBufferParameterivARB;
extern PFNGLGETBUFFERPOINTERVARBPROC	glGetBufferPointervARB;

// GL_ATI_vertex_array_object
extern PFNGLNEWOBJECTBUFFERATIPROC			glNewObjectBufferATI;
extern PFNGLISOBJECTBUFFERATIPROC			glIsObjectBufferATI;
extern PFNGLUPDATEOBJECTBUFFERATIPROC		glUpdateObjectBufferATI;
extern PFNGLGETOBJECTBUFFERFVATIPROC		glGetObjectBufferfvATI;
extern PFNGLGETOBJECTBUFFERIVATIPROC		glGetObjectBufferivATI;
extern PFNGLFREEOBJECTBUFFERATIPROC		    glFreeObjectBufferATI;
extern PFNGLARRAYOBJECTATIPROC				glArrayObjectATI;
extern PFNGLVERTEXATTRIBARRAYOBJECTATIPROC	glVertexAttribArrayObjectATI;
extern PFNGLGETARRAYOBJECTFVATIPROC			glGetArrayObjectfvATI;
extern PFNGLGETARRAYOBJECTIVATIPROC			glGetArrayObjectivATI;
extern PFNGLVARIANTARRAYOBJECTATIPROC		glVariantObjectArrayATI;
extern PFNGLGETVARIANTARRAYOBJECTFVATIPROC	glGetVariantArrayObjectfvATI;
extern PFNGLGETVARIANTARRAYOBJECTIVATIPROC	glGetVariantArrayObjectivATI;

// GL_ARB_occlusion_query
extern PFNGLGENQUERIESARBPROC glGenQueriesARB;
extern PFNGLDELETEQUERIESARBPROC glDeleteQueriesARB;
extern PFNGLISQUERYARBPROC glIsQueryARB;
extern PFNGLBEGINQUERYARBPROC glBeginQueryARB;
extern PFNGLENDQUERYARBPROC glEndQueryARB;
extern PFNGLGETQUERYIVARBPROC glGetQueryivARB;
extern PFNGLGETQUERYOBJECTIVARBPROC glGetQueryObjectivARB;
extern PFNGLGETQUERYOBJECTUIVARBPROC glGetQueryObjectuivARB;

// GL_ARB_point_parameters
extern PFNGLPOINTPARAMETERFARBPROC glPointParameterfARB;
extern PFNGLPOINTPARAMETERFVARBPROC glPointParameterfvARB;

// GL_ARB_shader_objects
extern PFNGLDELETEOBJECTARBPROC glDeleteObjectARB;
extern PFNGLGETHANDLEARBPROC glGetHandleARB;
extern PFNGLDETACHOBJECTARBPROC glDetachObjectARB;
extern PFNGLCREATESHADEROBJECTARBPROC glCreateShaderObjectARB;
extern PFNGLSHADERSOURCEARBPROC glShaderSourceARB;
extern PFNGLCOMPILESHADERARBPROC glCompileShaderARB;
extern PFNGLCREATEPROGRAMOBJECTARBPROC glCreateProgramObjectARB;
extern PFNGLATTACHOBJECTARBPROC glAttachObjectARB;
extern PFNGLLINKPROGRAMARBPROC glLinkProgramARB;
extern PFNGLUSEPROGRAMOBJECTARBPROC glUseProgramObjectARB;
extern PFNGLVALIDATEPROGRAMARBPROC glValidateProgramARB;
extern PFNGLUNIFORM1FARBPROC glUniform1fARB;
extern PFNGLUNIFORM2FARBPROC glUniform2fARB;
extern PFNGLUNIFORM3FARBPROC glUniform3fARB;
extern PFNGLUNIFORM4FARBPROC glUniform4fARB;
extern PFNGLUNIFORM1IARBPROC glUniform1iARB;
extern PFNGLUNIFORM2IARBPROC glUniform2iARB;
extern PFNGLUNIFORM3IARBPROC glUniform3iARB;
extern PFNGLUNIFORM4IARBPROC glUniform4iARB;
extern PFNGLUNIFORM1FVARBPROC glUniform1fvARB;
extern PFNGLUNIFORM2FVARBPROC glUniform2fvARB;
extern PFNGLUNIFORM3FVARBPROC glUniform3fvARB;
extern PFNGLUNIFORM4FVARBPROC glUniform4fvARB;
extern PFNGLUNIFORM1IVARBPROC glUniform1ivARB;
extern PFNGLUNIFORM2IVARBPROC glUniform2ivARB;
extern PFNGLUNIFORM3IVARBPROC glUniform3ivARB;
extern PFNGLUNIFORM4IVARBPROC glUniform4ivARB;
extern PFNGLUNIFORMMATRIX2FVARBPROC glUniformMatrix2fvARB;
extern PFNGLUNIFORMMATRIX3FVARBPROC glUniformMatrix3fvARB;
extern PFNGLUNIFORMMATRIX4FVARBPROC glUniformMatrix4fvARB;
extern PFNGLGETOBJECTPARAMETERFVARBPROC glGetObjectParameterfvARB;
extern PFNGLGETOBJECTPARAMETERIVARBPROC glGetObjectParameterivARB;
extern PFNGLGETINFOLOGARBPROC glGetInfoLogARB;
extern PFNGLGETATTACHEDOBJECTSARBPROC glGetAttachedObjectsARB;
extern PFNGLGETUNIFORMLOCATIONARBPROC glGetUniformLocationARB;
extern PFNGLGETACTIVEUNIFORMARBPROC glGetActiveUniformARB;
extern PFNGLGETUNIFORMFVARBPROC glGetUniformfvARB;
extern PFNGLGETUNIFORMIVARBPROC glGetUniformivARB;
extern PFNGLGETSHADERSOURCEARBPROC glGetShaderSourceARB;

// GL_ARB_vertex_shader
extern PFNGLVERTEXATTRIB1DARBPROC glVertexAttrib1dARB;
extern PFNGLVERTEXATTRIB1DVARBPROC glVertexAttrib1dvARB;
extern PFNGLVERTEXATTRIB1FARBPROC glVertexAttrib1fARB;
extern PFNGLVERTEXATTRIB1FVARBPROC glVertexAttrib1fvARB;
extern PFNGLVERTEXATTRIB1SARBPROC glVertexAttrib1sARB;
extern PFNGLVERTEXATTRIB1SVARBPROC glVertexAttrib1svARB;
extern PFNGLVERTEXATTRIB2DARBPROC glVertexAttrib2dARB;
extern PFNGLVERTEXATTRIB2DVARBPROC glVertexAttrib2dvARB;
extern PFNGLVERTEXATTRIB2FARBPROC glVertexAttrib2fARB;
extern PFNGLVERTEXATTRIB2FVARBPROC glVertexAttrib2fvARB;
extern PFNGLVERTEXATTRIB2SARBPROC glVertexAttrib2sARB;
extern PFNGLVERTEXATTRIB2SVARBPROC glVertexAttrib2svARB;
extern PFNGLVERTEXATTRIB3DARBPROC glVertexAttrib3dARB;
extern PFNGLVERTEXATTRIB3DVARBPROC glVertexAttrib3dvARB;
extern PFNGLVERTEXATTRIB3FARBPROC glVertexAttrib3fARB;
extern PFNGLVERTEXATTRIB3FVARBPROC glVertexAttrib3fvARB;
extern PFNGLVERTEXATTRIB3SARBPROC glVertexAttrib3sARB;
extern PFNGLVERTEXATTRIB3SVARBPROC glVertexAttrib3svARB;
extern PFNGLVERTEXATTRIB4NBVARBPROC glVertexAttrib4nbvARB;
extern PFNGLVERTEXATTRIB4NIVARBPROC glVertexAttrib4nivARB;
extern PFNGLVERTEXATTRIB4NSVARBPROC glVertexAttrib4nsvARB;
extern PFNGLVERTEXATTRIB4NUBARBPROC glVertexAttrib4nubARB;
extern PFNGLVERTEXATTRIB4NUBVARBPROC glVertexAttrib4nubvARB;
extern PFNGLVERTEXATTRIB4NUIVARBPROC glVertexAttrib4nuivARB;
extern PFNGLVERTEXATTRIB4NUSVARBPROC glVertexAttrib4nusvARB;
extern PFNGLVERTEXATTRIB4BVARBPROC glVertexAttrib4bvARB;
extern PFNGLVERTEXATTRIB4DARBPROC glVertexAttrib4dARB;
extern PFNGLVERTEXATTRIB4DVARBPROC glVertexAttrib4dvARB;
extern PFNGLVERTEXATTRIB4FARBPROC glVertexAttrib4fARB;
extern PFNGLVERTEXATTRIB4FVARBPROC glVertexAttrib4fvARB;
extern PFNGLVERTEXATTRIB4IVARBPROC glVertexAttrib4ivARB;
extern PFNGLVERTEXATTRIB4SARBPROC glVertexAttrib4sARB;
extern PFNGLVERTEXATTRIB4SVARBPROC glVertexAttrib4svARB;
extern PFNGLVERTEXATTRIB4UBVARBPROC glVertexAttrib4ubvARB;
extern PFNGLVERTEXATTRIB4UIVARBPROC glVertexAttrib4uivARB;
extern PFNGLVERTEXATTRIB4USVARBPROC glVertexAttrib4usvARB;
extern PFNGLVERTEXATTRIBPOINTERARBPROC glVertexAttribPointerARB;
extern PFNGLENABLEVERTEXATTRIBARRAYARBPROC glEnableVertexAttribArrayARB;
extern PFNGLDISABLEVERTEXATTRIBARRAYARBPROC glDisableVertexAttribArrayARB;
extern PFNGLPROGRAMSTRINGARBPROC glProgramStringARB;
extern PFNGLBINDPROGRAMARBPROC glBindProgramARB;
extern PFNGLDELETEPROGRAMSARBPROC glDeleteProgramsARB;
extern PFNGLGENPROGRAMSARBPROC glGenProgramsARB;
extern PFNGLPROGRAMENVPARAMETER4DARBPROC glProgramEnvParameter4dARB;
extern PFNGLPROGRAMENVPARAMETER4DVARBPROC glProgramEnvParameter4dvARB;
extern PFNGLPROGRAMENVPARAMETER4FARBPROC glProgramEnvParameter4fARB;
extern PFNGLPROGRAMENVPARAMETER4FVARBPROC glProgramEnvParameter4fvARB;
extern PFNGLPROGRAMLOCALPARAMETER4DARBPROC glProgramLocalParameter4dARB;
extern PFNGLPROGRAMLOCALPARAMETER4DVARBPROC glProgramLocalParameter4dvARB;
extern PFNGLPROGRAMLOCALPARAMETER4FARBPROC glProgramLocalParameter4fARB;
extern PFNGLPROGRAMLOCALPARAMETER4FVARBPROC glProgramLocalParameter4fvARB;
extern PFNGLGETPROGRAMENVPARAMETERDVARBPROC glGetProgramEnvParameterdvARB;
extern PFNGLGETPROGRAMENVPARAMETERFVARBPROC glGetProgramEnvParameterfvARB;
extern PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC glGetProgramLocalParameterdvARB;
extern PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC glGetProgramLocalParameterfvARB;
extern PFNGLGETPROGRAMIVARBPROC glGetProgramivARB;
extern PFNGLGETPROGRAMSTRINGARBPROC glGetProgramStringARB;
extern PFNGLGETVERTEXATTRIBDVARBPROC glGetVertexAttribdvARB;
extern PFNGLGETVERTEXATTRIBFVARBPROC glGetVertexAttribfvARB;
extern PFNGLGETVERTEXATTRIBIVARBPROC glGetVertexAttribivARB;
extern PFNGLGETVERTEXATTRIBPOINTERVARBPROC glGetVertexAttribPointervARB;
extern PFNGLISPROGRAMARBPROC glIsProgramARB;
extern PFNGLBINDATTRIBLOCATIONARBPROC glBindAttribLocationARB;
extern PFNGLGETACTIVEATTRIBARBPROC glGetActiveAttribARB;
extern PFNGLGETATTRIBLOCATIONARBPROC glGetAttribLocationARB;

extern PFNGLCOMPRESSEDTEXIMAGE2DARBPROC glCompressedTexImage2DARB;
extern PFNGLGETCOMPRESSEDTEXIMAGEARBPROC glGetCompressedTexImageARB;

extern PFNGLCOLORTABLEEXTPROC glColorTableEXT;

//GL_EXT_framebuffer_object
extern PFNGLISRENDERBUFFEREXTPROC glIsRenderbufferEXT;
extern PFNGLBINDRENDERBUFFEREXTPROC glBindRenderbufferEXT;
extern PFNGLDELETERENDERBUFFERSEXTPROC glDeleteRenderbuffersEXT;
extern PFNGLGENRENDERBUFFERSEXTPROC glGenRenderbuffersEXT;
extern PFNGLRENDERBUFFERSTORAGEEXTPROC glRenderbufferStorageEXT;
extern PFNGLGETRENDERBUFFERPARAMETERIVEXTPROC glGetRenderbufferParameterivEXT;
extern PFNGLISFRAMEBUFFEREXTPROC glIsFramebufferEXT;
extern PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebufferEXT;
extern PFNGLDELETEFRAMEBUFFERSEXTPROC glDeleteFramebuffersEXT;
extern PFNGLGENFRAMEBUFFERSEXTPROC glGenFramebuffersEXT;
extern PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC glCheckFramebufferStatusEXT;
extern PFNGLFRAMEBUFFERTEXTURE1DEXTPROC glFramebufferTexture1DEXT;
extern PFNGLFRAMEBUFFERTEXTURE2DEXTPROC glFramebufferTexture2DEXT;
extern PFNGLFRAMEBUFFERTEXTURE3DEXTPROC glFramebufferTexture3DEXT;
extern PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glFramebufferRenderbufferEXT;
extern PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC glGetFramebufferAttachmentParameterivEXT;
extern PFNGLGENERATEMIPMAPEXTPROC glGenerateMipmapEXT;


#elif LL_WINDOWS

// windows gl headers depend on things like APIENTRY, so include windows.
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>

//----------------------------------------------------------------------------
#include <GL/gl.h>
#include <GL/glu.h>

// quotes so we get libraries/.../GL/ version
#include "GL/glext.h"
#include "GL/glh_extensions.h"


// GL_ARB_vertex_buffer_object
extern PFNGLBINDBUFFERARBPROC		glBindBufferARB;
extern PFNGLDELETEBUFFERSARBPROC	glDeleteBuffersARB;
extern PFNGLGENBUFFERSARBPROC		glGenBuffersARB;
extern PFNGLISBUFFERARBPROC			glIsBufferARB;
extern PFNGLBUFFERDATAARBPROC		glBufferDataARB;
extern PFNGLBUFFERSUBDATAARBPROC	glBufferSubDataARB;
extern PFNGLGETBUFFERSUBDATAARBPROC glGetBufferSubDataARB;
extern PFNGLMAPBUFFERARBPROC		glMapBufferARB;
extern PFNGLUNMAPBUFFERARBPROC		glUnmapBufferARB;
extern PFNGLGETBUFFERPARAMETERIVARBPROC	glGetBufferParameterivARB;
extern PFNGLGETBUFFERPOINTERVARBPROC	glGetBufferPointervARB;

// GL_ATI_vertex_array_object
extern PFNGLNEWOBJECTBUFFERATIPROC			glNewObjectBufferATI;
extern PFNGLISOBJECTBUFFERATIPROC			glIsObjectBufferATI;
extern PFNGLUPDATEOBJECTBUFFERATIPROC		glUpdateObjectBufferATI;
extern PFNGLGETOBJECTBUFFERFVATIPROC		glGetObjectBufferfvATI;
extern PFNGLGETOBJECTBUFFERIVATIPROC		glGetObjectBufferivATI;
extern PFNGLFREEOBJECTBUFFERATIPROC		    glFreeObjectBufferATI;
extern PFNGLARRAYOBJECTATIPROC				glArrayObjectATI;
extern PFNGLVERTEXATTRIBARRAYOBJECTATIPROC	glVertexAttribArrayObjectATI;
extern PFNGLGETARRAYOBJECTFVATIPROC			glGetArrayObjectfvATI;
extern PFNGLGETARRAYOBJECTIVATIPROC			glGetArrayObjectivATI;
extern PFNGLVARIANTARRAYOBJECTATIPROC		glVariantObjectArrayATI;
extern PFNGLGETVARIANTARRAYOBJECTFVATIPROC	glGetVariantArrayObjectfvATI;
extern PFNGLGETVARIANTARRAYOBJECTIVATIPROC	glGetVariantArrayObjectivATI;

extern PFNWGLSWAPINTERVALEXTPROC			wglSwapIntervalEXT;

// GL_ARB_occlusion_query
extern PFNGLGENQUERIESARBPROC glGenQueriesARB;
extern PFNGLDELETEQUERIESARBPROC glDeleteQueriesARB;
extern PFNGLISQUERYARBPROC glIsQueryARB;
extern PFNGLBEGINQUERYARBPROC glBeginQueryARB;
extern PFNGLENDQUERYARBPROC glEndQueryARB;
extern PFNGLGETQUERYIVARBPROC glGetQueryivARB;
extern PFNGLGETQUERYOBJECTIVARBPROC glGetQueryObjectivARB;
extern PFNGLGETQUERYOBJECTUIVARBPROC glGetQueryObjectuivARB;

// GL_ARB_point_parameters
extern PFNGLPOINTPARAMETERFARBPROC glPointParameterfARB;
extern PFNGLPOINTPARAMETERFVARBPROC glPointParameterfvARB;

// GL_ARB_shader_objects
extern PFNGLDELETEOBJECTARBPROC glDeleteObjectARB;
extern PFNGLGETHANDLEARBPROC glGetHandleARB;
extern PFNGLDETACHOBJECTARBPROC glDetachObjectARB;
extern PFNGLCREATESHADEROBJECTARBPROC glCreateShaderObjectARB;
extern PFNGLSHADERSOURCEARBPROC glShaderSourceARB;
extern PFNGLCOMPILESHADERARBPROC glCompileShaderARB;
extern PFNGLCREATEPROGRAMOBJECTARBPROC glCreateProgramObjectARB;
extern PFNGLATTACHOBJECTARBPROC glAttachObjectARB;
extern PFNGLLINKPROGRAMARBPROC glLinkProgramARB;
extern PFNGLUSEPROGRAMOBJECTARBPROC glUseProgramObjectARB;
extern PFNGLVALIDATEPROGRAMARBPROC glValidateProgramARB;
extern PFNGLUNIFORM1FARBPROC glUniform1fARB;
extern PFNGLUNIFORM2FARBPROC glUniform2fARB;
extern PFNGLUNIFORM3FARBPROC glUniform3fARB;
extern PFNGLUNIFORM4FARBPROC glUniform4fARB;
extern PFNGLUNIFORM1IARBPROC glUniform1iARB;
extern PFNGLUNIFORM2IARBPROC glUniform2iARB;
extern PFNGLUNIFORM3IARBPROC glUniform3iARB;
extern PFNGLUNIFORM4IARBPROC glUniform4iARB;
extern PFNGLUNIFORM1FVARBPROC glUniform1fvARB;
extern PFNGLUNIFORM2FVARBPROC glUniform2fvARB;
extern PFNGLUNIFORM3FVARBPROC glUniform3fvARB;
extern PFNGLUNIFORM4FVARBPROC glUniform4fvARB;
extern PFNGLUNIFORM1IVARBPROC glUniform1ivARB;
extern PFNGLUNIFORM2IVARBPROC glUniform2ivARB;
extern PFNGLUNIFORM3IVARBPROC glUniform3ivARB;
extern PFNGLUNIFORM4IVARBPROC glUniform4ivARB;
extern PFNGLUNIFORMMATRIX2FVARBPROC glUniformMatrix2fvARB;
extern PFNGLUNIFORMMATRIX3FVARBPROC glUniformMatrix3fvARB;
extern PFNGLUNIFORMMATRIX4FVARBPROC glUniformMatrix4fvARB;
extern PFNGLGETOBJECTPARAMETERFVARBPROC glGetObjectParameterfvARB;
extern PFNGLGETOBJECTPARAMETERIVARBPROC glGetObjectParameterivARB;
extern PFNGLGETINFOLOGARBPROC glGetInfoLogARB;
extern PFNGLGETATTACHEDOBJECTSARBPROC glGetAttachedObjectsARB;
extern PFNGLGETUNIFORMLOCATIONARBPROC glGetUniformLocationARB;
extern PFNGLGETACTIVEUNIFORMARBPROC glGetActiveUniformARB;
extern PFNGLGETUNIFORMFVARBPROC glGetUniformfvARB;
extern PFNGLGETUNIFORMIVARBPROC glGetUniformivARB;
extern PFNGLGETSHADERSOURCEARBPROC glGetShaderSourceARB;

// GL_ARB_vertex_shader
extern PFNGLVERTEXATTRIB1DARBPROC glVertexAttrib1dARB;
extern PFNGLVERTEXATTRIB1DVARBPROC glVertexAttrib1dvARB;
extern PFNGLVERTEXATTRIB1FARBPROC glVertexAttrib1fARB;
extern PFNGLVERTEXATTRIB1FVARBPROC glVertexAttrib1fvARB;
extern PFNGLVERTEXATTRIB1SARBPROC glVertexAttrib1sARB;
extern PFNGLVERTEXATTRIB1SVARBPROC glVertexAttrib1svARB;
extern PFNGLVERTEXATTRIB2DARBPROC glVertexAttrib2dARB;
extern PFNGLVERTEXATTRIB2DVARBPROC glVertexAttrib2dvARB;
extern PFNGLVERTEXATTRIB2FARBPROC glVertexAttrib2fARB;
extern PFNGLVERTEXATTRIB2FVARBPROC glVertexAttrib2fvARB;
extern PFNGLVERTEXATTRIB2SARBPROC glVertexAttrib2sARB;
extern PFNGLVERTEXATTRIB2SVARBPROC glVertexAttrib2svARB;
extern PFNGLVERTEXATTRIB3DARBPROC glVertexAttrib3dARB;
extern PFNGLVERTEXATTRIB3DVARBPROC glVertexAttrib3dvARB;
extern PFNGLVERTEXATTRIB3FARBPROC glVertexAttrib3fARB;
extern PFNGLVERTEXATTRIB3FVARBPROC glVertexAttrib3fvARB;
extern PFNGLVERTEXATTRIB3SARBPROC glVertexAttrib3sARB;
extern PFNGLVERTEXATTRIB3SVARBPROC glVertexAttrib3svARB;
extern PFNGLVERTEXATTRIB4NBVARBPROC glVertexAttrib4nbvARB;
extern PFNGLVERTEXATTRIB4NIVARBPROC glVertexAttrib4nivARB;
extern PFNGLVERTEXATTRIB4NSVARBPROC glVertexAttrib4nsvARB;
extern PFNGLVERTEXATTRIB4NUBARBPROC glVertexAttrib4nubARB;
extern PFNGLVERTEXATTRIB4NUBVARBPROC glVertexAttrib4nubvARB;
extern PFNGLVERTEXATTRIB4NUIVARBPROC glVertexAttrib4nuivARB;
extern PFNGLVERTEXATTRIB4NUSVARBPROC glVertexAttrib4nusvARB;
extern PFNGLVERTEXATTRIB4BVARBPROC glVertexAttrib4bvARB;
extern PFNGLVERTEXATTRIB4DARBPROC glVertexAttrib4dARB;
extern PFNGLVERTEXATTRIB4DVARBPROC glVertexAttrib4dvARB;
extern PFNGLVERTEXATTRIB4FARBPROC glVertexAttrib4fARB;
extern PFNGLVERTEXATTRIB4FVARBPROC glVertexAttrib4fvARB;
extern PFNGLVERTEXATTRIB4IVARBPROC glVertexAttrib4ivARB;
extern PFNGLVERTEXATTRIB4SARBPROC glVertexAttrib4sARB;
extern PFNGLVERTEXATTRIB4SVARBPROC glVertexAttrib4svARB;
extern PFNGLVERTEXATTRIB4UBVARBPROC glVertexAttrib4ubvARB;
extern PFNGLVERTEXATTRIB4UIVARBPROC glVertexAttrib4uivARB;
extern PFNGLVERTEXATTRIB4USVARBPROC glVertexAttrib4usvARB;
extern PFNGLVERTEXATTRIBPOINTERARBPROC glVertexAttribPointerARB;
extern PFNGLENABLEVERTEXATTRIBARRAYARBPROC glEnableVertexAttribArrayARB;
extern PFNGLDISABLEVERTEXATTRIBARRAYARBPROC glDisableVertexAttribArrayARB;
extern PFNGLPROGRAMSTRINGARBPROC glProgramStringARB;
extern PFNGLBINDPROGRAMARBPROC glBindProgramARB;
extern PFNGLDELETEPROGRAMSARBPROC glDeleteProgramsARB;
extern PFNGLGENPROGRAMSARBPROC glGenProgramsARB;
extern PFNGLPROGRAMENVPARAMETER4DARBPROC glProgramEnvParameter4dARB;
extern PFNGLPROGRAMENVPARAMETER4DVARBPROC glProgramEnvParameter4dvARB;
extern PFNGLPROGRAMENVPARAMETER4FARBPROC glProgramEnvParameter4fARB;
extern PFNGLPROGRAMENVPARAMETER4FVARBPROC glProgramEnvParameter4fvARB;
extern PFNGLPROGRAMLOCALPARAMETER4DARBPROC glProgramLocalParameter4dARB;
extern PFNGLPROGRAMLOCALPARAMETER4DVARBPROC glProgramLocalParameter4dvARB;
extern PFNGLPROGRAMLOCALPARAMETER4FARBPROC glProgramLocalParameter4fARB;
extern PFNGLPROGRAMLOCALPARAMETER4FVARBPROC glProgramLocalParameter4fvARB;
extern PFNGLGETPROGRAMENVPARAMETERDVARBPROC glGetProgramEnvParameterdvARB;
extern PFNGLGETPROGRAMENVPARAMETERFVARBPROC glGetProgramEnvParameterfvARB;
extern PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC glGetProgramLocalParameterdvARB;
extern PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC glGetProgramLocalParameterfvARB;
extern PFNGLGETPROGRAMIVARBPROC glGetProgramivARB;
extern PFNGLGETPROGRAMSTRINGARBPROC glGetProgramStringARB;
extern PFNGLGETVERTEXATTRIBDVARBPROC glGetVertexAttribdvARB;
extern PFNGLGETVERTEXATTRIBFVARBPROC glGetVertexAttribfvARB;
extern PFNGLGETVERTEXATTRIBIVARBPROC glGetVertexAttribivARB;
extern PFNGLGETVERTEXATTRIBPOINTERVARBPROC glGetVertexAttribPointervARB;
extern PFNGLISPROGRAMARBPROC glIsProgramARB;
extern PFNGLBINDATTRIBLOCATIONARBPROC glBindAttribLocationARB;
extern PFNGLGETACTIVEATTRIBARBPROC glGetActiveAttribARB;
extern PFNGLGETATTRIBLOCATIONARBPROC glGetAttribLocationARB;

//GL_EXT_framebuffer_object
extern PFNGLISRENDERBUFFEREXTPROC glIsRenderbufferEXT;
extern PFNGLBINDRENDERBUFFEREXTPROC glBindRenderbufferEXT;
extern PFNGLDELETERENDERBUFFERSEXTPROC glDeleteRenderbuffersEXT;
extern PFNGLGENRENDERBUFFERSEXTPROC glGenRenderbuffersEXT;
extern PFNGLRENDERBUFFERSTORAGEEXTPROC glRenderbufferStorageEXT;
extern PFNGLGETRENDERBUFFERPARAMETERIVEXTPROC glGetRenderbufferParameterivEXT;
extern PFNGLISFRAMEBUFFEREXTPROC glIsFramebufferEXT;
extern PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebufferEXT;
extern PFNGLDELETEFRAMEBUFFERSEXTPROC glDeleteFramebuffersEXT;
extern PFNGLGENFRAMEBUFFERSEXTPROC glGenFramebuffersEXT;
extern PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC glCheckFramebufferStatusEXT;
extern PFNGLFRAMEBUFFERTEXTURE1DEXTPROC glFramebufferTexture1DEXT;
extern PFNGLFRAMEBUFFERTEXTURE2DEXTPROC glFramebufferTexture2DEXT;
extern PFNGLFRAMEBUFFERTEXTURE3DEXTPROC glFramebufferTexture3DEXT;
extern PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glFramebufferRenderbufferEXT;
extern PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC glGetFramebufferAttachmentParameterivEXT;
extern PFNGLGENERATEMIPMAPEXTPROC glGenerateMipmapEXT;


#elif LL_DARWIN
//----------------------------------------------------------------------------
// LL_DARWIN

#include <OpenGL/gl.h>
#include <OpenGL/glu.h>

#define GL_EXT_separate_specular_color 1
#include <OpenGL/glext.h>

#include "GL/glh_extensions.h"

// These symbols don't exist on 10.3.9, so they have to be declared weak.  Redeclaring them here fixes the problem.
// Note that they also must not be called on 10.3.9.  This should be taken care of by a runtime check for the existence of the GL extension.
#include <AvailabilityMacros.h>

// GL_EXT_framebuffer_object
extern GLboolean glIsRenderbufferEXT(GLuint renderbuffer) AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;
extern void glBindRenderbufferEXT(GLenum target, GLuint renderbuffer) AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;
extern void glDeleteRenderbuffersEXT(GLsizei n, const GLuint *renderbuffers) AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;
extern void glGenRenderbuffersEXT(GLsizei n, GLuint *renderbuffers) AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;
extern void glRenderbufferStorageEXT(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;
extern void glGetRenderbufferParameterivEXT(GLenum target, GLenum pname, GLint *params) AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;
extern GLboolean glIsFramebufferEXT(GLuint framebuffer) AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;
extern void glBindFramebufferEXT(GLenum target, GLuint framebuffer) AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;
extern void glDeleteFramebuffersEXT(GLsizei n, const GLuint *framebuffers) AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;
extern void glGenFramebuffersEXT(GLsizei n, GLuint *framebuffers) AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;
extern GLenum glCheckFramebufferStatusEXT(GLenum target) AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;
extern void glFramebufferTexture1DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;
extern void glFramebufferTexture2DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;
extern void glFramebufferTexture3DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset) AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;
extern void glFramebufferRenderbufferEXT(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;
extern void glGetFramebufferAttachmentParameterivEXT(GLenum target, GLenum attachment, GLenum pname, GLint *params) AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;
extern void glGenerateMipmapEXT(GLenum target) AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;


#ifdef __cplusplus
extern "C" {
#endif
//
// Define vertex buffer object headers on Mac
//
#ifndef GL_ARB_vertex_buffer_object
#define GL_BUFFER_SIZE_ARB                0x8764
#define GL_BUFFER_USAGE_ARB               0x8765
#define GL_ARRAY_BUFFER_ARB               0x8892
#define GL_ELEMENT_ARRAY_BUFFER_ARB       0x8893
#define GL_ARRAY_BUFFER_BINDING_ARB       0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB 0x8895
#define GL_VERTEX_ARRAY_BUFFER_BINDING_ARB 0x8896
#define GL_NORMAL_ARRAY_BUFFER_BINDING_ARB 0x8897
#define GL_COLOR_ARRAY_BUFFER_BINDING_ARB 0x8898
#define GL_INDEX_ARRAY_BUFFER_BINDING_ARB 0x8899
#define GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING_ARB 0x889A
#define GL_EDGE_FLAG_ARRAY_BUFFER_BINDING_ARB 0x889B
#define GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING_ARB 0x889C
#define GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING_ARB 0x889D
#define GL_WEIGHT_ARRAY_BUFFER_BINDING_ARB 0x889E
#define GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING_ARB 0x889F
#define GL_READ_ONLY_ARB                  0x88B8
#define GL_WRITE_ONLY_ARB                 0x88B9
#define GL_READ_WRITE_ARB                 0x88BA
#define GL_BUFFER_ACCESS_ARB              0x88BB
#define GL_BUFFER_MAPPED_ARB              0x88BC
#define GL_BUFFER_MAP_POINTER_ARB         0x88BD
#define GL_STREAM_DRAW_ARB                0x88E0
#define GL_STREAM_READ_ARB                0x88E1
#define GL_STREAM_COPY_ARB                0x88E2
#define GL_STATIC_DRAW_ARB                0x88E4
#define GL_STATIC_READ_ARB                0x88E5
#define GL_STATIC_COPY_ARB                0x88E6
#define GL_DYNAMIC_DRAW_ARB               0x88E8
#define GL_DYNAMIC_READ_ARB               0x88E9
#define GL_DYNAMIC_COPY_ARB               0x88EA
#endif



#ifndef GL_ARB_vertex_buffer_object
/* GL types for handling large vertex buffer objects */
typedef intptr_t GLintptrARB;
typedef intptr_t GLsizeiptrARB;
#endif


#ifndef GL_ARB_vertex_buffer_object
#define GL_ARB_vertex_buffer_object 1
#ifdef GL_GLEXT_FUNCTION_POINTERS
typedef void (* glBindBufferARBProcPtr) (GLenum target, GLuint buffer);
typedef void (* glDeleteBufferARBProcPtr) (GLsizei n, const GLuint *buffers);
typedef void (* glGenBuffersARBProcPtr) (GLsizei n, GLuint *buffers);
typedef GLboolean (* glIsBufferARBProcPtr) (GLuint buffer);
typedef void (* glBufferDataARBProcPtr) (GLenum target, GLsizeiptrARB size, const GLvoid *data, GLenum usage);
typedef void (* glBufferSubDataARBProcPtr) (GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid *data);
typedef void (* glGetBufferSubDataARBProcPtr) (GLenum target, GLintptrARB offset, GLsizeiptrARB size, GLvoid *data);
typedef GLvoid* (* glMapBufferARBProcPtr) (GLenum target, GLenum access);	/* Flawfinder: ignore */
typedef GLboolean (* glUnmapBufferARBProcPtr) (GLenum target);
typedef void (* glGetBufferParameterivARBProcPtr) (GLenum target, GLenum pname, GLint *params);
typedef void (* glGetBufferPointervARBProcPtr) (GLenum target, GLenum pname, GLvoid* *params);
#else
extern void glBindBufferARB (GLenum, GLuint);
extern void glDeleteBuffersARB (GLsizei, const GLuint *);
extern void glGenBuffersARB (GLsizei, GLuint *);
extern GLboolean glIsBufferARB (GLuint);
extern void glBufferDataARB (GLenum, GLsizeiptrARB, const GLvoid *, GLenum);
extern void glBufferSubDataARB (GLenum, GLintptrARB, GLsizeiptrARB, const GLvoid *);
extern void glGetBufferSubDataARB (GLenum, GLintptrARB, GLsizeiptrARB, GLvoid *);
extern GLvoid* glMapBufferARB (GLenum, GLenum);
extern GLboolean glUnmapBufferARB (GLenum);
extern void glGetBufferParameterivARB (GLenum, GLenum, GLint *);
extern void glGetBufferPointervARB (GLenum, GLenum, GLvoid* *);
#endif /* GL_GLEXT_FUNCTION_POINTERS */
#endif

// May be needed for DARWIN...
// #ifndef GL_ARB_compressed_tex_image
// #define GL_ARB_compressed_tex_image 1
// #ifdef GL_GLEXT_FUNCTION_POINTERS
// typedef void (* glCompressedTexImage1D) (GLenum, GLint, GLenum, GLsizei, GLint, GLsizei, const GLvoid*);
// typedef void (* glCompressedTexImage2D) (GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const GLvoid*);
// typedef void (* glCompressedTexImage3D) (GLenum, GLint, GLenum, GLsizei, GLsizei, GLsizei, GLint, GLsizei, const GLvoid*);
// typedef void (* glCompressedTexSubImage1D) (GLenum, GLint, GLint, GLsizei, GLenum, GLsizei, const GLvoid*);
// typedef void (* glCompressedTexSubImage2D) (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const GLvoid*);
// typedef void (* glCompressedTexSubImage3D) (GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLsizei, const GLvoid*);
// typedef void (* glGetCompressedTexImage) (GLenum, GLint, GLvoid*);
// #else
// extern void glCompressedTexImage1D (GLenum, GLint, GLenum, GLsizei, GLint, GLsizei, const GLvoid*);
// extern void glCompressedTexImage2D (GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const GLvoid*);
// extern void glCompressedTexImage3D (GLenum, GLint, GLenum, GLsizei, GLsizei, GLsizei, GLint, GLsizei, const GLvoid*);
// extern void glCompressedTexSubImage1D (GLenum, GLint, GLint, GLsizei, GLenum, GLsizei, const GLvoid*);
// extern void glCompressedTexSubImage2D (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const GLvoid*);
// extern void glCompressedTexSubImage3D (GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLsizei, const GLvoid*);
// extern void glGetCompressedTexImage (GLenum, GLint, GLvoid*);
// #endif /* GL_GLEXT_FUNCTION_POINTERS */
// #endif

#ifdef __cplusplus
}
#endif

#include <AGL/gl.h>

#endif // LL_MESA / LL_WINDOWS / LL_DARWIN


#endif // LL_LLGLHEADERS_H
