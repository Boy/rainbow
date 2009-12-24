/** 
 * @file llwindowsdl.cpp
 * @brief SDL implementation of LLWindow class
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

#if LL_SDL

#include "linden_common.h"

#include "llwindowsdl.h"
#include "llkeyboardsdl.h"
#include "llerror.h"
#include "llgl.h"
#include "llstring.h"
#include "lldir.h"
#include "llfindlocale.h"

#include "indra_constants.h"

#if LL_GTK
extern "C" {
# include "gtk/gtk.h"
}
#include <locale.h>
#endif // LL_GTK

extern "C" {
# include "fontconfig/fontconfig.h"
}

#if LL_LINUX || LL_SOLARIS
// not necessarily available on random SDL platforms, so #if LL_LINUX
// for execv(), waitpid(), fork()
# include <unistd.h>
# include <sys/types.h>
# include <sys/wait.h>
#endif // LL_LINUX || LL_SOLARIS

extern BOOL gDebugWindowProc;

const S32 MAX_NUM_RESOLUTIONS = 200;

// static variable for ATI mouse cursor crash work-around:
static bool ATIbug = false;

//
// LLWindowSDL
//

#if LL_X11
# include <X11/Xutil.h>
#endif //LL_X11

// TOFU HACK -- (*exactly* the same hack as LLWindowMacOSX for a similar
// set of reasons): Stash a pointer to the LLWindowSDL object here and
// maintain in the constructor and destructor.  This assumes that there will
// be only one object of this class at any time.  Currently this is true.
static LLWindowSDL *gWindowImplementation = NULL;

static BOOL was_fullscreen = FALSE;


void maybe_lock_display(void)
{
	if (gWindowImplementation && gWindowImplementation->Lock_Display) {
		gWindowImplementation->Lock_Display();
	}
}


void maybe_unlock_display(void)
{
	if (gWindowImplementation && gWindowImplementation->Unlock_Display) {
		gWindowImplementation->Unlock_Display();
	}
}


#if LL_GTK
// Lazily initialize and check the runtime GTK version for goodness.
// static
bool LLWindowSDL::ll_try_gtk_init(void)
{
	static BOOL done_gtk_diag = FALSE;
	static BOOL gtk_is_good = FALSE;
	static BOOL done_setlocale = FALSE;
	static BOOL tried_gtk_init = FALSE;

	if (!done_setlocale)
	{
		llinfos << "Starting GTK Initialization." << llendl;
		maybe_lock_display();
		gtk_disable_setlocale();
		maybe_unlock_display();
		done_setlocale = TRUE;
	}
	
	if (!tried_gtk_init)
	{
		tried_gtk_init = TRUE;
		if (!g_thread_supported ()) g_thread_init (NULL);
		maybe_lock_display();
		gtk_is_good = gtk_init_check(NULL, NULL);
		maybe_unlock_display();
		if (!gtk_is_good)
			llwarns << "GTK Initialization failed." << llendl;
	}

	if (gtk_is_good && !done_gtk_diag)
	{
		llinfos << "GTK Initialized." << llendl;
		llinfos << "- Compiled against GTK version "
			<< GTK_MAJOR_VERSION << "."
			<< GTK_MINOR_VERSION << "."
			<< GTK_MICRO_VERSION << llendl;
		llinfos << "- Running against GTK version "
			<< gtk_major_version << "."
			<< gtk_minor_version << "."
			<< gtk_micro_version << llendl;
		maybe_lock_display();
		const gchar* gtk_warning = gtk_check_version(
			GTK_MAJOR_VERSION,
			GTK_MINOR_VERSION,
			GTK_MICRO_VERSION);
		maybe_unlock_display();
		if (gtk_warning)
		{
			llwarns << "- GTK COMPATIBILITY WARNING: " <<
				gtk_warning << llendl;
			gtk_is_good = FALSE;
		}

		done_gtk_diag = TRUE;
	}

	return gtk_is_good;
}
#endif // LL_GTK


#if LL_X11
// static
Window LLWindowSDL::get_SDL_XWindowID(void)
{
	if (gWindowImplementation) {
		return gWindowImplementation->mSDL_XWindowID;
	}
	return None;
}

//static
Display* LLWindowSDL::get_SDL_Display(void)
{
	if (gWindowImplementation) {
		return gWindowImplementation->mSDL_Display;
	}
	return NULL;
}
#endif // LL_X11


LLWindowSDL::LLWindowSDL(const std::string& title, S32 x, S32 y, S32 width,
							   S32 height, U32 flags,
							   BOOL fullscreen, BOOL clearBg,
							   BOOL disable_vsync, BOOL use_gl,
							   BOOL ignore_pixel_depth, U32 fsaa_samples)
	: LLWindow(fullscreen, flags), mGamma(1.0f)
{
	// Initialize the keyboard
	gKeyboard = new LLKeyboardSDL();
	// Note that we can't set up key-repeat until after SDL has init'd video

	// Ignore use_gl for now, only used for drones on PC
	mWindow = NULL;
	mCursorDecoupled = FALSE;
	mCursorLastEventDeltaX = 0;
	mCursorLastEventDeltaY = 0;
	mCursorIgnoreNextDelta = FALSE;
	mNeedsResize = FALSE;
	mOverrideAspectRatio = 0.f;
	mGrabbyKeyFlags = 0;
	mReallyCapturedCount = 0;
	mHaveInputFocus = -1;
	mIsMinimized = -1;
	mFSAASamples = fsaa_samples;

#if LL_X11
	mSDL_XWindowID = None;
	mSDL_Display = NULL;
#endif // LL_X11

#if LL_GTK
	// We MUST be the first to initialize GTK, i.e. we have to beat
	// our embedded Mozilla to the punch so that GTK doesn't get badly
	// initialized with a non-C locale and cause lots of serious random
	// weirdness.
	ll_try_gtk_init();
#endif // LL_GTK

	// Get the original aspect ratio of the main device.
	mOriginalAspectRatio = 1024.0 / 768.0;  // !!! *FIX: ? //(double)CGDisplayPixelsWide(mDisplay) / (double)CGDisplayPixelsHigh(mDisplay);

	if (title.empty())
		mWindowTitle = "SDL Window";  // *FIX: (???)
	else
		mWindowTitle = title;

	// Create the GL context and set it up for windowed or fullscreen, as appropriate.
	if(createContext(x, y, width, height, 32, fullscreen, disable_vsync))
	{
		gGLManager.initGL();

		//start with arrow cursor
		initCursors();
		setCursor( UI_CURSOR_ARROW );
	}

	stop_glerror();

	// Stash an object pointer for OSMessageBox()
	gWindowImplementation = this;

#if LL_X11
	mFlashing = FALSE;
#endif // LL_X11
}

static SDL_Surface *Load_BMP_Resource(const char *basename)
{
	const int PATH_BUFFER_SIZE=1000;
	char path_buffer[PATH_BUFFER_SIZE];	/* Flawfinder: ignore */
	
	// Figure out where our BMP is living on the disk
	snprintf(path_buffer, PATH_BUFFER_SIZE-1, "%s%sres-sdl%s%s",	
		 gDirUtilp->getAppRODataDir().c_str(),
		 gDirUtilp->getDirDelimiter().c_str(),
		 gDirUtilp->getDirDelimiter().c_str(),
		 basename);
	path_buffer[PATH_BUFFER_SIZE-1] = '\0';
	
	return SDL_LoadBMP(path_buffer);
}

#if LL_X11
// This is an XFree86/XOrg-specific hack for detecting the amount of Video RAM
// on this machine.  It works by searching /var/log/var/log/Xorg.?.log or
// /var/log/XFree86.?.log for a ': (VideoRAM|Memory): (%d+) kB' regex, where
// '?' is the X11 display number derived from $DISPLAY
static int x11_detect_VRAM_kb_fp(FILE *fp, const char *prefix_str)
{
	const int line_buf_size = 1000;
	char line_buf[line_buf_size];
	while (fgets(line_buf, line_buf_size, fp))
	{
		//lldebugs << "XLOG: " << line_buf << llendl;

		// Why the ad-hoc parser instead of using a regex?  Our
		// favourite regex implementation - libboost_regex - is
		// quite a heavy and troublesome dependency for the client, so
		// it seems a shame to introduce it for such a simple task.
		const char *part1_template = prefix_str;
		const char part2_template[] = " kB";
		char *part1 = strstr(line_buf, part1_template);
		if (part1) // found start of matching line
		{
			part1 = &part1[strlen(part1_template)]; // -> after
			char *part2 = strstr(part1, part2_template);
			if (part2) // found end of matching line
			{
				// now everything between part1 and part2 is
				// supposed to be numeric, describing the
				// number of kB of Video RAM supported
				int rtn = 0;
				for (; part1 < part2; ++part1)
				{
					if (*part1 < '0' || *part1 > '9')
					{
						// unexpected char, abort parse
						rtn = 0;
						break;
					}
					rtn *= 10;
					rtn += (*part1) - '0';
				}
				if (rtn > 0)
				{
					// got the kB number.  return it now.
					return rtn;
				}
			}
		}
	}
	return 0; // 'could not detect'
}

static int x11_detect_VRAM_kb()
{
#if LL_SOLARIS
#error Can this be done without an explicit architecture test, ie a test FOR xorg? Was followed by: && defined(__sparc)
      //  NOTE: there's no Xorg server on SPARC so just return 0
      //        and allow SDL to attempt to get the amount of VRAM
      return(0);
#else

	std::string x_log_location("/var/log/");
	std::string fname;
	int rtn = 0; // 'could not detect'
	int display_num = 0;
	FILE *fp;
	char *display_env = getenv("DISPLAY"); // e.g. :0 or :0.0 or :1.0 etc
	// parse DISPLAY number so we can go grab the right log file
	if (display_env[0] == ':' &&
	    display_env[1] >= '0' && display_env[1] <= '9')
	{
		display_num = display_env[1] - '0';
	}

	// *TODO: we could be smarter and see which of Xorg/XFree86 has the
	// freshest time-stamp.

	// Try Xorg log first
	fname = x_log_location;
	fname += "Xorg.";
	fname += ('0' + display_num);
	fname += ".log";
	fp = fopen(fname.c_str(), "r");
	if (fp)
	{
		llinfos << "Looking in " << fname
			<< " for VRAM info..." << llendl;
		rtn = x11_detect_VRAM_kb_fp(fp, ": VideoRAM: ");
		fclose(fp);
		if (0 == rtn)
		{
			fp = fopen(fname.c_str(), "r");
			if (fp)
			{
				rtn = x11_detect_VRAM_kb_fp(fp, ": Memory: ");
				fclose(fp);
			}
		}
	}
	else
	{
		llinfos << "Could not open " << fname
			<< " - skipped." << llendl;	
		// Try old XFree86 log otherwise
		fname = x_log_location;
		fname += "XFree86.";
		fname += ('0' + display_num);
		fname += ".log";
		fp = fopen(fname.c_str(), "r");
		if (fp)
		{
			llinfos << "Looking in " << fname
				<< " for VRAM info..." << llendl;
			rtn = x11_detect_VRAM_kb_fp(fp, ": VideoRAM: ");
			fclose(fp);
			if (0 == rtn)
			{
				fp = fopen(fname.c_str(), "r");
				if (fp)
				{
					rtn = x11_detect_VRAM_kb_fp(fp, ": Memory: ");
					fclose(fp);
				}
			}
		}
		else
		{
			llinfos << "Could not open " << fname
				<< " - skipped." << llendl;
		}
	}
	return rtn;
#endif // LL_SOLARIS
}
#endif // LL_X11

