/** 
 * @file audioengine_fmod.cpp
 * @brief Implementation of LLAudioEngine class abstracting the audio support as a FMOD 3D implementation
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

#ifdef __GNUC__
#if __GNUC__ > 3 && __GNUC_MINOR__ > 1
#pragma GCC diagnostic ignored "-Wwrite-strings"
#endif
#endif

#include "linden_common.h"

#include "audioengine_fmod.h"
#include "listener_fmod.h"

#include "llerror.h"
#include "llmath.h"
#include "llrand.h"

#include "fmod.h"
#include "fmod_errors.h"
#include "lldir.h"
#include "llapr.h"

#include "sound_ids.h"

extern "C" {
	void * F_CALLBACKAPI windCallback(void *originalbuffer, void *newbuffer, int length, void* userdata);
}

FSOUND_DSPUNIT *gWindDSP = NULL;


// Safe strcpy
#if 0 //(unused)  //LL_WINDOWS || LL_LINUX
static size_t strlcpy( char* dest, const char* src, size_t dst_size )
{
	size_t source_len = 0;
	size_t min_len = 0;
	if( dst_size > 0 )
	{
		if( src )
		{
			source_len = strlen(src);	/*Flawfinder: ignore*/
			min_len = llmin( dst_size - 1, source_len );
			memcpy(dest, src, min_len);	/*Flawfinder: ignore*/	
		}
		dest[min_len] = '\0';
	}
	return source_len;
}
#else
// apple ships with the non-standard strlcpy in /usr/include/string.h:
// size_t strlcpy(char *, const char *, size_t);
#endif


LLAudioEngine_FMOD::LLAudioEngine_FMOD()
{
	mInited = false;
	mCurrentInternetStreamp = NULL;
	mInternetStreamChannel = -1;
	mWindGen = NULL;
}


LLAudioEngine_FMOD::~LLAudioEngine_FMOD()
{
}


