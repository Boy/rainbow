/** 
 * @file llaudiodecodemgr.cpp
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 * 
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#include "llaudiodecodemgr.h"

#include "vorbisdecode.h"
#include "audioengine.h"
#include "lllfsthread.h"
#include "llvfile.h"
#include "llstring.h"
#include "lldir.h"
#include "llendianswizzle.h"
#include "audioengine.h"
#include "llassetstorage.h"

#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"

extern LLAudioEngine *gAudiop;

LLAudioDecodeMgr *gAudioDecodeMgrp = NULL;

static const S32 WAV_HEADER_SIZE = 44;


//////////////////////////////////////////////////////////////////////////////


class LLVorbisDecodeState : public LLRefCount
{
public:
	class WriteResponder : public LLLFSThread::Responder
	{
	public:
		WriteResponder(LLVorbisDecodeState* decoder) : mDecoder(decoder) {}
		~WriteResponder() {}
		void completed(S32 bytes)
		{
			mDecoder->ioComplete(bytes);
		}
		LLPointer<LLVorbisDecodeState> mDecoder;
	};
	
	LLVorbisDecodeState(const LLUUID &uuid, const std::string &out_filename);

	BOOL initDecode();
	BOOL decodeSection(); // Return TRUE if done.
	BOOL finishDecode();

	void flushBadFile();

	void ioComplete(S32 bytes)			{ mBytesRead = bytes; }
	BOOL isValid() const				{ return mValid; }
	BOOL isDone() const					{ return mDone; }
	const LLUUID &getUUID() const		{ return mUUID; }

protected:
	virtual ~LLVorbisDecodeState();

	BOOL mValid;
	BOOL mDone;
	LLAtomicS32 mBytesRead;
	LLUUID mUUID;

	std::vector<U8> mWAVBuffer;
#if !defined(USE_WAV_VFILE)
	std::string mOutFilename;
	LLLFSThread::handle_t mFileHandle;
#endif
	
	LLVFile *mInFilep;
	OggVorbis_File mVF;
	S32 mCurrentSection;
};

size_t vfs_read(void *ptr, size_t size, size_t nmemb, void *datasource)
{
	LLVFile *file = (LLVFile *)datasource;

	if (file->read((U8*)ptr, (S32)(size * nmemb)))	/*Flawfinder: ignore*/
	{
		S32 read = file->getLastBytesRead();
		return  read / size;	/*Flawfinder: ignore*/
	}
	else
	{
		return 0;
	}
}

int vfs_seek(void *datasource, ogg_int64_t offset, int whence)
{
	LLVFile *file = (LLVFile *)datasource;

	// vfs has 31-bit files
	if (offset > S32_MAX)
	{
		return -1;
	}

	S32 origin;
	switch (whence) {
	case SEEK_SET:
		origin = 0;
		break;
	case SEEK_END:
		origin = file->getSize();
		break;
	case SEEK_CUR:
		origin = -1;
		break;
	default:
		llerrs << "Invalid whence argument to vfs_seek" << llendl;
		return -1;
	}

	if (file->seek((S32)offset, origin))
	{
		return 0;
	}
	else
	{
		return -1;
	}
}

int vfs_close (void *datasource)
{
	LLVFile *file = (LLVFile *)datasource;
	delete file;
	return 0;
}

long vfs_tell (void *datasource)
{
	LLVFile *file = (LLVFile *)datasource;
	return file->tell();
}

LLVorbisDecodeState::LLVorbisDecodeState(const LLUUID &uuid, const std::string &out_filename)
{
	mDone = FALSE;
	mValid = FALSE;
	mBytesRead = -1;
	mUUID = uuid;
	mInFilep = NULL;
	mCurrentSection = 0;
#if !defined(USE_WAV_VFILE)
	mOutFilename = out_filename;
	mFileHandle = LLLFSThread::nullHandle();
#endif
	// No default value for mVF, it's an ogg structure?
}

LLVorbisDecodeState::~LLVorbisDecodeState()
{
	if (!mDone)
	{
		delete mInFilep;
		mInFilep = NULL;
	}
}