BOOL LLWindowSDL::createContext(int x, int y, int width, int height, int bits, BOOL fullscreen, BOOL disable_vsync)
{
	//bool			glneedsinit = false;
//    const char *gllibname = null;

	llinfos << "createContext, fullscreen=" << fullscreen <<
	    " size=" << width << "x" << height << llendl;

	// captures don't survive contexts
	mGrabbyKeyFlags = 0;
	mReallyCapturedCount = 0;
	
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		llinfos << "sdl_init() failed! " << SDL_GetError() << llendl;
		setupFailure("window creation error", "error", OSMB_OK);
		return false;
	}

	SDL_version c_sdl_version;
	SDL_VERSION(&c_sdl_version);
	llinfos << "Compiled against SDL "
		<< int(c_sdl_version.major) << "."
		<< int(c_sdl_version.minor) << "."
		<< int(c_sdl_version.patch) << llendl;
	const SDL_version *r_sdl_version;
	r_sdl_version = SDL_Linked_Version();
	llinfos << " Running against SDL "
		<< int(r_sdl_version->major) << "."
		<< int(r_sdl_version->minor) << "."
		<< int(r_sdl_version->patch) << llendl;

	const SDL_VideoInfo *videoInfo = SDL_GetVideoInfo( );
	if (!videoInfo)
	{
		llinfos << "SDL_GetVideoInfo() failed! " << SDL_GetError() << llendl;
		setupFailure("Window creation error", "Error", OSMB_OK);
		return FALSE;
	}

	SDL_EnableUNICODE(1);
	SDL_WM_SetCaption(mWindowTitle.c_str(), mWindowTitle.c_str());

	// Set the application icon.
	SDL_Surface *bmpsurface;
	bmpsurface = Load_BMP_Resource("ll_icon.BMP");
	if (bmpsurface)
	{
		// This attempts to give a black-keyed mask to the icon.
		SDL_SetColorKey(bmpsurface,
				SDL_SRCCOLORKEY,
				SDL_MapRGB(bmpsurface->format, 0,0,0) );
		SDL_WM_SetIcon(bmpsurface, NULL);
		// The SDL examples cheerfully avoid freeing the icon
		// surface, but I'm betting that's leaky.
		SDL_FreeSurface(bmpsurface);
		bmpsurface = NULL;
	}

	// note: these SetAttributes make Tom's 9600-on-AMD64 fail to
	// get a visual, but it's broken anyway when it does, and without
	// these SetAttributes we might easily get an avoidable substandard
	// visual to work with on most other machines.
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE,  8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
#if !LL_SOLARIS
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, (bits <= 16) ? 16 : 24);
	// We need stencil support for a few (minor) things.
	if (!getenv("LL_GL_NO_STENCIL"))
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
#else
	// NOTE- use smaller Z-buffer to enable more graphics cards
        //     - This should not affect better GPUs and has been proven
        //	 to provide 24-bit z-buffers when available.
	//
        // As the API states: 
	//
        // GLX_DEPTH_SIZE    Must be followed by a nonnegative
        //                   minimum size specification.  If this
        //                   value is zero, visuals with no depth
        //                   buffer are preferred.  Otherwise, the
        //                   largest available depth buffer of at
        //                   least the minimum size is preferred.

        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
#endif
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, (bits <= 16) ? 1 : 8);

        // *FIX: try to toggle vsync here?

	mFullscreen = fullscreen;
	was_fullscreen = fullscreen;

	int sdlflags = SDL_OPENGL | SDL_RESIZABLE | SDL_ANYFORMAT;

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	if (mFSAASamples > 0)
	{
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, mFSAASamples);
	}
	
    	mSDLFlags = sdlflags;

	if (mFullscreen)
	{
		llinfos << "createContext: setting up fullscreen " << width << "x" << height << llendl;

		// If the requested width or height is 0, find the best default for the monitor.
		if((width == 0) || (height == 0))
		{
			// Scan through the list of modes, looking for one which has:
			//		height between 700 and 800
			//		aspect ratio closest to the user's original mode
			S32 resolutionCount = 0;
			LLWindowResolution *resolutionList = getSupportedResolutions(resolutionCount);

			if(resolutionList != NULL)
			{
				F32 closestAspect = 0;
				U32 closestHeight = 0;
				U32 closestWidth = 0;
				int i;

				llinfos << "createContext: searching for a display mode, original aspect is " << mOriginalAspectRatio << llendl;

				for(i=0; i < resolutionCount; i++)
				{
					F32 aspect = (F32)resolutionList[i].mWidth / (F32)resolutionList[i].mHeight;

					llinfos << "createContext: width " << resolutionList[i].mWidth << " height " << resolutionList[i].mHeight << " aspect " << aspect << llendl;

					if( (resolutionList[i].mHeight >= 700) && (resolutionList[i].mHeight <= 800) &&
						(fabs(aspect - mOriginalAspectRatio) < fabs(closestAspect - mOriginalAspectRatio)))
					{
						llinfos << " (new closest mode) " << llendl;

						// This is the closest mode we've seen yet.
						closestWidth = resolutionList[i].mWidth;
						closestHeight = resolutionList[i].mHeight;
						closestAspect = aspect;
					}
				}

				width = closestWidth;
				height = closestHeight;
			}
		}

		if((width == 0) || (height == 0))
		{
			// Mode search failed for some reason.  Use the old-school default.
			width = 1024;
			height = 768;
		}

		mWindow = SDL_SetVideoMode(width, height, bits, sdlflags | SDL_FULLSCREEN);
		if (!mWindow && bits > 16)
		{
			SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
			mWindow = SDL_SetVideoMode(width, height, bits, sdlflags | SDL_FULLSCREEN);
		}

		if (mWindow)
		{
			mFullscreen = TRUE;
			was_fullscreen = TRUE;
			mFullscreenWidth   = mWindow->w;
			mFullscreenHeight  = mWindow->h;
			mFullscreenBits    = mWindow->format->BitsPerPixel;
			mFullscreenRefresh = -1;

			llinfos << "Running at " << mFullscreenWidth
				<< "x"   << mFullscreenHeight
				<< "x"   << mFullscreenBits
				<< " @ " << mFullscreenRefresh
				<< llendl;
		}
		else
		{
			llwarns << "createContext: fullscreen creation failure. SDL: " << SDL_GetError() << llendl;
			// No fullscreen support
			mFullscreen = FALSE;
			was_fullscreen = FALSE;
			mFullscreenWidth   = -1;
			mFullscreenHeight  = -1;
			mFullscreenBits    = -1;
			mFullscreenRefresh = -1;

			std::string error = llformat("Unable to run fullscreen at %d x %d.\nRunning in window.", width, height);	
			OSMessageBox(error, "Error", OSMB_OK);
		}
	}

	if(!mFullscreen && (mWindow == NULL))
	{
		if (width == 0)
		    width = 1024;
		if (height == 0)
		    width = 768;

		llinfos << "createContext: creating window " << width << "x" << height << "x" << bits << llendl;
		mWindow = SDL_SetVideoMode(width, height, bits, sdlflags);
		if (!mWindow && bits > 16)
		{
			SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
			mWindow = SDL_SetVideoMode(width, height, bits, sdlflags);
		}

		if (!mWindow)
		{
			llwarns << "createContext: window creation failure. SDL: " << SDL_GetError() << llendl;
			setupFailure("Window creation error", "Error", OSMB_OK);
			return FALSE;
		}
	} else if (!mFullscreen && (mWindow != NULL))
	{
		llinfos << "createContext: SKIPPING - !fullscreen, but +mWindow " << width << "x" << height << "x" << bits << llendl;
	}
	
	// Detect video memory size.
# if LL_X11
	gGLManager.mVRAM = x11_detect_VRAM_kb() / 1024;
	if (gGLManager.mVRAM != 0)
	{
		llinfos << "X11 log-parser detected " << gGLManager.mVRAM << "MB VRAM." << llendl;
	} else
# endif // LL_X11
	{
		// fallback to letting SDL detect VRAM.
		// note: I've not seen SDL's detection ever actually find
		// VRAM != 0, but if SDL *does* detect it then that's a bonus.
		gGLManager.mVRAM = videoInfo->video_mem / 1024;
		if (gGLManager.mVRAM != 0)
		{
			llinfos << "SDL detected " << gGLManager.mVRAM << "MB VRAM." << llendl;
		}
	}
	// If VRAM is not detected, that is handled later

	// *TODO: Now would be an appropriate time to check for some
	// explicitly unsupported cards.
	//const char* RENDERER = (const char*) glGetString(GL_RENDERER);

	GLint depthBits, stencilBits, redBits, greenBits, blueBits, alphaBits;

	glGetIntegerv(GL_RED_BITS, &redBits);
	glGetIntegerv(GL_GREEN_BITS, &greenBits);
	glGetIntegerv(GL_BLUE_BITS, &blueBits);
	glGetIntegerv(GL_ALPHA_BITS, &alphaBits);
	glGetIntegerv(GL_DEPTH_BITS, &depthBits);
	glGetIntegerv(GL_STENCIL_BITS, &stencilBits);
	
	llinfos << "GL buffer:" << llendl
        llinfos << "  Red Bits " << S32(redBits) << llendl
        llinfos << "  Green Bits " << S32(greenBits) << llendl
        llinfos << "  Blue Bits " << S32(blueBits) << llendl
	llinfos	<< "  Alpha Bits " << S32(alphaBits) << llendl
	llinfos	<< "  Depth Bits " << S32(depthBits) << llendl
	llinfos	<< "  Stencil Bits " << S32(stencilBits) << llendl;

	GLint colorBits = redBits + greenBits + blueBits + alphaBits;
	// fixme: actually, it's REALLY important for picking that we get at
	// least 8 bits each of red,green,blue.  Alpha we can be a bit more
	// relaxed about if we have to.
#if LL_SOLARIS
#error && defined(__sparc)
	if(colorBits < 24)		//HACK:  on SPARC allow 24-bit color
#else
	if (colorBits < 32)
#endif
	{
		close();
		setupFailure(
#if LL_SOLARIS
#error && defined(__sparc)
			"Second Life requires at least 24-bit color on SPARC to run in a window.\n"
			"Please use fbconfig to set your default color depth to 24 bits.\n"
			"You may also need to adjust the X11 setting in SMF.  To do so use\n"
			"  'svccfg -s svc:/application/x11/x11-server setprop options/default_depth=24'\n"
#else
			"Second Life requires True Color (32-bit) to run in a window.\n"
			"Please go to Control Panels -> Display -> Settings and\n"
			"set the screen to 32-bit color.\n"
#endif
			"Alternately, if you choose to run fullscreen, Second Life\n"
			"will automatically adjust the screen each time it runs.",
			"Error",
			OSMB_OK);
		return FALSE;
	}

#if 0  // *FIX: we're going to brave it for now...
	if (alphaBits < 8)
	{
		close();
		setupFailure(
			"Second Life is unable to run because it can't get an 8 bit alpha\n"
			"channel.  Usually this is due to video card driver issues.\n"
			"Please make sure you have the latest video card drivers installed.\n"
			"Also be sure your monitor is set to True Color (32-bit) in\n"
			"Control Panels -> Display -> Settings.\n"
			"If you continue to receive this message, contact customer service.",
			"Error",
			OSMB_OK);
		return FALSE;
	}
#endif

#if LL_X11
	init_x11clipboard();
#endif // LL_X11

	//make sure multisampling is disabled by default
	glDisable(GL_MULTISAMPLE_ARB);
	
	// We need to do this here, once video is init'd
	if (-1 == SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,
				      SDL_DEFAULT_REPEAT_INTERVAL))
	    llwarns << "Couldn't enable key-repeat: " << SDL_GetError() <<llendl;

	// Don't need to get the current gamma, since there's a call that restores it to the system defaults.
	return TRUE;
}


// changing fullscreen resolution, or switching between windowed and fullscreen mode.
BOOL LLWindowSDL::switchContext(BOOL fullscreen, const LLCoordScreen &size, BOOL disable_vsync, const LLCoordScreen * const posp)
{
	const BOOL needsRebuild = TRUE;  // Just nuke the context and start over.
	BOOL result = true;

	llinfos << "switchContext, fullscreen=" << fullscreen << llendl;
	stop_glerror();
	if(needsRebuild)
	{
		destroyContext();
		result = createContext(0, 0, size.mX, size.mY, 0, fullscreen, disable_vsync);
		if (result)
		{
			gGLManager.initGL();

			//start with arrow cursor
			initCursors();
			setCursor( UI_CURSOR_ARROW );
		}
	}

	stop_glerror();

	return result;
}