bool LLAudioEngine_FMOD::init(const S32 num_channels, void* userdata)
{
	mFadeIn = -10000;

	LLAudioEngine::init(num_channels, userdata);

	// Reserve one extra channel for the http stream.
	if (!FSOUND_SetMinHardwareChannels(num_channels + 1))
	{
		LL_WARNS("AppInit") << "FMOD::init[0](), error: " << FMOD_ErrorString(FSOUND_GetError()) << LL_ENDL;
	}

	LL_DEBUGS("AppInit") << "LLAudioEngine_FMOD::init() initializing FMOD" << LL_ENDL;

	F32 version = FSOUND_GetVersion();
	if (version < FMOD_VERSION)
	{
		LL_WARNS("AppInit") << "Error : You are using the wrong FMOD version (" << version
			<< ")!  You should be using FMOD " << FMOD_VERSION << LL_ENDL;
		//return false;
	}

	U32 fmod_flags = 0x0;

#if LL_WINDOWS
	// Windows needs to know which window is frontmost.
	// This must be called before FSOUND_Init() per the FMOD docs.
	// This could be used to let FMOD handle muting when we lose focus,
	// but we don't actually want to do that because we want to distinguish
	// between minimized and not-focused states.
	if (!FSOUND_SetHWND(userdata))
	{
		LL_WARNS("AppInit") << "Error setting FMOD window: "
			<< FMOD_ErrorString(FSOUND_GetError()) << LL_ENDL;
		return false;
	}
	// Play audio when we don't have focus.
	// (For example, IM client on top of us.)
	// This means we also try to play audio when minimized,
	// so we manually handle muting in that case. JC
	fmod_flags |= FSOUND_INIT_GLOBALFOCUS;
#endif

#if LL_LINUX
	// initialize the FMOD engine

	// This is a hack to use only FMOD's basic FPU mixer
	// when the LL_VALGRIND environmental variable is set,
	// otherwise valgrind will fall over on FMOD's MMX detection
	if (getenv("LL_VALGRIND"))		/*Flawfinder: ignore*/
	{
		LL_INFOS("AppInit") << "Pacifying valgrind in FMOD init." << LL_ENDL;
		FSOUND_SetMixer(FSOUND_MIXER_QUALITY_FPU);
	}

	// If we don't set an output method, Linux FMOD always
	// decides on OSS and fails otherwise.  So we'll manually
	// try ESD, then OSS, then ALSA.
	// Why this order?  See SL-13250, but in short, OSS emulated
	// on top of ALSA is ironically more reliable than raw ALSA.
	// Ack, and ESD has more reliable failure modes - but has worse
	// latency - than all of them, so wins for now.
	bool audio_ok = false;

	if (!audio_ok)
		if (NULL == getenv("LL_BAD_FMOD_ESD")) /*Flawfinder: ignore*/
		{
			LL_DEBUGS("AppInit") << "Trying ESD audio output..." << LL_ENDL;
			if(FSOUND_SetOutput(FSOUND_OUTPUT_ESD) &&
			   FSOUND_Init(44100, num_channels, fmod_flags))
			{
				LL_DEBUGS("AppInit") << "ESD audio output initialized OKAY"
					<< LL_ENDL;
				audio_ok = true;
			} else {
				LL_WARNS("AppInit") << "ESD audio output FAILED to initialize: "
					<< FMOD_ErrorString(FSOUND_GetError()) << LL_ENDL;
			}
		} else {
			LL_DEBUGS("AppInit") << "ESD audio output SKIPPED" << LL_ENDL;
		}

	if (!audio_ok)
		if (NULL == getenv("LL_BAD_FMOD_OSS")) 	 /*Flawfinder: ignore*/
		{
			LL_DEBUGS("AppInit") << "Trying OSS audio output..."	<< LL_ENDL;
			if(FSOUND_SetOutput(FSOUND_OUTPUT_OSS) &&
			   FSOUND_Init(44100, num_channels, fmod_flags))
			{
				LL_DEBUGS("AppInit") << "OSS audio output initialized OKAY" << LL_ENDL;
				audio_ok = true;
			} else {
				LL_WARNS("AppInit") << "OSS audio output FAILED to initialize: "
					<< FMOD_ErrorString(FSOUND_GetError()) << LL_ENDL;
			}
		} else {
			LL_DEBUGS("AppInit") << "OSS audio output SKIPPED" << LL_ENDL;
		}

	if (!audio_ok)
		if (NULL == getenv("LL_BAD_FMOD_ALSA"))		/*Flawfinder: ignore*/
		{
			LL_DEBUGS("AppInit") << "Trying ALSA audio output..." << LL_ENDL;
			if(FSOUND_SetOutput(FSOUND_OUTPUT_ALSA) &&
			   FSOUND_Init(44100, num_channels, fmod_flags))
			{
				LL_DEBUGS("AppInit") << "ALSA audio output initialized OKAY" << LL_ENDL;
				audio_ok = true;
			} else {
				LL_WARNS("AppInit") << "ALSA audio output FAILED to initialize: "
					<< FMOD_ErrorString(FSOUND_GetError()) << LL_ENDL;
			}
		} else {
			LL_DEBUGS("AppInit") << "OSS audio output SKIPPED" << LL_ENDL;
		}

	if (!audio_ok)
	{
		LL_WARNS("AppInit") << "Overall audio init failure." << LL_ENDL;
		return false;
	}

	// On Linux, FMOD causes a SIGPIPE for some netstream error
	// conditions (an FMOD bug); ignore SIGPIPE so it doesn't crash us.
	// NOW FIXED in FMOD 3.x since 2006-10-01.
	//signal(SIGPIPE, SIG_IGN);

	// We're interested in logging which output method we
	// ended up with, for QA purposes.
	switch (FSOUND_GetOutput())
	{
	case FSOUND_OUTPUT_NOSOUND: LL_DEBUGS("AppInit") << "Audio output: NoSound" << LL_ENDL; break;
	case FSOUND_OUTPUT_OSS:	LL_DEBUGS("AppInit") << "Audio output: OSS" << LL_ENDL; break;
	case FSOUND_OUTPUT_ESD:	LL_DEBUGS("AppInit") << "Audio output: ESD" << LL_ENDL; break;
	case FSOUND_OUTPUT_ALSA: LL_DEBUGS("AppInit") << "Audio output: ALSA" << LL_ENDL; break;
	default: LL_INFOS("AppInit") << "Audio output: Unknown!" << LL_ENDL; break;
	};

#else // LL_LINUX

	// initialize the FMOD engine
	if (!FSOUND_Init(44100, num_channels, fmod_flags))
	{
		LL_WARNS("AppInit") << "Error initializing FMOD: "
			<< FMOD_ErrorString(FSOUND_GetError()) << LL_ENDL;
		return false;
	}
	
#endif

	initInternetStream();

	LL_DEBUGS("AppInit") << "LLAudioEngine_FMOD::init() FMOD initialized correctly" << LL_ENDL;

	mInited = true;

	return true;
}


