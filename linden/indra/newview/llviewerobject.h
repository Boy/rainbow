/** 
 * @file llviewerobject.h
 * @brief Description of LLViewerObject class, which is the base class for most objects in the viewer.
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

#ifndef LL_LLVIEWEROBJECT_H
#define LL_LLVIEWEROBJECT_H

#include <map>

#include "llassetstorage.h"
#include "lldarrayptr.h"
#include "llhudtext.h"
#include "llhudicon.h"
#include "llinventory.h"
#include "llmemory.h"
#include "llmemtype.h"
#include "llprimitive.h"
#include "lluuid.h"
#include "llvoinventorylistener.h"
#include "object_flags.h"
#include "llquaternion.h"
#include "v3dmath.h"
#include "v3math.h"
#include "llvertexbuffer.h"

class LLAgent;			// TODO: Get rid of this.
class LLAudioSource;
class LLAudioSourceVO;
class LLBBox;
class LLDataPacker;
class LLColor4;
class LLFrameTimer;
class LLDrawable;
class LLHost;
class LLWorld;
class LLNameValue;
class LLNetMap;
class LLMessageSystem;
class LLPrimitive;
class LLPipeline;
class LLTextureEntry;
class LLViewerImage;
class LLViewerInventoryItem;
class LLViewerObject;
class LLViewerPartSourceScript;
class LLViewerRegion;
class LLViewerObjectMedia;
class LLVOInventoryListener;

typedef enum e_object_update_type
{
	OUT_FULL,
	OUT_TERSE_IMPROVED,
	OUT_FULL_COMPRESSED,
	OUT_FULL_CACHED,
} EObjectUpdateType;


// callback typedef for inventory
typedef void (*inventory_callback)(LLViewerObject*,
								   InventoryObjectList*,
								   S32 serial_num,
								   void*);

// a small struct for keeping track of joints
struct LLVOJointInfo
{
	EHavokJointType mJointType;
	LLVector3 mPivot;			// parent-frame
	// whether the below an axis or anchor (and thus its frame)
	// depends on the joint type:
	//     HINGE   ==>   axis=parent-frame
	//     P2P     ==>   anchor=child-frame
	LLVector3 mAxisOrAnchor;	
};

// for exporting textured materials from SL
struct LLMaterialExportInfo
{
public:
	LLMaterialExportInfo(S32 mat_index, S32 texture_index, LLColor4 color) : 
	  mMaterialIndex(mat_index), mTextureIndex(texture_index), mColor(color) {};

	S32			mMaterialIndex;
	S32			mTextureIndex;
	LLColor4	mColor;
};

//============================================================================

class LLViewerObject : public LLPrimitive, public LLRefCount
{
protected:
	~LLViewerObject(); // use unref()

	// TomY: Provide for a list of extra parameter structures, mapped by structure name
	struct ExtraParameter
	{
		BOOL in_use;
		LLNetworkData *data;
	};
	std::map<U16, ExtraParameter*> mExtraParameterList;

public:
	typedef std::list<LLPointer<LLViewerObject> > child_list_t;
	typedef const child_list_t const_child_list_t;

	LLViewerObject(const LLUUID &id, const LLPCode type, LLViewerRegion *regionp, BOOL is_global = FALSE);
	MEM_TYPE_NEW(LLMemType::MTYPE_OBJECT);

	virtual void markDead();				// Mark this object as dead, and clean up its references
	BOOL isDead() const									{return mDead;}
	BOOL isOrphaned() const								{ return mOrphaned; }
	BOOL isParticleSource() const;

	static void initVOClasses();
	static void cleanupVOClasses();

	void			addNVPair(const std::string& data);
	BOOL			removeNVPair(const std::string& name);
	LLNameValue*	getNVPair(const std::string& name) const;			// null if no name value pair by that name

	// Object create and update functions
	virtual BOOL	idleUpdate(LLAgent &agent, LLWorld &world, const F64 &time);

	// Types of media we can associate
	enum { MEDIA_TYPE_NONE = 0, MEDIA_TYPE_WEB_PAGE = 1 };

	// Return codes for processUpdateMessage
	enum { MEDIA_URL_REMOVED = 0x1, MEDIA_URL_ADDED = 0x2, MEDIA_URL_UPDATED = 0x4, INVALID_UPDATE = 0x80000000 };

	virtual U32		processUpdateMessage(LLMessageSystem *mesgsys,
										void **user_data,
										U32 block_num,
										const EObjectUpdateType update_type,
										LLDataPacker *dp);


	virtual BOOL    isActive() const; // Whether this object needs to do an idleUpdate.
	BOOL			onActiveList() const				{return mOnActiveList;}
	void			setOnActiveList(BOOL on_active)		{ mOnActiveList = on_active; }

	virtual BOOL	isAttachment() const { return FALSE; }
	virtual BOOL	isHUDAttachment() const { return FALSE; }
	virtual void 	updateRadius() {};
	virtual F32 	getVObjRadius() const; // default implemenation is mDrawable->getRadius()
	
	BOOL 			isJointChild() const { return mJointInfo ? TRUE : FALSE; } 
	EHavokJointType	getJointType() const { return mJointInfo ? mJointInfo->mJointType : HJT_INVALID; }
	// for jointed and other parent-relative hacks
	LLViewerObject* getSubParent();
	const LLViewerObject* getSubParent() const;
	
	// Object visiblility and GPW functions
	virtual void setPixelAreaAndAngle(LLAgent &agent); // Override to generate accurate apparent angle and area

	virtual U32 getNumVertices() const;
	virtual U32 getNumIndices() const;
	S32 getNumFaces() const { return mNumFaces; }

	// Graphical stuff for objects - maybe broken out into render class later?
	virtual void updateTextures(LLAgent &agent);
	virtual void boostTexturePriority(BOOL boost_children = TRUE);	// When you just want to boost priority of this object
	
	virtual LLDrawable* createDrawable(LLPipeline *pipeline);
	virtual BOOL		updateGeometry(LLDrawable *drawable);
	virtual void		updateFaceSize(S32 idx);
	virtual BOOL		updateLOD();
	virtual BOOL		setDrawableParent(LLDrawable* parentp);
	F32					getRotTime() { return mRotTime; }
	void				resetRot();
	void				applyAngularVelocity(F32 dt);

	void setLineWidthForWindowSize(S32 window_width);

	static void increaseArrowLength();				// makes axis arrows for selections longer
	static void decreaseArrowLength();				// makes axis arrows for selections shorter

	// Accessor functions
	LLViewerRegion* getRegion() const				{ return mRegionp; }

	BOOL isSelected() const							{ return mUserSelected; }
	virtual void setSelected(BOOL sel)				{ mUserSelected = sel; mRotTime = 0.f;}

	const LLUUID &getID() const						{ return mID; }
	U32 getLocalID() const							{ return mLocalID; }
	U32 getCRC() const								{ return mTotalCRC; }

	virtual BOOL isFlexible() const					{ return FALSE; }
	virtual BOOL isSculpted() const 				{ return FALSE; }

	// This method returns true if the object is over land owned by
	// the agent.
	BOOL isOverAgentOwnedLand() const;

	// True if over land owned by group of which the agent is
	// either officer or member.
	BOOL isOverGroupOwnedLand() const;

	/*
	// This method will scan through this object, and then query the
	// selection manager to see if the local agent probably has the
	// ability to modify the object. Since this calls into the
	// selection manager, you should avoid calling this method from
	// there.
	BOOL isProbablyModifiable() const;
	*/

	virtual BOOL setParent(LLViewerObject* parent);
	virtual void addChild(LLViewerObject *childp);
	virtual void removeChild(LLViewerObject *childp);
	const_child_list_t& getChildren() const { 	return mChildList; }
	S32 numChildren() const { return mChildList.size(); }
	void addThisAndAllChildren(LLDynamicArray<LLViewerObject*>& objects);
	void addThisAndNonJointChildren(LLDynamicArray<LLViewerObject*>& objects);
	BOOL isChild(LLViewerObject *childp) const;
	BOOL isSeat() const;
	

	//detect if given line segment (in agent space) intersects with this viewer object.
	//returns TRUE if intersection detected and returns information about intersection
	virtual BOOL lineSegmentIntersect(const LLVector3& start, const LLVector3& end,
									  S32 face = -1,                          // which face to check, -1 = ALL_SIDES
									  BOOL pick_transparent = FALSE,
									  S32* face_hit = NULL,                   // which face was hit
									  LLVector3* intersection = NULL,         // return the intersection point
									  LLVector2* tex_coord = NULL,            // return the texture coordinates of the intersection point
									  LLVector3* normal = NULL,               // return the surface normal at the intersection point
									  LLVector3* bi_normal = NULL             // return the surface bi-normal at the intersection point
		);
	
	virtual BOOL lineSegmentBoundingBox(const LLVector3& start, const LLVector3& end);

	virtual const LLVector3d getPositionGlobal() const;
	virtual const LLVector3 &getPositionRegion() const;
	virtual const LLVector3 getPositionEdit() const;
	virtual const LLVector3 &getPositionAgent() const;
	virtual const LLVector3 getRenderPosition() const;

	virtual const LLVector3 getPivotPositionAgent() const; // Usually = to getPositionAgent, unless like flex objects it's not

	LLViewerObject* getRootEdit() const;

	const LLQuaternion getRotationRegion() const;
	const LLQuaternion getRotationEdit() const;
	const LLQuaternion getRenderRotation() const;
	virtual	const LLMatrix4 getRenderMatrix() const;

	void setPosition(const LLVector3 &pos, BOOL damped = FALSE);
	void setPositionGlobal(const LLVector3d &position, BOOL damped = FALSE);
	void setPositionRegion(const LLVector3 &position, BOOL damped = FALSE);
	void setPositionEdit(const LLVector3 &position, BOOL damped = FALSE);
	void setPositionAgent(const LLVector3 &pos_agent, BOOL damped = FALSE);
	void setPositionParent(const LLVector3 &pos_parent, BOOL damped = FALSE);
	void setPositionAbsoluteGlobal( const LLVector3d &pos_global, BOOL damped = FALSE );

	virtual const LLMatrix4& getWorldMatrix(LLXformMatrix* xform) const		{ return xform->getWorldMatrix(); }

	inline void setRotation(const F32 x, const F32 y, const F32 z, BOOL damped = FALSE);
	inline void setRotation(const LLQuaternion& quat, BOOL damped = FALSE);
	void sendRotationUpdate() const;

	/*virtual*/	void	setNumTEs(const U8 num_tes);
	/*virtual*/	void	setTE(const U8 te, const LLTextureEntry &texture_entry);
	/*virtual*/ S32		setTETexture(const U8 te, const LLUUID &uuid);
	S32 setTETextureCore(const U8 te, const LLUUID& uuid, LLHost host);
	/*virtual*/ S32		setTEColor(const U8 te, const LLColor3 &color);
	/*virtual*/ S32		setTEColor(const U8 te, const LLColor4 &color);
	/*virtual*/ S32		setTEScale(const U8 te, const F32 s, const F32 t);
	/*virtual*/ S32		setTEScaleS(const U8 te, const F32 s);
	/*virtual*/ S32		setTEScaleT(const U8 te, const F32 t);
	/*virtual*/ S32		setTEOffset(const U8 te, const F32 s, const F32 t);
	/*virtual*/ S32		setTEOffsetS(const U8 te, const F32 s);
	/*virtual*/ S32		setTEOffsetT(const U8 te, const F32 t);
	/*virtual*/ S32		setTERotation(const U8 te, const F32 r);
	/*virtual*/	S32		setTEBumpmap(const U8 te, const U8 bump );
	/*virtual*/	S32		setTETexGen(const U8 te, const U8 texgen );
	/*virtual*/	S32		setTEShiny(const U8 te, const U8 shiny );
	/*virtual*/	S32		setTEFullbright(const U8 te, const U8 fullbright );
	/*virtual*/	S32		setTEMediaFlags(const U8 te, const U8 media_flags );
	/*virtual*/ S32     setTEGlow(const U8 te, const F32 glow);
	/*virtual*/	BOOL	setMaterial(const U8 material);
	virtual		void	setTEImage(const U8 te, LLViewerImage *imagep); // Not derived from LLPrimitive
	LLViewerImage		*getTEImage(const U8 te) const;
	
	void fitFaceTexture(const U8 face);
	void sendTEUpdate() const;			// Sends packed representation of all texture entry information
	
	virtual void setScale(const LLVector3 &scale, BOOL damped = FALSE);

	void sendShapeUpdate();

	U8 getState()							{ return mState; }

	F32 getAppAngle() const					{ return mAppAngle; }
	F32 getPixelArea() const				{ return mPixelArea; }
	void setPixelArea(F32 area)				{ mPixelArea = area; }
	F32 getMaxScale() const;
	F32 getMidScale() const;
	F32 getMinScale() const;

	// Owner id is this object's owner
	void setAttachedSound(const LLUUID &audio_uuid, const LLUUID& owner_id, const F32 gain, const U8 flags);
	void adjustAudioGain(const F32 gain);
	void clearAttachedSound()								{ mAudioSourcep = NULL; }

	 // Create if necessary
	LLAudioSource *getAudioSource(const LLUUID& owner_id);
	bool isAudioSource() {return mAudioSourcep != NULL;}

	U8 getMediaType() const;
	void setMediaType(U8 media_type);

	std::string getMediaURL() const;
	void setMediaURL(const std::string& media_url);

	BOOL getMediaPassedWhitelist() const;
	void setMediaPassedWhitelist(BOOL passed);

	void sendMaterialUpdate() const;

	void setCanSelect(BOOL canSelect);

	void setDebugText(const std::string &utf8text);
	void setIcon(LLViewerImage* icon_image);
	void clearIcon();

	void markForUpdate(BOOL priority);
	void updateVolume(const LLVolumeParams& volume_params);
	virtual	void updateSpatialExtents(LLVector3& min, LLVector3& max);
	virtual F32 getBinRadius();
	
	LLBBox				getBoundingBoxAgent() const;

	void updatePositionCaches() const; // Update the global and region position caches from the object (and parent's) xform.
	void updateText(); // update text label position
	virtual void updateDrawable(BOOL force_damped); // force updates on static objects

	void setDrawableState(U32 state, BOOL recursive = TRUE);
	void clearDrawableState(U32 state, BOOL recursive = TRUE);

	// Called when the drawable shifts
	virtual void onShift(const LLVector3 &shift_vector)	{ }
		
	//////////////////////////////////////
	//
	// Inventory methods
	//

	// This function is called when someone is interested in a viewer
	// object's inventory. The callback is called as soon as the
	// viewer object has the inventory stored locally.
	void registerInventoryListener(LLVOInventoryListener* listener, void* user_data);
	void removeInventoryListener(LLVOInventoryListener* listener);
	BOOL isInventoryPending() { return mInventoryPending; }
	void clearInventoryListeners();
	void requestInventory();
	void fetchInventoryFromServer();
	static void processTaskInv(LLMessageSystem* msg, void** user_data);
	void removeInventory(const LLUUID& item_id);

	// The updateInventory() call potentially calls into the selection
	// manager, so do no call updateInventory() from the selection
	// manager until we have better iterators.
	void updateInventory(LLViewerInventoryItem* item, U8 key, bool is_new);
	void updateInventoryLocal(LLInventoryItem* item, U8 key); // Update without messaging.
	LLInventoryObject* getInventoryObject(const LLUUID& item_id);
	void getInventoryContents(InventoryObjectList& objects);
	LLInventoryObject* getInventoryRoot();
	LLViewerInventoryItem* getInventoryItemByAsset(const LLUUID& asset_id);
	S16 getInventorySerial() const { return mInventorySerialNum; }

	// These functions does viewer-side only object inventory modifications
	void updateViewerInventoryAsset(
		const LLViewerInventoryItem* item,
		const LLUUID& new_asset);

	// This function will make sure that we refresh the inventory.
	void dirtyInventory();
	BOOL isInventoryDirty() { return mInventoryDirty; }

	// save a script, which involves removing the old one, and rezzing
	// in the new one. This method should be called with the asset id
	// of the new and old script AFTER the bytecode has been saved.
	void saveScript(const LLViewerInventoryItem* item, BOOL active, bool is_new);

	// move an inventory item out of the task and into agent
	// inventory. This operation is based on messaging. No permissions
	// checks are made on the viewer - the server will double check.
	void moveInventory(const LLUUID& agent_folder, const LLUUID& item_id);

	// Find the number of instances of this object's inventory that are of the given type
	S32 countInventoryContents( LLAssetType::EType type );

	BOOL			permAnyOwner() const;	
	BOOL			permYouOwner() const;
	BOOL			permGroupOwner() const;
	BOOL			permOwnerModify() const;
	BOOL			permModify() const;	
	BOOL			permCopy() const;	
	BOOL			permMove() const;		
	BOOL			permTransfer() const;
	inline BOOL		usePhysics() const				{ return ((mFlags & FLAGS_USE_PHYSICS) != 0); }
	inline BOOL		flagScripted() const			{ return ((mFlags & FLAGS_SCRIPTED) != 0); }
	inline BOOL		flagHandleTouch() const			{ return ((mFlags & FLAGS_HANDLE_TOUCH) != 0); }
	inline BOOL		flagTakesMoney() const			{ return ((mFlags & FLAGS_TAKES_MONEY) != 0); }
	inline BOOL		flagPhantom() const				{ return ((mFlags & FLAGS_PHANTOM) != 0); }
	inline BOOL		flagInventoryEmpty() const		{ return ((mFlags & FLAGS_INVENTORY_EMPTY) != 0); }
	inline BOOL		flagCastShadows() const			{ return ((mFlags & FLAGS_CAST_SHADOWS) != 0); }
	inline BOOL		flagAllowInventoryAdd() const	{ return ((mFlags & FLAGS_ALLOW_INVENTORY_DROP) != 0); }
	inline BOOL		flagTemporary() const			{ return ((mFlags & FLAGS_TEMPORARY) != 0); }
	inline BOOL		flagTemporaryOnRez() const		{ return ((mFlags & FLAGS_TEMPORARY_ON_REZ) != 0); }
	inline BOOL		flagAnimSource() const			{ return ((mFlags & FLAGS_ANIM_SOURCE) != 0); }
	inline BOOL		flagCameraSource() const		{ return ((mFlags & FLAGS_CAMERA_SOURCE) != 0); }
	inline BOOL		flagCameraDecoupled() const		{ return ((mFlags & FLAGS_CAMERA_DECOUPLED) != 0); }

	bool getIncludeInSearch() const;
	void setIncludeInSearch(bool include_in_search);

	// Does "open" object menu item apply?
	BOOL allowOpen() const;

	void setClickAction(U8 action) { mClickAction = action; }
	U8 getClickAction() const { return mClickAction; }
	bool specialHoverCursor() const;	// does it have a special hover cursor?

	void			setRegion(LLViewerRegion *regionp);
	virtual void	updateRegion(LLViewerRegion *regionp) {}

	void updateFlags();
	BOOL setFlags(U32 flag, BOOL state);
	
	virtual void dump() const;
	static U32		getNumZombieObjects()			{ return sNumZombieObjects; }

	void printNameValuePairs() const;

	virtual S32 getLOD() const { return 3; } 
	virtual U32 getPartitionType() const;
	virtual void dirtySpatialGroup() const;
	virtual void dirtyMesh();

	virtual LLNetworkData* getParameterEntry(U16 param_type) const;
	virtual bool setParameterEntry(U16 param_type, const LLNetworkData& new_value, bool local_origin);
	virtual BOOL getParameterEntryInUse(U16 param_type) const;
	virtual bool setParameterEntryInUse(U16 param_type, BOOL in_use, bool local_origin);
	// Called when a parameter is changed
	virtual void parameterChanged(U16 param_type, bool local_origin);
	virtual void parameterChanged(U16 param_type, LLNetworkData* data, BOOL in_use, bool local_origin);
	
	friend class LLViewerObjectList;
	friend class LLViewerMediaList;