void LLWindowSDL::destroyContext()
{
	llinfos << "destroyContext begins" << llendl;
#if LL_X11
	quit_x11clipboard();
#endif // LL_X11

	// Clean up remaining GL state before blowing away window
	llinfos << "shutdownGL begins" << llendl;
	gGLManager.shutdownGL();
	llinfos << "SDL_QuitSS/VID begins" << llendl;
	SDL_QuitSubSystem(SDL_INIT_VIDEO);  // *FIX: this might be risky...

	mWindow = NULL;
}

LLWindowSDL::~LLWindowSDL()
{
	quitCursors();
	destroyContext();

	if(mSupportedResolutions != NULL)
	{
		delete []mSupportedResolutions;
	}

	gWindowImplementation = NULL;
}


void LLWindowSDL::show()
{
    // *FIX: What to do with SDL?
}

void LLWindowSDL::hide()
{
    // *FIX: What to do with SDL?
}

void LLWindowSDL::minimize()
{
    // *FIX: What to do with SDL?
}

void LLWindowSDL::restore()
{
    // *FIX: What to do with SDL?
}


// close() destroys all OS-specific code associated with a window.
// Usually called from LLWindowManager::destroyWindow()
void LLWindowSDL::close()
{
	// Is window is already closed?
	//	if (!mWindow)
	//	{
	//		return;
	//	}

	// Make sure cursor is visible and we haven't mangled the clipping state.
	setMouseClipping(FALSE);
	showCursor();

	destroyContext();
}

BOOL LLWindowSDL::isValid()
{
	return (mWindow != NULL);
}

BOOL LLWindowSDL::getVisible()
{
	BOOL result = FALSE;

    // *FIX: This isn't really right...
	// Then what is?
	if (mWindow)
	{
		result = TRUE;
	}

	return(result);
}

BOOL LLWindowSDL::getMinimized()
{
	BOOL result = FALSE;

	if (mWindow && (1 == mIsMinimized))
	{
		result = TRUE;
	}
	return(result);
}

BOOL LLWindowSDL::getMaximized()
{
	BOOL result = FALSE;

	if (mWindow)
	{
		// TODO
	}

	return(result);
}

BOOL LLWindowSDL::maximize()
{
	// TODO
	return FALSE;
}

BOOL LLWindowSDL::getFullscreen()
{
	return mFullscreen;
}

BOOL LLWindowSDL::getPosition(LLCoordScreen *position)
{
    // *FIX: can anything be done with this?
	position->mX = 0;
	position->mY = 0;
    return TRUE;
}

BOOL LLWindowSDL::getSize(LLCoordScreen *size)
{
    if (mWindow)
    {
        size->mX = mWindow->w;
        size->mY = mWindow->h;
	return (TRUE);
    }

    llwarns << "LLWindowSDL::getPosition(): no window and not fullscreen!" << llendl;
    return (FALSE);
}

BOOL LLWindowSDL::getSize(LLCoordWindow *size)
{
    if (mWindow)
    {
        size->mX = mWindow->w;
        size->mY = mWindow->h;
	return (TRUE);
    }

    llwarns << "LLWindowSDL::getPosition(): no window and not fullscreen!" << llendl;
    return (FALSE);
}

BOOL LLWindowSDL::setPosition(const LLCoordScreen position)
{
	if(mWindow)
	{
        // *FIX: (???)
		//MacMoveWindow(mWindow, position.mX, position.mY, false);
	}

	return TRUE;
}

BOOL LLWindowSDL::setSize(const LLCoordScreen size)
{
	if(mWindow)
	{
        // *FIX: (???)
		//SizeWindow(mWindow, size.mX, size.mY, true);
	}

	return TRUE;
}

void LLWindowSDL::swapBuffers()
{
	if (mWindow)
		SDL_GL_SwapBuffers();
}

U32 LLWindowSDL::getFSAASamples()
{
	return mFSAASamples;
}

void LLWindowSDL::setFSAASamples(const U32 samples)
{
	mFSAASamples = samples;
}

F32 LLWindowSDL::getGamma()
{
	return 1/mGamma;
}

BOOL LLWindowSDL::restoreGamma()
{
	//CGDisplayRestoreColorSyncSettings();
    SDL_SetGamma(1.0f, 1.0f, 1.0f);
	return true;
}

BOOL LLWindowSDL::setGamma(const F32 gamma)
{
	mGamma = gamma;
	if (mGamma == 0) mGamma = 0.1f;
	mGamma = 1/mGamma;
	SDL_SetGamma(mGamma, mGamma, mGamma);
	return true;
}

BOOL LLWindowSDL::isCursorHidden()
{
	return mCursorHidden;
}



// Constrains the mouse to the window.
void LLWindowSDL::setMouseClipping( BOOL b )
{
	//llinfos << "LLWindowSDL::setMouseClipping " << b << llendl;
	// Just stash the requested state.  We'll simulate this when the cursor is hidden by decoupling.
	mIsMouseClipping = b;
    //SDL_WM_GrabInput(b ? SDL_GRAB_ON : SDL_GRAB_OFF);
	adjustCursorDecouple();
}

BOOL LLWindowSDL::setCursorPosition(const LLCoordWindow position)
{
	BOOL result = TRUE;
	LLCoordScreen screen_pos;

	if (!convertCoords(position, &screen_pos))
	{
		return FALSE;
	}

	//llinfos << "setCursorPosition(" << screen_pos.mX << ", " << screen_pos.mY << ")" << llendl;

    SDL_WarpMouse(screen_pos.mX, screen_pos.mY);

	// Under certain circumstances, this will trigger us to decouple the cursor.
	adjustCursorDecouple(true);

	return result;
}

BOOL LLWindowSDL::getCursorPosition(LLCoordWindow *position)
{
	//Point cursor_point;
	LLCoordScreen screen_pos;

	//GetMouse(&cursor_point);
    int x, y;
    SDL_GetMouseState(&x, &y);

	screen_pos.mX = x;
	screen_pos.mY = y;

	return convertCoords(screen_pos, position);
}

void LLWindowSDL::adjustCursorDecouple(bool warpingMouse)
{
	if(mIsMouseClipping && mCursorHidden)
	{
		if(warpingMouse)
		{
			// The cursor should be decoupled.  Make sure it is.
			if(!mCursorDecoupled)
			{
				//			llinfos << "adjustCursorDecouple: decoupling cursor" << llendl;
				//CGAssociateMouseAndMouseCursorPosition(false);
				mCursorDecoupled = true;
				mCursorIgnoreNextDelta = TRUE;
			}
		}
	}
	else
	{
		// The cursor should not be decoupled.  Make sure it isn't.
		if(mCursorDecoupled)
		{
			//			llinfos << "adjustCursorDecouple: recoupling cursor" << llendl;
			//CGAssociateMouseAndMouseCursorPosition(true);
			mCursorDecoupled = false;
		}
	}
}

F32 LLWindowSDL::getNativeAspectRatio()
{
#if 0
	// RN: this hack presumes that the largest supported resolution is monitor-limited
	// and that pixels in that mode are square, therefore defining the native aspect ratio
	// of the monitor...this seems to work to a close approximation for most CRTs/LCDs
	S32 num_resolutions;
	LLWindowResolution* resolutions = getSupportedResolutions(num_resolutions);


	return ((F32)resolutions[num_resolutions - 1].mWidth / (F32)resolutions[num_resolutions - 1].mHeight);
	//rn: AC
#endif

	// MBW -- there are a couple of bad assumptions here.  One is that the display list won't include
	//		ridiculous resolutions nobody would ever use.  The other is that the list is in order.

	// New assumptions:
	// - pixels are square (the only reasonable choice, really)
	// - The user runs their display at a native resolution, so the resolution of the display
	//    when the app is launched has an aspect ratio that matches the monitor.

	//RN: actually, the assumption that there are no ridiculous resolutions (above the display's native capabilities) has 
	// been born out in my experience.  
	// Pixels are often not square (just ask the people who run their LCDs at 1024x768 or 800x600 when running fullscreen, like me)
	// The ordering of display list is a blind assumption though, so we should check for max values
	// Things might be different on the Mac though, so I'll defer to MBW

	// The constructor for this class grabs the aspect ratio of the monitor before doing any resolution
	// switching, and stashes it in mOriginalAspectRatio.  Here, we just return it.

	if (mOverrideAspectRatio > 0.f)
	{
		return mOverrideAspectRatio;
	}

	return mOriginalAspectRatio;
}

F32 LLWindowSDL::getPixelAspectRatio()
{
	F32 pixel_aspect = 1.f;
	if (getFullscreen())
	{
		LLCoordScreen screen_size;
		if (getSize(&screen_size))
		{
			pixel_aspect = getNativeAspectRatio() * (F32)screen_size.mY / (F32)screen_size.mX;
		}
	}

	return pixel_aspect;
}


// some of this stuff is to support 'temporarily windowed' mode so that
// dialogs are still usable in fullscreen.  HOWEVER! - it's not enabled/working
// yet.
static LLCoordScreen old_size;
static BOOL old_fullscreen;
void LLWindowSDL::beforeDialog()
{
	llinfos << "LLWindowSDL::beforeDialog()" << llendl;

	if (SDLReallyCaptureInput(FALSE) // must ungrab input so popup works!
	    && getSize(&old_size))
	{
		old_fullscreen = was_fullscreen;
		
		if (old_fullscreen)
		{
			// NOT YET WORKING
			//switchContext(FALSE, old_size, TRUE);
		}
	}

#if LL_X11
	if (mSDL_Display)
	{
		// Everything that we/SDL asked for should happen before we
		// potentially hand control over to GTK.
		maybe_lock_display();
		XSync(mSDL_Display, False);
		maybe_unlock_display();
	}
#endif // LL_X11

#if LL_GTK
	// this is a good time to grab some GTK version information for
	// diagnostics, if not already done.
	ll_try_gtk_init();
#endif // LL_GTK

	maybe_lock_display();
}

void LLWindowSDL::afterDialog()
{
	llinfos << "LLWindowSDL::afterDialog()" << llendl;

	maybe_unlock_display();

	if (old_fullscreen && !was_fullscreen)
	{
		// *FIX: NOT YET WORKING (see below)
		//switchContext(TRUE, old_size, TRUE);
	}
	// *FIX: we need to restore the GL context using
	// LLViewerWindow::restoreGL() - but how??
}


#if LL_X11
// set/reset the XWMHints flag for 'urgency' that usually makes the icon flash
void LLWindowSDL::x11_set_urgent(BOOL urgent)
{
	if (mSDL_Display && !mFullscreen)
	{
		XWMHints *wm_hints;
		
		llinfos << "X11 hint for urgency, " << urgent << llendl;

		maybe_lock_display();
		wm_hints = XGetWMHints(mSDL_Display, mSDL_XWindowID);
		if (!wm_hints)
			wm_hints = XAllocWMHints();

		if (urgent)
			wm_hints->flags |= XUrgencyHint;
		else
			wm_hints->flags &= ~XUrgencyHint;

		XSetWMHints(mSDL_Display, mSDL_XWindowID, wm_hints);
		XFree(wm_hints);
		XSync(mSDL_Display, False);
		maybe_unlock_display();
	}
}
#endif // LL_X11

void LLWindowSDL::flashIcon(F32 seconds)
{
#if !LL_X11
	llinfos << "Stub LLWindowSDL::flashIcon(" << seconds << ")" << llendl;
#else	
	llinfos << "X11 LLWindowSDL::flashIcon(" << seconds << ")" << llendl;
	
	F32 remaining_time = mFlashTimer.getRemainingTimeF32();
	if (remaining_time < seconds)
		remaining_time = seconds;
	mFlashTimer.reset();
	mFlashTimer.setTimerExpirySec(remaining_time);

	x11_set_urgent(TRUE);
	mFlashing = TRUE;
#endif // LL_X11
}

#if LL_X11
/* Lots of low-level X11 stuff to handle X11 copy-and-paste */

