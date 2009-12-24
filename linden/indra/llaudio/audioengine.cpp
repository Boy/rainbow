 /** 
 * @file audioengine.cpp
 * @brief implementation of LLAudioEngine class abstracting the Open
 * AL audio support
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

#include "linden_common.h"

#include "audioengine.h"

#include "llerror.h"
#include "llmath.h"

#include "sound_ids.h"  // temporary hack for min/max distances

#include "llvfs.h"
#include "lldir.h"
#include "llaudiodecodemgr.h"
#include "llassetstorage.h"

#include "llmediamanager.h"

// necessary for grabbing sounds from sim (implemented in viewer)	
extern void request_sound(const LLUUID &sound_guid);

LLAudioEngine* gAudiop = NULL;


//
// LLAudioEngine implementation
//


LLAudioEngine::LLAudioEngine()
{
	setDefaults();
}


LLAudioEngine::~LLAudioEngine()
{
}


void LLAudioEngine::setDefaults()
{
	mMaxWindGain = 1.f;

	mListenerp = NULL;

	mMuted = false;
	mUserData = NULL;

	mLastStatus = 0;

	mNumChannels = 0;
	mEnableWind = false;

	S32 i;
	for (i = 0; i < MAX_CHANNELS; i++)
	{
		mChannels[i] = NULL;
	}
	for (i = 0; i < MAX_BUFFERS; i++)
	{
		mBuffers[i] = NULL;
	}

	mMasterGain = 1.f;
	mInternetStreamGain = 0.125f;
	mNextWindUpdate = 0.f;

	for (U32 i = 0; i < LLAudioEngine::AUDIO_TYPE_COUNT; i++)
		mSecondaryGain[i] = 1.0f;

	mInternetStreamMedia = NULL;
	mInternetStreamURL.clear();
}


bool LLAudioEngine::init(const S32 num_channels, void* userdata)
{
	setDefaults();

	mNumChannels = num_channels;
	mUserData = userdata;
	
	allocateListener();

	// Initialize the decode manager
	gAudioDecodeMgrp = new LLAudioDecodeMgr;

	llinfos << "LLAudioEngine::init() AudioEngine successfully initialized" << llendl;

	return true;
}


void LLAudioEngine::shutdown()
{
	// Clean up decode manager
	delete gAudioDecodeMgrp;
	gAudioDecodeMgrp = NULL;

	// Clean up wind source
	cleanupWind();

	// Clean up audio sources
	source_map::iterator iter_src;
	for (iter_src = mAllSources.begin(); iter_src != mAllSources.end(); iter_src++)
	{
		delete iter_src->second;
	}


	// Clean up audio data
	data_map::iterator iter_data;
	for (iter_data = mAllData.begin(); iter_data != mAllData.end(); iter_data++)
	{
		delete iter_data->second;
	}


	// Clean up channels
	S32 i;
	for (i = 0; i < MAX_CHANNELS; i++)
	{
		delete mChannels[i];
		mChannels[i] = NULL;
	}

	// Clean up buffers
	for (i = 0; i < MAX_BUFFERS; i++)
	{
		delete mBuffers[i];
		mBuffers[i] = NULL;
	}

	delete mInternetStreamMedia;
	mInternetStreamMedia = NULL;
	mInternetStreamURL.clear();
}


// virtual
void LLAudioEngine::startInternetStream(const std::string& url)
{
	llinfos << "entered startInternetStream()" << llendl;

	if (!mInternetStreamMedia)
	{
		LLMediaManager* mgr = LLMediaManager::getInstance();
		if (mgr)
		{
			mInternetStreamMedia = mgr->createSourceFromMimeType(LLURI(url).scheme(), "audio/mpeg"); // assumes that whatever media implementation supports mp3 also supports vorbis.
			llinfos << "mInternetStreamMedia is now " << mInternetStreamMedia << llendl;
		}
	}

	if(!mInternetStreamMedia)
		return;
	
	if (!url.empty()) {
		llinfos << "Starting internet stream: " << url << llendl;
		mInternetStreamURL = url;
		mInternetStreamMedia->navigateTo ( url );
		llinfos << "Playing....." << llendl;		
		mInternetStreamMedia->addCommand(LLMediaBase::COMMAND_START);
		mInternetStreamMedia->updateMedia();
	} else {
		llinfos << "setting stream to NULL"<< llendl;
		mInternetStreamURL.clear();
		mInternetStreamMedia->addCommand(LLMediaBase::COMMAND_STOP);
		mInternetStreamMedia->updateMedia();
	}
}

// virtual
void LLAudioEngine::stopInternetStream()
{
	llinfos << "entered stopInternetStream()" << llendl;
	
        if(mInternetStreamMedia)
	{
		if( ! mInternetStreamMedia->addCommand(LLMediaBase::COMMAND_STOP)){
			llinfos << "attempting to stop stream failed!" << llendl;
		}
		mInternetStreamMedia->updateMedia();
	}

	mInternetStreamURL.clear();
}

// virtual
void LLAudioEngine::pauseInternetStream(int pause)
{
	llinfos << "entered pauseInternetStream()" << llendl;

	if(!mInternetStreamMedia)
		return;
	
	if(pause)
	{
		if(! mInternetStreamMedia->addCommand(LLMediaBase::COMMAND_PAUSE))
		{
			llinfos << "attempting to pause stream failed!" << llendl;
		}
	} else {
		if(! mInternetStreamMedia->addCommand(LLMediaBase::COMMAND_START))
		{
			llinfos << "attempting to unpause stream failed!" << llendl;
		}
	}
	mInternetStreamMedia->updateMedia();
}

// virtual
void LLAudioEngine::updateInternetStream()
{
	if (mInternetStreamMedia)
		mInternetStreamMedia->updateMedia();
}

// virtual
int LLAudioEngine::isInternetStreamPlaying()
{
	if (!mInternetStreamMedia)
		return 0;
	
	if (mInternetStreamMedia->getStatus() == LLMediaBase::STATUS_STARTED)
	{
		return 1; // Active and playing
	}	

	if (mInternetStreamMedia->getStatus() == LLMediaBase::STATUS_PAUSED)
	{
		return 2; // paused
	}

	return 0; // Stopped
}

// virtual
void LLAudioEngine::getInternetStreamInfo(char* artist, char* title)
{
	artist[0] = 0;
	title[0] = 0;
}

// virtual
void LLAudioEngine::setInternetStreamGain(F32 vol)
{
	mInternetStreamGain = vol;

	if(!mInternetStreamMedia)
		return;

	vol = llclamp(vol, 0.f, 1.f);
	mInternetStreamMedia->setVolume(vol);
	mInternetStreamMedia->updateMedia();
}

// virtual
const std::string& LLAudioEngine::getInternetStreamURL()
{
	return mInternetStreamURL;
}


void LLAudioEngine::updateChannels()
{
	S32 i;
	for (i = 0; i < MAX_CHANNELS; i++)
	{
		if (mChannels[i])
		{
			mChannels[i]->updateBuffer();
			mChannels[i]->update3DPosition();
			mChannels[i]->updateLoop();
		}
	}
}

static const F32 default_max_decode_time = .002f; // 2 ms
void LLAudioEngine::idle(F32 max_decode_time)
{
	if (max_decode_time <= 0.f)
	{
		max_decode_time = default_max_decode_time;
	}
	
	// "Update" all of our audio sources, clean up dead ones.
	// Primarily does position updating, cleanup of unused audio sources.
	// Also does regeneration of the current priority of each audio source.

	if (getMuted())
	{
		setInternalGain(0.f);
	}
	else
	{
		setInternalGain(getMasterGain());
	}

	S32 i;
	for (i = 0; i < MAX_BUFFERS; i++)
	{
		if (mBuffers[i])
		{
			mBuffers[i]->mInUse = false;
		}
	}

	F32 max_priority = -1.f;
	LLAudioSource *max_sourcep = NULL; // Maximum priority source without a channel
	source_map::iterator iter;
	for (iter = mAllSources.begin(); iter != mAllSources.end();)
	{
		LLAudioSource *sourcep = iter->second;

		// Update this source
		sourcep->update();
		sourcep->updatePriority();

		if (sourcep->isDone())
		{
			// The source is done playing, clean it up.
			delete sourcep;
			mAllSources.erase(iter++);
			continue;
		}

		if (!sourcep->getChannel() && sourcep->getCurrentBuffer())
		{
			// We could potentially play this sound if its priority is high enough.
			if (sourcep->getPriority() > max_priority)
			{
				max_priority = sourcep->getPriority();
				max_sourcep = sourcep;
			}
		}

		// Move on to the next source
		iter++;
	}

	// Now, do priority-based organization of audio sources.
	// All channels used, check priorities.
	// Find channel with lowest priority
	if (max_sourcep)
	{
		LLAudioChannel *channelp = getFreeChannel(max_priority);
		if (channelp)
		{
			//llinfos << "Replacing source in channel due to priority!" << llendl;
			max_sourcep->setChannel(channelp);
			channelp->setSource(max_sourcep);
			if (max_sourcep->isSyncSlave())
			{
				// A sync slave, it doesn't start playing until it's synced up with the master.
				// Flag this channel as waiting for sync, and return true.
				channelp->setWaiting(true);
			}
			else
			{
				channelp->setWaiting(false);
				channelp->play();
			}
		}
	}

	
	// Do this BEFORE we update the channels
	// Update the channels to sync up with any changes that the source made,
	// such as changing what sound was playing.
	updateChannels();

	// Update queued sounds (switch to next queued data if the current has finished playing)
	for (iter = mAllSources.begin(); iter != mAllSources.end(); ++iter)
	{
		// This is lame, instead of this I could actually iterate through all the sources
		// attached to each channel, since only those with active channels
		// can have anything interesting happen with their queue? (Maybe not true)
		LLAudioSource *sourcep = iter->second;
		if (!sourcep->mQueuedDatap)
		{
			// Nothing queued, so we don't care.
			continue;
		}

		LLAudioChannel *channelp = sourcep->getChannel();
		if (!channelp)
		{
			// This sound isn't playing, so we just process move the queue
			sourcep->mCurrentDatap = sourcep->mQueuedDatap;
			sourcep->mQueuedDatap = NULL;

			// Reset the timer so the source doesn't die.
			sourcep->mAgeTimer.reset();
			// Make sure we have the buffer set up if we just decoded the data
			if (sourcep->mCurrentDatap)
			{
				updateBufferForData(sourcep->mCurrentDatap);
			}

			// Actually play the associated data.
			sourcep->setupChannel();
			channelp = sourcep->getChannel();
			if (channelp)
			{
				channelp->updateBuffer();
				sourcep->getChannel()->play();
			}
			continue;
		}
		else
		{
			// Check to see if the current sound is done playing, or looped.
			if (!channelp->isPlaying())
			{
				sourcep->mCurrentDatap = sourcep->mQueuedDatap;
				sourcep->mQueuedDatap = NULL;

				// Reset the timer so the source doesn't die.
				sourcep->mAgeTimer.reset();

				// Make sure we have the buffer set up if we just decoded the data
				if (sourcep->mCurrentDatap)
				{
					updateBufferForData(sourcep->mCurrentDatap);
				}

				// Actually play the associated data.
				sourcep->setupChannel();
				channelp->updateBuffer();
				sourcep->getChannel()->play();
			}
			else if (sourcep->isLoop())
			{
				// It's a loop, we need to check and see if we're done with it.
				if (channelp->mLoopedThisFrame)
				{
					sourcep->mCurrentDatap = sourcep->mQueuedDatap;
					sourcep->mQueuedDatap = NULL;

					// Actually, should do a time sync so if we're a loop master/slave
					// we don't drift away.
					sourcep->setupChannel();
					sourcep->getChannel()->play();
				}
			}
		}
	}

	// Lame, update the channels AGAIN.
	// Update the channels to sync up with any changes that the source made,
	// such as changing what sound was playing.
	updateChannels();
	
	// Hack!  For now, just use a global sync master;
	LLAudioSource *sync_masterp = NULL;
	LLAudioChannel *master_channelp = NULL;
	F32 max_sm_priority = -1.f;
	for (iter = mAllSources.begin(); iter != mAllSources.end(); ++iter)
	{
		LLAudioSource *sourcep = iter->second;
		if (sourcep->isSyncMaster())
		{
			if (sourcep->getPriority() > max_sm_priority)
			{
				sync_masterp = sourcep;
				master_channelp = sync_masterp->getChannel();
				max_sm_priority = sourcep->getPriority();
			}
		}
	}

	if (master_channelp && master_channelp->mLoopedThisFrame)
	{
		// Synchronize loop slaves with their masters
		// Update queued sounds (switch to next queued data if the current has finished playing)
		for (iter = mAllSources.begin(); iter != mAllSources.end(); ++iter)
		{
			LLAudioSource *sourcep = iter->second;

			if (!sourcep->isSyncSlave())
			{
				// Not a loop slave, we don't need to do anything
				continue;
			}

			LLAudioChannel *channelp = sourcep->getChannel();
			if (!channelp)
			{
				// Not playing, don't need to bother.
				continue;
			}

			if (!channelp->isPlaying())
			{
				// Now we need to check if our loop master has just looped, and
				// start playback if that's the case.
				if (sync_masterp->getChannel())
				{
					channelp->playSynced(master_channelp);
					channelp->setWaiting(false);
				}
			}
		}
	}

	// Sync up everything that the audio engine needs done.
	commitDeferredChanges();
	
	// Flush unused buffers that are stale enough
	for (i = 0; i < MAX_BUFFERS; i++)
	{
		if (mBuffers[i])
		{
			if (!mBuffers[i]->mInUse && mBuffers[i]->mLastUseTimer.getElapsedTimeF32() > 30.f)
			{
				//llinfos << "Flushing unused buffer!" << llendl;
				mBuffers[i]->mAudioDatap->mBufferp = NULL;
				delete mBuffers[i];
				mBuffers[i] = NULL;
			}
		}
	}


	// Clear all of the looped flags for the channels
	for (i = 0; i < MAX_CHANNELS; i++)
	{
		if (mChannels[i])
		{
			mChannels[i]->mLoopedThisFrame = false;
		}
	}

	// Decode audio files
	gAudioDecodeMgrp->processQueue(max_decode_time);
	
	// Call this every frame, just in case we somehow
	// missed picking it up in all the places that can add
	// or request new data.
	startNextTransfer();

	updateInternetStream();
}



bool LLAudioEngine::updateBufferForData(LLAudioData *adp, const LLUUID &audio_uuid)
{
	if (!adp)
	{
		return false;
	}

	// Update the audio buffer first - load a sound if we have it.
	// Note that this could potentially cause us to waste time updating buffers
	// for sounds that actually aren't playing, although this should be mitigated
	// by the fact that we limit the number of buffers, and we flush buffers based
	// on priority.
	if (!adp->getBuffer())
	{
		if (adp->hasDecodedData())
		{
			adp->load();
		}
		else if (adp->hasLocalData())
		{
			if (audio_uuid.notNull())
			{
				gAudioDecodeMgrp->addDecodeRequest(audio_uuid);
			}
		}
		else
		{
			return false;
		}
	}
	return true;
}


void LLAudioEngine::enableWind(bool enable)
{
	if (enable && (!mEnableWind))
	{
		initWind();
		mEnableWind = enable;
	}
	else if (mEnableWind && (!enable))
	{
		mEnableWind = enable;
		cleanupWind();
	}
}


LLAudioBuffer *LLAudioEngine::getFreeBuffer()
{
	S32 i;
	for (i = 0; i < MAX_BUFFERS; i++)
	{
		if (!mBuffers[i])
		{
			mBuffers[i] = createBuffer();
			return mBuffers[i];
		}
	}


	// Grab the oldest unused buffer
	F32 max_age = -1.f;
	S32 buffer_id = -1;
	for (i = 0; i < MAX_BUFFERS; i++)
	{
		if (mBuffers[i])
		{
			if (!mBuffers[i]->mInUse)
			{
				if (mBuffers[i]->mLastUseTimer.getElapsedTimeF32() > max_age)
				{
					max_age = mBuffers[i]->mLastUseTimer.getElapsedTimeF32();
					buffer_id = i;
				}
			}
		}
	}

	if (buffer_id >= 0)
	{
		llinfos << "Taking over unused buffer " << buffer_id << llendl;
		//llinfos << "Flushing unused buffer!" << llendl;
		mBuffers[buffer_id]->mAudioDatap->mBufferp = NULL;
		delete mBuffers[buffer_id];
		mBuffers[buffer_id] = createBuffer();
		return mBuffers[buffer_id];
	}
	return NULL;
}


LLAudioChannel * LLAudioEngine::getFreeChannel(const F32 priority)
{
	S32 i;
	for (i = 0; i < mNumChannels; i++)
	{
		if (!mChannels[i])
		{
			// No channel allocated here, use it.
			mChannels[i] = createChannel();
			return mChannels[i];
		}
		else
		{
			// Channel is allocated but not playing right now, use it.
			if (!mChannels[i]->isPlaying() && !mChannels[i]->isWaiting())
			{
				mChannels[i]->cleanup();
				if (mChannels[i]->getSource())
				{
					mChannels[i]->getSource()->setChannel(NULL);
				}
				return mChannels[i];
			}
		}
	}

	// All channels used, check priorities.
	// Find channel with lowest priority and see if we want to replace it.
	F32 min_priority = 10000.f;
	LLAudioChannel *min_channelp = NULL;

	for (i = 0; i < mNumChannels; i++)
	{
		LLAudioChannel *channelp = mChannels[i];
		LLAudioSource *sourcep = channelp->getSource();
		if (sourcep->getPriority() < min_priority)
		{
			min_channelp = channelp;
			min_priority = sourcep->getPriority();
		}
	}

	if (min_priority > priority || !min_channelp)
	{
		// All playing channels have higher priority, return.
		return NULL;
	}

	// Flush the minimum priority channel, and return it.
	min_channelp->cleanup();
	min_channelp->getSource()->setChannel(NULL);
	return min_channelp;
}


void LLAudioEngine::cleanupBuffer(LLAudioBuffer *bufferp)
{
	S32 i;
	for (i = 0; i < MAX_BUFFERS; i++)
	{
		if (mBuffers[i] == bufferp)
		{
			delete mBuffers[i];
			mBuffers[i] = NULL;
		}
	}
}


bool LLAudioEngine::preloadSound(const LLUUID &uuid)
{
	gAudiop->getAudioData(uuid);	// We don't care about the return value, this is just to make sure
									// that we have an entry, which will mean that the audio engine knows about this

	if (gAudioDecodeMgrp->addDecodeRequest(uuid))
	{
		// This means that we do have a local copy, and we're working on decoding it.
		return true;
	}

	// At some point we need to have the audio/asset system check the static VFS
	// before it goes off and fetches stuff from the server.
	//llwarns << "Used internal preload for non-local sound" << llendl;
	return false;
}


bool LLAudioEngine::isWindEnabled()
{
	return mEnableWind;
}


void LLAudioEngine::setMuted(bool muted)
{
	mMuted = muted;
	enableWind(!mMuted);
}


void LLAudioEngine::setMasterGain(const F32 gain)
{
	mMasterGain = gain;
	setInternalGain(gain);
}

F32 LLAudioEngine::getMasterGain()
{
	return mMasterGain;
}

void LLAudioEngine::setSecondaryGain(S32 type, F32 gain)
{
	llassert(type < LLAudioEngine::AUDIO_TYPE_COUNT);
	
	mSecondaryGain[type] = gain;
}

F32 LLAudioEngine::getSecondaryGain(S32 type)
{
	return mSecondaryGain[type];
}

F32 LLAudioEngine::getInternetStreamGain()
{
	return mInternetStreamGain;
}

void LLAudioEngine::setMaxWindGain(F32 gain)
{
	mMaxWindGain = gain;
}


F64 LLAudioEngine::mapWindVecToGain(LLVector3 wind_vec)
{
	F64 gain = 0.0;
	
	gain = wind_vec.magVec();

	if (gain)
	{
		if (gain > 20)
		{
			gain = 20;
		}
		gain = gain/20.0;
	}

	return (gain);
} 


F64 LLAudioEngine::mapWindVecToPitch(LLVector3 wind_vec)
{
	LLVector3 listen_right;
	F64 theta;
  
	// Wind frame is in listener-relative coordinates
	LLVector3 norm_wind = wind_vec;
	norm_wind.normVec();
	listen_right.setVec(1.0,0.0,0.0);

	// measure angle between wind vec and listener right axis (on 0,PI)
	theta = acos(norm_wind * listen_right);

	// put it on 0, 1
	theta /= F_PI;					

	// put it on [0, 0.5, 0]
	if (theta > 0.5) theta = 1.0-theta;
	if (theta < 0) theta = 0;

	return (theta);
}


F64 LLAudioEngine::mapWindVecToPan(LLVector3 wind_vec)
{
	LLVector3 listen_right;
	F64 theta;
  
	// Wind frame is in listener-relative coordinates
	listen_right.setVec(1.0,0.0,0.0);

	LLVector3 norm_wind = wind_vec;
	norm_wind.normVec();

	// measure angle between wind vec and listener right axis (on 0,PI)
	theta = acos(norm_wind * listen_right);

	// put it on 0, 1
	theta /= F_PI;					

	return (theta);
}


void LLAudioEngine::triggerSound(const LLUUID &audio_uuid, const LLUUID& owner_id, const F32 gain,
								 const S32 type, const LLVector3d &pos_global)
{
	// Create a new source (since this can't be associated with an existing source.
	//llinfos << "Localized: " << audio_uuid << llendl;

	if (mMuted)
	{
		return;
	}

	LLUUID source_id;
	source_id.generate();

	LLAudioSource *asp = new LLAudioSource(source_id, owner_id, gain, type);
	gAudiop->addAudioSource(asp);
	if (pos_global.isExactlyZero())
	{
		asp->setAmbient(true);
	}
	else
	{
		asp->setPositionGlobal(pos_global);
	}
	asp->updatePriority();
	asp->play(audio_uuid);
}


void LLAudioEngine::setListenerPos(LLVector3 aVec)
{
	mListenerp->setPosition(aVec);  
}


LLVector3 LLAudioEngine::getListenerPos()
{
	if (mListenerp)
	{
		return(mListenerp->getPosition());  
	}
	else
	{
		return(LLVector3::zero);
	}
}


void LLAudioEngine::setListenerVelocity(LLVector3 aVec)
{
	mListenerp->setVelocity(aVec);  
}


void LLAudioEngine::translateListener(LLVector3 aVec)
{
	mListenerp->translate(aVec);	
}


void LLAudioEngine::orientListener(LLVector3 up, LLVector3 at)
{
	mListenerp->orient(up, at);  
}


void LLAudioEngine::setListener(LLVector3 pos, LLVector3 vel, LLVector3 up, LLVector3 at)
{
	mListenerp->set(pos,vel,up,at);  
}


void LLAudioEngine::setDopplerFactor(F32 factor)
{
	if (mListenerp)
	{
		mListenerp->setDopplerFactor(factor);  
	}
}


F32 LLAudioEngine::getDopplerFactor()
{
	if (mListenerp)
	{
		return mListenerp->getDopplerFactor();
	}
	else
	{
		return 0.f;
	}
}


void LLAudioEngine::setDistanceFactor(F32 factor)
{
	if (mListenerp)
	{
		mListenerp->setDistanceFactor(factor);  
	}
}


F32 LLAudioEngine::getDistanceFactor()
{
	if (mListenerp)
	{
		return mListenerp->getDistanceFactor();
	}
	else
	{
		return 0.f;
	}
}


void LLAudioEngine::setRolloffFactor(F32 factor)
{
	if (mListenerp)
	{
		mListenerp->setRolloffFactor(factor);  
	}
}


F32 LLAudioEngine::getRolloffFactor()
{
	if (mListenerp)
	{
		return mListenerp->getRolloffFactor();  
	}
	else
	{
		return 0.f;
	}
}


void LLAudioEngine::commitDeferredChanges()
{
	mListenerp->commitDeferredChanges();  
}


LLAudioSource *LLAudioEngine::findAudioSource(const LLUUID &source_id)
{
	source_map::iterator iter;
	iter = mAllSources.find(source_id);

	if (iter == mAllSources.end())
	{
		return NULL;
	}
	else
	{
		return iter->second;
	}
}


LLAudioData *LLAudioEngine::getAudioData(const LLUUID &audio_uuid)
{
	data_map::iterator iter;
	iter = mAllData.find(audio_uuid);
	if (iter == mAllData.end())
	{
		// Create the new audio data
		LLAudioData *adp = new LLAudioData(audio_uuid);
		mAllData[audio_uuid] = adp;
		return adp;
	}
	else
	{
		return iter->second;
	}
}

void LLAudioEngine::addAudioSource(LLAudioSource *asp)
{
	mAllSources[asp->getID()] = asp;
}


void LLAudioEngine::cleanupAudioSource(LLAudioSource *asp)
{
	source_map::iterator iter;
	iter = mAllSources.find(asp->getID());
	if (iter == mAllSources.end())
	{
		llwarns << "Cleaning up unknown audio source!" << llendl;
		return;
	}
	delete asp;
	mAllSources.erase(iter);
}


bool LLAudioEngine::hasDecodedFile(const LLUUID &uuid)
{
	std::string uuid_str;
	uuid.toString(uuid_str);

	std::string wav_path;
	wav_path = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,uuid_str);
	wav_path += ".dsf";

	if (gDirUtilp->fileExists(wav_path))
	{
		return true;
	}
	else
	{
		return false;
	}
}


bool LLAudioEngine::hasLocalFile(const LLUUID &uuid)
{
	// See if it's in the VFS.
	return gVFS->getExists(uuid, LLAssetType::AT_SOUND);
}


void LLAudioEngine::startNextTransfer()
{
	//llinfos << "LLAudioEngine::startNextTransfer()" << llendl;
	if (mCurrentTransfer.notNull() || getMuted())
	{
		//llinfos << "Transfer in progress, aborting" << llendl;
		return;
	}

	// Get the ID for the next asset that we want to transfer.
	// Pick one in the following order:
	LLUUID asset_id;
	S32 i;
	LLAudioSource *asp = NULL;
	LLAudioData *adp = NULL;
	data_map::iterator data_iter;

	// Check all channels for currently playing sounds.
	F32 max_pri = -1.f;
	for (i = 0; i < MAX_CHANNELS; i++)
	{
		if (!mChannels[i])
		{
			continue;
		}

		asp = mChannels[i]->getSource();
		if (!asp)
		{
			continue;
		}
		if (asp->getPriority() <= max_pri)
		{
			continue;
		}

		if (asp->getPriority() <= max_pri)
		{
			continue;
		}

		adp = asp->getCurrentData();
		if (!adp)
		{
			continue;
		}

		if (!adp->hasLocalData() && adp->hasValidData())
		{
			asset_id = adp->getID();
			max_pri = asp->getPriority();
		}
	}

	// Check all channels for currently queued sounds.
	if (asset_id.isNull())
	{
		max_pri = -1.f;
		for (i = 0; i < MAX_CHANNELS; i++)
		{
			if (!mChannels[i])
			{
				continue;
			}

			LLAudioSource *asp;
			asp = mChannels[i]->getSource();
			if (!asp)
			{
				continue;
			}

			if (asp->getPriority() <= max_pri)
			{
				continue;
			}

			adp = asp->getQueuedData();
			if (!adp)
			{
				continue;
			}

			if (!adp->hasLocalData() && adp->hasValidData())
			{
				asset_id = adp->getID();
				max_pri = asp->getPriority();
			}
		}
	}

	// Check all live channels for other sounds (preloads).
	if (asset_id.isNull())
	{
		max_pri = -1.f;
		for (i = 0; i < MAX_CHANNELS; i++)
		{
			if (!mChannels[i])
			{
				continue;
			}

			LLAudioSource *asp;
			asp = mChannels[i]->getSource();
			if (!asp)
			{
				continue;
			}

			if (asp->getPriority() <= max_pri)
			{
				continue;
			}


			for (data_iter = asp->mPreloadMap.begin(); data_iter != asp->mPreloadMap.end(); data_iter++)
			{
				LLAudioData *adp = data_iter->second;
				if (!adp)
				{
					continue;
				}

				if (!adp->hasLocalData() && adp->hasValidData())
				{
					asset_id = adp->getID();
					max_pri = asp->getPriority();
				}
			}
		}
	}

	// Check all sources
	if (asset_id.isNull())
	{
		max_pri = -1.f;
		source_map::iterator source_iter;
		for (source_iter = mAllSources.begin(); source_iter != mAllSources.end(); source_iter++)
		{
			asp = source_iter->second;
			if (!asp)
			{
				continue;
			}

			if (asp->getPriority() <= max_pri)
			{
				continue;
			}

			adp = asp->getCurrentData();
			if (adp && !adp->hasLocalData() && adp->hasValidData())
			{
				asset_id = adp->getID();
				max_pri = asp->getPriority();
				continue;
			}

			adp = asp->getQueuedData();
			if (adp && !adp->hasLocalData() && adp->hasValidData())
			{
				asset_id = adp->getID();
				max_pri = asp->getPriority();
				continue;
			}

			for (data_iter = asp->mPreloadMap.begin(); data_iter != asp->mPreloadMap.end(); data_iter++)
			{
				LLAudioData *adp = data_iter->second;
				if (!adp)
				{
					continue;
				}

				if (!adp->hasLocalData() && adp->hasValidData())
				{
					asset_id = adp->getID();
					max_pri = asp->getPriority();
					break;
				}
			}
		}
	}

	if (asset_id.notNull())
	{
		llinfos << "Getting asset data for: " << asset_id << llendl;
		gAudiop->mCurrentTransfer = asset_id;
		gAudiop->mCurrentTransferTimer.reset();
		gAssetStorage->getAssetData(asset_id, LLAssetType::AT_SOUND,
									assetCallback, NULL);
	}
	else
	{
		//llinfos << "No pending transfers?" << llendl;
	}
}


// static
void LLAudioEngine::assetCallback(LLVFS *vfs, const LLUUID &uuid, LLAssetType::EType type, void *user_data, S32 result_code, LLExtStat ext_status)
{
	if (result_code)
	{
		llinfos << "Boom, error in audio file transfer: " << LLAssetStorage::getErrorString( result_code ) << " (" << result_code << ")" << llendl;
		// Need to mark data as bad to avoid constant rerequests.
		LLAudioData *adp = gAudiop->getAudioData(uuid);
		if (adp)
        {
			adp->setHasValidData(false);
			adp->setHasLocalData(false);
			adp->setHasDecodedData(false);
		}
	}
	else
	{
		LLAudioData *adp = gAudiop->getAudioData(uuid);
		if (!adp)
        {
			// Should never happen
			llwarns << "Got asset callback without audio data for " << uuid << llendl;
        }
		else
		{
			adp->setHasValidData(true);
		    adp->setHasLocalData(true);
		    gAudioDecodeMgrp->addDecodeRequest(uuid);
		}
	}
	gAudiop->mCurrentTransfer = LLUUID::null;
	gAudiop->startNextTransfer();
}


//
// LLAudioSource implementation
//


LLAudioSource::LLAudioSource(const LLUUID& id, const LLUUID& owner_id, const F32 gain, const S32 type)
:	mID(id),
	mOwnerID(owner_id),
	mPriority(0.f),
	mGain(gain),
	mType(type),
	mAmbient(false),
	mLoop(false),
	mSyncMaster(false),
	mSyncSlave(false),
	mQueueSounds(false),
	mPlayedOnce(false),
	mChannelp(NULL),
	mCurrentDatap(NULL),
	mQueuedDatap(NULL)
{
}


LLAudioSource::~LLAudioSource()
{
	if (mChannelp)
	{
		// Stop playback of this sound
		mChannelp->setSource(NULL);
		mChannelp = NULL;
	}
}


void LLAudioSource::setChannel(LLAudioChannel *channelp)
{
	if (channelp == mChannelp)
	{
		return;
	}

	mChannelp = channelp;
}


void LLAudioSource::update()
{
	if (!getCurrentBuffer())
	{
		if (getCurrentData())
		{
			// Hack - try and load the sound.  Will do this as a callback
			// on decode later.
			if (getCurrentData()->load())
			{
				play(getCurrentData()->getID());
			}			
		}
	}
}

void LLAudioSource::updatePriority()
{
	if (isAmbient())
	{
		mPriority = 1.f;
	}
	else
	{
		// Priority is based on distance
		LLVector3 dist_vec;
		dist_vec.setVec(getPositionGlobal());
		dist_vec -= gAudiop->getListenerPos();
		F32 dist_squared = llmax(1.f, dist_vec.magVecSquared());

		mPriority = mGain / dist_squared;
	}
}

bool LLAudioSource::setupChannel()
{
	LLAudioData *adp = getCurrentData();

	if (!adp->getBuffer())
	{
		// We're not ready to play back the sound yet, so don't try and allocate a channel for it.
		//llwarns << "Aborting, no buffer" << llendl;
		return false;
	}


	if (!mChannelp)
	{
		// Update the priority, in case we need to push out another channel.
		updatePriority();

		setChannel(gAudiop->getFreeChannel(getPriority()));
	}

	if (!mChannelp)
	{
		// Ugh, we don't have any free channels.
		// Now we have to reprioritize.
		// For now, just don't play the sound.
		//llwarns << "Aborting, no free channels" << llendl;
		return false;
	}

	mChannelp->setSource(this);
	return true;
}


bool LLAudioSource::play(const LLUUID &audio_uuid)
{
	if (audio_uuid.isNull())
	{
		if (getChannel())
		{
			getChannel()->setSource(NULL);
			setChannel(NULL);
			addAudioData(NULL, true);
		}
	}
	// Reset our age timeout if someone attempts to play the source.
	mAgeTimer.reset();

	LLAudioData *adp = gAudiop->getAudioData(audio_uuid);

	bool has_buffer = gAudiop->updateBufferForData(adp, audio_uuid);


	addAudioData(adp);

	if (!has_buffer)
	{
		// Don't bother trying to set up a channel or anything, we don't have an audio buffer.
		return false;
	}

	if (!setupChannel())
	{
		return false;
	}

	if (isSyncSlave())
	{
		// A sync slave, it doesn't start playing until it's synced up with the master.
		// Flag this channel as waiting for sync, and return true.
		getChannel()->setWaiting(true);
		return true;
	}

	getChannel()->play();
	return true;
}


bool LLAudioSource::isDone()
{
	const F32 MAX_AGE = 60.f;
	const F32 MAX_UNPLAYED_AGE = 15.f;

	if (isLoop())
	{
		// Looped sources never die on their own.
		return false;
	}


	if (hasPendingPreloads())
	{
		return false;
	}

	if (mQueuedDatap)
	{
		// Don't kill this sound if we've got something queued up to play.
		return false;
	}

	F32 elapsed = mAgeTimer.getElapsedTimeF32();

	// This is a single-play source
	if (!mChannelp)
	{
		if ((elapsed > MAX_UNPLAYED_AGE) || mPlayedOnce)
		{
			// We don't have a channel assigned, and it's been
			// over 5 seconds since we tried to play it.  Don't bother.
			//llinfos << "No channel assigned, source is done" << llendl;
			return true;
		}
		else
		{
			return false;
		}
	}

	if (mChannelp->isPlaying())
	{
		if (elapsed > MAX_AGE)
		{
			// Arbitarily cut off non-looped sounds when they're old.
			return true;
		}
		else
		{
			// Sound is still playing and we haven't timed out, don't kill it.
			return false;
		}
	}

	if ((elapsed > MAX_UNPLAYED_AGE) || mPlayedOnce)
	{
		// The sound isn't playing back after 5 seconds or we're already done playing it, kill it.
		return true;
	}

	return false;
}


void LLAudioSource::addAudioData(LLAudioData *adp, const bool set_current)
{
	// Only handle a single piece of audio data associated with a source right now,
	// until I implement prefetch.
	if (set_current)
	{
		if (!mCurrentDatap)
		{
			mCurrentDatap = adp;
			if (mChannelp)
			{
				mChannelp->updateBuffer();
				mChannelp->play();
			}

			// Make sure the audio engine knows that we want to request this sound.
			gAudiop->startNextTransfer();
			return;
		}
		else if (mQueueSounds)
		{
			// If we have current data, and we're queuing, put
			// the object onto the queue.
			if (mQueuedDatap)
			{
				// We only queue one sound at a time, and it's a FIFO.
				// Don't put it onto the queue.
				return;
			}

			if (adp == mCurrentDatap && isLoop())
			{
				// No point in queueing the same sound if
				// we're looping.
				return;
			}
			mQueuedDatap = adp;

			// Make sure the audio engine knows that we want to request this sound.
			gAudiop->startNextTransfer();
		}
		else
		{
			if (mCurrentDatap != adp)
			{
				// Right now, if we're currently playing this sound in a channel, we
				// update the buffer that the channel's associated with
				// and play it.  This may not be the correct behavior.
				mCurrentDatap = adp;
				if (mChannelp)
				{
					mChannelp->updateBuffer();
					mChannelp->play();
				}
				// Make sure the audio engine knows that we want to request this sound.
				gAudiop->startNextTransfer();
			}
		}
	}
	else
	{
		// Add it to the preload list.
		mPreloadMap[adp->getID()] = adp;
		gAudiop->startNextTransfer();
	}
}


bool LLAudioSource::hasPendingPreloads() const
{
	// Check to see if we've got any preloads on deck for this source
	data_map::const_iterator iter;
	for (iter = mPreloadMap.begin(); iter != mPreloadMap.end(); iter++)
	{
		LLAudioData *adp = iter->second;
		if (!adp->hasDecodedData())
		{
			// This source is still waiting for a preload
			return true;
		}
	}

	return false;
}


LLAudioData *LLAudioSource::getCurrentData()
{
	return mCurrentDatap;
}

LLAudioData *LLAudioSource::getQueuedData()
{
	return mQueuedDatap;
}

LLAudioBuffer *LLAudioSource::getCurrentBuffer()
{
	if (!mCurrentDatap)
	{
		return NULL;
	}

	return mCurrentDatap->getBuffer();
}




//
// LLAudioChannel implementation
//


LLAudioChannel::LLAudioChannel() :
	mCurrentSourcep(NULL),
	mCurrentBufferp(NULL),
	mLoopedThisFrame(false),
	mWaiting(false),
	mSecondaryGain(1.0f)
{
}


LLAudioChannel::~LLAudioChannel()
{
	// Need to disconnect any sources which are using this channel.
	//llinfos << "Cleaning up audio channel" << llendl;
	if (mCurrentSourcep)
	{
		mCurrentSourcep->setChannel(NULL);
	}
	mCurrentBufferp = NULL;
}


void LLAudioChannel::setSource(LLAudioSource *sourcep)
{
	//llinfos << this << ": setSource(" << sourcep << ")" << llendl;

	if (!sourcep)
	{
		// Clearing the source for this channel, don't need to do anything.
		//llinfos << "Clearing source for channel" << llendl;
		cleanup();
		mCurrentSourcep = NULL;
		mWaiting = false;
		return;
	}

	if (sourcep == mCurrentSourcep)
	{
		// Don't reallocate the channel, this will make FMOD goofy.
		//llinfos << "Calling setSource with same source!" << llendl;
	}

	mCurrentSourcep = sourcep;


	updateBuffer();
	update3DPosition();
}


bool LLAudioChannel::updateBuffer()
{
	if (!mCurrentSourcep)
	{
		// This channel isn't associated with any source, nothing
		// to be updated
		return false;
	}

	// Initialize the channel's gain setting for this sound.
	if(gAudiop)
	{
		setSecondaryGain(gAudiop->getSecondaryGain(mCurrentSourcep->getType()));
	}

	LLAudioBuffer *bufferp = mCurrentSourcep->getCurrentBuffer();
	if (bufferp == mCurrentBufferp)
	{
		if (bufferp)
		{
			// The source hasn't changed what buffer it's playing
			bufferp->mLastUseTimer.reset();
			bufferp->mInUse = true;
		}
		return false;
	}

	//
	// The source changed what buffer it's playing.  We need to clean up
	// the existing channel
	//
	cleanup();

	mCurrentBufferp = bufferp;
	if (bufferp)
	{
		bufferp->mLastUseTimer.reset();
		bufferp->mInUse = true;
	}

	if (!mCurrentBufferp)
	{
		// There's no new buffer to be played, so we just abort.
		return false;
	}

	return true;
}




//
// LLAudioData implementation
//


LLAudioData::LLAudioData(const LLUUID &uuid) :
	mID(uuid),
	mBufferp(NULL),
	mHasLocalData(false),
	mHasDecodedData(false),
	mHasValidData(true)
{
	if (uuid.isNull())
	{
		// This is a null sound.
		return;
	}
	
	if (gAudiop && gAudiop->hasDecodedFile(uuid))
	{
		// Already have a decoded version, don't need to decode it.
		mHasLocalData = true;
		mHasDecodedData = true;
	}
	else if (gAssetStorage && gAssetStorage->hasLocalAsset(uuid, LLAssetType::AT_SOUND))
	{
		mHasLocalData = true;
	}
}


bool LLAudioData::load()
{
	// For now, just assume we're going to use one buffer per audiodata.
	if (mBufferp)
	{
		// We already have this sound in a buffer, don't do anything.
		llinfos << "Already have a buffer for this sound, don't bother loading!" << llendl;
		return true;
	}
	
	mBufferp = gAudiop->getFreeBuffer();
	if (!mBufferp)
	{
		// No free buffers, abort.
		llinfos << "Not able to allocate a new audio buffer, aborting." << llendl;
		return false;
	}

	std::string uuid_str;
	std::string wav_path;
	mID.toString(uuid_str);
	wav_path= gDirUtilp->getExpandedFilename(LL_PATH_CACHE,uuid_str) + ".dsf";

	if (!mBufferp->loadWAV(wav_path))
	{
		// Hrm.  Right now, let's unset the buffer, since it's empty.
		gAudiop->cleanupBuffer(mBufferp);
		mBufferp = NULL;

		return false;
	}
	mBufferp->mAudioDatap = this;
	return true;
}


