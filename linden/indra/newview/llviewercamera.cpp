/** 
 * @file llviewercamera.cpp
 * @brief LLViewerCamera class implementation
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
#include "llviewercamera.h"

#include <iomanip> // for setprecision

#include "llquaternion.h"

#include "llagent.h"
#include "llviewercontrol.h"
#include "lldrawable.h"
#include "llface.h"
#include "llgl.h"
#include "llglheaders.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llvovolume.h"
#include "llworld.h"
#include "lltoolmgr.h"
#include "llviewerjoystick.h"

//glu pick matrix implementation borrowed from Mesa3D
glh::matrix4f gl_pick_matrix(GLfloat x, GLfloat y, GLfloat width, GLfloat height, GLint* viewport)
{
	GLfloat m[16];
	GLfloat sx, sy;
	GLfloat tx, ty;

	sx = viewport[2] / width;
	sy = viewport[3] / height;
	tx = (viewport[2] + 2.f * (viewport[0] - x)) / width;
	ty = (viewport[3] + 2.f * (viewport[1] - y)) / height;

	#define M(row,col) m[col*4+row]
	M(0,0) = sx; M(0,1) = 0.f; M(0,2) = 0.f; M(0,3) = tx;
	M(1,0) = 0.f; M(1,1) = sy; M(1,2) = 0.f; M(1,3) = ty;
	M(2,0) = 0.f; M(2,1) = 0.f; M(2,2) = 1.f; M(2,3) = 0.f;
	M(3,0) = 0.f; M(3,1) = 0.f; M(3,2) = 0.f; M(3,3) = 1.f;
	#undef M

	return glh::matrix4f(m);
}

glh::matrix4f gl_perspective(GLfloat fovy, GLfloat aspect, GLfloat zNear, GLfloat zFar)
{
	GLfloat f = 1.f/tanf(DEG_TO_RAD*fovy/2.f);

	return glh::matrix4f(f/aspect, 0, 0, 0,
						 0, f, 0, 0,
						 0, 0, (zFar+zNear)/(zNear-zFar), (2.f*zFar*zNear)/(zNear-zFar),
						 0, 0, -1.f, 0);
}

LLViewerCamera::LLViewerCamera() : LLCamera()
{
	calcProjection(getFar());
	mCameraFOVDefault = DEFAULT_FIELD_OF_VIEW;
	mPixelMeterRatio = 0.f;
	mScreenPixelArea = 0;
	mZoomFactor = 1.f;
	mZoomSubregion = 1;
}

void LLViewerCamera::updateCameraLocation(const LLVector3 &center,
											const LLVector3 &up_direction,
											const LLVector3 &point_of_interest)
{
	// do not update if we are in build mode AND avatar didn't move
	if (LLToolMgr::getInstance()->inBuildMode() 
		&& !LLViewerJoystick::getInstance()->getCameraNeedsUpdate())
	{
		return;
	}

	LLVector3 last_position;
	LLVector3 last_axis;
	last_position = getOrigin();
	last_axis = getAtAxis();

	mLastPointOfInterest = point_of_interest;

	// constrain to max distance from avatar
	LLVector3 camera_offset = center - gAgent.getPositionAgent();

	LLViewerRegion * regp = gAgent.getRegion();
	F32 water_height = (NULL != regp) ? regp->getWaterHeight() : 0.f;

	LLVector3 origin = center;
	if (origin.mV[2] > water_height)
	{
		origin.mV[2] = llmax(origin.mV[2], water_height+0.20f);
	}
	else
	{
		origin.mV[2] = llmin(origin.mV[2], water_height-0.20f);
	}

	setOriginAndLookAt(origin, up_direction, point_of_interest);

	F32 dpos = (center - last_position).magVec();
	LLQuaternion rotation;
	rotation.shortestArc(last_axis, getAtAxis());

	F32 x, y, z;
	F32 drot;
	rotation.getAngleAxis(&drot, &x, &y, &z);
	mVelocityStat.addValue(dpos);
	mAngularVelocityStat.addValue(drot);
	// update pixel meter ratio using default fov, not modified one
	mPixelMeterRatio = mViewHeightInPixels / (2.f*tanf(mCameraFOVDefault*0.5));
	// update screen pixel area
	mScreenPixelArea =(S32)((F32)mViewHeightInPixels * ((F32)mViewHeightInPixels * mAspect));
}

const LLMatrix4 &LLViewerCamera::getProjection() const
{
	calcProjection(getFar());
	return mProjectionMatrix;

}

const LLMatrix4 &LLViewerCamera::getModelview() const
{
	LLMatrix4 cfr(OGL_TO_CFR_ROTATION);
	getMatrixToLocal(mModelviewMatrix);
	mModelviewMatrix *= cfr;
	return mModelviewMatrix;
}

void LLViewerCamera::calcProjection(const F32 far_distance) const
{
	F32 fov_y, z_far, z_near, aspect, f;
	fov_y = getView();
	z_far = far_distance;
	z_near = getNear();
	aspect = getAspect();

	f = 1/tan(fov_y*0.5f);

	mProjectionMatrix.setZero();
	mProjectionMatrix.mMatrix[0][0] = f/aspect;
	mProjectionMatrix.mMatrix[1][1] = f;
	mProjectionMatrix.mMatrix[2][2] = (z_far + z_near)/(z_near - z_far);
	mProjectionMatrix.mMatrix[3][2] = (2*z_far*z_near)/(z_near - z_far);
	mProjectionMatrix.mMatrix[2][3] = -1;
}

// Sets up opengl state for 3D drawing.  If for selection, also
// sets up a pick matrix.  x and y are ignored if for_selection is false.
// The picking region is centered on x,y and has the specified width and
// height.

//static
void LLViewerCamera::updateFrustumPlanes(LLCamera& camera, BOOL ortho, BOOL zflip)
{
	GLint* viewport = (GLint*) gGLViewport;
	GLdouble* model = gGLModelView;
	GLdouble* proj = gGLProjection;
	GLdouble objX,objY,objZ;

	LLVector3 frust[8];

	if (zflip)
	{
		gluUnProject(viewport[0],viewport[1]+viewport[3],0,model,proj,viewport,&objX,&objY,&objZ);
		frust[0].setVec((F32)objX,(F32)objY,(F32)objZ);
		gluUnProject(viewport[0]+viewport[2],viewport[1]+viewport[3],0,model,proj,viewport,&objX,&objY,&objZ);
		frust[1].setVec((F32)objX,(F32)objY,(F32)objZ);
		gluUnProject(viewport[0]+viewport[2],viewport[1],0,model,proj,viewport,&objX,&objY,&objZ);
		frust[2].setVec((F32)objX,(F32)objY,(F32)objZ);
		gluUnProject(viewport[0],viewport[1],0,model,proj,viewport,&objX,&objY,&objZ);
		frust[3].setVec((F32)objX,(F32)objY,(F32)objZ);

		gluUnProject(viewport[0],viewport[1]+viewport[3],1,model,proj,viewport,&objX,&objY,&objZ);
		frust[4].setVec((F32)objX,(F32)objY,(F32)objZ);
		gluUnProject(viewport[0]+viewport[2],viewport[1]+viewport[3],1,model,proj,viewport,&objX,&objY,&objZ);
		frust[5].setVec((F32)objX,(F32)objY,(F32)objZ);
		gluUnProject(viewport[0]+viewport[2],viewport[1],1,model,proj,viewport,&objX,&objY,&objZ);
		frust[6].setVec((F32)objX,(F32)objY,(F32)objZ);
		gluUnProject(viewport[0],viewport[1],1,model,proj,viewport,&objX,&objY,&objZ);
		frust[7].setVec((F32)objX,(F32)objY,(F32)objZ);

		for (U32 i = 0; i < 4; i++)
		{
			frust[i+4] = frust[i+4]-frust[i];
			frust[i+4].normVec();
			frust[i+4] = frust[i] + frust[i+4]*camera.getFar();
		}
	}
	else
	{
		gluUnProject(viewport[0],viewport[1],0,model,proj,viewport,&objX,&objY,&objZ);
		frust[0].setVec((F32)objX,(F32)objY,(F32)objZ);
		gluUnProject(viewport[0]+viewport[2],viewport[1],0,model,proj,viewport,&objX,&objY,&objZ);
		frust[1].setVec((F32)objX,(F32)objY,(F32)objZ);
		gluUnProject(viewport[0]+viewport[2],viewport[1]+viewport[3],0,model,proj,viewport,&objX,&objY,&objZ);
		frust[2].setVec((F32)objX,(F32)objY,(F32)objZ);
		gluUnProject(viewport[0],viewport[1]+viewport[3],0,model,proj,viewport,&objX,&objY,&objZ);
		frust[3].setVec((F32)objX,(F32)objY,(F32)objZ);
		
		if (ortho)
		{
			LLVector3 far_shift = LLVector3(camera.getFar()*2.0f,0,0);
			for (U32 i = 0; i < 4; i++)
			{
				frust[i+4] = frust[i] + far_shift;
			}
		}
		else
		{
			for (U32 i = 0; i < 4; i++)
			{
				LLVector3 vec = frust[i] - camera.getOrigin();
				vec.normVec();
				frust[i+4] = camera.getOrigin() + vec*camera.getFar();
			}
		}
	}

	camera.calcAgentFrustumPlanes(frust);
}

void LLViewerCamera::setPerspective(BOOL for_selection,
									S32 x, S32 y_from_bot, S32 width, S32 height,
									BOOL limit_select_distance,
									F32 z_near, F32 z_far)
{
	F32 fov_y, aspect;
	fov_y = RAD_TO_DEG * getView();
	BOOL z_default_near, z_default_far = FALSE;
	if (z_far <= 0)
	{
		z_default_far = TRUE;
		z_far = getFar();
	}
	if (z_near <= 0)
	{
		z_default_near = TRUE;
		z_near = getNear();
	}
	aspect = getAspect();

	// Load camera view matrix
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();

	glh::matrix4f proj_mat;

	if (for_selection)
	{
		// make a tiny little viewport
		// anything drawn into this viewport will be "selected"
		GLint*		viewport = (GLint*) gGLViewport;
		
		proj_mat = gl_pick_matrix(x+width/2.f, y_from_bot+height/2.f, (GLfloat) width, (GLfloat) height, viewport);

		if (limit_select_distance)
		{
			// ...select distance from control
			z_far = gSavedSettings.getF32("MaxSelectDistance");
		}
		else
		{
			z_far = gAgent.mDrawDistance;
		}
	}
	else
	{
		// Only override the far clip if it's not passed in explicitly.
		if (z_default_far)
		{
			z_far = MAX_FAR_CLIP;
		}
		glViewport(x, y_from_bot, width, height);
		gGLViewport[0] = x;
		gGLViewport[1] = y_from_bot;
		gGLViewport[2] = width;
		gGLViewport[3] = height;
	}
	
	if (mZoomFactor > 1.f)
	{
		float offset = mZoomFactor - 1.f;
		int pos_y = mZoomSubregion / llceil(mZoomFactor);
		int pos_x = mZoomSubregion - (pos_y*llceil(mZoomFactor));
		glh::matrix4f translate;
		translate.set_translate(glh::vec3f(offset - (F32)pos_x * 2.f, offset - (F32)pos_y * 2.f, 0.f));
		glh::matrix4f scale;
		scale.set_scale(glh::vec3f(mZoomFactor, mZoomFactor, 1.f));

		proj_mat = scale*proj_mat;
		proj_mat = translate*proj_mat;
	}

	calcProjection(z_far); // Update the projection matrix cache

	proj_mat *= gl_perspective(fov_y,aspect,z_near,z_far);

	glLoadMatrixf(proj_mat.m);

	for (U32 i = 0; i < 16; i++)
	{
		gGLProjection[i] = proj_mat.m[i];
	}

	glMatrixMode( GL_MODELVIEW );

	glh::matrix4f modelview((GLfloat*) OGL_TO_CFR_ROTATION);

	GLfloat			ogl_matrix[16];

	getOpenGLTransform(ogl_matrix);

	modelview *= glh::matrix4f(ogl_matrix);
	
	glLoadMatrixf(modelview.m);
	
	if (for_selection && (width > 1 || height > 1))
	{
		calculateFrustumPlanesFromWindow((F32)(x - width / 2) / (F32)gViewerWindow->getWindowWidth() - 0.5f,
								(F32)(y_from_bot - height / 2) / (F32)gViewerWindow->getWindowHeight() - 0.5f,
								(F32)(x + width / 2) / (F32)gViewerWindow->getWindowWidth() - 0.5f,
								(F32)(y_from_bot + height / 2) / (F32)gViewerWindow->getWindowHeight() - 0.5f);

	}

	// if not picking and not doing a snapshot, cache various GL matrices
	if (!for_selection && mZoomFactor == 1.f)
	{
		// Save GL matrices for access elsewhere in code, especially project_world_to_screen
		//glGetDoublev(GL_MODELVIEW_MATRIX, gGLModelView);
		for (U32 i = 0; i < 16; i++)
		{
			gGLModelView[i] = modelview.m[i];
		}
	}

	updateFrustumPlanes(*this);

	if (gSavedSettings.getBOOL("CameraOffset"))
	{
		glMatrixMode(GL_PROJECTION);
		glTranslatef(0,0,-50);
		glRotatef(20.0,1,0,0);
		glMatrixMode(GL_MODELVIEW);
	}
}


// Uses the last GL matrices set in set_perspective to project a point from
// screen coordinates to the agent's region.
void LLViewerCamera::projectScreenToPosAgent(const S32 screen_x, const S32 screen_y, LLVector3* pos_agent) const
{

	GLdouble x, y, z;
	gluUnProject(
		GLdouble(screen_x), GLdouble(screen_y), 0.0,
		gGLModelView, gGLProjection, (GLint*)gGLViewport,
		&x,
		&y,
		&z );
	pos_agent->setVec( (F32)x, (F32)y, (F32)z );
}

// Uses the last GL matrices set in set_perspective to project a point from
// the agent's region space to screen coordinates.  Returns TRUE if point in within
// the current window.
BOOL LLViewerCamera::projectPosAgentToScreen(const LLVector3 &pos_agent, LLCoordGL &out_point, const BOOL clamp) const
{
	BOOL in_front = TRUE;
	GLdouble	x, y, z;			// object's window coords, GL-style

	LLVector3 dir_to_point = pos_agent - getOrigin();
	dir_to_point /= dir_to_point.magVec();

	if (dir_to_point * getAtAxis() < 0.f)
	{
		if (clamp)
		{
			return FALSE;
		}
		else
		{
			in_front = FALSE;
		}
	}

	if (GL_TRUE == gluProject(pos_agent.mV[VX], pos_agent.mV[VY], pos_agent.mV[VZ],
								gGLModelView, gGLProjection, (GLint*)gGLViewport,
								&x, &y, &z))
	{
		// convert screen coordinates to virtual UI coordinates
		x /= gViewerWindow->getDisplayScale().mV[VX];
		y /= gViewerWindow->getDisplayScale().mV[VY];

		// should now have the x,y coords of grab_point in screen space
		const LLRect& window_rect = gViewerWindow->getWindowRect();

		// ...sanity check
		S32 int_x = lltrunc(x);
		S32 int_y = lltrunc(y);

		BOOL valid = TRUE;

		if (clamp)
		{
			if (int_x < window_rect.mLeft)
			{
				out_point.mX = window_rect.mLeft;
				valid = FALSE;
			}
			else if (int_x > window_rect.mRight)
			{
				out_point.mX = window_rect.mRight;
				valid = FALSE;
			}
			else
			{
				out_point.mX = int_x;
			}

			if (int_y < window_rect.mBottom)
			{
				out_point.mY = window_rect.mBottom;
				valid = FALSE;
			}
			else if (int_y > window_rect.mTop)
			{
				out_point.mY = window_rect.mTop;
				valid = FALSE;
			}
			else
			{
				out_point.mY = int_y;
			}
			return valid;
		}
		else
		{
			out_point.mX = int_x;
			out_point.mY = int_y;

			if (int_x < window_rect.mLeft)
			{
				valid = FALSE;
			}
			else if (int_x > window_rect.mRight)
			{
				valid = FALSE;
			}
			if (int_y < window_rect.mBottom)
			{
				valid = FALSE;
			}
			else if (int_y > window_rect.mTop)
			{
				valid = FALSE;
			}

			return in_front && valid;
		}
	}
	else
	{
		return FALSE;
	}
}

// Uses the last GL matrices set in set_perspective to project a point from
// the agent's region space to the nearest edge in screen coordinates.
// Returns TRUE if projection succeeds.
BOOL LLViewerCamera::projectPosAgentToScreenEdge(const LLVector3 &pos_agent,
												LLCoordGL &out_point) const
{
	LLVector3 dir_to_point = pos_agent - getOrigin();
	dir_to_point /= dir_to_point.magVec();

	BOOL in_front = TRUE;
	if (dir_to_point * getAtAxis() < 0.f)
	{
		in_front = FALSE;
	}

	GLdouble	x, y, z;			// object's window coords, GL-style
	if (GL_TRUE == gluProject(pos_agent.mV[VX], pos_agent.mV[VY],
							  pos_agent.mV[VZ], gGLModelView,
							  gGLProjection, (GLint*)gGLViewport,
							  &x, &y, &z))
	{
		x /= gViewerWindow->getDisplayScale().mV[VX];
		y /= gViewerWindow->getDisplayScale().mV[VY];
		// should now have the x,y coords of grab_point in screen space
		const LLRect& window_rect = gViewerWindow->getVirtualWindowRect();

		// ...sanity check
		S32 int_x = lltrunc(x);
		S32 int_y = lltrunc(y);

		// find the center
		GLdouble center_x = (GLdouble)(0.5f * (window_rect.mLeft + window_rect.mRight));
		GLdouble center_y = (GLdouble)(0.5f * (window_rect.mBottom + window_rect.mTop));

		if (x == center_x  &&  y == center_y)
		{
			// can't project to edge from exact center
			return FALSE;
		}

		// find the line from center to local
		GLdouble line_x = x - center_x;
		GLdouble line_y = y - center_y;

		int_x = lltrunc(center_x);
		int_y = lltrunc(center_y);


		if (0.f == line_x)
		{
			// the slope of the line is undefined
			if (line_y > 0.f)
			{
				int_y = window_rect.mTop;
			}
			else
			{
				int_y = window_rect.mBottom;
			}
		}
		else if (0 == window_rect.getWidth())
		{
			// the diagonal slope of the view is undefined
			if (y < window_rect.mBottom)
			{
				int_y = window_rect.mBottom;
			}
			else if ( y > window_rect.mTop)
			{
				int_y = window_rect.mTop;
			}
		}
		else
		{
			F32 line_slope = (F32)(line_y / line_x);
			F32 rect_slope = ((F32)window_rect.getHeight()) / ((F32)window_rect.getWidth());

			if (fabs(line_slope) > rect_slope)
			{
				if (line_y < 0.f)
				{
					// bottom
					int_y = window_rect.mBottom;
				}
				else
				{
					// top
					int_y = window_rect.mTop;
				}
				int_x = lltrunc(((GLdouble)int_y - center_y) / line_slope + center_x);
			}
			else if (fabs(line_slope) < rect_slope)
			{
				if (line_x < 0.f)
				{
					// left
					int_x = window_rect.mLeft;
				}
				else
				{
					// right
					int_x = window_rect.mRight;
				}
				int_y = lltrunc(((GLdouble)int_x - center_x) * line_slope + center_y);
			}
			else
			{
				// exactly parallel ==> push to the corners
				if (line_x > 0.f)
				{
					int_x = window_rect.mRight;
				}
				else
				{
					int_x = window_rect.mLeft;
				}
				if (line_y > 0.0f)
				{
					int_y = window_rect.mTop;
				}
				else
				{
					int_y = window_rect.mBottom;
				}
			}
		}
		if (!in_front)
		{
			int_x = window_rect.mLeft + window_rect.mRight - int_x;
			int_y = window_rect.mBottom + window_rect.mTop - int_y;
		}
		out_point.mX = int_x;
		out_point.mY = int_y;
		return TRUE;
	}
	return FALSE;
}


void LLViewerCamera::getPixelVectors(const LLVector3 &pos_agent, LLVector3 &up, LLVector3 &right)
{
	LLVector3 to_vec = pos_agent - getOrigin();

	F32 at_dist = to_vec * getAtAxis();

	F32 height_meters = at_dist* (F32)tan(getView()/2.f);
	F32 height_pixels = getViewHeightInPixels()/2.f;

	F32 pixel_aspect = gViewerWindow->getWindow()->getPixelAspectRatio();

	F32 meters_per_pixel = height_meters / height_pixels;
	up = getUpAxis() * meters_per_pixel * gViewerWindow->getDisplayScale().mV[VY];
	right = -1.f * pixel_aspect * meters_per_pixel * getLeftAxis() * gViewerWindow->getDisplayScale().mV[VX];
}

LLVector3 LLViewerCamera::roundToPixel(const LLVector3 &pos_agent)
{
	F32 dist = (pos_agent - getOrigin()).magVec();
	// Convert to screen space and back, preserving the depth.
	LLCoordGL screen_point;
	if (!projectPosAgentToScreen(pos_agent, screen_point, FALSE))
	{
		// Off the screen, just return the original position.
		return pos_agent;
	}

	LLVector3 ray_dir;

	projectScreenToPosAgent(screen_point.mX, screen_point.mY, &ray_dir);
	ray_dir -= getOrigin();
	ray_dir.normVec();

	LLVector3 pos_agent_rounded = getOrigin() + ray_dir*dist;

	/*
	LLVector3 pixel_x, pixel_y;
	getPixelVectors(pos_agent_rounded, pixel_y, pixel_x);
	pos_agent_rounded += 0.5f*pixel_x, 0.5f*pixel_y;
	*/
	return pos_agent_rounded;
}

