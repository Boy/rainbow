/** 
 * @file llviewborder.cpp
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
#include "llviewborder.h"
#include "llrender.h"
#include "llfocusmgr.h"

static LLRegisterWidget<LLViewBorder> r("view_border");

LLViewBorder::LLViewBorder( const std::string& name, const LLRect& rect, EBevel bevel, EStyle style, S32 width )
	:
	LLView( name, rect, FALSE ),
	mBevel( bevel ),
	mStyle( style ),
	mHighlightLight( LLUI::sColorsGroup->getColor( "DefaultHighlightLight" ) ),
	mHighlightDark(	LLUI::sColorsGroup->getColor( "DefaultHighlightDark" ) ),
	mShadowLight( LLUI::sColorsGroup->getColor( "DefaultShadowLight" ) ),
	mShadowDark( LLUI::sColorsGroup->getColor( "DefaultShadowDark" ) ),
	mBorderWidth( width ),
	mTexture( NULL ),
	mHasKeyboardFocus( FALSE )
{
	setFollowsAll();
}

void LLViewBorder::setColors( const LLColor4& shadow_dark, const LLColor4& highlight_light )
{
	mShadowDark = shadow_dark;
	mHighlightLight = highlight_light;
}

void LLViewBorder::setColorsExtended( const LLColor4& shadow_light, const LLColor4& shadow_dark,
				  			   const LLColor4& highlight_light, const LLColor4& highlight_dark )
{
	mShadowDark = shadow_dark;
	mShadowLight = shadow_light;
	mHighlightLight = highlight_light;
	mHighlightDark = highlight_dark;
}

void LLViewBorder::setTexture( const LLUUID &image_id )
{
	mTexture = LLUI::sImageProvider->getUIImageByID(image_id);
}


void LLViewBorder::draw()
{
	if( STYLE_LINE == mStyle )
	{
		if( 0 == mBorderWidth )
		{
			// no visible border
		}
		else
		if( 1 == mBorderWidth )
		{
			drawOnePixelLines();
		}
		else
		if( 2 == mBorderWidth )
		{
			drawTwoPixelLines();
		}
		else
		{
			llassert( FALSE );  // not implemented
		}
	}
	else
	if( STYLE_TEXTURE == mStyle )
	{
		if( mTexture )
		{
			drawTextures();
		}
	}

	// draw the children
	LLView::draw();

}

void LLViewBorder::drawOnePixelLines()
{
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	LLColor4 top_color = mHighlightLight;
	LLColor4 bottom_color = mHighlightLight;
	switch( mBevel )
	{
	case BEVEL_OUT:
		top_color		= mHighlightLight;
		bottom_color	= mShadowDark;
		break;
	case BEVEL_IN:
		top_color		= mShadowDark;
		bottom_color	= mHighlightLight;
		break;
	case BEVEL_NONE:
		// use defaults
		break;
	default:
		llassert(0);
	}

	if( mHasKeyboardFocus )
	{
		top_color = gFocusMgr.getFocusColor();
		bottom_color = top_color;

		LLUI::setLineWidth(lerp(1.f, 3.f, gFocusMgr.getFocusFlashAmt()));
	}

	S32 left	= 0;
	S32 top		= getRect().getHeight();
	S32 right	= getRect().getWidth();
	S32 bottom	= 0;

	gGL.color4fv( top_color.mV );
	gl_line_2d(left, bottom, left, top);
	gl_line_2d(left, top, right, top);

	gGL.color4fv( bottom_color.mV );
	gl_line_2d(right, top, right, bottom);
	gl_line_2d(left, bottom, right, bottom);

	LLUI::setLineWidth(1.f);
}

void LLViewBorder::drawTwoPixelLines()
{
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	
	LLColor4 focus_color = gFocusMgr.getFocusColor();

	F32* top_in_color		= mShadowDark.mV;
	F32* top_out_color		= mShadowDark.mV;
	F32* bottom_in_color	= mShadowDark.mV;
	F32* bottom_out_color	= mShadowDark.mV;
	switch( mBevel )
	{
	case BEVEL_OUT:
		top_in_color		= mHighlightLight.mV;
		top_out_color		= mHighlightDark.mV;
		bottom_in_color		= mShadowLight.mV;
		bottom_out_color	= mShadowDark.mV;
		break;
	case BEVEL_IN:
		top_in_color		= mShadowDark.mV;
		top_out_color		= mShadowLight.mV;
		bottom_in_color		= mHighlightDark.mV;
		bottom_out_color	= mHighlightLight.mV;
		break;
	case BEVEL_BRIGHT:
		top_in_color		= mHighlightLight.mV;
		top_out_color		= mHighlightLight.mV;
		bottom_in_color		= mHighlightLight.mV;
		bottom_out_color	= mHighlightLight.mV;
		break;
	case BEVEL_NONE:
		// use defaults
		break;
	default:
		llassert(0);
	}

	if( mHasKeyboardFocus )
	{
		top_out_color = focus_color.mV;
		bottom_out_color = focus_color.mV;
	}

	S32 left	= 0;
	S32 top		= getRect().getHeight();
	S32 right	= getRect().getWidth();
	S32 bottom	= 0;

	// draw borders
	gGL.color3fv( top_out_color );
	gl_line_2d(left, bottom, left, top-1);
	gl_line_2d(left, top-1, right, top-1);

	gGL.color3fv( top_in_color );
	gl_line_2d(left+1, bottom+1, left+1, top-2);
	gl_line_2d(left+1, top-2, right-1, top-2);

	gGL.color3fv( bottom_out_color );
	gl_line_2d(right-1, top-1, right-1, bottom);
	gl_line_2d(left, bottom, right, bottom);

	gGL.color3fv( bottom_in_color );
	gl_line_2d(right-2, top-2, right-2, bottom+1);
	gl_line_2d(left+1, bottom+1, right-1, bottom+1);
}

void LLViewBorder::drawTextures()
{
	//LLGLSUIDefault gls_ui;

	//llassert( FALSE );  // TODO: finish implementing

	//gGL.color4fv(UI_VERTEX_COLOR.mV);

	//gGL.getTexUnit(0)->bind(mTexture);
	//glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
	//glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );

	//drawTextureTrapezoid(   0.f, mBorderWidth, getRect().getWidth(),  0,					0 );
	//drawTextureTrapezoid(  90.f, mBorderWidth, getRect().getHeight(), (F32)getRect().getWidth(),0 );
	//drawTextureTrapezoid( 180.f, mBorderWidth, getRect().getWidth(),  (F32)getRect().getWidth(),(F32)getRect().getHeight() );
	//drawTextureTrapezoid( 270.f, mBorderWidth, getRect().getHeight(), 0,					(F32)getRect().getHeight() );
}


void LLViewBorder::drawTextureTrapezoid( F32 degrees, S32 width, S32 length, F32 start_x, F32 start_y )
{
	gGL.pushMatrix();
	{
		gGL.translatef(start_x, start_y, 0.f);
		glRotatef( degrees, 0, 0, 1 );

		gGL.begin(LLRender::QUADS);
		{
			//      width, width   /---------\ length-width, width		//
			//	   			      /           \							//
			//				     /			   \						//
			//				    /---------------\						//
			//    			0,0					  length, 0				//

			gGL.texCoord2f( 0, 0 );
			gGL.vertex2i( 0, 0 );

			gGL.texCoord2f( (GLfloat)length, 0 );
			gGL.vertex2i( length, 0 );

			gGL.texCoord2f( (GLfloat)(length - width), (GLfloat)width );
			gGL.vertex2i( length - width, width );

			gGL.texCoord2f( (GLfloat)width, (GLfloat)width );
			gGL.vertex2i( width, width );
		}
		gGL.end();
	}
	gGL.popMatrix();
}

BOOL LLViewBorder::getBevelFromAttribute(LLXMLNodePtr node, LLViewBorder::EBevel& bevel_style)
{
	if (node->hasAttribute("bevel_style"))
	{
		std::string bevel_string;
		node->getAttributeString("bevel_style", bevel_string);
		LLStringUtil::toLower(bevel_string);

		if (bevel_string == "none")
		{
			bevel_style = LLViewBorder::BEVEL_NONE;
		}
		else if (bevel_string == "in")
		{
			bevel_style = LLViewBorder::BEVEL_IN;
		}
		else if (bevel_string == "out")
		{
			bevel_style = LLViewBorder::BEVEL_OUT;
		}
		else if (bevel_string == "bright")
		{
			bevel_style = LLViewBorder::BEVEL_BRIGHT;
		}
		return TRUE;
	}
	return FALSE;
}


// static
LLView* LLViewBorder::fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory)
{
	std::string name("view_border");
	node->getAttributeString("name", name);

	LLViewBorder::EBevel bevel_style = LLViewBorder::BEVEL_IN;
	getBevelFromAttribute(node, bevel_style);

	S32 border_thickness = 1;
	node->getAttributeS32("border_thickness", border_thickness);

	LLViewBorder* border = new LLViewBorder(name, 
									LLRect(), 
									bevel_style,
									LLViewBorder::STYLE_LINE,
									border_thickness);

	border->initFromXML(node, parent);
	
	return border;
}