public:
	//counter-translation
	void resetChildrenPosition(const LLVector3& offset, BOOL simplified = FALSE) ;
	//counter-rotation
	void resetChildrenRotationAndPosition(const std::vector<LLQuaternion>& rotations, 
											const std::vector<LLVector3>& positions) ;
	void saveUnselectedChildrenRotation(std::vector<LLQuaternion>& rotations) ;
	void saveUnselectedChildrenPosition(std::vector<LLVector3>& positions) ;
	std::vector<LLVector3> mUnselectedChildrenPositions ;

private:
	ExtraParameter* createNewParameterEntry(U16 param_type);
	ExtraParameter* getExtraParameterEntry(U16 param_type) const;
	ExtraParameter* getExtraParameterEntryCreate(U16 param_type);
	bool unpackParameterEntry(U16 param_type, LLDataPacker *dp);
	
public:
	//
	// Viewer-side only types - use the LL_PCODE_APP mask.
	//
	typedef enum e_vo_types
	{
		LL_VO_CLOUDS =				LL_PCODE_APP | 0x20,
		LL_VO_SURFACE_PATCH =		LL_PCODE_APP | 0x30,
		//LL_VO_STARS =				LL_PCODE_APP | 0x40,
		LL_VO_SQUARE_TORUS =		LL_PCODE_APP | 0x50,
		LL_VO_SKY =					LL_PCODE_APP | 0x60,
		LL_VO_WATER =				LL_PCODE_APP | 0x70,
		LL_VO_GROUND =				LL_PCODE_APP | 0x80,
		LL_VO_PART_GROUP =			LL_PCODE_APP | 0x90,
		LL_VO_TRIANGLE_TORUS =		LL_PCODE_APP | 0xa0,
		LL_VO_WL_SKY =				LL_PCODE_APP | 0xb0, // should this be moved to 0x40?
		LL_VO_HUD_PART_GROUP =		LL_PCODE_APP | 0xc0,
	} EVOType;

	LLUUID			mID;

	// unique within region, not unique across regions
	// Local ID = 0 is not used
	U32				mLocalID;

	// Last total CRC received from sim, used for caching
	U32				mTotalCRC;

	LLPointer<LLViewerImage> *mTEImages;

	// Selection, picking and rendering variables
	U32				mGLName;			// GL "name" used by selection code
	BOOL			mbCanSelect;		// true if user can select this object by clicking

	// Grabbed from UPDATE_FLAGS
	U32				mFlags;

	// Pipeline classes
	LLPointer<LLDrawable> mDrawable;

	// Band-aid to select object after all creation initialization is done
	BOOL mCreateSelected;

	// Replace textures with web pages on this object while drawing
	BOOL mRenderMedia;

	// In bits
	S32				mBestUpdatePrecision;

	// TODO: Make all this stuff private.  JC
	LLPointer<LLHUDText> mText;
	LLPointer<LLHUDIcon> mIcon;

	static			BOOL		sUseSharedDrawables;

