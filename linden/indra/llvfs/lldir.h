/** 
 * @file lldir.h
 * @brief Definition of directory utilities class
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 * 
 * Copyright (c) 2000-2009, Linden Research, Inc.
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

#ifndef LL_LLDIR_H
#define LL_LLDIR_H

// these numbers *may* get serialized, so we need to be explicit
typedef enum ELLPath
{
	LL_PATH_NONE = 0,
	LL_PATH_USER_SETTINGS = 1,
	LL_PATH_APP_SETTINGS = 2,	
	LL_PATH_PER_SL_ACCOUNT = 3,	
	LL_PATH_CACHE = 4,	
	LL_PATH_CHARACTER = 5,	
	LL_PATH_MOTIONS = 6,
	LL_PATH_HELP = 7,		
	LL_PATH_LOGS = 8,
	LL_PATH_TEMP = 9,
	LL_PATH_SKINS = 10,
	LL_PATH_TOP_SKIN = 11,
	LL_PATH_CHAT_LOGS = 12,
	LL_PATH_PER_ACCOUNT_CHAT_LOGS = 13,
	LL_PATH_MOZILLA_PROFILE = 14,
//	LL_PATH_HTML = 15,
	LL_PATH_EXECUTABLE = 16,
	LL_PATH_LAST
} ELLPath;


class LLDir
{
 public:
	LLDir();
	virtual ~LLDir();

	virtual void initAppDirs(const std::string &app_name) = 0;
 public:	
	virtual S32 deleteFilesInDir(const std::string &dirname, const std::string &mask);

// pure virtual functions
	virtual U32 countFilesInDir(const std::string &dirname, const std::string &mask) = 0;
	virtual BOOL getNextFileInDir(const std::string &dirname, const std::string &mask, std::string &fname, BOOL wrap) = 0;
	virtual void getRandomFileInDir(const std::string &dirname, const std::string &mask, std::string &fname) = 0;
	virtual std::string getCurPath() = 0;
	virtual BOOL fileExists(const std::string &filename) const = 0;

	const std::string findFile(const std::string &filename, const std::string searchPath1 = "", const std::string searchPath2 = "", const std::string searchPath3 = "") const;
	const std::string &getExecutablePathAndName() const;	// Full pathname of the executable
	const std::string &getAppName() const;			// install directory under progams/ ie "SecondLife"
	const std::string &getExecutableDir() const;	// Directory where the executable is located
	const std::string &getExecutableFilename() const;// Filename of .exe
	const std::string &getWorkingDir() const; // Current working directory
	const std::string &getAppRODataDir() const;	// Location of read-only data files
	const std::string &getOSUserDir() const;		// Location of the os-specific user dir
	const std::string &getOSUserAppDir() const;	// Location of the os-specific user app dir
	const std::string &getLindenUserDir() const;	// Location of the Linden user dir.
	const std::string &getChatLogsDir() const;	// Location of the chat logs dir.
	const std::string &getPerAccountChatLogsDir() const;	// Location of the per account chat logs dir.
	const std::string &getTempDir() const;			// Common temporary directory
	const std::string  getCacheDir(bool get_default = false) const;	// Location of the cache.
	const std::string &getOSCacheDir() const;		// location of OS-specific cache folder (may be empty string)
	const std::string &getCAFile() const;			// File containing TLS certificate authorities
	const std::string &getDirDelimiter() const;	// directory separator for platform (ie. '\' or '/' or ':')
	const std::string &getSkinDir() const;		// User-specified skin folder.
	const std::string &getUserSkinDir() const;		// User-specified skin folder with user modifications. e.g. c:\documents and settings\username\application data\second life\skins\curskin
	const std::string &getDefaultSkinDir() const;	// folder for default skin. e.g. c:\program files\second life\skins\default
	const std::string getSkinBaseDir() const;		// folder that contains all installed skins (not user modifications). e.g. c:\program files\second life\skins

	// Expanded filename
	std::string getExpandedFilename(ELLPath location, const std::string &filename) const;
	std::string getExpandedFilename(ELLPath location, const std::string &subdir, const std::string &filename) const;
	std::string getExpandedFilename(ELLPath location, const std::string &subdir1, const std::string &subdir2, const std::string &filename) const;

	// Base and Directory name extraction
	std::string getBaseFileName(const std::string& filepath, bool strip_exten = false) const;
	std::string getDirName(const std::string& filepath) const;
	std::string getExtension(const std::string& filepath) const; // Excludes '.', e.g getExtension("foo.wav") == "wav"

	// these methods search the various skin paths for the specified file in the following order:
	// getUserSkinDir(), getSkinDir(), getDefaultSkinDir()
	std::string findSkinnedFilename(const std::string &filename) const;
	std::string findSkinnedFilename(const std::string &subdir, const std::string &filename) const;
	std::string findSkinnedFilename(const std::string &subdir1, const std::string &subdir2, const std::string &filename) const;

	// random filename in common temporary directory
	std::string getTempFilename() const;

	// For producing safe download file names from potentially unsafe ones
	static std::string getScrubbedFileName(const std::string uncleanFileName);
	static std::string getForbiddenFileChars();

	virtual void setChatLogsDir(const std::string &path);		// Set the chat logs dir to this user's dir
	virtual void setPerAccountChatLogsDir(const std::string &grid, const std::string &first, const std::string &last);	// Set the per user chat log directory.
	virtual void setLindenUserDir(const std::string &grid, const std::string &first, const std::string &last);	// Set the linden user dir to this user's dir
	virtual void setSkinFolder(const std::string &skin_folder);
	virtual bool setCacheDir(const std::string &path);

	virtual void dumpCurrentDirectories();
	
protected:
	std::string mAppName;               // install directory under progams/ ie "SecondLife"   
	std::string mExecutablePathAndName; // full path + Filename of .exe
	std::string mExecutableFilename;    // Filename of .exe
	std::string mExecutableDir;	 	 // Location of executable
	std::string mWorkingDir;	 	 // Current working directory
	std::string mAppRODataDir;			 // Location for static app data
	std::string mOSUserDir;			 // OS Specific user directory
	std::string mOSUserAppDir;			 // OS Specific user app directory
	std::string mLindenUserDir;		 // Location for Linden user-specific data
	std::string mPerAccountChatLogsDir;		 // Location for chat logs.
	std::string mChatLogsDir;		 // Location for chat logs.
	std::string mCAFile;				 // Location of the TLS certificate authority PEM file.
	std::string mTempDir;
	std::string mCacheDir;
	std::string mOSCacheDir;
	std::string mDirDelimiter;
	std::string mSkinDir;			// Location for current skin info.
	std::string mDefaultSkinDir;			// Location for default skin info.
	std::string mUserSkinDir;			// Location for user-modified skin info.
};

void dir_exists_or_crash(const std::string &dir_name);

extern LLDir *gDirUtilp;

#endif // LL_LLDIR_H