/* Our X11 clipboard support is a bit bizarre in various
   organically-grown ways.  Ideally it should be fixed to do
   real string-type negotiation (this would make pasting to
   xterm faster and pasting to UTF-8 emacs work properly), but
   right now it has the rare and desirable trait of being
   generally stable and working. */

typedef Atom x11clipboard_type;

/* PRIMARY and CLIPBOARD are the two main kinds of
   X11 clipboard.  A third are the CUT_BUFFERs which an
   obsolete holdover from X10 days and use a quite orthogonal
   mechanism.  CLIPBOARD is the type whose design most
   closely matches SL's own win32-alike explicit copy-and-paste
   paradigm.

   Pragmatically we support all three to varying degrees.  When
   we paste into SL, it is strictly from CLIPBOARD.  When we copy,
   we support (to as full an extent as the clipboard content type
   allows) CLIPBOARD, PRIMARY, and CUT_BUFFER0.
 */
static x11clipboard_type get_x11_readwrite_clipboard_type(void)
{
	return XInternAtom(LLWindowSDL::get_SDL_Display(), "CLIPBOARD", False);
}

static x11clipboard_type get_x11_write_clipboard_type(void)
{
	return XA_PRIMARY;
}

/* This is where our own private cutbuffer goes - we don't use
   a regular cutbuffer (XA_CUT_BUFFER0 etc) for intermediate
   storage because their use isn't really defined for holding UTF8. */
static x11clipboard_type get_x11_cutbuffer_clipboard_type(void)
{
	return XInternAtom(LLWindowSDL::get_SDL_Display(), "SECONDLIFE_CUTBUFFER", False);
}

/* Some X11 atom-generators */
static Atom get_x11_targets_atom(void)
{
	return XInternAtom(LLWindowSDL::get_SDL_Display(), "TARGETS", False);
}

static Atom get_x11_text_atom(void)
{
	return XInternAtom(LLWindowSDL::get_SDL_Display(), "TEXT", False);
}

/* These defines, and convert_data/convert_x11clipboard,
   mostly exist to support non-text or unusually-encoded
   clipboard data, which we don't really have a need for at
   the moment. */
#define SDLCLIPTYPE(A, B, C, D) (int)(((A)<<24)|((B)<<16)|((C)<<8)|((D)<<0))
#define FORMAT_PREFIX	"SECONDLIFE_x11clipboard_0x"

static
x11clipboard_type convert_format(int type)
{
	if (!gWindowImplementation)
	{
		llwarns << "!gWindowImplementation in convert_format()"
			<< llendl;
		return XA_STRING;
	}

	switch (type)
	{
	case SDLCLIPTYPE('T', 'E', 'X', 'T'):
		// old-style X11 clipboard, strictly only ISO 8859-1 encoding
		return XA_STRING;
	case SDLCLIPTYPE('U', 'T', 'F', '8'):
		// newer de-facto UTF8 clipboard atom
		return XInternAtom(gWindowImplementation->mSDL_Display,
				   "UTF8_STRING", False);
	default:
	{
		/* completely arbitrary clipboard types... we don't actually use
		these right now, and support is skeletal. */
		char format[sizeof(FORMAT_PREFIX)+8+1];	/* Flawfinder: ignore */

		snprintf(format, sizeof(format), "%s%08lx", FORMAT_PREFIX, (unsigned long)type);
		return XInternAtom(gWindowImplementation->mSDL_Display,
				   format, False);
	}
    }
}

/* convert platform string to x11 clipboard format.  for our
   purposes this is pretty trivial right now. */
static int
convert_data(int type, char *dst, const char *src, int srclen)
{
	int dstlen;

	dstlen = 0;
	switch (type)
	{
	case SDLCLIPTYPE('T', 'E', 'X', 'T'):
	case SDLCLIPTYPE('U', 'T', 'F', '8'):
		if (src == NULL)
		{
			break;
		}
		if ( srclen == 0 )
			srclen = strlen(src);	/* Flawfinder: ignore */
		
		dstlen = srclen + 1;
		
		if ( dst ) // assume caller made it big enough by asking us
		{
			memcpy(dst, src, srclen);	/* Flawfinder: ignore */
			dst[srclen] = '\0';
		}
		break;
		
	default:
		llwarns << "convert_data: Unknown medium type" << llendl;
		break;
	}
	return(dstlen);
}

/* Convert x11clipboard data to platform string.  This too is
   pretty trivial for our needs right now, and just about identical
   to above. */
static int
convert_x11clipboard(int type, char *dst, const char *src, int srclen)
{
	int dstlen;

	dstlen = 0;
	switch (type)
	{
	case SDLCLIPTYPE('U', 'T', 'F', '8'):
	case SDLCLIPTYPE('T', 'E', 'X', 'T'):
		if (src == NULL)
		{
			break;
		}
		if ( srclen == 0 )
			srclen = strlen(src);	/* Flawfinder: ignore */
		
		dstlen = srclen + 1;
		
		if ( dst ) // assume caller made it big enough by asking us
		{
			memcpy(dst, src, srclen);	/* Flawfinder: ignore */
			dst[srclen] = '\0';
		}
		break;
		
	default:
		llwarns << "convert_x11clipboard: Unknown medium type" << llendl;
		break;
	}
	return dstlen;
}

int
LLWindowSDL::is_empty_x11clipboard(void)
{
	int retval;

	maybe_lock_display();
	retval = ( XGetSelectionOwner(mSDL_Display, get_x11_readwrite_clipboard_type()) == None );
	maybe_unlock_display();

	return(retval);
}

void
LLWindowSDL::put_x11clipboard(int type, int srclen, const char *src)
{
	x11clipboard_type format;
	int dstlen;
	char *dst;

	format = convert_format(type);
	dstlen = convert_data(type, NULL, src, srclen);

	dst = (char *)malloc(dstlen);
	if ( dst != NULL )
	{
		maybe_lock_display();
		Window root = DefaultRootWindow(mSDL_Display);
		convert_data(type, dst, src, srclen);
		// Cutbuffers are only allowed to have STRING atom types,
		// but Emacs puts UTF8 inside them anyway.  We cautiously
		// don't.
		if (type == SDLCLIPTYPE('T','E','X','T'))
		{
			// dstlen-1 so we don't include the trailing \0
			llinfos << "X11: Populating cutbuffer." <<llendl;
			XChangeProperty(mSDL_Display, root,
					XA_CUT_BUFFER0, XA_STRING, 8, PropModeReplace,
					(unsigned char*)dst, dstlen-1);
		} else {
			// Should we clear the cutbuffer if we can't put the selection in
			// it because it's a UTF8 selection?  Eh, no great reason I think.
			//XDeleteProperty(SDL_Display, root, XA_CUT_BUFFER0);
		}
		// Private cutbuffer of an appropriate type.
		XChangeProperty(mSDL_Display, root,
				get_x11_cutbuffer_clipboard_type(), format, 8, PropModeReplace,
				(unsigned char*)dst, dstlen-1);
		free(dst);
		
		/* Claim ownership of both PRIMARY and CLIPBOARD */
		XSetSelectionOwner(mSDL_Display, get_x11_readwrite_clipboard_type(),
				   mSDL_XWindowID, CurrentTime);
		XSetSelectionOwner(mSDL_Display, get_x11_write_clipboard_type(),
				   mSDL_XWindowID, CurrentTime);
		
		maybe_unlock_display();
	}
}

void
LLWindowSDL::get_x11clipboard(int type, int *dstlen, char **dst)
{
	x11clipboard_type format;
	
	*dstlen = 0;
	format = convert_format(type);

	Window owner;
	Atom selection;
	Atom seln_type;
	int seln_format;
	unsigned long nbytes;
	unsigned long overflow;
	char *src;
	
	maybe_lock_display();
	owner = XGetSelectionOwner(mSDL_Display, get_x11_readwrite_clipboard_type());
	maybe_unlock_display();
	if (owner == None)
	{
		// Fall right back to ancient X10 cut-buffers
		owner = DefaultRootWindow(mSDL_Display);
		selection = XA_CUT_BUFFER0;
	} else if (owner == mSDL_XWindowID)
	{
		// Use our own uncooked opaque string property
		owner = DefaultRootWindow(mSDL_Display);
		selection = get_x11_cutbuffer_clipboard_type();
	}
	else
	{
		// Use full-on X11-style clipboard negotiation with the owning app
		int selection_response = 0;
		SDL_Event event;
		
		owner = mSDL_XWindowID;
		maybe_lock_display();
		selection = XInternAtom(mSDL_Display, "SDL_SELECTION", False);
		XConvertSelection(mSDL_Display, get_x11_readwrite_clipboard_type(), format,
				  selection, owner, CurrentTime);
		maybe_unlock_display();
		llinfos << "X11: Waiting for clipboard to arrive." <<llendl;
		while ( ! selection_response )
		{
			// Only look for SYSWMEVENTs, or we may lose keypresses
			// etc.
			SDL_PumpEvents();
			if (1 == SDL_PeepEvents(&event, 1, SDL_GETEVENT,
						SDL_SYSWMEVENTMASK) )
			{
				if ( event.type == SDL_SYSWMEVENT )
				{
					XEvent xevent =
						event.syswm.msg->event.xevent;
				
					if ( (xevent.type == SelectionNotify)&&
					     (xevent.xselection.requestor == owner) )
						selection_response = 1;
				}
			} else {
				llinfos << "X11: Waiting for SYSWM event..." <<  llendl;
			}
		}
		llinfos << "X11: Clipboard arrived." <<llendl;
	}

	maybe_lock_display();
	if ( XGetWindowProperty(mSDL_Display, owner, selection, 0, INT_MAX/4,
				False, format, &seln_type, &seln_format,
				&nbytes, &overflow, (unsigned char **)&src) == Success )
	{
		if ( seln_type == format )
		{
			*dstlen = convert_x11clipboard(type, NULL, src, nbytes);
			*dst = (char *)realloc(*dst, *dstlen);
			if ( *dst == NULL )
				*dstlen = 0;
			else
				convert_x11clipboard(type, *dst, src, nbytes);
		}
		XFree(src);
	}
	maybe_unlock_display();
}

int clipboard_filter_callback(const SDL_Event *event)
{
	/* Post all non-window manager specific events */
	if ( event->type != SDL_SYSWMEVENT )
	{
		return(1);
	}

	/* Handle window-manager specific clipboard events */
	switch (event->syswm.msg->event.xevent.type) {
	/* Copy the selection from SECONDLIFE_CUTBUFFER to the requested property */
	case SelectionRequest: {
		XSelectionRequestEvent *req;
		XEvent sevent;
		int seln_format;
		unsigned long nbytes;
		unsigned long overflow;
		unsigned char *seln_data;

		req = &event->syswm.msg->event.xevent.xselectionrequest;
		sevent.xselection.type = SelectionNotify;
		sevent.xselection.display = req->display;
		sevent.xselection.selection = req->selection;
		sevent.xselection.target = None;
		sevent.xselection.property = None;
		sevent.xselection.requestor = req->requestor;
		sevent.xselection.time = req->time;
		if ( XGetWindowProperty(LLWindowSDL::get_SDL_Display(), DefaultRootWindow(LLWindowSDL::get_SDL_Display()),
					get_x11_cutbuffer_clipboard_type(), 0, INT_MAX/4, False, req->target,
					&sevent.xselection.target, &seln_format,
					&nbytes, &overflow, &seln_data) == Success )
		{
			if ( sevent.xselection.target == req->target)
			{
				if ( sevent.xselection.target == XA_STRING ||
				     sevent.xselection.target ==
				     convert_format(SDLCLIPTYPE('U','T','F','8')) )
				{
					if ( seln_data[nbytes-1] == '\0' )
						--nbytes;
				}
				XChangeProperty(LLWindowSDL::get_SDL_Display(), req->requestor, req->property,
						req->target, seln_format, PropModeReplace,
						seln_data, nbytes);
				sevent.xselection.property = req->property;
			} else if (get_x11_targets_atom() == req->target) {
				/* only advertise what we currently support */
				const int num_supported = 3;
				Atom supported[num_supported] = {
					XA_STRING, // will be over-written below
					get_x11_text_atom(),
					get_x11_targets_atom()
				};
				supported[0] = sevent.xselection.target;
				XChangeProperty(LLWindowSDL::get_SDL_Display(), req->requestor,
						req->property, XA_ATOM, 32, PropModeReplace,
						(unsigned char*)supported,
						num_supported);
				sevent.xselection.property = req->property;
				llinfos << "Clipboard: An app asked us what selections format we offer." << llendl;
			} else {
				llinfos << "Clipboard: An app requested an unsupported selection format " << req->target << ", we have " << sevent.xselection.target << llendl;
			    sevent.xselection.target = None;
			}
			XFree(seln_data);
		}
		int sendret =
			XSendEvent(LLWindowSDL::get_SDL_Display(),req->requestor,False,0,&sevent);
		if ((sendret==BadValue) || (sendret==BadWindow))
			llwarns << "Clipboard SendEvent failed" << llendl;
		XSync(LLWindowSDL::get_SDL_Display(), False);
	}
		break;
	}
	
	/* Post the event for X11 clipboard reading above */
	return(1);
}