std::string LLAudioEngine_FMOD::getDriverName(bool verbose)
{
	if (verbose)
	{
		F32 version = FSOUND_GetVersion();
		return llformat("FMOD version %f", version);
	}
	else
	{
		return "FMOD";
	}
}


void LLAudioEngine_FMOD::allocateListener(void)
{	
	mListenerp = (LLListener *) new LLListener_FMOD();
	if (!mListenerp)
	{
		llwarns << "Listener creation failed" << llendl;
	}
}


void LLAudioEngine_FMOD::shutdown()
{
	if (gWindDSP)
	{
		FSOUND_DSP_SetActive(gWindDSP,false);
		FSOUND_DSP_Free(gWindDSP);
	}

	stopInternetStream();

	LLAudioEngine::shutdown();
	
	llinfos << "LLAudioEngine_FMOD::shutdown() closing FMOD" << llendl;
	FSOUND_Close();
	llinfos << "LLAudioEngine_FMOD::shutdown() done closing FMOD" << llendl;

	delete mListenerp;
	mListenerp = NULL;
}


LLAudioBuffer *LLAudioEngine_FMOD::createBuffer()
{
	return new LLAudioBufferFMOD();
}


LLAudioChannel *LLAudioEngine_FMOD::createChannel()
{
	return new LLAudioChannelFMOD();
}


void LLAudioEngine_FMOD::initWind()
{
	mWindGen = new LLWindGen<MIXBUFFERFORMAT>;

	if (!gWindDSP)
	{
		gWindDSP = FSOUND_DSP_Create(&windCallback, FSOUND_DSP_DEFAULTPRIORITY_CLEARUNIT + 20, mWindGen);
	}
	if (gWindDSP)
	{
		FSOUND_DSP_SetActive(gWindDSP, true);
	}
	mNextWindUpdate = 0.0;
}


void LLAudioEngine_FMOD::cleanupWind()
{
	if (gWindDSP)
	{
		FSOUND_DSP_SetActive(gWindDSP, false);
		FSOUND_DSP_Free(gWindDSP);
		gWindDSP = NULL;
	}

	delete mWindGen;
	mWindGen = NULL;
}


//-----------------------------------------------------------------------
void LLAudioEngine_FMOD::updateWind(LLVector3 wind_vec, F32 camera_height_above_water)
{
	LLVector3 wind_pos;
	F64 pitch;
	F64 center_freq;

	if (!mEnableWind)
	{
		return;
	}
	
	if (mWindUpdateTimer.checkExpirationAndReset(LL_WIND_UPDATE_INTERVAL))
	{
		
		// wind comes in as Linden coordinate (+X = forward, +Y = left, +Z = up)
		// need to convert this to the conventional orientation DS3D and OpenAL use
		// where +X = right, +Y = up, +Z = backwards

		wind_vec.setVec(-wind_vec.mV[1], wind_vec.mV[2], -wind_vec.mV[0]);

		// cerr << "Wind update" << endl;

		pitch = 1.0 + mapWindVecToPitch(wind_vec);
		center_freq = 80.0 * pow(pitch,2.5*(mapWindVecToGain(wind_vec)+1.0));
		
		mWindGen->mTargetFreq = (F32)center_freq;
		mWindGen->mTargetGain = (F32)mapWindVecToGain(wind_vec) * mMaxWindGain;
		mWindGen->mTargetPanGainR = (F32)mapWindVecToPan(wind_vec);
  	}
}