protected:
	// delete an item in the inventory, but don't tell the
	// server. This is used internally by remove, update, and
	// savescript.
	void deleteInventoryItem(const LLUUID& item_id);

	// do the update/caching logic. called by saveScript and
	// updateInventory.
	void doUpdateInventory(LLPointer<LLViewerInventoryItem>& item, U8 key, bool is_new);


	static LLViewerObject *createObject(const LLUUID &id, LLPCode pcode, LLViewerRegion *regionp);

	BOOL setData(const U8 *datap, const U32 data_size);

	// Hide or show HUD, icon and particles
	void	hideExtraDisplayItems( BOOL hidden );

	//////////////////////////
	//
	// inventory functionality
	//

	static void processTaskInvFile(void** user_data, S32 error_code, LLExtStat ext_status);
	void loadTaskInvFile(const std::string& filename);
	void doInventoryCallback();
	
	BOOL isOnMap();

	void unpackParticleSource(const S32 block_num, const LLUUID& owner_id);
	void unpackParticleSource(LLDataPacker &dp, const LLUUID& owner_id);
	void deleteParticleSource();
	void setParticleSource(const LLPartSysData& particle_parameters, const LLUUID& owner_id);

private:
	void setNameValueList(const std::string& list);		// clears nv pairs and then individually adds \n separated NV pairs from \0 terminated string
	void deleteTEImages(); // correctly deletes list of images
	