BOOL LLVorbisDecodeState::initDecode()
{
	ov_callbacks vfs_callbacks;
	vfs_callbacks.read_func = vfs_read;
	vfs_callbacks.seek_func = vfs_seek;
	vfs_callbacks.close_func = vfs_close;
	vfs_callbacks.tell_func = vfs_tell;

	//llinfos << "Initing decode from vfile: " << mUUID << llendl;

	mInFilep = new LLVFile(gVFS, mUUID, LLAssetType::AT_SOUND);
	if (!mInFilep || !mInFilep->getSize())
	{
		llwarns << "unable to open vorbis source vfile for reading" << llendl;
		delete mInFilep;
		mInFilep = NULL;
		return FALSE;
	}

	int r = ov_open_callbacks(mInFilep, &mVF, NULL, 0, vfs_callbacks);
	if(r < 0) 
	{
		llwarns << r << " Input to vorbis decode does not appear to be an Ogg bitstream: " << mUUID << llendl;
		return(FALSE);
	}
	
	size_t size_guess = (size_t)ov_pcm_total(&mVF, -1);
	vorbis_info* vi = ov_info(&mVF, -1);
	size_guess *= vi->channels;
	size_guess *= 2;
	size_guess += 2048;
	mWAVBuffer.reserve(size_guess);
	mWAVBuffer.resize(WAV_HEADER_SIZE);

	{
		// write the .wav format header
		//"RIFF"
		mWAVBuffer[0] = 0x52;
		mWAVBuffer[1] = 0x49;
		mWAVBuffer[2] = 0x46;
		mWAVBuffer[3] = 0x46;

		// length = datalen + 36 (to be filled in later)
		mWAVBuffer[4] = 0x00;
		mWAVBuffer[5] = 0x00;
		mWAVBuffer[6] = 0x00;
		mWAVBuffer[7] = 0x00;

		//"WAVE"
		mWAVBuffer[8] = 0x57;
		mWAVBuffer[9] = 0x41;
		mWAVBuffer[10] = 0x56;
		mWAVBuffer[11] = 0x45;

		// "fmt "
		mWAVBuffer[12] = 0x66;
		mWAVBuffer[13] = 0x6D;
		mWAVBuffer[14] = 0x74;
		mWAVBuffer[15] = 0x20;

		// chunk size = 16
		mWAVBuffer[16] = 0x10;
		mWAVBuffer[17] = 0x00;
		mWAVBuffer[18] = 0x00;
		mWAVBuffer[19] = 0x00;

		// format (1 = PCM)
		mWAVBuffer[20] = 0x01;
		mWAVBuffer[21] = 0x00;

		// number of channels
		mWAVBuffer[22] = 0x01;
		mWAVBuffer[23] = 0x00;

		// samples per second
		mWAVBuffer[24] = 0x44;
		mWAVBuffer[25] = 0xAC;
		mWAVBuffer[26] = 0x00;
		mWAVBuffer[27] = 0x00;

		// average bytes per second
		mWAVBuffer[28] = 0x88;
		mWAVBuffer[29] = 0x58;
		mWAVBuffer[30] = 0x01;
		mWAVBuffer[31] = 0x00;

		// bytes to output at a single time
		mWAVBuffer[32] = 0x02;
		mWAVBuffer[33] = 0x00;
		 
		// 16 bits per sample
		mWAVBuffer[34] = 0x10;
		mWAVBuffer[35] = 0x00;

		// "data"
		mWAVBuffer[36] = 0x64;
		mWAVBuffer[37] = 0x61;
		mWAVBuffer[38] = 0x74;
		mWAVBuffer[39] = 0x61;

		// these are the length of the data chunk, to be filled in later
		mWAVBuffer[40] = 0x00;
		mWAVBuffer[41] = 0x00;
		mWAVBuffer[42] = 0x00;
		mWAVBuffer[43] = 0x00;
	}
	
	//{
		//char **ptr=ov_comment(&mVF,-1)->user_comments;
//		vorbis_info *vi=ov_info(&vf,-1);
		//while(*ptr){
		//	fprintf(stderr,"%s\n",*ptr);
		//	++ptr;
		//}
//    fprintf(stderr,"\nBitstream is %d channel, %ldHz\n",vi->channels,vi->rate);
//    fprintf(stderr,"\nDecoded length: %ld samples\n", (long)ov_pcm_total(&vf,-1));
//    fprintf(stderr,"Encoded by: %s\n\n",ov_comment(&vf,-1)->vendor);
	//}
	return TRUE;
}

BOOL LLVorbisDecodeState::decodeSection()
{
	if (!mInFilep)
	{
		llwarns << "No VFS file to decode in vorbis!" << llendl;
		return TRUE;
	}
	if (mDone)
	{
// 		llwarns << "Already done with decode, aborting!" << llendl;
		return TRUE;
	}
	char pcmout[4096];	/*Flawfinder: ignore*/

	BOOL eof = FALSE;
	long ret=ov_read(&mVF, pcmout, sizeof(pcmout), 0, 2, 1, &mCurrentSection);
	if (ret == 0)
	{
		/* EOF */
		eof = TRUE;
		mDone = TRUE;
		mValid = TRUE;
//			llinfos << "Vorbis EOF" << llendl;
	}
	else if (ret < 0)
	{
		/* error in the stream.  Not a problem, just reporting it in
		   case we (the app) cares.  In this case, we don't. */

		llwarns << "BAD vorbis decode in decodeSection." << llendl;

		mValid = FALSE;
		mDone = TRUE;
		// We're done, return TRUE.
		return TRUE;
	}
	else
	{
//			llinfos << "Vorbis read " << ret << "bytes" << llendl;
		/* we don't bother dealing with sample rate changes, etc, but.
		   you'll have to*/
		std::copy(pcmout, pcmout+ret, std::back_inserter(mWAVBuffer));
	}
	return eof;
}