/*
//-----------------------------------------------------------------------
void LLAudioEngine_FMOD::setSourceMinDistance(U16 source_num, F64 distance)
{
	if (!mInited)
	{
		return;
	}
	if (mBuffer[source_num])
	{
		mMinDistance[source_num] = (F32) distance;
		if (!FSOUND_Sample_SetMinMaxDistance(mBuffer[source_num],mMinDistance[source_num], mMaxDistance[source_num]))
		{
			llwarns << "FMOD::setSourceMinDistance(" << source_num << "), error: " << FMOD_ErrorString(FSOUND_GetError()) << llendl;
		}
	}
}

//-----------------------------------------------------------------------
void LLAudioEngine_FMOD::setSourceMaxDistance(U16 source_num, F64 distance)
{
	if (!mInited)
	{
		return;
	}
	if (mBuffer[source_num])
	{
		mMaxDistance[source_num] = (F32) distance;
		if (!FSOUND_Sample_SetMinMaxDistance(mBuffer[source_num],mMinDistance[source_num], mMaxDistance[source_num]))
		{
			llwarns << "FMOD::setSourceMaxDistance(" << source_num << "), error: " << FMOD_ErrorString(FSOUND_GetError()) << llendl;
		}
	}
}

//-----------------------------------------------------------------------
void LLAudioEngine_FMOD::get3DParams(S32 source_num, S32 *volume, S32 *freq, S32 *inside, S32 *outside, LLVector3 *orient, S32 *out_volume, F32 *min_dist, F32 *max_dist)
{
	*volume = 0;
	*freq = 0;
	*inside = 0;
	*outside = 0;
	*orient = LLVector3::zero;
	*out_volume = 0;
	*min_dist = 0.f;
	*max_dist = 0.f;
}

*/


//-----------------------------------------------------------------------
void LLAudioEngine_FMOD::setInternalGain(F32 gain)
{
	if (!mInited)
	{
		return;
	}

	gain = llclamp( gain, 0.0f, 1.0f );
	FSOUND_SetSFXMasterVolume( llround( 255.0f * gain ) );

	if ( mInternetStreamChannel != -1 )
	{
		F32 clamp_internet_stream_gain = llclamp( mInternetStreamGain, 0.0f, 1.0f );
		FSOUND_SetVolumeAbsolute( mInternetStreamChannel, llround( 255.0f * clamp_internet_stream_gain ) );
	}
}

//
// LLAudioChannelFMOD implementation
//

LLAudioChannelFMOD::LLAudioChannelFMOD() : LLAudioChannel(), mChannelID(0), mLastSamplePos(0)
{
}


LLAudioChannelFMOD::~LLAudioChannelFMOD()
{
	cleanup();
}


bool LLAudioChannelFMOD::updateBuffer()
{
	if (LLAudioChannel::updateBuffer())
	{
		// Base class update returned true, which means that we need to actually
		// set up the channel for a different buffer.

		LLAudioBufferFMOD *bufferp = (LLAudioBufferFMOD *)mCurrentSourcep->getCurrentBuffer();

		// Grab the FMOD sample associated with the buffer
		FSOUND_SAMPLE *samplep = bufferp->getSample();
		if (!samplep)
		{
			// This is bad, there should ALWAYS be a sample associated with a legit
			// buffer.
			llerrs << "No FMOD sample!" << llendl;
			return false;
		}


		// Actually play the sound.  Start it off paused so we can do all the necessary
		// setup.
		mChannelID = FSOUND_PlaySoundEx(FSOUND_FREE, samplep, FSOUND_DSP_GetSFXUnit(), true);
		if (!mChannelID)
		{
// 			llwarns << "Could not allocate a new channel ID" << llendl;
			mCurrentBufferp = NULL;
			return FALSE;
		}

		//llinfos << "Setting up channel " << std::hex << mChannelID << std::dec << llendl;
	}

	// If we have a source for the channel, we need to update its gain.
	if (mCurrentSourcep)
	{
		// SJB: warnings can spam and hurt framerate, disabling
		if (!FSOUND_SetVolume(mChannelID, llround(getSecondaryGain() * mCurrentSourcep->getGain() * 255.0f)))
		{
// 			llwarns << "LLAudioChannelFMOD::updateBuffer error: " << FMOD_ErrorString(FSOUND_GetError()) << llendl;
		}
		
		if (!FSOUND_SetLoopMode(mChannelID, mCurrentSourcep->isLoop() ? FSOUND_LOOP_NORMAL : FSOUND_LOOP_OFF))
		{
// 			llwarns << "Channel " << mChannelID << "Source ID: " << mCurrentSourcep->getID()
// 					<< " at " << mCurrentSourcep->getPositionGlobal() << llendl;
// 			llwarns << "LLAudioChannelFMOD::updateBuffer error: " << FMOD_ErrorString(FSOUND_GetError()) << llendl;
		}
	}

	return true;
}