protected:
	typedef std::map<char *, LLNameValue *> name_value_map_t;
	name_value_map_t mNameValuePairs;	// Any name-value pairs stored by script

	child_list_t	mChildList;
	
	F64				mLastInterpUpdateSecs;			// Last update for purposes of interpolation
	F64				mLastMessageUpdateSecs;			// Last update from a message from the simulator
	TPACKETID		mLatestRecvPacketID;			// Latest time stamp on message from simulator
	// extra data sent from the sim...currently only used for tree species info
	U8* mData;

	LLPointer<LLViewerPartSourceScript>		mPartSourcep;	// Particle source associated with this object.
	LLAudioSourceVO* mAudioSourcep;
	F32				mAudioGain;
	
	F32				mAppAngle;	// Apparent visual arc in degrees
	F32				mPixelArea; // Apparent area in pixels

	// This is the object's inventory from the viewer's perspective.
	InventoryObjectList* mInventory;
	class LLInventoryCallbackInfo
	{
	public:
		~LLInventoryCallbackInfo();
		LLVOInventoryListener* mListener;
		void* mInventoryData;
	};
	typedef std::list<LLInventoryCallbackInfo*> callback_list_t;
	callback_list_t mInventoryCallbacks;
	S16 mInventorySerialNum;

	LLViewerRegion	*mRegionp;					// Region that this object belongs to.
	BOOL			mInventoryPending;
	BOOL			mInventoryDirty;
	BOOL			mDead;
	BOOL			mOrphaned;					// This is an orphaned child
	BOOL			mUserSelected;				// Cached user select information
	BOOL			mOnActiveList;
	BOOL			mOnMap;						// On the map.
	BOOL			mStatic;					// Object doesn't move.
	S32				mNumFaces;

	F32				mTimeDilation;				// Time dilation sent with the object.
	F32				mRotTime;					// Amount (in seconds) that object has rotated according to angular velocity (llSetTargetOmega)
	LLQuaternion	mLastRot;					// last rotation received from the simulator

	LLVOJointInfo*  mJointInfo;
	U8				mState;	// legacy
	LLViewerObjectMedia* mMedia;	// NULL if no media associated
	U8 mClickAction;

	static			U32			sNumZombieObjects;			// Objects which are dead, but not deleted

	static			BOOL		sMapDebug;					// Map render mode
	static			LLColor4	sEditSelectColor;
	static			LLColor4	sNoEditSelectColor;
	static			F32			sCurrentPulse;
	static			BOOL		sPulseEnabled;

	static			S32			sAxisArrowLength;

	// These two caches are only correct for non-parented objects right now!
	mutable LLVector3		mPositionRegion;
	mutable LLVector3		mPositionAgent;

