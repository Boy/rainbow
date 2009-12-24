/** 
 * @file llconsole.h
 * @brief a simple console-style output device
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

#ifndef LL_LLCONSOLE_H
#define LL_LLCONSOLE_H

#include "llfixedbuffer.h"
#include "llview.h"
#include "v4color.h"
#include <deque>

class LLFontGL;
class LLSD;

class LLConsole : public LLFixedBuffer, public LLView
{
private:
	F32			mLinePersistTime; // Age at which to stop drawing.
	F32			mFadeTime; // Age at which to start fading
	LLFontGL*	mFont;
	S32			mLastBoxHeight;
	S32			mLastBoxWidth;
	S32			mConsoleWidth;
	S32			mConsoleHeight;

public:
	//A paragraph color segment defines the color of text in a line 
	//of text that was received for console display.  It has no 
	//notion of line wraps, screen position, or the text it contains.
	//It is only the number of characters that are a color and the
	//color.
	struct ParagraphColorSegment
	{
		S32		mNumChars;
		LLColor4 mColor;
	};
	
	//A line color segment is a chunk of text, the color associated
	//with it, and the X Position it was calculated to begin at 
	//on the screen.  X Positions are re-calculated if the 
	//screen changes size.
	class LineColorSegment
	{
		public:
			LineColorSegment(LLWString text, LLColor4 color, F32 xpos) : mText(text), mColor(color), mXPosition(xpos) {}
		public:
			LLWString mText;
			LLColor4  mColor;
			F32		  mXPosition;
	};
	 	
	typedef std::list<LineColorSegment> line_color_segments_t;
	
	//A line is composed of one or more color segments.
	class Line
	{
		public:
			line_color_segments_t mLineColorSegments;
	};
	
	typedef std::list<Line> lines_t;
	typedef std::list<ParagraphColorSegment> paragraph_color_segments_t;
	
	//A paragraph is a processed element containing the entire text of the
	//message (used for recalculating positions on screen resize)
	//The time this message was added to the console output
	//The visual screen width of the longest line in this block
	//And a list of one or more lines which are used to display this message.
	class Paragraph
	{
		public:
			Paragraph (LLWString str, const LLColor4 &color, F32 add_time, LLFontGL* font, F32 screen_width);
			void makeParagraphColorSegments ( const LLColor4 &color);
			void updateLines ( F32 screen_width,  LLFontGL* font, bool force_resize=false );
		public:
			LLWString mParagraphText;	//The entire text of the paragraph
			paragraph_color_segments_t	mParagraphColorSegments;
			F32 mAddTime;				//Time this paragraph was added to the display.
			F32 mMaxWidth;				//Width of the widest line of text in this paragraph.
			lines_t	mLines;
			
	};
		
	//The console contains a deque of paragraphs which represent the individual messages.
	typedef std::deque<Paragraph> paragraph_t;
	paragraph_t mParagraphs;

	// Font size:
	// -1 = monospace, 0 means small, font size = 1 means big
	LLConsole(const std::string& name, const U32 max_lines, const LLRect &rect, 
		S32 font_size_index, F32 persist_time );
	~LLConsole(){};

	// each line lasts this long after being added
	void			setLinePersistTime(F32 seconds);

	void			reshape(S32 width, S32 height, BOOL called_from_parent = TRUE);

	// -1 = monospace, 0 means small, font size = 1 means big
	void			setFontSize(S32 size_index);

	void			addLine(const std::string& utf8line, F32 size, const LLColor4 &color);
	void			addLine(const LLWString& wline, F32 size, const LLColor4 &color);
	
	// Overrides
	/*virtual*/ void	draw();
	/*virtual*/ void	addLine(const std::string& utf8line);
	/*virtual*/ void	addLine(const LLWString& line);
};

extern LLConsole* gConsole;

#endif