BOOL LLVorbisDecodeState::finishDecode()
{
	if (!isValid())
	{
		llwarns << "Bogus vorbis decode state for " << getUUID() << ", aborting!" << llendl;
		return TRUE; // We've finished
	}

#if !defined(USE_WAV_VFILE)	
	if (mFileHandle == LLLFSThread::nullHandle())
#endif
	{
		ov_clear(&mVF);
  
		// write "data" chunk length, in little-endian format
		S32 data_length = mWAVBuffer.size() - WAV_HEADER_SIZE;
		mWAVBuffer[40] = (data_length) & 0x000000FF;
		mWAVBuffer[41] = (data_length >> 8) & 0x000000FF;
		mWAVBuffer[42] = (data_length >> 16) & 0x000000FF;
		mWAVBuffer[43] = (data_length >> 24) & 0x000000FF;
		// write overall "RIFF" length, in little-endian format
		data_length += 36;
		mWAVBuffer[4] = (data_length) & 0x000000FF;
		mWAVBuffer[5] = (data_length >> 8) & 0x000000FF;
		mWAVBuffer[6] = (data_length >> 16) & 0x000000FF;
		mWAVBuffer[7] = (data_length >> 24) & 0x000000FF;

		//
		// FUDGECAKES!!! Vorbis encode/decode messes up loop point transitions (pop)
		// do a cheap-and-cheesy crossfade 
		//
		{
			S16 *samplep;
			S32 i;
			S32 fade_length;
			char pcmout[4096];		/*Flawfinder: ignore*/ 	

			fade_length = llmin((S32)128,(S32)(data_length-36)/8);			
			if((S32)mWAVBuffer.size() >= (WAV_HEADER_SIZE + 2* fade_length))
			{
				memcpy(pcmout, &mWAVBuffer[WAV_HEADER_SIZE], (2 * fade_length));	/*Flawfinder: ignore*/
			}
			llendianswizzle(&pcmout, 2, fade_length);
	
			samplep = (S16 *)pcmout;
			for (i = 0 ;i < fade_length; i++)
			{
				*samplep = llfloor((F32)*samplep * ((F32)i/(F32)fade_length));
				samplep++;
			}

			llendianswizzle(&pcmout, 2, fade_length);			
			if((WAV_HEADER_SIZE+(2 * fade_length)) < (S32)mWAVBuffer.size())
			{
				memcpy(&mWAVBuffer[WAV_HEADER_SIZE], pcmout, (2 * fade_length));	/*Flawfinder: ignore*/
			}
			S32 near_end = mWAVBuffer.size() - (2 * fade_length);
			if ((S32)mWAVBuffer.size() >= ( near_end + 2* fade_length))
			{
				memcpy(pcmout, &mWAVBuffer[near_end], (2 * fade_length));	/*Flawfinder: ignore*/
			}
			llendianswizzle(&pcmout, 2, fade_length);

			samplep = (S16 *)pcmout;
			for (i = fade_length-1 ; i >=  0; i--)
			{
				*samplep = llfloor((F32)*samplep * ((F32)i/(F32)fade_length));
				samplep++;
			}
	
			llendianswizzle(&pcmout, 2, fade_length);			
			if (near_end + (2 * fade_length) < (S32)mWAVBuffer.size())
			{
				memcpy(&mWAVBuffer[near_end], pcmout, (2 * fade_length));/*Flawfinder: ignore*/
			}
		}

		if (36 == data_length)
		{
			llwarns << "BAD Vorbis decode in finishDecode!" << llendl;
			mValid = FALSE;
			return TRUE; // we've finished
		}
#if !defined(USE_WAV_VFILE)
		mBytesRead = -1;
		mFileHandle = LLLFSThread::sLocal->write(mOutFilename, &mWAVBuffer[0], 0, mWAVBuffer.size(),
							 new WriteResponder(this));
#endif
	}

	if (mFileHandle != LLLFSThread::nullHandle())
	{
		if (mBytesRead >= 0)
		{
			if (mBytesRead == 0)
			{
				llwarns << "Unable to write file in LLVorbisDecodeState::finishDecode" << llendl;
				mValid = FALSE;
				return TRUE; // we've finished
			}
		}
		else
		{
			return FALSE; // not done
		}
	}
	
	mDone = TRUE;

#if defined(USE_WAV_VFILE)
	// write the data.
	LLVFile output(gVFS, mUUID, LLAssetType::AT_SOUND_WAV);
	output.write(&mWAVBuffer[0], mWAVBuffer.size());
#endif
	//llinfos << "Finished decode for " << getUUID() << llendl;

	return TRUE;
}

