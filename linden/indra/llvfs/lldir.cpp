/** 
 * @file lldir.cpp
 * @brief implementation of directory utilities base class
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

#if !LL_WINDOWS
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#else
#include <direct.h>
#endif

#include "lldir.h"
#include "llerror.h"
#include "lluuid.h"

#if LL_WINDOWS
#include "lldir_win32.h"
LLDir_Win32 gDirUtil;
#elif LL_DARWIN
#include "lldir_mac.h"
LLDir_Mac gDirUtil;
#elif LL_SOLARIS
#include "lldir_solaris.h"
LLDir_Solaris gDirUtil;
#else
#include "lldir_linux.h"
LLDir_Linux gDirUtil;
#endif

LLDir *gDirUtilp = (LLDir *)&gDirUtil;

LLDir::LLDir()
:	mAppName(""),
	mExecutablePathAndName(""),
	mExecutableFilename(""),
	mExecutableDir(""),
	mAppRODataDir(""),
	mOSUserDir(""),
	mOSUserAppDir(""),
	mLindenUserDir(""),
	mOSCacheDir(""),
	mCAFile(""),
	mTempDir(""),
	mDirDelimiter("/") // fallback to forward slash if not overridden
{
}

LLDir::~LLDir()
{
}


S32 LLDir::deleteFilesInDir(const std::string &dirname, const std::string &mask)
{
	S32 count = 0;
	std::string filename; 
	std::string fullpath;
	S32 result;
	while (getNextFileInDir(dirname, mask, filename, FALSE))
	{
		if ((filename == ".") || (filename == ".."))
		{
			// skipping directory traversal filenames
			count++;
			continue;
		}
		fullpath = dirname;
		fullpath += getDirDelimiter();
		fullpath += filename;

		S32 retry_count = 0;
		while (retry_count < 5)
		{
			if (0 != LLFile::remove(fullpath))
			{
				result = errno;
				llwarns << "Problem removing " << fullpath << " - errorcode: "
						<< result << " attempt " << retry_count << llendl;
				ms_sleep(1000);
			}
			else
			{
				if (retry_count)
				{
					llwarns << "Successfully removed " << fullpath << llendl;
				}
				break;
			}
			retry_count++;
		}
		count++;
	}
	return count;
}

const std::string LLDir::findFile(const std::string &filename, 
						   const std::string searchPath1, 
						   const std::string searchPath2, 
						   const std::string searchPath3) const
{
	std::vector<std::string> search_paths;
	search_paths.push_back(searchPath1);
	search_paths.push_back(searchPath2);
	search_paths.push_back(searchPath3);

	std::vector<std::string>::iterator search_path_iter;
	for (search_path_iter = search_paths.begin();
		search_path_iter != search_paths.end();
		++search_path_iter)
	{
		if (!search_path_iter->empty())
		{
			std::string filename_and_path = (*search_path_iter) + getDirDelimiter() + filename;
			if (fileExists(filename_and_path))
			{
				return filename_and_path;
			}
		}
	}
	return "";
}


const std::string &LLDir::getExecutablePathAndName() const
{
	return mExecutablePathAndName;
}

const std::string &LLDir::getExecutableFilename() const
{
	return mExecutableFilename;
}

const std::string &LLDir::getExecutableDir() const
{
	return mExecutableDir;
}

const std::string &LLDir::getWorkingDir() const
{
	return mWorkingDir;
}

const std::string &LLDir::getAppName() const
{
	return mAppName;
}

const std::string &LLDir::getAppRODataDir() const
{
	return mAppRODataDir;
}

const std::string &LLDir::getOSUserDir() const
{
	return mOSUserDir;
}

const std::string &LLDir::getOSUserAppDir() const
{
	return mOSUserAppDir;
}

const std::string &LLDir::getLindenUserDir() const
{
	return mLindenUserDir;
}

const std::string &LLDir::getChatLogsDir() const
{
	return mChatLogsDir;
}

const std::string &LLDir::getPerAccountChatLogsDir() const
{
	return mPerAccountChatLogsDir;
}

const std::string &LLDir::getTempDir() const
{
	return mTempDir;
}

const std::string  LLDir::getCacheDir(bool get_default) const
{
	if (mCacheDir.empty() || get_default)
	{
		std::string res;
		if (getOSCacheDir().empty())
		{
			if (getOSUserAppDir().empty())
			{
				res = "data";
			}
			else
			{
				res = getOSUserAppDir() + mDirDelimiter + "cache";
			}
		}
		else
		{
			res = getOSCacheDir() + mDirDelimiter + "RainbowViewer";
		}
		return res;
	}
	else
	{
		return mCacheDir;
	}
}

const std::string &LLDir::getOSCacheDir() const
{
	return mOSCacheDir;
}


const std::string &LLDir::getCAFile() const
{
	return mCAFile;
}

const std::string &LLDir::getDirDelimiter() const
{
	return mDirDelimiter;
}

const std::string &LLDir::getSkinDir() const
{
	return mSkinDir;
}

const std::string &LLDir::getUserSkinDir() const
{
	return mUserSkinDir;
}

const std::string& LLDir::getDefaultSkinDir() const
{
	return mDefaultSkinDir;
}

const std::string LLDir::getSkinBaseDir() const
{
	std::string dir = getAppRODataDir();
	dir += mDirDelimiter;
	dir += "skins";

	return dir;
}


std::string LLDir::getExpandedFilename(ELLPath location, const std::string& filename) const
{
	return getExpandedFilename(location, "", filename);
}

std::string LLDir::getExpandedFilename(ELLPath location, const std::string& subdir, const std::string& filename) const
{
	return getExpandedFilename(location, "", subdir, filename);
}

std::string LLDir::getExpandedFilename(ELLPath location, const std::string& subdir1, const std::string& subdir2, const std::string& in_filename) const
{
	std::string prefix;
	switch (location)
	{
	case LL_PATH_NONE:
		// Do nothing
		break;

	case LL_PATH_APP_SETTINGS:
		prefix = getAppRODataDir();
		prefix += mDirDelimiter;
		prefix += "app_settings";
		break;
		
	case LL_PATH_CHARACTER:
		prefix = getAppRODataDir();
		prefix += mDirDelimiter;
		prefix += "character";
		break;
		
	case LL_PATH_MOTIONS:
		prefix = getAppRODataDir();
		prefix += mDirDelimiter;
		prefix += "motions";
		break;
		
	case LL_PATH_HELP:
		prefix = "help";
		break;
		
	case LL_PATH_CACHE:
	    prefix = getCacheDir();
		break;
		
	case LL_PATH_USER_SETTINGS:
		prefix = getOSUserAppDir();
		prefix += mDirDelimiter;
		prefix += "user_settings";
		break;

	case LL_PATH_PER_SL_ACCOUNT:
		prefix = getLindenUserDir();
		break;
		
	case LL_PATH_CHAT_LOGS:
		prefix = getChatLogsDir();
		break;
		
	case LL_PATH_PER_ACCOUNT_CHAT_LOGS:
		prefix = getPerAccountChatLogsDir();
		break;

	case LL_PATH_LOGS:
		prefix = getOSUserAppDir();
		prefix += mDirDelimiter;
		prefix += "logs";
		break;

	case LL_PATH_TEMP:
		prefix = getTempDir();
		break;

	case LL_PATH_TOP_SKIN:
		prefix = getSkinDir();
		break;

	case LL_PATH_SKINS:
		prefix = getAppRODataDir();
		prefix += mDirDelimiter;
		prefix += "skins";
		break;

	//case LL_PATH_HTML:
	//	prefix = getSkinDir();
	//	prefix += mDirDelimiter;
	//	prefix += "html";
	//	break;

	case LL_PATH_MOZILLA_PROFILE:
		prefix = getOSUserAppDir();
		prefix += mDirDelimiter;
		prefix += "browser_profile";
		break;

	case LL_PATH_EXECUTABLE:
		prefix = getExecutableDir();
		break;

	default:
		llassert(0);
	}

	std::string filename = in_filename;
	if (!subdir2.empty())
	{
		filename = subdir2 + mDirDelimiter + filename;
	}

	if (!subdir1.empty())
	{
		filename = subdir1 + mDirDelimiter + filename;
	}

	std::string expanded_filename;
	if (!filename.empty())
	{
		if (!prefix.empty())
		{
			expanded_filename += prefix;
			expanded_filename += mDirDelimiter;
			expanded_filename += filename;
		}
		else
		{
			expanded_filename = filename;
		}
	}
	else if (!prefix.empty())
	{
		// Directory only, no file name.
		expanded_filename = prefix;
	}
	else
	{
		expanded_filename.assign("");
	}

	//llinfos << "*** EXPANDED FILENAME: <" << mExpandedFilename << ">" << llendl;

	return expanded_filename;
}

std::string LLDir::getBaseFileName(const std::string& filepath, bool strip_exten) const
{
	std::size_t offset = filepath.find_last_of(getDirDelimiter());
	offset = (offset == std::string::npos) ? 0 : offset+1;
	std::string res = filepath.substr(offset, std::string::npos);
	if (strip_exten)
	{
		offset = res.find_last_of('.');
		if (offset != std::string::npos &&
		    offset != 0) // if basename STARTS with '.', don't strip
		{
			res = res.substr(0, offset);
		}
	}
	return res;
}

std::string LLDir::getDirName(const std::string& filepath) const
{
	std::size_t offset = filepath.find_last_of(getDirDelimiter());
	S32 len = (offset == std::string::npos) ? 0 : offset;
	std::string dirname = filepath.substr(0, len);
	return dirname;
}

std::string LLDir::getExtension(const std::string& filepath) const
{
	std::string basename = getBaseFileName(filepath, false);
	std::size_t offset = basename.find_last_of('.');
	std::string exten = (offset == std::string::npos || offset == 0) ? "" : basename.substr(offset+1);
	LLStringUtil::toLower(exten);
	return exten;
}

std::string LLDir::findSkinnedFilename(const std::string &filename) const
{
	return findSkinnedFilename("", "", filename);
}

std::string LLDir::findSkinnedFilename(const std::string &subdir, const std::string &filename) const
{
	return findSkinnedFilename("", subdir, filename);
}

std::string LLDir::findSkinnedFilename(const std::string &subdir1, const std::string &subdir2, const std::string &filename) const
{
	// generate subdirectory path fragment, e.g. "/foo/bar", "/foo", ""
	std::string subdirs = ((subdir1.empty() ? "" : mDirDelimiter) + subdir1)
						 + ((subdir2.empty() ? "" : mDirDelimiter) + subdir2);

	std::string found_file = findFile(filename,
		getUserSkinDir() + subdirs,		// first look in user skin override
		getSkinDir() + subdirs,			// then in current skin
		getDefaultSkinDir() + subdirs); // and last in default skin

	return found_file;
}

std::string LLDir::getTempFilename() const
{
	LLUUID random_uuid;
	std::string uuid_str;

	random_uuid.generate();
	random_uuid.toString(uuid_str);

	std::string temp_filename = getTempDir();
	temp_filename += mDirDelimiter;
	temp_filename += uuid_str;
	temp_filename += ".tmp";

	return temp_filename;
}

// static
std::string LLDir::getScrubbedFileName(const std::string uncleanFileName)
{
	std::string name(uncleanFileName);
	const std::string illegalChars(getForbiddenFileChars());
	// replace any illegal file chars with and underscore '_'
	for( unsigned int i = 0; i < illegalChars.length(); i++ )
	{
		int j = -1;
		while((j = name.find(illegalChars[i])) > -1)
		{
			name[j] = '_';
		}
	}
	return name;
}

// static
std::string LLDir::getForbiddenFileChars()
{
	return "\\/:*?\"<>|";
}

void LLDir::setLindenUserDir(const std::string &grid, const std::string &first, const std::string &last)
{
	// if both first and last aren't set, assume we're grabbing the cached dir
	if (!first.empty() && !last.empty())
	{
		// some platforms have case-sensitive filesystems, so be
		// utterly consistent with our firstname/lastname case.
		std::string firstlower(first);
		LLStringUtil::toLower(firstlower);
		std::string lastlower(last);
		LLStringUtil::toLower(lastlower);
		std::string gridlower(grid);
		LLStringUtil::toLower(gridlower);
		mLindenUserDir = getOSUserAppDir();
		mLindenUserDir += mDirDelimiter;
		mLindenUserDir += firstlower;
		mLindenUserDir += "_";
		mLindenUserDir += lastlower;
		// Append the name of the grid, but only when not in SL to stay upward
		// compatible with SL viewers.
		if (!grid.empty() && gridlower.find("secondlife") == std::string::npos)
		{
			if (gridlower == "none" || gridlower == "other")
			{
				// Unknown grid name...
				gridlower = "unknown";
			}
			mLindenUserDir += "@" + gridlower;
		}
	}
	else
	{
		llerrs << "Invalid name for LLDir::setLindenUserDir" << llendl;
	}

	dumpCurrentDirectories();	
}

void LLDir::setChatLogsDir(const std::string &path)
{
	if (!path.empty() )
	{
		mChatLogsDir = path;
	}
	else
	{
		llwarns << "Invalid name for LLDir::setChatLogsDir" << llendl;
	}
}

void LLDir::setPerAccountChatLogsDir(const std::string &grid, const std::string &first, const std::string &last)
{
	// if both first and last aren't set, assume we're grabbing the cached dir
	if (!first.empty() && !last.empty())
	{
		// some platforms have case-sensitive filesystems, so be
		// utterly consistent with our firstname/lastname case.
		std::string firstlower(first);
		LLStringUtil::toLower(firstlower);
		std::string lastlower(last);
		LLStringUtil::toLower(lastlower);
		std::string gridlower(grid);
		LLStringUtil::toLower(gridlower);
		mPerAccountChatLogsDir = getChatLogsDir();
		mPerAccountChatLogsDir += mDirDelimiter;
		mPerAccountChatLogsDir += firstlower;
		mPerAccountChatLogsDir += "_";
		mPerAccountChatLogsDir += lastlower;
		// Append the name of the grid, but only when not in SL to stay upward
		// compatible with SL viewers.
		if (!grid.empty() && gridlower.find("secondlife") == std::string::npos)
		{
			if (gridlower == "none" || gridlower == "other")
			{
				// Unknown grid name...
				gridlower = "unknown";
			}
			mPerAccountChatLogsDir += "@" + gridlower;
		}
	}
	else
	{
		llwarns << "Invalid name for LLDir::setPerAccountChatLogsDir" << llendl;
	}
}

void LLDir::setSkinFolder(const std::string &skin_folder)
{
	mSkinDir = getAppRODataDir();
	mSkinDir += mDirDelimiter;
	mSkinDir += "skins";
	mSkinDir += mDirDelimiter;
	mSkinDir += skin_folder;

	// user modifications to current skin
	// e.g. c:\documents and settings\users\username\application data\rainbowviewer\skins\dazzle
	mUserSkinDir = getOSUserAppDir();
	mUserSkinDir += mDirDelimiter;
	mUserSkinDir += "skins";
	mUserSkinDir += mDirDelimiter;	
	mUserSkinDir += skin_folder;

	// base skin which is used as fallback for all skinned files
	// e.g. c:\program files\rainbowviewer\skins\default
	mDefaultSkinDir = getAppRODataDir();
	mDefaultSkinDir += mDirDelimiter;
	mDefaultSkinDir += "skins";
	mDefaultSkinDir += mDirDelimiter;	
	mDefaultSkinDir += "default";
}

bool LLDir::setCacheDir(const std::string &path)
{
	if (path.empty() )
	{
		// reset to default
		mCacheDir = "";
		return true;
	}
	else
	{
		LLFile::mkdir(path);
		std::string tempname = path + mDirDelimiter + "temp";
		LLFILE* file = LLFile::fopen(tempname,"wt");
		if (file)
		{
			fclose(file);
			LLFile::remove(tempname);
			mCacheDir = path;
			return true;
		}
		return false;
	}
}

void LLDir::dumpCurrentDirectories()
{
	LL_DEBUGS2("AppInit","Directories") << "Current Directories:" << LL_ENDL;

	LL_DEBUGS2("AppInit","Directories") << "  CurPath:               " << getCurPath() << LL_ENDL;
	LL_DEBUGS2("AppInit","Directories") << "  AppName:               " << getAppName() << LL_ENDL;
	LL_DEBUGS2("AppInit","Directories") << "  ExecutableFilename:    " << getExecutableFilename() << LL_ENDL;
	LL_DEBUGS2("AppInit","Directories") << "  ExecutableDir:         " << getExecutableDir() << LL_ENDL;
	LL_DEBUGS2("AppInit","Directories") << "  ExecutablePathAndName: " << getExecutablePathAndName() << LL_ENDL;
	LL_DEBUGS2("AppInit","Directories") << "  WorkingDir:            " << getWorkingDir() << LL_ENDL;
	LL_DEBUGS2("AppInit","Directories") << "  AppRODataDir:          " << getAppRODataDir() << LL_ENDL;
	LL_DEBUGS2("AppInit","Directories") << "  OSUserDir:             " << getOSUserDir() << LL_ENDL;
	LL_DEBUGS2("AppInit","Directories") << "  OSUserAppDir:          " << getOSUserAppDir() << LL_ENDL;
	LL_DEBUGS2("AppInit","Directories") << "  LindenUserDir:         " << getLindenUserDir() << LL_ENDL;
	LL_DEBUGS2("AppInit","Directories") << "  TempDir:               " << getTempDir() << LL_ENDL;
	LL_DEBUGS2("AppInit","Directories") << "  CAFile:				 " << getCAFile() << LL_ENDL;
	LL_DEBUGS2("AppInit","Directories") << "  SkinDir:               " << getSkinDir() << LL_ENDL;
}


void dir_exists_or_crash(const std::string &dir_name)
{
#if LL_WINDOWS
	// *FIX: lame - it doesn't do the same thing on windows. not so
	// important since we don't deploy simulator to windows boxes.
	LLFile::mkdir(dir_name, 0700);
#else
	struct stat dir_stat;
	if(0 != LLFile::stat(dir_name, &dir_stat))
	{
		S32 stat_rv = errno;
		if(ENOENT == stat_rv)
		{
		   if(0 != LLFile::mkdir(dir_name, 0700))		// octal
		   {
			   llerrs << "Unable to create directory: " << dir_name << llendl;
		   }
		}
		else
		{
			llerrs << "Unable to stat: " << dir_name << " errno = " << stat_rv
				   << llendl;
		}
	}
	else
	{
		// data_dir exists, make sure it's a directory.
		if(!S_ISDIR(dir_stat.st_mode))
		{
			llerrs << "Data directory collision: " << dir_name << llendl;
		}
	}
#endif
}