BOOL LLViewerCamera::cameraUnderWater() const
{
	return getOrigin().mV[VZ] < gAgent.getRegion()->getWaterHeight();
}

BOOL LLViewerCamera::areVertsVisible(LLViewerObject* volumep, BOOL all_verts)
{
	S32 i, num_faces;
	LLDrawable* drawablep = volumep->mDrawable;

	if (!drawablep)
	{
		return FALSE;
	}

	LLVolume* volume = volumep->getVolume();
	if (!volume)
	{
		return FALSE;
	}

	LLVOVolume* vo_volume = (LLVOVolume*) volumep;

	vo_volume->updateRelativeXform();
	LLMatrix4 mat = vo_volume->getRelativeXform();
	
	LLMatrix4 render_mat(vo_volume->getRenderRotation(), LLVector4(vo_volume->getRenderPosition()));

	num_faces = volume->getNumVolumeFaces();
	for (i = 0; i < num_faces; i++)
	{
		const LLVolumeFace& face = volume->getVolumeFace(i);
				
		for (U32 v = 0; v < face.mVertices.size(); v++)
		{
			LLVector4 vec = LLVector4(face.mVertices[v].mPosition) * mat;

			if (drawablep->isActive())
			{
				vec = vec * render_mat;	
			}

			BOOL in_frustum = pointInFrustum(LLVector3(vec)) > 0;

			if ( !in_frustum && all_verts ||
				 in_frustum && !all_verts)
			{
				return !all_verts;
			}
		}
	}
	return all_verts;
}