void LLVorbisDecodeState::flushBadFile()
{
	if (mInFilep)
	{
		llwarns << "Flushing bad vorbis file from VFS for " << mUUID << llendl;
		mInFilep->remove();
	}
}

//////////////////////////////////////////////////////////////////////////////

class LLAudioDecodeMgr::Impl
{
	friend class LLAudioDecodeMgr;
public:
	Impl() {};
	~Impl() {};

	void processQueue(const F32 num_secs = 0.005);

protected:
	LLLinkedQueue<LLUUID> mDecodeQueue;
	LLPointer<LLVorbisDecodeState> mCurrentDecodep;
};


void LLAudioDecodeMgr::Impl::processQueue(const F32 num_secs)
{
	LLUUID uuid;

	LLTimer decode_timer;

	BOOL done = FALSE;
	while (!done)
	{
		if (mCurrentDecodep)
		{
			BOOL res;

			// Decode in a loop until we're done or have run out of time.
			while(!(res = mCurrentDecodep->decodeSection()) && (decode_timer.getElapsedTimeF32() < num_secs))
			{
				// decodeSection does all of the work above
			}

			if (mCurrentDecodep->isDone() && !mCurrentDecodep->isValid())
			{
				// We had an error when decoding, abort.
				llwarns << mCurrentDecodep->getUUID() << " has invalid vorbis data, aborting decode" << llendl;
				mCurrentDecodep->flushBadFile();
				LLAudioData *adp = gAudiop->getAudioData(mCurrentDecodep->getUUID());
				adp->setHasValidData(FALSE);
				mCurrentDecodep = NULL;
				done = TRUE;
			}

			if (!res)
			{
				// We've used up out time slice, bail...
				done = TRUE;
			}
			else if (mCurrentDecodep)
			{
				if (mCurrentDecodep->finishDecode())
				{
					// We finished!
					if (mCurrentDecodep->isValid() && mCurrentDecodep->isDone())
					{
						LLAudioData *adp = gAudiop->getAudioData(mCurrentDecodep->getUUID());
						adp->setHasDecodedData(TRUE);
						adp->setHasValidData(TRUE);

						// At this point, we could see if anyone needs this sound immediately, but
						// I'm not sure that there's a reason to - we need to poll all of the playing
						// sounds anyway.
						//llinfos << "Finished the vorbis decode, now what?" << llendl;
					}
					else
					{
						llinfos << "Vorbis decode failed!!!" << llendl;
					}
					mCurrentDecodep = NULL;
				}
				done = TRUE; // done for now
			}
		}

		if (!done)
		{
			if (!mDecodeQueue.getLength())
			{
				// Nothing else on the queue.
				done = TRUE;
			}
			else
			{
				LLUUID uuid;
				mDecodeQueue.pop(uuid);
				if (gAudiop->hasDecodedFile(uuid))
				{
					// This file has already been decoded, don't decode it again.
					continue;
				}

				lldebugs << "Decoding " << uuid << " from audio queue!" << llendl;

				std::string uuid_str;
				std::string d_path;

				LLTimer timer;
				timer.reset();

				uuid.toString(uuid_str);
				d_path = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,uuid_str) + ".dsf";

				mCurrentDecodep = new LLVorbisDecodeState(uuid, d_path);
				if (!mCurrentDecodep->initDecode())
				{
					mCurrentDecodep = NULL;
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////

LLAudioDecodeMgr::LLAudioDecodeMgr()
{
	mImpl = new Impl;
}

LLAudioDecodeMgr::~LLAudioDecodeMgr()
{
	delete mImpl;
}

void LLAudioDecodeMgr::processQueue(const F32 num_secs)
{
	mImpl->processQueue(num_secs);
}

BOOL LLAudioDecodeMgr::addDecodeRequest(const LLUUID &uuid)
{
	if (gAudiop->hasDecodedFile(uuid))
	{
		// Already have a decoded version, don't need to decode it.
		return TRUE;
	}

	if (gAssetStorage->hasLocalAsset(uuid, LLAssetType::AT_SOUND))
	{
		// Just put it on the decode queue.
		mImpl->mDecodeQueue.push(uuid);
		return TRUE;
	}

	return FALSE;
}