void LLAudioChannelFMOD::update3DPosition()
{
	if (!mChannelID)
	{
		// We're not actually a live channel (i.e., we're not playing back anything)
		return;
	}

	LLAudioBufferFMOD *bufferp = (LLAudioBufferFMOD *)mCurrentBufferp;
	if (!bufferp)
	{
		// We don't have a buffer associated with us (should really have been picked up
		// by the above if.
		return;
	}

	if (mCurrentSourcep->isAmbient())
	{
		// Ambient sound, don't need to do any positional updates.
		bufferp->set3DMode(false);
	}
	else
	{
		// Localized sound.  Update the position and velocity of the sound.
		bufferp->set3DMode(true);

		LLVector3 float_pos;
		float_pos.setVec(mCurrentSourcep->getPositionGlobal());
		if (!FSOUND_3D_SetAttributes(mChannelID, float_pos.mV, mCurrentSourcep->getVelocity().mV))
		{
			LL_DEBUGS("FMOD") << "LLAudioChannelFMOD::update3DPosition error: " << FMOD_ErrorString(FSOUND_GetError()) << LL_ENDL;
		}
	}
}


void LLAudioChannelFMOD::updateLoop()
{
	if (!mChannelID)
	{
		// May want to clear up the loop/sample counters.
		return;
	}

	//
	// Hack:  We keep track of whether we looped or not by seeing when the
	// sample position looks like it's going backwards.  Not reliable; may
	// yield false negatives.
	//
	U32 cur_pos = FSOUND_GetCurrentPosition(mChannelID);
	if (cur_pos < (U32)mLastSamplePos)
	{
		mLoopedThisFrame = true;
	}
	mLastSamplePos = cur_pos;
}


void LLAudioChannelFMOD::cleanup()
{
	if (!mChannelID)
	{
		//llinfos << "Aborting cleanup with no channelID." << llendl;
		return;
	}

	//llinfos << "Cleaning up channel: " << mChannelID << llendl;
	if (!FSOUND_StopSound(mChannelID))
	{
		LL_DEBUGS("FMOD") << "LLAudioChannelFMOD::cleanup error: " << FMOD_ErrorString(FSOUND_GetError()) << llendl;
	}

	mCurrentBufferp = NULL;
	mChannelID = 0;
}


void LLAudioChannelFMOD::play()
{
	if (!mChannelID)
	{
		llwarns << "Playing without a channelID, aborting" << llendl;
		return;
	}

	if (!FSOUND_SetPaused(mChannelID, false))
	{
		llwarns << "LLAudioChannelFMOD::play error: " << FMOD_ErrorString(FSOUND_GetError()) << llendl;
	}
	getSource()->setPlayedOnce(true);
}


void LLAudioChannelFMOD::playSynced(LLAudioChannel *channelp)
{
	LLAudioChannelFMOD *fmod_channelp = (LLAudioChannelFMOD*)channelp;
	if (!(fmod_channelp->mChannelID && mChannelID))
	{
		// Don't have channels allocated to both the master and the slave
		return;
	}

	U32 position = FSOUND_GetCurrentPosition(fmod_channelp->mChannelID) % mCurrentBufferp->getLength();
	// Try to match the position of our sync master
	if (!FSOUND_SetCurrentPosition(mChannelID, position))
	{
		llwarns << "LLAudioChannelFMOD::playSynced unable to set current position" << llendl;
	}

	// Start us playing
	play();
}


bool LLAudioChannelFMOD::isPlaying()
{
	if (!mChannelID)
	{
		return false;
	}

	return FSOUND_IsPlaying(mChannelID) && (!FSOUND_GetPaused(mChannelID));
}



//
// LLAudioBufferFMOD implementation
//


LLAudioBufferFMOD::LLAudioBufferFMOD()
{
	mSamplep = NULL;
}


LLAudioBufferFMOD::~LLAudioBufferFMOD()
{
	if (mSamplep)
	{
		// Clean up the associated FMOD sample if it exists.
		FSOUND_Sample_Free(mSamplep);
		mSamplep = NULL;
	}
}


