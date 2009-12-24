/** 
 * @file llhudrender.cpp
 * @brief LLHUDRender class implementation
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

#include "llviewerprecompiledheaders.h"

#include "llhudrender.h"

#include "llrender.h"
#include "llgl.h"
#include "llviewercamera.h"
#include "v3math.h"
#include "llquaternion.h"
#include "llfontgl.h"
#include "llimagegl.h"
#include "llglheaders.h"
#include "llviewerwindow.h"

void hud_render_utf8text(const std::string &str, const LLVector3 &pos_agent,
					 const LLFontGL &font,
					 const U8 style,
					 const F32 x_offset, const F32 y_offset,
					 const LLColor4& color,
					 const BOOL orthographic)
{
	LLWString wstr(utf8str_to_wstring(str));
	hud_render_text(wstr, pos_agent, font, style, x_offset, y_offset, color, orthographic);
}

void hud_render_text(const LLWString &wstr, const LLVector3 &pos_agent,
					const LLFontGL &font,
					const U8 style,
					const F32 x_offset, const F32 y_offset,
					const LLColor4& color,
					const BOOL orthographic)
{
	// Do cheap plane culling
	LLVector3 dir_vec = pos_agent - LLViewerCamera::getInstance()->getOrigin();
	dir_vec /= dir_vec.magVec();

	if (wstr.empty() || (!orthographic && dir_vec * LLViewerCamera::getInstance()->getAtAxis() <= 0.f))
	{
		return;
	}

	LLVector3 right_axis;
	LLVector3 up_axis;
	if (orthographic)
	{
		right_axis.setVec(0.f, -1.f / gViewerWindow->getWindowHeight(), 0.f);
		up_axis.setVec(0.f, 0.f, 1.f / gViewerWindow->getWindowHeight());
	}
	else
	{
		LLViewerCamera::getInstance()->getPixelVectors(pos_agent, up_axis, right_axis);
	}
	LLCoordFrame render_frame = *LLViewerCamera::getInstance();
	LLQuaternion rot;
	if (!orthographic)
	{
		rot = render_frame.getQuaternion();
		rot = rot * LLQuaternion(-F_PI_BY_TWO, LLViewerCamera::getInstance()->getYAxis());
		rot = rot * LLQuaternion(F_PI_BY_TWO, LLViewerCamera::getInstance()->getXAxis());
	}
	else
	{
		rot = LLQuaternion(-F_PI_BY_TWO, LLVector3(0.f, 0.f, 1.f));
		rot = rot * LLQuaternion(-F_PI_BY_TWO, LLVector3(0.f, 1.f, 0.f));
	}
	F32 angle;
	LLVector3 axis;
	rot.getAngleAxis(&angle, axis);

	LLVector3 render_pos = pos_agent + (floorf(x_offset) * right_axis) + (floorf(y_offset) * up_axis);

	//get the render_pos in screen space
	
	F64 winX, winY, winZ;
	gluProject(render_pos.mV[0], render_pos.mV[1], render_pos.mV[2],
				gGLModelView, gGLProjection, (GLint*) gGLViewport,
				&winX, &winY, &winZ);
		
	//fonts all render orthographically, set up projection
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	
	LLUI::pushMatrix();
		
	gViewerWindow->setup2DRender();

	LLUI::loadIdentity();
	LLUI::translate((F32) winX*1.0f/LLFontGL::sScaleX, (F32) winY*1.0f/(LLFontGL::sScaleY), -(((F32) winZ*2.f)-1.f));
	//glRotatef(angle * RAD_TO_DEG, axis.mV[VX], axis.mV[VY], axis.mV[VZ]);
	//glScalef(right_scale, up_scale, 1.f);
	F32 right_x;
	
	font.render(wstr, 0, 0, 0, color, LLFontGL::LEFT, LLFontGL::BASELINE, style, wstr.length(), 1000, &right_x);
	LLUI::popMatrix();
	
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}
