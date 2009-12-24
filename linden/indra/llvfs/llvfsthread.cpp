/** 
 * @file llvfsthread.cpp
 * @brief LLVFSThread implementation
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
#include "llvfsthread.h"
#include "llstl.h"

//============================================================================

/*static*/ std::string LLVFSThread::sDataPath = "";

/*static*/ LLVFSThread* LLVFSThread::sLocal = NULL;

//============================================================================
// Run on MAIN thread
//static
void LLVFSThread::initClass(bool local_is_threaded)
{
	llassert(sLocal == NULL);
	sLocal = new LLVFSThread(local_is_threaded);
}

//static
S32 LLVFSThread::updateClass(U32 ms_elapsed)
{
	sLocal->update(ms_elapsed);
	return sLocal->getPending();
}

//static
void LLVFSThread::cleanupClass()
{
	sLocal->setQuitting();
	while (sLocal->getPending())
	{
		sLocal->update(0);
	}
	delete sLocal;
	sLocal = 0;
}

//----------------------------------------------------------------------------

LLVFSThread::LLVFSThread(bool threaded) :
	LLQueuedThread("VFS", threaded)
{
}

LLVFSThread::~LLVFSThread()
{
	// ~LLQueuedThread() will be called here
}

//----------------------------------------------------------------------------

LLVFSThread::handle_t LLVFSThread::read(LLVFS* vfs, const LLUUID &file_id, const LLAssetType::EType file_type,
										U8* buffer, S32 offset, S32 numbytes, U32 priority, U32 flags)
{
	handle_t handle = generateHandle();

	priority = llmax(priority, (U32)PRIORITY_LOW); // All reads are at least PRIORITY_LOW
	Request* req = new Request(handle, priority, flags, FILE_READ, vfs, file_id, file_type,
							   buffer, offset, numbytes);

	bool res = addRequest(req);
	if (!res)
	{
		llerrs << "LLVFSThread::read called after LLVFSThread::cleanupClass()" << llendl;
		req->deleteRequest();
		handle = nullHandle();
	}

	return handle;
}

S32 LLVFSThread::readImmediate(LLVFS* vfs, const LLUUID &file_id, const LLAssetType::EType file_type,
							   U8* buffer, S32 offset, S32 numbytes)
{
	handle_t handle = generateHandle();

	Request* req = new Request(handle, PRIORITY_IMMEDIATE, 0, FILE_READ, vfs, file_id, file_type,
							   buffer, offset, numbytes);
	
	S32 res = addRequest(req) ? 1 : 0;
	if (res == 0)
	{
		llerrs << "LLVFSThread::read called after LLVFSThread::cleanupClass()" << llendl;
		req->deleteRequest();
	}
	else
	{
		llverify(waitForResult(handle, false) == true);
		res = req->getBytesRead();
		completeRequest(handle);
	}
	return res;
}

LLVFSThread::handle_t LLVFSThread::write(LLVFS* vfs, const LLUUID &file_id, const LLAssetType::EType file_type,
										 U8* buffer, S32 offset, S32 numbytes, U32 flags)
{
	handle_t handle = generateHandle();

	Request* req = new Request(handle, 0, flags, FILE_WRITE, vfs, file_id, file_type,
							   buffer, offset, numbytes);

	bool res = addRequest(req);
	if (!res)
	{
		llerrs << "LLVFSThread::read called after LLVFSThread::cleanupClass()" << llendl;
		req->deleteRequest();
		handle = nullHandle();
	}
	
	return handle;
}

S32 LLVFSThread::writeImmediate(LLVFS* vfs, const LLUUID &file_id, const LLAssetType::EType file_type,
								 U8* buffer, S32 offset, S32 numbytes)
{
	handle_t handle = generateHandle();

	Request* req = new Request(handle, PRIORITY_IMMEDIATE, 0, FILE_WRITE, vfs, file_id, file_type,
							   buffer, offset, numbytes);

	S32 res = addRequest(req) ? 1 : 0;
	if (res == 0)
	{
		llerrs << "LLVFSThread::read called after LLVFSThread::cleanupClass()" << llendl;
		req->deleteRequest();
	}
	else
	{
		llverify(waitForResult(handle, false) == true);
		res = req->getBytesRead();
		completeRequest(handle);
	}
	return res;
}


// LLVFSThread::handle_t LLVFSThread::rename(LLVFS* vfs, const LLUUID &file_id, const LLAssetType::EType file_type,
// 										  const LLUUID &new_id, const LLAssetType::EType new_type, U32 flags)
// {
// 	handle_t handle = generateHandle();