bool LLAudioBufferFMOD::loadWAV(const std::string& filename)
{
	// Try to open a wav file from disk.  This will eventually go away, as we don't
	// really want to block doing this.
	if (filename.empty())
	{
		// invalid filename, abort.
		return false;
	}

	S32 file_size = 0;
	apr_file_t* apr_file = ll_apr_file_open(filename, LL_APR_RPB, &file_size);
	if (!apr_file)
	{
		// File not found, abort.
		return false;
	}
	apr_file_close(apr_file);

	if (mSamplep)
	{
		// If there's already something loaded in this buffer, clean it up.
		FSOUND_Sample_Free(mSamplep);
		mSamplep = NULL;
	}

	// Load up the wav file into an fmod sample
#if LL_WINDOWS
	// MikeS. - Loading the sound file manually and then handing it over to FMOD,
	//	since FMOD uses posix IO internally,
	// which doesn't work with unicode file paths.
	LLFILE* sound_file = LLFile::fopen(filename,"rb");	/* Flawfinder: ignore */
	if (sound_file)
	{
		fseek(sound_file,0,SEEK_END);
		U32	file_length = ftell(sound_file);	//Find the length of the file by seeking to the end and getting the offset
		size_t	read_count;
		fseek(sound_file,0,SEEK_SET);	//Seek back to the beginning
		char*	buffer = new char[file_length];
		llassert(buffer);
		read_count = fread((void*)buffer,file_length,1,sound_file);//Load it..
		if(ferror(sound_file)==0 && (read_count == 1)){//No read error, and we got 1 chunk of our size...
			unsigned int mode_flags = FSOUND_LOOP_NORMAL | FSOUND_LOADMEMORY;
									//FSOUND_16BITS | FSOUND_MONO | FSOUND_LOADMEMORY | FSOUND_LOOP_NORMAL;
			mSamplep = FSOUND_Sample_Load(FSOUND_UNMANAGED, buffer, mode_flags , 0, file_length);
		}
		delete[] buffer;
		fclose(sound_file);
	}
#else
	mSamplep = FSOUND_Sample_Load(FSOUND_UNMANAGED, filename.c_str(), FSOUND_LOOP_NORMAL, 0, 0);
#endif

	if (!mSamplep)
	{
		// We failed to load the file for some reason.
		llwarns << "Could not load data '" << filename << "': "
				<< FMOD_ErrorString(FSOUND_GetError()) << llendl;

		//
		// If we EVER want to load wav files provided by end users, we need
		// to rethink this!
		//
		// file is probably corrupt - remove it.
		LLFile::remove(filename);
		return false;
	}

	// Everything went well, return true
	return true;
}


U32 LLAudioBufferFMOD::getLength()
{
	if (!mSamplep)
	{
		return 0;
	}

	return FSOUND_Sample_GetLength(mSamplep);
}


void LLAudioBufferFMOD::set3DMode(bool use3d)
{
	U16 current_mode = FSOUND_Sample_GetMode(mSamplep);
	
	if (use3d)
	{
		if (!FSOUND_Sample_SetMode(mSamplep, (current_mode & (~FSOUND_2D))))
		{
			llwarns << "LLAudioBufferFMOD::set3DMode error: " << FMOD_ErrorString(FSOUND_GetError()) << llendl;
		}
	}
	else
	{
		if (!FSOUND_Sample_SetMode(mSamplep, current_mode | FSOUND_2D))
		{
			llwarns << "LLAudioBufferFMOD::set3DMode error: " << FMOD_ErrorString(FSOUND_GetError()) << llendl;
		}
	}
}



//---------------------------------------------------------------------------
// Internet Streaming
//---------------------------------------------------------------------------
void LLAudioEngine_FMOD::initInternetStream()
{
	// Number of milliseconds of audio to buffer for the audio card.
	// Must be larger than the usual Second Life frame stutter time.
	FSOUND_Stream_SetBufferSize(200);

	// Here's where we set the size of the network buffer and some buffering 
	// parameters.  In this case we want a network buffer of 16k, we want it 
	// to prebuffer 40% of that when we first connect, and we want it 
	// to rebuffer 80% of that whenever we encounter a buffer underrun.

	// Leave the net buffer properties at the default.
	//FSOUND_Stream_Net_SetBufferProperties(20000, 40, 80);
	mInternetStreamURL.clear();
}


