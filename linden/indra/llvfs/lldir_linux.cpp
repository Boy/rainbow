/** 
 * @file lldir_linux.cpp
 * @brief Implementation of directory utilities for linux
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

#include "linden_common.h"

#include "lldir_linux.h"
#include "llerror.h"
#include "llrand.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glob.h>
#include <pwd.h>


static std::string getCurrentUserHome(char* fallback)
{
	const uid_t uid = getuid();
	struct passwd *pw;
	char *result_cstr = fallback;
	
	pw = getpwuid(uid);
	if ((pw != NULL) && (pw->pw_dir != NULL))
	{
		result_cstr = (char*) pw->pw_dir;
	}
	else
	{
		llinfos << "Couldn't detect home directory from passwd - trying $HOME" << llendl;
		const char *const home_env = getenv("HOME");	/* Flawfinder: ignore */ 
		if (home_env)
		{
			result_cstr = (char*) home_env;
		}
		else
		{
			llwarns << "Couldn't detect home directory!  Falling back to " << fallback << llendl;
		}
	}
	
	return std::string(result_cstr);
}


LLDir_Linux::LLDir_Linux()
{
	mDirDelimiter = "/";
	mCurrentDirIndex = -1;
	mCurrentDirCount = -1;
	mDirp = NULL;

	char tmp_str[LL_MAX_PATH];	/* Flawfinder: ignore */ 
	if (getcwd(tmp_str, LL_MAX_PATH) == NULL)
	{
		strcpy(tmp_str, "/tmp");
		llwarns << "Could not get current directory; changing to "
				<< tmp_str << llendl;
		if (chdir(tmp_str) == -1)
		{
			llerrs << "Could not change directory to " << tmp_str << llendl;
		}
	}

	mExecutableFilename = "";
	mExecutablePathAndName = "";
	mExecutableDir = tmp_str;
	mWorkingDir = tmp_str;
#ifdef APP_RO_DATA_DIR
	mAppRODataDir = APP_RO_DATA_DIR;
#else
	mAppRODataDir = tmp_str;
#endif
	mOSUserDir = getCurrentUserHome(tmp_str);
	mOSUserAppDir = "";
	mLindenUserDir = tmp_str;

	char path [32];	/* Flawfinder: ignore */ 

	// *NOTE: /proc/%d/exe doesn't work on FreeBSD. But that's ok,
	// because this is the linux implementation.

	snprintf (path, sizeof(path), "/proc/%d/exe", (int) getpid ()); 
	int rc = readlink (path, tmp_str, sizeof (tmp_str)-1);	/* Flawfinder: ignore */ 
	if ( (rc != -1) && (rc <= ((int) sizeof (tmp_str)-1)) )
	{
		tmp_str[rc] = '\0'; //readlink() doesn't 0-terminate the buffer
		mExecutablePathAndName = tmp_str;
		char *path_end;
		if ((path_end = strrchr(tmp_str,'/')))
		{
			*path_end = '\0';
			mExecutableDir = tmp_str;
			mWorkingDir = tmp_str;
			mExecutableFilename = path_end+1;
		}
		else
		{
			mExecutableFilename = tmp_str;
		}
	}

	// *TODO: don't use /tmp, use $HOME/.rainbowviewer/tmp or something.
	mTempDir = "/tmp";
}

LLDir_Linux::~LLDir_Linux()
{
}

// Implementation


void LLDir_Linux::initAppDirs(const std::string &app_name)
{
	mAppName = app_name;

	std::string upper_app_name(app_name);
	LLStringUtil::toUpper(upper_app_name);

	char* app_home_env = getenv((upper_app_name + "_USER_DIR").c_str());	/* Flawfinder: ignore */ 
	if (app_home_env)
	{
		// user has specified own userappdir i.e. $RAINBOWVIEWER_USER_DIR
		mOSUserAppDir = app_home_env;
	}
	else
	{
		// traditionally on unixoids, MyApp gets ~/.myapp dir for data
		mOSUserAppDir = mOSUserDir;
		mOSUserAppDir += "/";
		mOSUserAppDir += ".";
		std::string lower_app_name(app_name);
		LLStringUtil::toLower(lower_app_name);
		mOSUserAppDir += lower_app_name;
	}

	// create any directories we expect to write to.

	int res = LLFile::mkdir(mOSUserAppDir);
	if (res == -1)
	{
		if (errno != EEXIST)
		{
			llwarns << "Couldn't create app user dir " << mOSUserAppDir << llendl;
			llwarns << "Default to base dir" << mOSUserDir << llendl;
			mOSUserAppDir = mOSUserDir;
		}
	}

	res = LLFile::mkdir(getExpandedFilename(LL_PATH_LOGS,""));
	if (res == -1)
	{
		if (errno != EEXIST)
		{
			llwarns << "Couldn't create LL_PATH_LOGS dir " << getExpandedFilename(LL_PATH_LOGS,"") << llendl;
		}
	}
	
	res = LLFile::mkdir(getExpandedFilename(LL_PATH_USER_SETTINGS,""));
	if (res == -1)
	{
		if (errno != EEXIST)
		{
			llwarns << "Couldn't create LL_PATH_USER_SETTINGS dir " << getExpandedFilename(LL_PATH_USER_SETTINGS,"") << llendl;
		}
	}
	
	res = LLFile::mkdir(getExpandedFilename(LL_PATH_CACHE,""));
	if (res == -1)
	{
		if (errno != EEXIST)
		{
			llwarns << "Couldn't create LL_PATH_CACHE dir " << getExpandedFilename(LL_PATH_CACHE,"") << llendl;
		}
	}
	
	res = LLFile::mkdir(getExpandedFilename(LL_PATH_MOZILLA_PROFILE,""));
	if (res == -1)
	{
		if (errno != EEXIST)
		{
			llwarns << "Couldn't create LL_PATH_MOZILLA_PROFILE dir " << getExpandedFilename(LL_PATH_MOZILLA_PROFILE,"") << llendl;
		}
	}

	mCAFile = getExpandedFilename(LL_PATH_APP_SETTINGS, "CA.pem");
}