int
LLWindowSDL::init_x11clipboard(void)
{
	SDL_SysWMinfo info;
	int retval;

	/* Grab the window manager specific information */
	retval = -1;
	SDL_SetError("SDL is not running on known window manager");

	SDL_VERSION(&info.version);
	if ( SDL_GetWMInfo(&info) )
	{
		/* Save the information for later use */
		if ( info.subsystem == SDL_SYSWM_X11 )
		{
			mSDL_Display = info.info.x11.display;
			mSDL_XWindowID = info.info.x11.wmwindow;
			Lock_Display = info.info.x11.lock_func;
			Unlock_Display = info.info.x11.unlock_func;
			
			/* Enable the special window hook events */
			SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
			SDL_SetEventFilter(clipboard_filter_callback);
			
			retval = 0;
		}
		else
		{
			SDL_SetError("SDL is not running on X11");
		}
	}
	return(retval);
}

void
LLWindowSDL::quit_x11clipboard(void)
{
	mSDL_Display = NULL;
	mSDL_XWindowID = None;
	Lock_Display = NULL;
	Unlock_Display = NULL;

	SDL_SetEventFilter(NULL); // Stop custom event filtering
}

/************************************************/

BOOL LLWindowSDL::isClipboardTextAvailable()
{
	return !is_empty_x11clipboard();
}

BOOL LLWindowSDL::pasteTextFromClipboard(LLWString &dst)
{
	int cliplen; // seems 1 or 2 bytes longer than expected
	char *cliptext = NULL;
	get_x11clipboard(SDLCLIPTYPE('U','T','F','8'), &cliplen, &cliptext);
	if (cliptext)
	{
		llinfos << "X11: Got UTF8 clipboard text." << llendl;
		// at some future time we can use cliplen instead of relying on \0,
		// if we ever grok non-ascii, non-utf8 encodings on the clipboard.
		std::string clip_str(cliptext);
		// we can't necessarily trust the incoming text to be valid UTF-8,
		// but utf8str_to_wstring() seems to do an appropriate level of
		// validation for avoiding over-reads.
		dst = utf8str_to_wstring(clip_str);
		/*llinfos << "X11 pasteTextFromClipboard: cliplen=" << cliplen <<
			" strlen(cliptext)=" << strlen(cliptext) <<
			" clip_str.length()=" << clip_str.length() <<
			" dst.length()=" << dst.length() <<
			llendl;*/
		free(cliptext);
		return TRUE; // success
	}
	get_x11clipboard(SDLCLIPTYPE('T','E','X','T'), &cliplen, &cliptext);
	if (cliptext)
	{
		llinfos << "X11: Got ISO 8859-1 clipboard text." << llendl;
		std::string clip_str(cliptext);
		std::string utf8_str = rawstr_to_utf8(clip_str);
		dst = utf8str_to_wstring(utf8_str);
		free(cliptext);
	}
	return FALSE; // failure
}

BOOL LLWindowSDL::copyTextToClipboard(const LLWString &s)
{
	std::string utf8text = wstring_to_utf8str(s);
	const char* cstr = utf8text.c_str();
	if (cstr == NULL)
	{
		return FALSE;
	}
	int cstrlen = strlen(cstr);	/* Flawfinder: ignore */
	int i;
	for (i=0; i<cstrlen; ++i)
	{
		if (0x80 & (unsigned char)cstr[i])
		{
			// Found an 8-bit character; use new-style UTF8 clipboard
			llinfos << "X11: UTF8 copyTextToClipboard" << llendl;
			put_x11clipboard(SDLCLIPTYPE('U','T','F','8'), cstrlen, cstr);
			return TRUE;
		}
	}
	// Didn't find any 8-bit characters; use old-style ISO 8859-1 clipboard
	llinfos << "X11: ISO 8859-1 copyTextToClipboard" << llendl;
	put_x11clipboard(SDLCLIPTYPE('T','E','X','T'), cstrlen, cstr);
	return TRUE;
}
#else

BOOL LLWindowSDL::isClipboardTextAvailable()
{
	return FALSE; // unsupported
}

BOOL LLWindowSDL::pasteTextFromClipboard(LLWString &dst)
{
	return FALSE; // unsupported
}

BOOL LLWindowSDL::copyTextToClipboard(const LLWString &s)
{
	return FALSE;  // unsupported
}
#endif // LL_X11

LLWindow::LLWindowResolution* LLWindowSDL::getSupportedResolutions(S32 &num_resolutions)
{
	if (!mSupportedResolutions)
	{
		mSupportedResolutions = new LLWindowResolution[MAX_NUM_RESOLUTIONS];
		mNumSupportedResolutions = 0;

        SDL_Rect **modes = SDL_ListModes(NULL, SDL_OPENGL | SDL_FULLSCREEN);
        if ( (modes != NULL) && (modes != ((SDL_Rect **) -1)) )
        {
            int count = 0;
            while (*modes && count<MAX_NUM_RESOLUTIONS)  // they're sorted biggest to smallest, so find end...
            {
                modes++;
                count++;
            }

            while (count--)
            {
                modes--;
                SDL_Rect *r = *modes;
                int w = r->w;
                int h = r->h;
                if ((w >= 800) && (h >= 600))
                {
                    // make sure we don't add the same resolution multiple times!
                    if ( (mNumSupportedResolutions == 0) ||
                         ((mSupportedResolutions[mNumSupportedResolutions-1].mWidth != w) &&
                          (mSupportedResolutions[mNumSupportedResolutions-1].mHeight != h)) )
                    {
                        mSupportedResolutions[mNumSupportedResolutions].mWidth = w;
                        mSupportedResolutions[mNumSupportedResolutions].mHeight = h;
                        mNumSupportedResolutions++;
                    }
                }
            }
        }
	}

	num_resolutions = mNumSupportedResolutions;
	return mSupportedResolutions;
}

BOOL LLWindowSDL::convertCoords(LLCoordGL from, LLCoordWindow *to)
{
    if (!to)
        return FALSE;

	to->mX = from.mX;
	to->mY = mWindow->h - from.mY - 1;

	return TRUE;
}

BOOL LLWindowSDL::convertCoords(LLCoordWindow from, LLCoordGL* to)
{
    if (!to)
        return FALSE;

	to->mX = from.mX;
	to->mY = mWindow->h - from.mY - 1;

	return TRUE;
}

BOOL LLWindowSDL::convertCoords(LLCoordScreen from, LLCoordWindow* to)
{
    if (!to)
		return FALSE;

	// In the fullscreen case, window and screen coordinates are the same.
	to->mX = from.mX;
	to->mY = from.mY;
    return (TRUE);
}

BOOL LLWindowSDL::convertCoords(LLCoordWindow from, LLCoordScreen *to)
{
    if (!to)
		return FALSE;

	// In the fullscreen case, window and screen coordinates are the same.
	to->mX = from.mX;
	to->mY = from.mY;
    return (TRUE);
}

BOOL LLWindowSDL::convertCoords(LLCoordScreen from, LLCoordGL *to)
{
	LLCoordWindow window_coord;

	return(convertCoords(from, &window_coord) && convertCoords(window_coord, to));
}

BOOL LLWindowSDL::convertCoords(LLCoordGL from, LLCoordScreen *to)
{
	LLCoordWindow window_coord;

	return(convertCoords(from, &window_coord) && convertCoords(window_coord, to));
}




void LLWindowSDL::setupFailure(const std::string& text, const std::string& caption, U32 type)
{
	destroyContext();

	OSMessageBox(text, caption, type);
}

BOOL LLWindowSDL::SDLReallyCaptureInput(BOOL capture)
{
	// note: this used to be safe to call nestedly, but in the
	// end that's not really a wise usage pattern, so don't.

	if (capture)
		mReallyCapturedCount = 1;
	else
		mReallyCapturedCount = 0;
	
	SDL_GrabMode wantmode, newmode;
	if (mReallyCapturedCount <= 0) // uncapture
	{
		wantmode = SDL_GRAB_OFF;
	} else // capture
	{
		wantmode = SDL_GRAB_ON;
	}
	
	if (mReallyCapturedCount < 0) // yuck, imbalance.
	{
		mReallyCapturedCount = 0;
		llwarns << "ReallyCapture count was < 0" << llendl;
	}

	if (!mFullscreen) /* only bother if we're windowed anyway */
	{
#if LL_X11
		if (mSDL_Display)
		{
			/* we dirtily mix raw X11 with SDL so that our pointer
			   isn't (as often) constrained to the limits of the
			   window while grabbed, which feels nicer and
			   hopefully eliminates some reported 'sticky pointer'
			   problems.  We use raw X11 instead of
			   SDL_WM_GrabInput() because the latter constrains
			   the pointer to the window and also steals all
			   *keyboard* input from the window manager, which was
			   frustrating users. */
			int result;
			if (wantmode == SDL_GRAB_ON)
			{
				//llinfos << "X11 POINTER GRABBY" << llendl;
				//newmode = SDL_WM_GrabInput(wantmode);
				maybe_lock_display();
				result = XGrabPointer(mSDL_Display, mSDL_XWindowID,
						      True, 0, GrabModeAsync,
						      GrabModeAsync,
						      None, None, CurrentTime);
				maybe_unlock_display();
				if (GrabSuccess == result)
					newmode = SDL_GRAB_ON;
				else
					newmode = SDL_GRAB_OFF;
			} else if (wantmode == SDL_GRAB_OFF)
			{
				//llinfos << "X11 POINTER UNGRABBY" << llendl;
				newmode = SDL_GRAB_OFF;
				//newmode = SDL_WM_GrabInput(SDL_GRAB_OFF);
				
				maybe_lock_display();
				XUngrabPointer(mSDL_Display, CurrentTime);
				// Make sure the ungrab happens RIGHT NOW.
				XSync(mSDL_Display, False);
				maybe_unlock_display();
			} else
			{
				newmode = SDL_GRAB_QUERY; // neutral
			}
		} else // not actually running on X11, for some reason
			newmode = wantmode;
#endif // LL_X11
	} else {
		// pretend we got what we wanted, when really we don't care.
		newmode = wantmode;
	}
	
	// return boolean success for whether we ended up in the desired state
	return (capture && SDL_GRAB_ON==newmode) ||
		(!capture && SDL_GRAB_OFF==newmode);
}