void LLAudioEngine_FMOD::startInternetStream(const std::string& url)
{
	if (!mInited)
	{
		llwarns << "startInternetStream before audio initialized" << llendl;
		return;
	}

	// "stop" stream but don't clear url, etc. in calse url == mInternetStreamURL
	stopInternetStream();
	if (!url.empty())
	{
		llinfos << "Starting internet stream: " << url << llendl;
		mCurrentInternetStreamp = new LLAudioStreamFMOD(url);
		mInternetStreamURL = url;
	}
	else
	{
		llinfos << "Set internet stream to null" << llendl;
		mInternetStreamURL.clear();
	}
}


signed char F_CALLBACKAPI LLAudioEngine_FMOD::callbackMetaData(char *name, char *value, void *userdata)
{
	/*
	LLAudioEngine_FMOD* self = (LLAudioEngine_FMOD*)userdata;

    if (!strcmp("ARTIST", name))
    {
        strlcpy(self->mInternetStreamArtist, value, 256);
        self->mInternetStreamNewMetaData = true;
        return true;
    }

    if (!strcmp("TITLE", name))
    {
        strlcpy(self->mInternetStreamTitle, value, 256);
        self->mInternetStreamNewMetaData = true;
        return true;
    }
	*/

    return true;
}


void LLAudioEngine_FMOD::updateInternetStream()
{
	// Kill dead internet streams, if possible
	std::list<LLAudioStreamFMOD *>::iterator iter;
	for (iter = mDeadStreams.begin(); iter != mDeadStreams.end();)
	{
		LLAudioStreamFMOD *streamp = *iter;
		if (streamp->stopStream())
		{
			llinfos << "Closed dead stream" << llendl;
			delete streamp;
			mDeadStreams.erase(iter++);
		}
		else
		{
			iter++;
		}
	}

	// Don't do anything if there are no streams playing
	if (!mCurrentInternetStreamp)
	{
		return;
	}

    int open_state = mCurrentInternetStreamp->getOpenState();

	if (!open_state)
	{
		// Stream is live


		// start the stream if it's ready
		if (mInternetStreamChannel < 0)
		{
			mInternetStreamChannel = mCurrentInternetStreamp->startStream();

			if (mInternetStreamChannel != -1)
			{
				// Reset volume to previously set volume
				setInternetStreamGain(mInternetStreamGain);
				FSOUND_SetPaused(mInternetStreamChannel, false);
				//FSOUND_Stream_Net_SetMetadataCallback(mInternetStream, callbackMetaData, this);
			}
		}
	}
		
	switch(open_state)
	{
	default:
	case 0:
		// success
		break;
	case -1:
		// stream handle is invalid
		llwarns << "InternetStream - invalid handle" << llendl;
		stopInternetStream();
		return;
	case -2:
		// opening
		//strlcpy(mInternetStreamArtist, "Opening", 256);
		break;
	case -3:
		// failed to open, file not found, perhaps
		llwarns << "InternetSteam - failed to open" << llendl;
		stopInternetStream();
		return;
	case -4:
		// connecting
		//strlcpy(mInternetStreamArtist, "Connecting", 256);
		break;
	case -5:
		// buffering
		//strlcpy(mInternetStreamArtist, "Buffering", 256);
		break;
	}

}

void LLAudioEngine_FMOD::stopInternetStream()
{
	if (mInternetStreamChannel != -1)
	{
		FSOUND_SetPaused(mInternetStreamChannel, true);
		FSOUND_SetPriority(mInternetStreamChannel, 0);
		mInternetStreamChannel = -1;
	}

	if (mCurrentInternetStreamp)
	{
		llinfos << "Stopping internet stream: " << mCurrentInternetStreamp->getURL() << llendl;
		if (mCurrentInternetStreamp->stopStream())
		{
			delete mCurrentInternetStreamp;
		}
		else
		{
			llwarns << "Pushing stream to dead list: " << mCurrentInternetStreamp->getURL() << llendl;
			mDeadStreams.push_back(mCurrentInternetStreamp);
		}
		mCurrentInternetStreamp = NULL;
		//mInternetStreamURL.clear();
	}
}