U32 LLDir_Linux::countFilesInDir(const std::string &dirname, const std::string &mask)
{
	U32 file_count = 0;
	glob_t g;

	std::string tmp_str;
	tmp_str = dirname;
	tmp_str += mask;
	
	if(glob(tmp_str.c_str(), GLOB_NOSORT, NULL, &g) == 0)
	{
		file_count = g.gl_pathc;

		globfree(&g);
	}

	return (file_count);
}

// get the next file in the directory
// automatically wrap if we've hit the end
BOOL LLDir_Linux::getNextFileInDir(const std::string &dirname, const std::string &mask, std::string &fname, BOOL wrap)
{
	glob_t g;
	BOOL result = FALSE;
	fname = "";
	
	if(!(dirname == mCurrentDir))
	{
		// different dir specified, close old search
		mCurrentDirIndex = -1;
		mCurrentDirCount = -1;
		mCurrentDir = dirname;
	}
	
	std::string tmp_str;
	tmp_str = dirname;
	tmp_str += mask;

	if(glob(tmp_str.c_str(), GLOB_NOSORT, NULL, &g) == 0)
	{
		if(g.gl_pathc > 0)
		{
			if((int)g.gl_pathc != mCurrentDirCount)
			{
				// Number of matches has changed since the last search, meaning a file has been added or deleted.
				// Reset the index.
				mCurrentDirIndex = -1;
				mCurrentDirCount = g.gl_pathc;
			}
	
			mCurrentDirIndex++;
	
			if((mCurrentDirIndex >= (int)g.gl_pathc) && wrap)
			{
				mCurrentDirIndex = 0;
			}
			
			if(mCurrentDirIndex < (int)g.gl_pathc)
			{
//				llinfos << "getNextFileInDir: returning number " << mCurrentDirIndex << ", path is " << g.gl_pathv[mCurrentDirIndex] << llendl;

				// The API wants just the filename, not the full path.
				//fname = g.gl_pathv[mCurrentDirIndex];

				char *s = strrchr(g.gl_pathv[mCurrentDirIndex], '/');
				
				if(s == NULL)
					s = g.gl_pathv[mCurrentDirIndex];
				else if(s[0] == '/')
					s++;
					
				fname = s;
				
				result = TRUE;
			}
		}
		
		globfree(&g);
	}
	
	return(result);
}


// get a random file in the directory
// automatically wrap if we've hit the end
void LLDir_Linux::getRandomFileInDir(const std::string &dirname, const std::string &mask, std::string &fname)
{
	S32 num_files;
	S32 which_file;
	DIR *dirp;
	dirent *entryp = NULL;

	fname = "";

	num_files = countFilesInDir(dirname,mask);
	if (!num_files)
	{
		return;
	}

	which_file = ll_rand(num_files);

//	llinfos << "Random select file #" << which_file << llendl;

    // which_file now indicates the (zero-based) index to which file to play
	
	if (!((dirp = opendir(dirname.c_str()))))
	{
		while (which_file--)
		{
			if (!((entryp = readdir(dirp))))
			{
				return;
			}
		}		   

		if ((!which_file) && entryp)
		{
			fname = entryp->d_name;
		}
		
		closedir(dirp);
	}
}

std::string LLDir_Linux::getCurPath()
{
	char tmp_str[LL_MAX_PATH];	/* Flawfinder: ignore */ 
	if (getcwd(tmp_str, LL_MAX_PATH) == NULL)
	{
		llwarns << "Could not get current directory" << llendl;
		tmp_str[0] = '\0';
	}
	return tmp_str;
}


BOOL LLDir_Linux::fileExists(const std::string &filename) const
{
	struct stat stat_data;
	// Check the age of the file
	// Now, we see if the files we've gathered are recent...
	int res = stat(filename.c_str(), &stat_data);
	if (!res)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