U32 LLWindowSDL::SDLCheckGrabbyKeys(SDLKey keysym, BOOL gain)
{
	/* part of the fix for SL-13243: Some popular window managers like
	   to totally eat alt-drag for the purposes of moving windows.  We
	   spoil their day by acquiring the exclusive X11 mouse lock for as
	   long as LALT is held down, so the window manager can't easily
	   see what's happening.  Tested successfully with Metacity.
	   And... do the same with CTRL, for other darn WMs.  We don't
	   care about other metakeys as SL doesn't use them with dragging
	   (for now). */

	/* We maintain a bitmap of critical keys which are up and down
	   instead of simply key-counting, because SDL sometimes reports
	   misbalanced keyup/keydown event pairs to us for whatever reason. */

	U32 mask = 0;
	switch (keysym)
	{
	case SDLK_LALT:
		mask = 1U << 0; break;
	case SDLK_LCTRL:
		mask = 1U << 1; break;
	case SDLK_RCTRL:
		mask = 1U << 2; break;
	default:
		break;
	}

	if (gain)
		mGrabbyKeyFlags |= mask;
	else
		mGrabbyKeyFlags &= ~mask;

	//llinfos << "mGrabbyKeyFlags=" << mGrabbyKeyFlags << llendl;

	/* 0 means we don't need to mousegrab, otherwise grab. */
	return mGrabbyKeyFlags;
}

// virtual
void LLWindowSDL::processMiscNativeEvents()
{
#if LL_GTK && (LL_LLMOZLIB_ENABLED || LL_DBUS_ENABLED)
	// Pump GTK events to avoid starvation for:
	// * Embedded Gecko
	// * DBUS servicing
	// * Anything else which quietly hooks into the default glib/GTK loop
    if (ll_try_gtk_init())
    {
	    // Yuck, Mozilla's GTK callbacks play with the locale - push/pop
	    // the locale to protect it, as exotic/non-C locales
	    // causes our code lots of general critical weirdness
	    // and crashness. (SL-35450)
	    static std::string saved_locale;
	    saved_locale = ll_safe_string(setlocale(LC_ALL, NULL));

	    // Pump until we've nothing left to do or passed 1/15th of a
	    // second pumping for this frame.
	    static LLTimer pump_timer;
	    pump_timer.reset();
	    pump_timer.setTimerExpirySec(1.0f / 15.0f);
	    do {
		     // Always do at least one non-blocking pump
		    gtk_main_iteration_do(0);
	    } while (gtk_events_pending() &&
		     !pump_timer.hasExpired());

	    setlocale(LC_ALL, saved_locale.c_str() );
    }
#endif // LL_GTK && (LL_LLMOZLIB_ENABLED || LL_DBUS_ENABLED)
}

void LLWindowSDL::gatherInput()
{
    const Uint32 CLICK_THRESHOLD = 300;  // milliseconds
    static int leftClick = 0;
    static int rightClick = 0;
    static Uint32 lastLeftDown = 0;
    static Uint32 lastRightDown = 0;
    SDL_Event event;

    // Handle all outstanding SDL events
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_MOUSEMOTION:
            {
                LLCoordWindow winCoord(event.button.x, event.button.y);
                LLCoordGL openGlCoord;
                convertCoords(winCoord, &openGlCoord);
				MASK mask = gKeyboard->currentMask(TRUE);
				mCallbacks->handleMouseMove(this, openGlCoord, mask);
                break;
            }

            case SDL_KEYDOWN:
                gKeyboard->handleKeyDown(event.key.keysym.sym, event.key.keysym.mod);
		// part of the fix for SL-13243
		if (SDLCheckGrabbyKeys(event.key.keysym.sym, TRUE) != 0)
			SDLReallyCaptureInput(TRUE);

                if (event.key.keysym.unicode)
				{
					handleUnicodeUTF16(event.key.keysym.unicode,
									   gKeyboard->currentMask(FALSE));
				}
                break;

            case SDL_KEYUP:
		if (SDLCheckGrabbyKeys(event.key.keysym.sym, FALSE) == 0)
			SDLReallyCaptureInput(FALSE); // part of the fix for SL-13243

		gKeyboard->handleKeyUp(event.key.keysym.sym, event.key.keysym.mod);
                break;

            case SDL_MOUSEBUTTONDOWN:
            {
                bool isDoubleClick = false;
                LLCoordWindow winCoord(event.button.x, event.button.y);
                LLCoordGL openGlCoord;
                convertCoords(winCoord, &openGlCoord);
		MASK mask = gKeyboard->currentMask(TRUE);

                if (event.button.button == SDL_BUTTON_LEFT)   // SDL doesn't manage double clicking...
                {
                    Uint32 now = SDL_GetTicks();
                    if ((now - lastLeftDown) > CLICK_THRESHOLD)
                        leftClick = 1;
                    else
                    {
                        if (++leftClick >= 2)
                        {
                            leftClick = 0;
			    isDoubleClick = true;
                        }
                    }
                    lastLeftDown = now;
                }
                else if (event.button.button == SDL_BUTTON_RIGHT)
                {
                    Uint32 now = SDL_GetTicks();
                    if ((now - lastRightDown) > CLICK_THRESHOLD)
                        rightClick = 1;
                    else
                    {
                        if (++rightClick >= 2)
                        {
                            rightClick = 0;
    					    isDoubleClick = true;
                        }
                    }
                    lastRightDown = now;
                }

                if (event.button.button == SDL_BUTTON_LEFT)  // left
                {
                    if (isDoubleClick)
				        mCallbacks->handleDoubleClick(this, openGlCoord, mask);
                    else
    				    mCallbacks->handleMouseDown(this, openGlCoord, mask);
                }

                else if (event.button.button == SDL_BUTTON_RIGHT)  // right ... yes, it's 3, not 2, in SDL...
                {
                    // right double click isn't handled right now in Second Life ... if (isDoubleClick)
				    mCallbacks->handleRightMouseDown(this, openGlCoord, mask);
                }

                else if (event.button.button == SDL_BUTTON_MIDDLE)  // middle
				{
				    mCallbacks->handleMiddleMouseDown(this, openGlCoord, mask);
				}
                else if (event.button.button == 4)  // mousewheel up...thanks to X11 for making SDL consider these "buttons".
					mCallbacks->handleScrollWheel(this, -1);
                else if (event.button.button == 5)  // mousewheel down...thanks to X11 for making SDL consider these "buttons".
					mCallbacks->handleScrollWheel(this, 1);

                break;
            }

            case SDL_MOUSEBUTTONUP:
            {
                LLCoordWindow winCoord(event.button.x, event.button.y);
                LLCoordGL openGlCoord;
                convertCoords(winCoord, &openGlCoord);
		MASK mask = gKeyboard->currentMask(TRUE);

                if (event.button.button == SDL_BUTTON_LEFT)  // left
				    mCallbacks->handleMouseUp(this, openGlCoord, mask);
                else if (event.button.button == SDL_BUTTON_RIGHT)  // right ... yes, it's 3, not 2, in SDL...
				    mCallbacks->handleRightMouseUp(this, openGlCoord, mask);
                else if (event.button.button == SDL_BUTTON_MIDDLE)  // middle
		{
			mCallbacks->handleMiddleMouseUp(this, openGlCoord, mask);
		}
                // don't handle mousewheel here...

                break;
            }

            case SDL_VIDEOEXPOSE:  // VIDEOEXPOSE doesn't specify the damage, but hey, it's OpenGL...repaint the whole thing!
			    mCallbacks->handlePaint(this, 0, 0, mWindow->w, mWindow->h);
                break;

            case SDL_VIDEORESIZE:  // *FIX: handle this?
		llinfos << "Handling a resize event: " << event.resize.w <<
			"x" << event.resize.h << llendl;

		// *FIX: I'm not sure this is necessary!
		mWindow = SDL_SetVideoMode(event.resize.w, event.resize.h, 32, mSDLFlags);
		if (!mWindow)
		{
			// *FIX: More informative dialog?
			llinfos << "Could not recreate context after resize! Quitting..." << llendl;
			if(mCallbacks->handleCloseRequest(this))
    			{
    				// Get the app to initiate cleanup.
    				mCallbacks->handleQuit(this);
    				// The app is responsible for calling destroyWindow when done with GL
    			}
                break;
		}
		
		mCallbacks->handleResize(this, event.resize.w, event.resize.h );
                break;

            case SDL_ACTIVEEVENT:
                if (event.active.state & SDL_APPINPUTFOCUS)
                {
			// Note that for SDL (particularly on X11), keyboard
			// and mouse focus are independent things.  Here we are
			// tracking keyboard focus state changes.

			// We have to do our own state massaging because SDL
			// can send us two unfocus events in a row for example,
			// which confuses the focus code [SL-24071].
			if (event.active.gain != mHaveInputFocus)
			{
				if (event.active.gain)
					mCallbacks->handleFocus(this);
				else
					mCallbacks->handleFocusLost(this);
			
				mHaveInputFocus = !!event.active.gain;
			}
                }
                if (event.active.state & SDL_APPACTIVE)
                {
			// Change in iconification/minimization state.
			if ((!event.active.gain) != mIsMinimized)
			{
				mCallbacks->handleActivate(this, !!event.active.gain);
				llinfos << "SDL deiconification state switched to " << BOOL(event.active.gain) << llendl;
	
				mIsMinimized = (!event.active.gain);
			}
			else
			{
				llinfos << "Ignored bogus redundant SDL deiconification state switch to " << BOOL(event.active.gain) << llendl;
			}
                }
                break;

            case SDL_QUIT:
			    if(mCallbacks->handleCloseRequest(this))
    			{
    				// Get the app to initiate cleanup.
    				mCallbacks->handleQuit(this);
    				// The app is responsible for calling destroyWindow when done with GL
    			}
                break;
	default:
		//llinfos << "Unhandled SDL event type " << event.type << llendl;
		break;
        }
    }

#if LL_X11
    // This is a good time to stop flashing the icon if our mFlashTimer has
    // expired.
    if (mFlashing && mFlashTimer.hasExpired())
    {
	    x11_set_urgent(FALSE);
	    mFlashing = FALSE;
    }
#endif // LL_X11
}

static SDL_Cursor *makeSDLCursorFromBMP(const char *filename, int hotx, int hoty)
{
	SDL_Cursor *sdlcursor = NULL;
	SDL_Surface *bmpsurface;

	// Load cursor pixel data from BMP file
	bmpsurface = Load_BMP_Resource(filename);
	if (bmpsurface && bmpsurface->w%8==0)
	{
		SDL_Surface *cursurface;
		lldebugs << "Loaded cursor file " << filename << " "
			 << bmpsurface->w << "x" << bmpsurface->h << llendl;
		cursurface = SDL_CreateRGBSurface (SDL_SWSURFACE,
						   bmpsurface->w,
						   bmpsurface->h,
						   32,
						   SDL_SwapLE32(0xFFU),
						   SDL_SwapLE32(0xFF00U),
						   SDL_SwapLE32(0xFF0000U),
						   SDL_SwapLE32(0xFF000000U));
		SDL_FillRect(cursurface, NULL, SDL_SwapLE32(0x00000000U));

		// Blit the cursor pixel data onto a 32-bit RGBA surface so we
		// only have to cope with processing one type of pixel format.
		if (0 == SDL_BlitSurface(bmpsurface, NULL,
					 cursurface, NULL))
		{
			// n.b. we already checked that width is a multiple of 8.
			const int bitmap_bytes = (cursurface->w * cursurface->h) / 8;
			unsigned char *cursor_data = new unsigned char[bitmap_bytes];
			unsigned char *cursor_mask = new unsigned char[bitmap_bytes];
			memset(cursor_data, 0, bitmap_bytes);
			memset(cursor_mask, 0, bitmap_bytes);
			int i,j;
			// Walk the RGBA cursor pixel data, extracting both data and
			// mask to build SDL-friendly cursor bitmaps from.  The mask
			// is inferred by color-keying against 200,200,200
			for (i=0; i<cursurface->h; ++i) {
				for (j=0; j<cursurface->w; ++j) {
					U8 *pixelp =
						((U8*)cursurface->pixels)
						+ cursurface->pitch * i
						+ j*cursurface->format->BytesPerPixel;
					U8 srcred = pixelp[0];
					U8 srcgreen = pixelp[1];
					U8 srcblue = pixelp[2];
					BOOL mask_bit = (srcred != 200)
						|| (srcgreen != 200)
						|| (srcblue != 200);
					BOOL data_bit = mask_bit && (srcgreen <= 80);//not 0x80
					unsigned char bit_offset = (cursurface->w/8) * i
						+ j/8;
					cursor_data[bit_offset]	|= (data_bit) << (7 - (j&7));
					cursor_mask[bit_offset]	|= (mask_bit) << (7 - (j&7));
				}
			}
			sdlcursor = SDL_CreateCursor((Uint8*)cursor_data,
						     (Uint8*)cursor_mask,
						     cursurface->w, cursurface->h,
						     hotx, hoty);
			delete[] cursor_data;
			delete[] cursor_mask;
		} else {
			llwarns << "CURSOR BLIT FAILURE, cursurface: " << cursurface << llendl;
		}
		SDL_FreeSurface(cursurface);
		SDL_FreeSurface(bmpsurface);
	} else {
		llwarns << "CURSOR LOAD FAILURE " << filename << llendl;
	}

	return sdlcursor;
}