// 	LLUUID* new_idp = new LLUUID(new_id); // deleted with Request
// 	// new_type is passed as "numbytes"
// 	Request* req = new Request(handle, 0, flags, FILE_RENAME, vfs, file_id, file_type,
// 							   (U8*)new_idp, 0, (S32)new_type);

// 	bool res = addRequest(req);
// 	if (!res)
// 	{
// 		llerrs << "LLVFSThread::read called after LLVFSThread::cleanupClass()" << llendl;
// 		req->deleteRequest();
// 		handle = nullHandle();
// 	}
	
// 	return handle;
// }

//============================================================================

LLVFSThread::Request::Request(handle_t handle, U32 priority, U32 flags,
							  operation_t op, LLVFS* vfs,
							  const LLUUID &file_id, const LLAssetType::EType file_type,
							  U8* buffer, S32 offset, S32 numbytes) :
	QueuedRequest(handle, priority, flags),
	mOperation(op),
	mVFS(vfs),
	mFileID(file_id),
	mFileType(file_type),
	mBuffer(buffer),
	mOffset(offset),
	mBytes(numbytes),
	mBytesRead(0)
{
	llassert(mBuffer);

	if (numbytes <= 0 && mOperation != FILE_RENAME)
	{
		llwarns << "LLVFSThread: Request with numbytes = " << numbytes 
			<< " operation = " << op
			<< " offset " << offset 
			<< " file_type " << file_type << llendl;
	}
	if (mOperation == FILE_WRITE)
	{
		S32 blocksize =  mVFS->getMaxSize(mFileID, mFileType);
		if (blocksize < 0)
		{
			llwarns << "VFS write to temporary block (shouldn't happen)" << llendl;
		}
		mVFS->incLock(mFileID, mFileType, VFSLOCK_APPEND);
	}
	else if (mOperation == FILE_RENAME)
	{
		mVFS->incLock(mFileID, mFileType, VFSLOCK_APPEND);
	}
	else // if (mOperation == FILE_READ)
	{
		mVFS->incLock(mFileID, mFileType, VFSLOCK_READ);
	}
}

// dec locks as soon as a request finishes
void LLVFSThread::Request::finishRequest(bool completed)
{
	if (mOperation == FILE_WRITE)
	{
		mVFS->decLock(mFileID, mFileType, VFSLOCK_APPEND);
	}
	else if (mOperation == FILE_RENAME)
	{
		mVFS->decLock(mFileID, mFileType, VFSLOCK_APPEND);
	}
	else // if (mOperation == FILE_READ)
	{
		mVFS->decLock(mFileID, mFileType, VFSLOCK_READ);
	}
}

void LLVFSThread::Request::deleteRequest()
{
	if (getStatus() == STATUS_QUEUED)
	{
		llerrs << "Attempt to delete a queued LLVFSThread::Request!" << llendl;
	}	
	if (mOperation == FILE_WRITE)
	{
		if (mFlags & FLAG_AUTO_DELETE)
		{
			delete [] mBuffer;
		}
	}
	else if (mOperation == FILE_RENAME)
	{
		LLUUID* new_idp = (LLUUID*)mBuffer;
		delete new_idp;
	}
	LLQueuedThread::QueuedRequest::deleteRequest();
}

bool LLVFSThread::Request::processRequest()
{
	bool complete = false;
	if (mOperation ==  FILE_READ)
	{
		llassert(mOffset >= 0);
		mBytesRead = mVFS->getData(mFileID, mFileType, mBuffer, mOffset, mBytes);
		complete = true;
		//llinfos << llformat("LLVFSThread::READ '%s': %d bytes arg:%d",getFilename(),mBytesRead) << llendl;
	}
	else if (mOperation ==  FILE_WRITE)
	{
		mBytesRead = mVFS->storeData(mFileID, mFileType, mBuffer, mOffset, mBytes);
		complete = true;
		//llinfos << llformat("LLVFSThread::WRITE '%s': %d bytes arg:%d",getFilename(),mBytesRead) << llendl;
	}
	else if (mOperation ==  FILE_RENAME)
	{
		LLUUID* new_idp = (LLUUID*)mBuffer;
		LLAssetType::EType new_type = (LLAssetType::EType)mBytes;
		mVFS->renameFile(mFileID, mFileType, *new_idp, new_type);
		mFileID = *new_idp;
		complete = true;
		//llinfos << llformat("LLVFSThread::RENAME '%s': %d bytes arg:%d",getFilename(),mBytesRead) << llendl;
	}
	else
	{
		llerrs << llformat("LLVFSThread::unknown operation: %d", mOperation) << llendl;
	}
	return complete;
}

//============================================================================