private:	
	static S32 sNumObjects;

public:
	const LLUUID &getAttachmentItemID() const { return mAttachmentItemID; }
	void setAttachmentItemID(const LLUUID &id) { mAttachmentItemID = id; }
	const LLUUID &extractAttachmentItemID(); // find&set the inventory item ID of the attached object
private:
	LLUUID mAttachmentItemID; // ItemID when item is in user inventory.
};

///////////////////
//
// Inlines
//
//

inline void LLViewerObject::setRotation(const LLQuaternion& quat, BOOL damped)
{
	LLPrimitive::setRotation(quat);
	setChanged(ROTATED | SILHOUETTE);
	updateDrawable(damped);
}

inline void LLViewerObject::setRotation(const F32 x, const F32 y, const F32 z, BOOL damped)
{
	LLPrimitive::setRotation(x, y, z);
	setChanged(ROTATED | SILHOUETTE);
	updateDrawable(damped);
}

class LLViewerObjectMedia
{
public:
	LLViewerObjectMedia() : mMediaURL(), mPassedWhitelist(FALSE), mMediaType(0) { }

	std::string mMediaURL;	// for web pages on surfaces, one per prim
	BOOL mPassedWhitelist;	// user has OK'd display
	U8 mMediaType;			// see LLTextureEntry::WEB_PAGE, etc.
};

// subclass of viewer object that can be added to particle partitions
class LLAlphaObject : public LLViewerObject
{
public:
	LLAlphaObject(const LLUUID &id, const LLPCode type, LLViewerRegion *regionp)
	: LLViewerObject(id,type,regionp) 
	{ mDepth = 0.f; }

	virtual F32 getPartSize(S32 idx);
	virtual void getGeometry(S32 idx,
								LLStrider<LLVector3>& verticesp,
								LLStrider<LLVector3>& normalsp, 
								LLStrider<LLVector2>& texcoordsp,
								LLStrider<LLColor4U>& colorsp, 
								LLStrider<U16>& indicesp) = 0;

	F32 mDepth;
};

class LLStaticViewerObject : public LLViewerObject
{
public:
	LLStaticViewerObject(const LLUUID& id, const LLPCode type, LLViewerRegion* regionp, BOOL is_global = FALSE)
		: LLViewerObject(id,type,regionp, is_global)
	{ }

	virtual void updateDrawable(BOOL force_damped);
};

extern BOOL gVelocityInterpolate;
extern BOOL gPingInterpolate;

#endif