void LLWindowSDL::setCursor(ECursorType cursor)
{
	if (ATIbug) {
		// cursor-updating is very flaky when this bug is
		// present; do nothing.
		return;
	}

	if (mCurrentCursor != cursor)
	{
		if (cursor < UI_CURSOR_COUNT)
		{
			SDL_Cursor *sdlcursor = mSDLCursors[cursor];
			// Try to default to the arrow for any cursors that
			// did not load correctly.
			if (!sdlcursor && mSDLCursors[UI_CURSOR_ARROW])
				sdlcursor = mSDLCursors[UI_CURSOR_ARROW];
			if (sdlcursor)
				SDL_SetCursor(sdlcursor);
		} else {
			llwarns << "Tried to set invalid cursor number " << cursor << llendl;
		}
		mCurrentCursor = cursor;
	}
}

ECursorType LLWindowSDL::getCursor()
{
	return mCurrentCursor;
}

void LLWindowSDL::initCursors()
{
	int i;
	// Blank the cursor pointer array for those we may miss.
	for (i=0; i<UI_CURSOR_COUNT; ++i)
	{
		mSDLCursors[i] = NULL;
	}
	// Pre-make an SDL cursor for each of the known cursor types.
	// We hardcode the hotspots - to avoid that we'd have to write
	// a .cur file loader.
	// NOTE: SDL doesn't load RLE-compressed BMP files.
	mSDLCursors[UI_CURSOR_ARROW] = makeSDLCursorFromBMP("llarrow.BMP",0,0);
	mSDLCursors[UI_CURSOR_WAIT] = makeSDLCursorFromBMP("wait.BMP",12,15);
	mSDLCursors[UI_CURSOR_HAND] = makeSDLCursorFromBMP("hand.BMP",7,10);
	mSDLCursors[UI_CURSOR_IBEAM] = makeSDLCursorFromBMP("ibeam.BMP",15,16);
	mSDLCursors[UI_CURSOR_CROSS] = makeSDLCursorFromBMP("cross.BMP",16,14);
	mSDLCursors[UI_CURSOR_SIZENWSE] = makeSDLCursorFromBMP("sizenwse.BMP",14,17);
	mSDLCursors[UI_CURSOR_SIZENESW] = makeSDLCursorFromBMP("sizenesw.BMP",17,17);
	mSDLCursors[UI_CURSOR_SIZEWE] = makeSDLCursorFromBMP("sizewe.BMP",16,14);
	mSDLCursors[UI_CURSOR_SIZENS] = makeSDLCursorFromBMP("sizens.BMP",17,16);
	mSDLCursors[UI_CURSOR_NO] = makeSDLCursorFromBMP("llno.BMP",8,8);
	mSDLCursors[UI_CURSOR_WORKING] = makeSDLCursorFromBMP("working.BMP",12,15);
	mSDLCursors[UI_CURSOR_TOOLGRAB] = makeSDLCursorFromBMP("lltoolgrab.BMP",2,13);
	mSDLCursors[UI_CURSOR_TOOLLAND] = makeSDLCursorFromBMP("lltoolland.BMP",1,6);
	mSDLCursors[UI_CURSOR_TOOLFOCUS] = makeSDLCursorFromBMP("lltoolfocus.BMP",8,5);
	mSDLCursors[UI_CURSOR_TOOLCREATE] = makeSDLCursorFromBMP("lltoolcreate.BMP",7,7);
	mSDLCursors[UI_CURSOR_ARROWDRAG] = makeSDLCursorFromBMP("arrowdrag.BMP",0,0);
	mSDLCursors[UI_CURSOR_ARROWCOPY] = makeSDLCursorFromBMP("arrowcop.BMP",0,0);
	mSDLCursors[UI_CURSOR_ARROWDRAGMULTI] = makeSDLCursorFromBMP("llarrowdragmulti.BMP",0,0);
	mSDLCursors[UI_CURSOR_ARROWCOPYMULTI] = makeSDLCursorFromBMP("arrowcopmulti.BMP",0,0);
	mSDLCursors[UI_CURSOR_NOLOCKED] = makeSDLCursorFromBMP("llnolocked.BMP",8,8);
	mSDLCursors[UI_CURSOR_ARROWLOCKED] = makeSDLCursorFromBMP("llarrowlocked.BMP",0,0);
	mSDLCursors[UI_CURSOR_GRABLOCKED] = makeSDLCursorFromBMP("llgrablocked.BMP",2,13);
	mSDLCursors[UI_CURSOR_TOOLTRANSLATE] = makeSDLCursorFromBMP("lltooltranslate.BMP",0,0);
	mSDLCursors[UI_CURSOR_TOOLROTATE] = makeSDLCursorFromBMP("lltoolrotate.BMP",0,0);
	mSDLCursors[UI_CURSOR_TOOLSCALE] = makeSDLCursorFromBMP("lltoolscale.BMP",0,0);
	mSDLCursors[UI_CURSOR_TOOLCAMERA] = makeSDLCursorFromBMP("lltoolcamera.BMP",7,5);
	mSDLCursors[UI_CURSOR_TOOLPAN] = makeSDLCursorFromBMP("lltoolpan.BMP",7,5);
	mSDLCursors[UI_CURSOR_TOOLZOOMIN] = makeSDLCursorFromBMP("lltoolzoomin.BMP",7,5);
	mSDLCursors[UI_CURSOR_TOOLPICKOBJECT3] = makeSDLCursorFromBMP("toolpickobject3.BMP",0,0);
	mSDLCursors[UI_CURSOR_TOOLSIT] = makeSDLCursorFromBMP("toolsit.BMP",0,0);
	mSDLCursors[UI_CURSOR_TOOLBUY] = makeSDLCursorFromBMP("toolbuy.BMP",0,0);
	mSDLCursors[UI_CURSOR_TOOLPAY] = makeSDLCursorFromBMP("toolpay.BMP",0,0);
	mSDLCursors[UI_CURSOR_TOOLOPEN] = makeSDLCursorFromBMP("toolopen.BMP",0,0);
	mSDLCursors[UI_CURSOR_TOOLPLAY] = makeSDLCursorFromBMP("toolplay.BMP",0,0);
	mSDLCursors[UI_CURSOR_TOOLPAUSE] = makeSDLCursorFromBMP("toolpause.BMP",0,0);
	mSDLCursors[UI_CURSOR_TOOLMEDIAOPEN] = makeSDLCursorFromBMP("toolmediaopen.BMP",0,0);
	mSDLCursors[UI_CURSOR_PIPETTE] = makeSDLCursorFromBMP("lltoolpipette.BMP",2,28);

	if (getenv("LL_ATI_MOUSE_CURSOR_BUG") != NULL) {
		llinfos << "Disabling cursor updating due to LL_ATI_MOUSE_CURSOR_BUG" << llendl;
		ATIbug = true;
	}
}

void LLWindowSDL::quitCursors()
{
	int i;
	if (mWindow)
	{
		for (i=0; i<UI_CURSOR_COUNT; ++i)
		{
			if (mSDLCursors[i])
			{
				SDL_FreeCursor(mSDLCursors[i]);
				mSDLCursors[i] = NULL;
			}
		}
	} else {
		// SDL doesn't refcount cursors, so if the window has
		// already been destroyed then the cursors have gone with it.
		llinfos << "Skipping quitCursors: mWindow already gone." << llendl;
		for (i=0; i<UI_CURSOR_COUNT; ++i)
			mSDLCursors[i] = NULL;
	}
}

void LLWindowSDL::captureMouse()
{
	// SDL already enforces the semantics that captureMouse is
	// used for, i.e. that we continue to get mouse events as long
	// as a button is down regardless of whether we left the
	// window, and in a less obnoxious way than SDL_WM_GrabInput
	// which would confine the cursor to the window too.

	//llinfos << "LLWindowSDL::captureMouse" << llendl;
}

void LLWindowSDL::releaseMouse()
{
	// see LWindowSDL::captureMouse()
	
	//llinfos << "LLWindowSDL::releaseMouse" << llendl;
}

void LLWindowSDL::hideCursor()
{
	if(!mCursorHidden)
	{
		// llinfos << "hideCursor: hiding" << llendl;
		mCursorHidden = TRUE;
		mHideCursorPermanent = TRUE;
		SDL_ShowCursor(0);
	}
	else
	{
		// llinfos << "hideCursor: already hidden" << llendl;
	}

	adjustCursorDecouple();
}

void LLWindowSDL::showCursor()
{
	if(mCursorHidden)
	{
		// llinfos << "showCursor: showing" << llendl;
		mCursorHidden = FALSE;
		mHideCursorPermanent = FALSE;
		SDL_ShowCursor(1);
	}
	else
	{
		// llinfos << "showCursor: already visible" << llendl;
	}

	adjustCursorDecouple();
}

void LLWindowSDL::showCursorFromMouseMove()
{
	if (!mHideCursorPermanent)
	{
		showCursor();
	}
}

void LLWindowSDL::hideCursorUntilMouseMove()
{
	if (!mHideCursorPermanent)
	{
		hideCursor();
		mHideCursorPermanent = FALSE;
	}
}



//
// LLSplashScreenSDL - I don't think we'll bother to implement this; it's
// fairly obsolete at this point.
//
LLSplashScreenSDL::LLSplashScreenSDL()
{
}

LLSplashScreenSDL::~LLSplashScreenSDL()
{
}

void LLSplashScreenSDL::showImpl()
{
}

void LLSplashScreenSDL::updateImpl(const std::string& mesg)
{
}

void LLSplashScreenSDL::hideImpl()
{
}



#if LL_GTK
static void response_callback (GtkDialog *dialog,
			       gint       arg1,
			       gpointer   user_data)
{
	gint *response = (gint*)user_data;
	*response = arg1;
	gtk_widget_destroy(GTK_WIDGET(dialog));
	gtk_main_quit();
}