void LLAudioEngine_FMOD::pauseInternetStream(int pause)
{
	if (pause < 0)
	{
		pause = mCurrentInternetStreamp ? 1 : 0;
	}

	if (pause)
	{
		if (mCurrentInternetStreamp)
		{
			stopInternetStream();
		}
	}
	else
	{
		startInternetStream(mInternetStreamURL);
	}
}


// A stream is "playing" if it has been requested to start.  That
// doesn't necessarily mean audio is coming out of the speakers.
int LLAudioEngine_FMOD::isInternetStreamPlaying()
{
	if (mCurrentInternetStreamp)
	{
		return 1; // Active and playing
	}
	else if (!mInternetStreamURL.empty())
	{
		return 2; // "Paused"
	}
	else
	{
		return 0;
	}
}


void LLAudioEngine_FMOD::setInternetStreamGain(F32 vol)
{
	mInternetStreamGain = vol;

	if (mInternetStreamChannel != -1)
	{
		vol = llclamp(vol, 0.f, 1.f);
		int vol_int = llround(vol * 255.f);
		FSOUND_SetVolumeAbsolute(mInternetStreamChannel, vol_int);
	}
}


LLAudioStreamFMOD::LLAudioStreamFMOD(const std::string& url) :
	mInternetStream(NULL),
	mReady(false)
{
	mInternetStreamURL = url;
	mInternetStream = FSOUND_Stream_Open(url.c_str(), FSOUND_NORMAL | FSOUND_NONBLOCKING, 0, 0);
	if (!mInternetStream)
	{
		llwarns << "Couldn't open fmod stream, error "
			<< FMOD_ErrorString(FSOUND_GetError())
			<< llendl;
		mReady = false;
		return;
	}

	mReady = true;
}

int LLAudioStreamFMOD::startStream()
{
	// We need a live and opened stream before we try and play it.
	if (!mInternetStream || getOpenState())
	{
		llwarns << "No internet stream to start playing!" << llendl;
		return -1;
	}

	// Make sure the stream is set to 2D mode.
	FSOUND_Stream_SetMode(mInternetStream, FSOUND_2D);

	return FSOUND_Stream_PlayEx(FSOUND_FREE, mInternetStream, NULL, true);
}

bool LLAudioStreamFMOD::stopStream()
{
	if (mInternetStream)
	{
		int read_percent = 0;
		int status = 0;
		int bitrate = 0;
		unsigned int flags = 0x0;
		FSOUND_Stream_Net_GetStatus(mInternetStream, &status, &read_percent, &bitrate, &flags);

		bool close = true;
		switch (status)
		{
		case FSOUND_STREAM_NET_CONNECTING:
			close = false;
			break;
		case FSOUND_STREAM_NET_NOTCONNECTED:
		case FSOUND_STREAM_NET_BUFFERING:
		case FSOUND_STREAM_NET_READY:
		case FSOUND_STREAM_NET_ERROR:
		default:
			close = true;
		}

		if (close)
		{
			FSOUND_Stream_Close(mInternetStream);
			mInternetStream = NULL;
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return true;
	}
}

int LLAudioStreamFMOD::getOpenState()
{
	int open_state = FSOUND_Stream_GetOpenState(mInternetStream);
	return open_state;
}

void * F_CALLBACKAPI windCallback(void *originalbuffer, void *newbuffer, int length, void* userdata)
{
	// originalbuffer = fmod's original mixbuffer.
	// newbuffer = the buffer passed from the previous DSP unit.
	// length = length in samples at this mix time.
	// param = user parameter passed through in FSOUND_DSP_Create.
	//
	// modify the buffer in some fashion

	LLWindGen<LLAudioEngine_FMOD::MIXBUFFERFORMAT> *windgen =
		(LLWindGen<LLAudioEngine_FMOD::MIXBUFFERFORMAT> *)userdata;
	U8 stride;

#if LL_DARWIN
	stride = sizeof(LLAudioEngine_FMOD::MIXBUFFERFORMAT);
#else
	int mixertype = FSOUND_GetMixer();
	if (mixertype == FSOUND_MIXER_BLENDMODE ||
	    mixertype == FSOUND_MIXER_QUALITY_FPU)
	{
		stride = 4;
	}
	else
	{
		stride = 2;
	}
#endif

	newbuffer = windgen->windGenerate((LLAudioEngine_FMOD::MIXBUFFERFORMAT *)newbuffer, length, stride);

	return newbuffer;
}