S32 OSMessageBoxSDL(const std::string& text, const std::string& caption, U32 type)
{
	S32 rtn = OSBTN_CANCEL;

	if(gWindowImplementation != NULL)
		gWindowImplementation->beforeDialog();

	if (LLWindowSDL::ll_try_gtk_init()
	    // We can NOT expect to combine GTK and SDL's aggressive fullscreen
	    && ((NULL==gWindowImplementation) || (!was_fullscreen))
	    )
	{
		GtkWidget *win = NULL;

		llinfos << "Creating a dialog because we're in windowed mode and GTK is happy." << llendl;
		
		GtkDialogFlags flags = GTK_DIALOG_MODAL;
		GtkMessageType messagetype;
		GtkButtonsType buttons;
		switch (type)
		{
		default:
		case OSMB_OK:
			messagetype = GTK_MESSAGE_WARNING;
			buttons = GTK_BUTTONS_OK;
			break;
		case OSMB_OKCANCEL:
			messagetype = GTK_MESSAGE_QUESTION;
			buttons = GTK_BUTTONS_OK_CANCEL;
			break;
		case OSMB_YESNO:
			messagetype = GTK_MESSAGE_QUESTION;
			buttons = GTK_BUTTONS_YES_NO;
			break;
		}
		win = gtk_message_dialog_new(NULL, flags, messagetype, buttons, "%s",
									 text.c_str());

# if LL_X11
		// Make GTK tell the window manager to associate this
		// dialog with our non-GTK SDL window, which should try
		// to keep it on top etc.
		if (gWindowImplementation &&
		    gWindowImplementation->mSDL_XWindowID != None)
		{
			gtk_widget_realize(GTK_WIDGET(win)); // so we can get its gdkwin
			GdkWindow *gdkwin = gdk_window_foreign_new(gWindowImplementation->mSDL_XWindowID);
			gdk_window_set_transient_for(GTK_WIDGET(win)->window,
						     gdkwin);
		}
# endif //LL_X11

		gtk_window_set_position(GTK_WINDOW(win),
					GTK_WIN_POS_CENTER_ON_PARENT);

		gtk_window_set_type_hint(GTK_WINDOW(win),
					 GDK_WINDOW_TYPE_HINT_DIALOG);

		if (!caption.empty())
			gtk_window_set_title(GTK_WINDOW(win), caption.c_str());

		gint response = GTK_RESPONSE_NONE;
		g_signal_connect (win,
				  "response", 
				  G_CALLBACK (response_callback),
				  &response);

		// we should be able to use a gtk_dialog_run(), but it's
		// apparently not written to exist in a world without a higher
		// gtk_main(), so we manage its signal/destruction outselves.
		gtk_widget_show_all (win);
		gtk_main();

		//llinfos << "response: " << response << llendl;
		switch (response)
		{
		case GTK_RESPONSE_OK:     rtn = OSBTN_OK; break;
		case GTK_RESPONSE_YES:    rtn = OSBTN_YES; break;
		case GTK_RESPONSE_NO:     rtn = OSBTN_NO; break;
		case GTK_RESPONSE_APPLY:  rtn = OSBTN_OK; break;
		case GTK_RESPONSE_NONE:
		case GTK_RESPONSE_CANCEL:
		case GTK_RESPONSE_CLOSE:
		case GTK_RESPONSE_DELETE_EVENT:
		default: rtn = OSBTN_CANCEL;
		}
	}
	else
	{
		llinfos << "MSGBOX: " << caption << ": " << text << llendl;
		llinfos << "Skipping dialog because we're in fullscreen mode or GTK is not happy." << llendl;
		rtn = OSBTN_OK;
	}

	if(gWindowImplementation != NULL)
		gWindowImplementation->afterDialog();

	return rtn;
}

static void color_changed_callback(GtkWidget *widget,
				   gpointer user_data)
{
	GtkColorSelection *colorsel = GTK_COLOR_SELECTION(widget);
	GdkColor *colorp = (GdkColor*)user_data;
	
	gtk_color_selection_get_current_color(colorsel, colorp);
}

BOOL LLWindowSDL::dialog_color_picker ( F32 *r, F32 *g, F32 *b)
{
	BOOL rtn = FALSE;

	beforeDialog();

	if (ll_try_gtk_init()
	    // We can NOT expect to combine GTK and SDL's aggressive fullscreen
	    && !was_fullscreen
	    )
	{
		GtkWidget *win = NULL;

		win = gtk_color_selection_dialog_new(NULL);

# if LL_X11
		// Get GTK to tell the window manager to associate this
		// dialog with our non-GTK SDL window, which should try
		// to keep it on top etc.
		if (mSDL_XWindowID != None)
		{
			gtk_widget_realize(GTK_WIDGET(win)); // so we can get its gdkwin
			GdkWindow *gdkwin = gdk_window_foreign_new(mSDL_XWindowID);
			gdk_window_set_transient_for(GTK_WIDGET(win)->window,
						     gdkwin);
		}
# endif //LL_X11

		GtkColorSelection *colorsel = GTK_COLOR_SELECTION (GTK_COLOR_SELECTION_DIALOG(win)->colorsel);

		GdkColor color, orig_color;
		orig_color.pixel = 0;
		orig_color.red = guint16(65535 * *r);
		orig_color.green= guint16(65535 * *g);
		orig_color.blue = guint16(65535 * *b);
		color = orig_color;

		gtk_color_selection_set_previous_color (colorsel, &color);
		gtk_color_selection_set_current_color (colorsel, &color);
		gtk_color_selection_set_has_palette (colorsel, TRUE);
		gtk_color_selection_set_has_opacity_control(colorsel, FALSE);

		gint response = GTK_RESPONSE_NONE;
		g_signal_connect (win,
				  "response", 
				  G_CALLBACK (response_callback),
				  &response);

		g_signal_connect (G_OBJECT (colorsel), "color_changed",
				  G_CALLBACK (color_changed_callback),
				  &color);

		gtk_window_set_modal(GTK_WINDOW(win), TRUE);
		gtk_widget_show_all(win);
		// hide the help button - we don't service it.
		gtk_widget_hide(GTK_COLOR_SELECTION_DIALOG(win)->help_button);
		gtk_main();

		if (response == GTK_RESPONSE_OK &&
		    (orig_color.red != color.red
		     || orig_color.green != color.green
		     || orig_color.blue != color.blue) )
		{
			*r = color.red / 65535.0f;
			*g = color.green / 65535.0f;
			*b = color.blue / 65535.0f;
			rtn = TRUE;
		}
	}

	afterDialog();

	return rtn;
}
#else
S32 OSMessageBoxSDL(const std::string& text, const std::string& caption, U32 type)
{
	llinfos << "MSGBOX: " << caption << ": " << text << llendl;
	return 0;
}

BOOL LLWindowSDL::dialog_color_picker ( F32 *r, F32 *g, F32 *b)
{
	return (FALSE);
}
#endif // LL_GTK

#if LL_LINUX || LL_SOLARIS
// extracted from spawnWebBrowser for clarity and to eliminate
//  compiler confusion regarding close(int fd) vs. LLWindow::close()
void exec_cmd(const std::string& cmd, const std::string& arg)
{
	char* const argv[] = {(char*)cmd.c_str(), (char*)arg.c_str(), NULL};
	fflush(NULL);
	pid_t pid = fork();
	if (pid == 0)
	{ // child
		// disconnect from stdin/stdout/stderr, or child will
		// keep our output pipe undesirably alive if it outlives us.
		close(0);
		close(1);
		close(2);
		// end ourself by running the command
		execv(cmd.c_str(), argv);	/* Flawfinder: ignore */
		// if execv returns at all, there was a problem.
		llwarns << "execv failure when trying to start " << cmd << llendl;
		_exit(1); // _exit because we don't want atexit() clean-up!
	} else {
		if (pid > 0)
		{
			// parent - wait for child to die
			int childExitStatus;
			waitpid(pid, &childExitStatus, 0);
		} else {
			llwarns << "fork failure." << llendl;
		}
	}
}
#endif

// Open a URL with the user's default web browser.
// Must begin with protocol identifier.
void LLWindowSDL::spawnWebBrowser(const std::string& escaped_url)
{
	llinfos << "spawn_web_browser: " << escaped_url << llendl;
	
#if LL_LINUX || LL_SOLARIS
# if LL_X11
	if (mSDL_Display)
	{
		maybe_lock_display();
		// Just in case - before forking.
		XSync(mSDL_Display, False);
		maybe_unlock_display();
	}
# endif // LL_X11

	std::string cmd, arg;
	cmd  = gDirUtilp->getAppRODataDir().c_str();
	cmd += gDirUtilp->getDirDelimiter().c_str();
	cmd += "launch_url.sh";
	arg = escaped_url;
	exec_cmd(cmd, arg);
#endif // LL_LINUX || LL_SOLARIS

	llinfos << "spawn_web_browser returning." << llendl;
}


void *LLWindowSDL::getPlatformWindow()
{
#if LL_GTK && LL_LLMOZLIB_ENABLED
	if (LLWindowSDL::ll_try_gtk_init())
	{
		maybe_lock_display();

		GtkWidget *owin = gtk_window_new(GTK_WINDOW_POPUP);
		// Why a layout widget?  A MozContainer would be ideal, but
		// it involves exposing Mozilla headers to mozlib-using apps.
		// A layout widget with a GtkWindow parent has the desired
		// properties of being plain GTK, having a window, and being
		// derived from a GtkContainer.
		GtkWidget *rtnw = gtk_layout_new(NULL, NULL);
		gtk_container_add(GTK_CONTAINER(owin), rtnw);
		gtk_widget_realize(rtnw);
		GTK_WIDGET_UNSET_FLAGS(GTK_WIDGET(rtnw), GTK_NO_WINDOW);
		
		maybe_unlock_display();
		
		return rtnw;
	}
#endif // LL_GTK && LL_LLMOZLIB_ENABLED
	// Unixoid mozilla really needs GTK.
	return NULL;
}

void LLWindowSDL::bringToFront()
{
	// This is currently used when we are 'launched' to a specific
	// map position externally.
	llinfos << "bringToFront" << llendl;
#if LL_X11
	if (mSDL_Display && !mFullscreen)
	{
		maybe_lock_display();
		XRaiseWindow(mSDL_Display, mSDL_XWindowID);
		XSync(mSDL_Display, False);
		maybe_unlock_display();
	}
#endif // LL_X11
}

//static
std::string LLWindowSDL::getFontListSans()
{
	// Use libfontconfig to find us a nice ordered list of fallback fonts
	// specific to this system.
	std::string final_fallback("/usr/share/fonts/truetype/kochi/kochi-gothic.ttf");
	// Our 'ideal' font properties which define the sorting results.
	// slant=0 means Roman, index=0 means the first face in a font file
	// (the one we actually use), weight=80 means medium weight,
	// spacing=0 means proportional spacing.
	std::string sort_order("slant=0:index=0:weight=80:spacing=0");
	// elide_unicode_coverage removes fonts from the list whose unicode
	// range is covered by fonts earlier in the list.  This usually
	// removes ~90% of the fonts as redundant (which is great because
	// the font list can be huge), but might unnecessarily reduce the
	// renderable range if for some reason our FreeType actually fails
	// to use some of the fonts we want it to.
	const bool elide_unicode_coverage = true;
	std::string rtn;
	FcFontSet *fs = NULL;
	FcPattern *sortpat = NULL;
	int font_count = 0;

	llinfos << "Getting system font list from FontConfig..." << llendl;

	// If the user has a system-wide language preference, then favor
	// fonts from that language group.  This doesn't affect the types
	// of languages that can be displayed, but ensures that their
	// preferred language is rendered from a single consistent font where
	// possible.
	FL_Locale *locale = NULL;
	FL_Success success = FL_FindLocale(&locale, FL_MESSAGES);
	if (success != 0)
	{
		if (success >= 2 && locale->lang) // confident!
		{
			LL_INFOS("AppInit") << "Language " << locale->lang << LL_ENDL;
			LL_INFOS("AppInit") << "Location " << locale->country << LL_ENDL;
			LL_INFOS("AppInit") << "Variant " << locale->variant << LL_ENDL;

			llinfos << "Preferring fonts of language: "
				<< locale->lang
				<< llendl;
			sort_order = "lang=" + std::string(locale->lang) + ":"
				+ sort_order;
		}
	}
	FL_FreeLocale(&locale);

	if (!FcInit())
	{
		llwarns << "FontConfig failed to initialize." << llendl;
		return final_fallback;
	}

	sortpat = FcNameParse((FcChar8*) sort_order.c_str());
	if (sortpat)
	{
		// Sort the list of system fonts from most-to-least-desirable.
		fs = FcFontSort(NULL, sortpat, elide_unicode_coverage,
				NULL, NULL);
		FcPatternDestroy(sortpat);
	}

	if (fs)
	{
		// Get the full pathnames to the fonts, where available,
		// which is what we really want.
		int i;
		for (i=0; i<fs->nfont; ++i)
		{
			FcChar8 *filename;
			if (FcResultMatch == FcPatternGetString(fs->fonts[i],
								FC_FILE, 0,
								&filename)
			    && filename)
			{
				rtn += std::string((const char*)filename)+";";
				++font_count;
			}
		}
		FcFontSetDestroy (fs);
	}

	lldebugs << "Using font list: " << rtn << llendl;
	llinfos << "Using " << font_count << " system font(s)." << llendl;

	return rtn + final_fallback;
}

#endif // LL_SDL
