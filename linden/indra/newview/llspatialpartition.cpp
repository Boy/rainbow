/** 
 * @file llspatialpartition.cpp
 * @brief LLSpatialGroup class implementation and supporting functions
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

#include "llviewerprecompiledheaders.h"

#include "llspatialpartition.h"

#include "llviewerwindow.h"
#include "llviewerobjectlist.h"
#include "llvovolume.h"
#include "llvolume.h"
#include "llviewercamera.h"
#include "llface.h"
#include "llviewercontrol.h"
#include "llagent.h"
#include "llviewerregion.h"
#include "llcamera.h"
#include "pipeline.h"
#include "llrender.h"
#include "lloctree.h"
#include "llvoavatar.h"

const F32 SG_OCCLUSION_FUDGE = 1.01f;
#define SG_DISCARD_TOLERANCE 0.01f

#if LL_OCTREE_PARANOIA_CHECK
#define assert_octree_valid(x) x->validate()
#define assert_states_valid(x) ((LLSpatialGroup*) x->mSpatialPartition->mOctree->getListener(0))->checkStates()
#else
#define assert_octree_valid(x)
#define assert_states_valid(x)
#endif


static U32 sZombieGroups = 0;
U32 LLSpatialGroup::sNodeCount = 0;
BOOL LLSpatialGroup::sNoDelete = FALSE;

static F32 sLastMaxTexPriority = 1.f;
static F32 sCurMaxTexPriority = 1.f;

class LLOcclusionQueryPool : public LLGLNamePool
{
protected:
	virtual GLuint allocateName()
	{
		GLuint name;
		glGenQueriesARB(1, &name);
		return name;
	}

	virtual void releaseName(GLuint name)
	{
		glDeleteQueriesARB(1, &name);
	}
};

static LLOcclusionQueryPool sQueryPool;

BOOL LLSpatialPartition::sFreezeState = FALSE;

//static counter for frame to switch LOD on

void sg_assert(BOOL expr)
{
#if LL_OCTREE_PARANOIA_CHECK
	if (!expr)
	{
		llerrs << "Octree invalid!" << llendl;
	}
#endif
}

#if LL_DEBUG
void validate_drawable(LLDrawable* drawablep)
{
	F64 rad = drawablep->getBinRadius();
	const LLVector3* ext = drawablep->getSpatialExtents();

	if (rad < 0 || rad > 4096 ||
		(ext[1]-ext[0]).magVec() > 4096)
	{
		llwarns << "Invalid drawable found in octree." << llendl;
	}
}
#else
#define validate_drawable(x)
#endif


S32 AABBSphereIntersect(const LLVector3& min, const LLVector3& max, const LLVector3 &origin, const F32 &rad)
{
	return AABBSphereIntersectR2(min, max, origin, rad*rad);
}

S32 AABBSphereIntersectR2(const LLVector3& min, const LLVector3& max, const LLVector3 &origin, const F32 &r)
{
	F32 d = 0.f;
	F32 t;
	
	if ((min-origin).magVecSquared() < r &&
		(max-origin).magVecSquared() < r)
	{
		return 2;
	}

	for (U32 i = 0; i < 3; i++)
	{
		if (origin.mV[i] < min.mV[i])
		{
			t = min.mV[i] - origin.mV[i];
			d += t*t;
		}
		else if (origin.mV[i] > max.mV[i])
		{
			t = origin.mV[i] - max.mV[i];
			d += t*t;
		}

		if (d > r)
		{
			return 0;
		}
	}

	return 1;
}


typedef enum
{
	b000 = 0x00,
	b001 = 0x01,
	b010 = 0x02,
	b011 = 0x03,
	b100 = 0x04,
	b101 = 0x05,
	b110 = 0x06,
	b111 = 0x07,
} eLoveTheBits;

//contact Runitai Linden for a copy of the SL object used to write this table
//basically, you give the table a bitmask of the look-at vector to a node and it
//gives you a triangle fan index array
static U8 sOcclusionIndices[] =
{
	 // 000
		b111, b110, b010, b011, b001, b101, b100, b110,
	//001 
		b110, b000, b010, b011, b111, b101, b100, b000,
	 //010
		b101, b100, b110, b111, b011, b001, b000, b100,
	 //011 
		b100, b010, b110, b111, b101, b001, b000, b010,
	//100 
		b011, b010, b000, b001, b101, b111, b110, b010,
	 //101 
		b010, b100, b000, b001, b011, b111, b110, b100,
	 //110
		b001, b000, b100, b101, b111, b011, b010, b000,
	 //111
		b000, b110, b100, b101, b001, b011, b010, b110,
};

U8* get_occlusion_indices(LLCamera* camera, const LLVector3& center)
{
	LLVector3 d = center - camera->getOrigin();

	U8 cypher = 0;
	if (d.mV[0] > 0)
	{
		cypher |= b100;
	}
	if (d.mV[1] > 0)
	{
		cypher |= b010;
	}
	if (d.mV[2] > 0)
	{
		cypher |= b001;
	}

	return sOcclusionIndices+cypher*8;
}
						
void LLSpatialGroup::buildOcclusion()
{
	if (!mOcclusionVerts)
	{
		mOcclusionVerts = new F32[8*3];
	}

	LLVector3 r = mBounds[1]*SG_OCCLUSION_FUDGE + LLVector3(0.1f,0.1f,0.1f);

	for (U32 k = 0; k < 3; k++)
	{
		r.mV[k] = llmin(mBounds[1].mV[k]+0.25f, r.mV[k]);
	}

	F32* v = mOcclusionVerts;
	F32* c = mBounds[0].mV;
	F32* s = r.mV;
	
	//vertex positions are encoded so the 3 bits of their vertex index 
	//correspond to their axis facing, with bit position 3,2,1 matching
	//axis facing x,y,z, bit set meaning positive facing, bit clear 
	//meaning negative facing
	v[0] = c[0]-s[0]; v[1]  = c[1]-s[1]; v[2]  = c[2]-s[2];  // 0 - 0000 
	v[3] = c[0]-s[0]; v[4]  = c[1]-s[1]; v[5]  = c[2]+s[2];  // 1 - 0001
	v[6] = c[0]-s[0]; v[7]  = c[1]+s[1]; v[8]  = c[2]-s[2];  // 2 - 0010
	v[9] = c[0]-s[0]; v[10] = c[1]+s[1]; v[11] = c[2]+s[2];  // 3 - 0011
																					   
	v[12] = c[0]+s[0]; v[13] = c[1]-s[1]; v[14] = c[2]-s[2]; // 4 - 0100
	v[15] = c[0]+s[0]; v[16] = c[1]-s[1]; v[17] = c[2]+s[2]; // 5 - 0101
	v[18] = c[0]+s[0]; v[19] = c[1]+s[1]; v[20] = c[2]-s[2]; // 6 - 0110
	v[21] = c[0]+s[0]; v[22] = c[1]+s[1]; v[23] = c[2]+s[2]; // 7 - 0111

	clearState(LLSpatialGroup::OCCLUSION_DIRTY);
}


BOOL earlyFail(LLCamera* camera, LLSpatialGroup* group);

//returns:
//	0 if sphere and AABB are not intersecting 
//	1 if they are
//	2 if AABB is entirely inside sphere

S32 LLSphereAABB(const LLVector3& center, const LLVector3& size, const LLVector3& pos, const F32 &rad)
{
	S32 ret = 2;

	LLVector3 min = center - size;
	LLVector3 max = center + size;
	for (U32 i = 0; i < 3; i++)
	{
		if (min.mV[i] > pos.mV[i] + rad ||
			max.mV[i] < pos.mV[i] - rad)
		{	//totally outside
			return 0;
		}
		
		if (min.mV[i] < pos.mV[i] - rad ||
			max.mV[i] > pos.mV[i] + rad)
		{	//intersecting
			ret = 1;
		}
	}

	return ret;
}

LLSpatialGroup::~LLSpatialGroup()
{
	if (sNoDelete)
	{
		llerrs << "Illegal deletion of LLSpatialGroup!" << llendl;
	}

	if (isState(DEAD))
	{
		sZombieGroups--;
	}
	
	sNodeCount--;

	if (gGLManager.mHasOcclusionQuery && mOcclusionQuery)
	{
		sQueryPool.release(mOcclusionQuery);
	}

	delete [] mOcclusionVerts;

	LLMemType mt(LLMemType::MTYPE_SPACE_PARTITION);
	clearDrawMap();
}

void LLSpatialGroup::clearDrawMap()
{
	mDrawMap.clear();
}

BOOL LLSpatialGroup::isVisible() const
{
	return mVisible == LLDrawable::getCurrentFrame() ? TRUE : FALSE;
}

void LLSpatialGroup::setVisible()
{
	if (!LLSpatialPartition::sFreezeState)
	{
		mVisible = LLDrawable::getCurrentFrame();
	}
}

void LLSpatialGroup::validate()
{
#if LL_OCTREE_PARANOIA_CHECK

	sg_assert(!isState(DIRTY));
	sg_assert(!isDead());

	LLVector3 myMin = mBounds[0] - mBounds[1];
	LLVector3 myMax = mBounds[0] + mBounds[1];

	validateDrawMap();

	for (element_iter i = getData().begin(); i != getData().end(); ++i)
	{
		LLDrawable* drawable = *i;
		sg_assert(drawable->getSpatialGroup() == this);
		if (drawable->getSpatialBridge())
		{
			sg_assert(drawable->getSpatialBridge() == mSpatialPartition->asBridge());
		}

		/*if (drawable->isSpatialBridge())
		{
			LLSpatialPartition* part = drawable->asPartition();
			if (!part)
			{
				llerrs << "Drawable reports it is a spatial bridge but not a partition." << llendl;
			}
			LLSpatialGroup* group = (LLSpatialGroup*) part->mOctree->getListener(0);
			group->validate();
		}*/
	}

	for (U32 i = 0; i < mOctreeNode->getChildCount(); ++i)
	{
		LLSpatialGroup* group = (LLSpatialGroup*) mOctreeNode->getChild(i)->getListener(0);

		group->validate();
		
		//ensure all children are enclosed in this node
		LLVector3 center = group->mBounds[0];
		LLVector3 size = group->mBounds[1];
		
		LLVector3 min = center - size;
		LLVector3 max = center + size;
		
		for (U32 j = 0; j < 3; j++)
		{
			sg_assert(min.mV[j] >= myMin.mV[j]-0.02f);
			sg_assert(max.mV[j] <= myMax.mV[j]+0.02f);
		}
	}

#endif
}



class LLOctreeStateCheck : public LLOctreeTraveler<LLDrawable>
{
public:
	U32 mInheritedMask;

	LLOctreeStateCheck(): mInheritedMask(0) { }

	virtual void traverse(const LLSpatialGroup::OctreeNode* node)
	{
		LLSpatialGroup* group = (LLSpatialGroup*) node->getListener(0);
		
		node->accept(this);

		U32 temp = mInheritedMask;
		mInheritedMask |= group->getState() & 
			(LLSpatialGroup::OCCLUDED);

		for (U32 i = 0; i < node->getChildCount(); i++)
		{
			traverse(node->getChild(i));
		}

		mInheritedMask = temp;
	}

	virtual void visit(const LLOctreeNode<LLDrawable>* state)
	{
		LLSpatialGroup* group = (LLSpatialGroup*) state->getListener(0);

		if (mInheritedMask && !group->isState(mInheritedMask))
		{
			llerrs << "Spatial group failed inherited mask test." << llendl;
		}

		if (group->isState(LLSpatialGroup::DIRTY))
		{
			assert_parent_state(group, LLSpatialGroup::DIRTY);
		}
	}

	void assert_parent_state(LLSpatialGroup* group, U32 state)
	{
		LLSpatialGroup* parent = group->getParent();
		while (parent)
		{
			if (!parent->isState(state))
			{
				llerrs << "Spatial group failed parent state check." << llendl;
			}
			parent = parent->getParent();
		}
	}	
};


void LLSpatialGroup::checkStates()
{
#if LL_OCTREE_PARANOIA_CHECK
	LLOctreeStateCheck checker;
	checker.traverse(mOctreeNode);
#endif
}

void validate_draw_info(LLDrawInfo& params)
{
#if LL_OCTREE_PARANOIA_CHECK
	if (params.mVertexBuffer.isNull())
	{
		llerrs << "Draw batch has no vertex buffer." << llendl;
	}
	
	//bad range
	if (params.mStart >= params.mEnd)
	{
		llerrs << "Draw batch has invalid range." << llendl;
	}
	
	if (params.mEnd >= (U32) params.mVertexBuffer->getNumVerts())
	{
		llerrs << "Draw batch has buffer overrun error." << llendl;
	}
	
	if (params.mOffset + params.mCount > (U32) params.mVertexBuffer->getNumIndices())
	{
		llerrs << "Draw batch has index buffer ovverrun error." << llendl;
	}
	
	//bad indices
	U32* indicesp = (U32*) params.mVertexBuffer->getIndicesPointer();
	if (indicesp)
	{
		for (U32 i = params.mOffset; i < params.mOffset+params.mCount; i++)
		{
			if (indicesp[i] < params.mStart)
			{
				llerrs << "Draw batch has vertex buffer index out of range error (index too low)." << llendl;
			}
			
			if (indicesp[i] > params.mEnd)
			{
				llerrs << "Draw batch has vertex buffer index out of range error (index too high)." << llendl;
			}
		}
	}
#endif
}

void LLSpatialGroup::validateDrawMap()
{
#if LL_OCTREE_PARANOIA_CHECK
	for (draw_map_t::iterator i = mDrawMap.begin(); i != mDrawMap.end(); ++i)
	{
		LLSpatialGroup::drawmap_elem_t& draw_vec = i->second;
		for (drawmap_elem_t::iterator j = draw_vec.begin(); j != draw_vec.end(); ++j)
		{
			LLDrawInfo& params = **j;
			
			validate_draw_info(params);
		}
	}
#endif
}

BOOL LLSpatialGroup::updateInGroup(LLDrawable *drawablep, BOOL immediate)
{
	LLMemType mt(LLMemType::MTYPE_SPACE_PARTITION);
		
	drawablep->updateSpatialExtents();
	validate_drawable(drawablep);

	OctreeNode* parent = mOctreeNode->getOctParent();
	
	if (mOctreeNode->isInside(drawablep->getPositionGroup()) && 
		(mOctreeNode->contains(drawablep) ||
		 (drawablep->getBinRadius() > mOctreeNode->getSize().mdV[0] &&
				parent && parent->getElementCount() >= LL_OCTREE_MAX_CAPACITY)))
	{
		unbound();
		setState(OBJECT_DIRTY);
		//setState(GEOM_DIRTY);
		validate_drawable(drawablep);
		return TRUE;
	}
		
	return FALSE;
}


BOOL LLSpatialGroup::addObject(LLDrawable *drawablep, BOOL add_all, BOOL from_octree)
{
	LLMemType mt(LLMemType::MTYPE_SPACE_PARTITION);
	if (!from_octree)
	{
		mOctreeNode->insert(drawablep);
	}
	else
	{
		drawablep->setSpatialGroup(this);
		validate_drawable(drawablep);
		setState(OBJECT_DIRTY | GEOM_DIRTY | DISCARD_QUERY);
		if (drawablep->isSpatialBridge())
		{
			mBridgeList.push_back((LLSpatialBridge*) drawablep);
		}
		if (drawablep->getRadius() > 1.f)
		{
			setState(IMAGE_DIRTY);
		}
	}

	return TRUE;
}

void LLSpatialGroup::rebuildGeom()
{
	LLMemType mt(LLMemType::MTYPE_SPACE_PARTITION);
	if (!isDead())
	{
		mSpatialPartition->rebuildGeom(this);
	}
}

void LLSpatialPartition::rebuildGeom(LLSpatialGroup* group)
{
	if (!LLPipeline::sSkipUpdate && group->changeLOD())	{
		group->mLastUpdateDistance = group->mDistance;
		group->mLastUpdateViewAngle = group->mViewAngle;
	}

	if (group->isDead() || !group->isState(LLSpatialGroup::GEOM_DIRTY))
	{
		return;
	}

	LLFastTimer ftm(LLFastTimer::FTM_REBUILD_VBO);	

	group->clearDrawMap();
	
	//get geometry count
	U32 index_count = 0;
	U32 vertex_count = 0;
	
	addGeometryCount(group, vertex_count, index_count);

	if (vertex_count > 0 && index_count > 0)
	{ //create vertex buffer containing volume geometry for this node
		group->mBuilt = 1.f;
		if (group->mVertexBuffer.isNull() || (group->mBufferUsage != group->mVertexBuffer->getUsage() && LLVertexBuffer::sEnableVBOs))
		{
			group->mVertexBuffer = createVertexBuffer(mVertexDataMask, group->mBufferUsage);
			group->mVertexBuffer->allocateBuffer(vertex_count, index_count, true);
			stop_glerror();
		}
		else
		{
			group->mVertexBuffer->resizeBuffer(vertex_count, index_count);
			stop_glerror();
		}
		
		getGeometry(group);
	}
	else
	{
		group->mVertexBuffer = NULL;
		group->mBufferMap.clear();
	}

	group->mLastUpdateTime = gFrameTimeSeconds;
	group->clearState(LLSpatialGroup::GEOM_DIRTY);
}

BOOL LLSpatialGroup::boundObjects(BOOL empty, LLVector3& minOut, LLVector3& maxOut)
{	
	const OctreeNode* node = mOctreeNode;

	if (node->getData().empty())
	{	//don't do anything if there are no objects
		if (empty && mOctreeNode->getParent())
		{	//only root is allowed to be empty
			OCT_ERRS << "Empty leaf found in octree." << llendl;
		}
		return FALSE;
	}

	LLVector3& newMin = mObjectExtents[0];
	LLVector3& newMax = mObjectExtents[1];
	
	if (isState(OBJECT_DIRTY))
	{ //calculate new bounding box
		clearState(OBJECT_DIRTY);

		//initialize bounding box to first element
		OctreeNode::const_element_iter i = node->getData().begin();
		LLDrawable* drawablep = *i;
		const LLVector3* minMax = drawablep->getSpatialExtents();

		newMin.setVec(minMax[0]);
		newMax.setVec(minMax[1]);

		for (++i; i != node->getData().end(); ++i)
		{
			drawablep = *i;
			minMax = drawablep->getSpatialExtents();
			
			//bin up the object
			for (U32 i = 0; i < 3; i++)
			{
				if (minMax[0].mV[i] < newMin.mV[i])
				{
					newMin.mV[i] = minMax[0].mV[i];
				}
				if (minMax[1].mV[i] > newMax.mV[i])
				{
					newMax.mV[i] = minMax[1].mV[i];
				}
			}
		}
		
		mObjectBounds[0] = (newMin + newMax) * 0.5f;
		mObjectBounds[1] = (newMax - newMin) * 0.5f;
	}
	
	if (empty)
	{
		minOut = newMin;
		maxOut = newMax;
	}
	else
	{
		for (U32 i = 0; i < 3; i++)
		{
			if (newMin.mV[i] < minOut.mV[i])
			{
				minOut.mV[i] = newMin.mV[i];
			}
			if (newMax.mV[i] > maxOut.mV[i])
			{
				maxOut.mV[i] = newMax.mV[i];
			}
		}
	}
		
	return TRUE;
}

void LLSpatialGroup::unbound()
{
	if (isState(DIRTY))
	{
		return;
	}

	setState(DIRTY);
	
	//all the parent nodes need to rebound this child
	if (mOctreeNode)
	{
		OctreeNode* parent = (OctreeNode*) mOctreeNode->getParent();
		while (parent != NULL)
		{
			LLSpatialGroup* group = (LLSpatialGroup*) parent->getListener(0);
			if (group->isState(DIRTY))
			{
				return;
			}
			
			group->setState(DIRTY);
			parent = (OctreeNode*) parent->getParent();
		}
	}
}

LLSpatialGroup* LLSpatialGroup::getParent()
{
	if (isDead())
	{
		return NULL;
	}

	OctreeNode* parent = mOctreeNode->getOctParent();

	if (parent)
	{
		return (LLSpatialGroup*) parent->getListener(0);
	}

	return NULL;
}

BOOL LLSpatialGroup::removeObject(LLDrawable *drawablep, BOOL from_octree)
{
	LLMemType mt(LLMemType::MTYPE_SPACE_PARTITION);
	unbound();
	if (mOctreeNode && !from_octree)
	{
		if (!mOctreeNode->remove(drawablep))
		{
			OCT_ERRS << "Could not remove drawable from spatial group" << llendl;
		}
	}
	else
	{
		drawablep->setSpatialGroup(NULL);
		setState(GEOM_DIRTY);
		if (drawablep->isSpatialBridge())
		{
			for (bridge_list_t::iterator i = mBridgeList.begin(); i != mBridgeList.end(); ++i)
			{
				if (*i == drawablep)
				{
					mBridgeList.erase(i);
					break;
				}
			}
		}

		if (getElementCount() == 0)
		{ //delete draw map on last element removal since a rebuild might never happen
			clearDrawMap();
		}
	}
	return TRUE;
}

void LLSpatialGroup::shift(const LLVector3 &offset)
{
	LLMemType mt(LLMemType::MTYPE_SPACE_PARTITION);
	LLVector3d offsetd(offset);
	mOctreeNode->setCenter(mOctreeNode->getCenter()+offsetd);
	mOctreeNode->updateMinMax();
	mBounds[0] += offset;
	mExtents[0] += offset;
	mExtents[1] += offset;
	mObjectBounds[0] += offset;
	mObjectExtents[0] += offset;
	mObjectExtents[1] += offset;

	if (!mSpatialPartition->mRenderByGroup)
	{
		setState(GEOM_DIRTY);
	}

	if (mOcclusionVerts)
	{
		for (U32 i = 0; i < 8; i++)
		{
			F32* v = mOcclusionVerts+i*3;
			v[0] += offset.mV[0];
			v[1] += offset.mV[1];
			v[2] += offset.mV[2];
		}
	}
}

class LLSpatialSetState : public LLSpatialGroup::OctreeTraveler
{
public:
	U32 mState;
	LLSpatialSetState(U32 state) : mState(state) { }
	virtual void visit(const LLSpatialGroup::OctreeNode* branch) { ((LLSpatialGroup*) branch->getListener(0))->setState(mState); }	
};

class LLSpatialSetStateDiff : public LLSpatialSetState
{
public:
	LLSpatialSetStateDiff(U32 state) : LLSpatialSetState(state) { }

	virtual void traverse(const LLSpatialGroup::TreeNode* n)
	{
		LLSpatialGroup* group = (LLSpatialGroup*) n->getListener(0);
		
		if (!group->isState(mState))
		{
			LLSpatialGroup::OctreeTraveler::traverse(n);
		}
	}
};

void LLSpatialGroup::setState(U32 state) 
{ 
	if (!LLSpatialPartition::sFreezeState)
	{
		mState |= state; 
	}
}	

void LLSpatialGroup::setState(U32 state, S32 mode) 
{
	LLMemType mt(LLMemType::MTYPE_SPACE_PARTITION);
	if (LLSpatialPartition::sFreezeState)
	{
		return;
	}

	if (mode > STATE_MODE_SINGLE)
	{
		if (mode == STATE_MODE_DIFF)
		{
			LLSpatialSetStateDiff setter(state);
			setter.traverse(mOctreeNode);
		}
		else
		{
			LLSpatialSetState setter(state);
			setter.traverse(mOctreeNode);
		}
	}
	else
	{
		mState |= state;
	}
}

class LLSpatialClearState : public LLSpatialGroup::OctreeTraveler
{
public:
	U32 mState;
	LLSpatialClearState(U32 state) : mState(state) { }
	virtual void visit(const LLSpatialGroup::OctreeNode* branch) { ((LLSpatialGroup*) branch->getListener(0))->clearState(mState); }
};

class LLSpatialClearStateDiff : public LLSpatialClearState
{
public:
	LLSpatialClearStateDiff(U32 state) : LLSpatialClearState(state) { }

	virtual void traverse(const LLSpatialGroup::TreeNode* n)
	{
		LLSpatialGroup* group = (LLSpatialGroup*) n->getListener(0);
		
		if (group->isState(mState))
		{
			LLSpatialGroup::OctreeTraveler::traverse(n);
		}
	}
};

void LLSpatialGroup::clearState(U32 state)
{
	if (!LLSpatialPartition::sFreezeState)
	{
		mState &= ~state; 
	}
}

void LLSpatialGroup::clearState(U32 state, S32 mode)
{
	LLMemType mt(LLMemType::MTYPE_SPACE_PARTITION);
	if (LLSpatialPartition::sFreezeState)
	{
		return;
	}

	if (mode > STATE_MODE_SINGLE)
	{
		if (mode == STATE_MODE_DIFF)
		{
			LLSpatialClearStateDiff clearer(state);
			clearer.traverse(mOctreeNode);
		}
		else
		{
			LLSpatialClearState clearer(state);
			clearer.traverse(mOctreeNode);
		}
	}
	else
	{
		mState &= ~state;
	}
}

//======================================
//		Octree Listener Implementation
//======================================

LLSpatialGroup::LLSpatialGroup(OctreeNode* node, LLSpatialPartition* part) :
	mState(0),
	mBuilt(0.f),
	mOctreeNode(node),
	mSpatialPartition(part),
	mVertexBuffer(NULL), 
	mBufferUsage(GL_STATIC_DRAW_ARB),
	mVisible(0),
	mDistance(0.f),
	mDepth(0.f),
	mLastUpdateDistance(-1.f), 
	mLastUpdateTime(gFrameTimeSeconds),
	mViewAngle(0.f),
	mLastUpdateViewAngle(-1.f)
{
	sNodeCount++;
	LLMemType mt(LLMemType::MTYPE_SPACE_PARTITION);

	sg_assert(mOctreeNode->getListenerCount() == 0);
	mOctreeNode->addListener(this);
	setState(SG_INITIAL_STATE_MASK);

	mBounds[0] = LLVector3(node->getCenter());
	mBounds[1] = LLVector3(node->getSize());

	part->mLODSeed = (part->mLODSeed+1)%part->mLODPeriod;
	mLODHash = part->mLODSeed;
	
	mOcclusionQuery = 0;
	mOcclusionVerts = NULL;

	mRadius = 1;
	mPixelArea = 1024.f;
}

void LLSpatialGroup::updateDistance(LLCamera &camera)
{
#if !LL_RELEASE_FOR_DOWNLOAD
	if (isState(LLSpatialGroup::OBJECT_DIRTY))
	{
		llerrs << "Spatial group dirty on distance update." << llendl;
	}
#endif
	if (!getData().empty() && !LLSpatialPartition::sFreezeState)
	{
		mRadius = mSpatialPartition->mRenderByGroup ? mObjectBounds[1].magVec() :
						(F32) mOctreeNode->getSize().magVec();
		mDistance = mSpatialPartition->calcDistance(this, camera);
		mPixelArea = mSpatialPartition->calcPixelArea(this, camera);
	}
}

F32 LLSpatialPartition::calcDistance(LLSpatialGroup* group, LLCamera& camera)
{
	LLVector3 eye = group->mObjectBounds[0] - camera.getOrigin();

	F32 dist = 0.f;

	if (group->mDrawMap.find(LLRenderPass::PASS_ALPHA) != group->mDrawMap.end())
	{
		LLVector3 v = eye;
		dist = eye.normVec();

		if (!group->isState(LLSpatialGroup::ALPHA_DIRTY))
		{
			if (!group->mSpatialPartition->isBridge())
			{
				LLVector3 view_angle = LLVector3(eye * LLVector3(1,0,0),
												 eye * LLVector3(0,1,0),
												 eye * LLVector3(0,0,1));

				if ((view_angle-group->mLastUpdateViewAngle).magVec() > 0.64f)
				{
					group->mViewAngle = view_angle;
					group->mLastUpdateViewAngle = view_angle;
					//for occasional alpha sorting within the group
					//NOTE: If there is a trivial way to detect that alpha sorting here would not change the render order,
					//not setting this node to dirty would be a very good thing
					group->setState(LLSpatialGroup::ALPHA_DIRTY);
				}
			}
		}

		//calculate depth of node for alpha sorting

		LLVector3 at = camera.getAtAxis();

		//front of bounding box
		for (U32 i = 0; i < 3; i++)
		{
			v.mV[i] -= group->mObjectBounds[1].mV[i]*0.25f * at.mV[i];
		}

		group->mDepth = v * at;
	}
	else
	{
		dist = eye.magVec();
	}

	if (dist < 16.f)
	{
		dist /= 16.f;
		dist *= dist;
		dist *= 16.f;
	}

	return dist;
}

F32 LLSpatialPartition::calcPixelArea(LLSpatialGroup* group, LLCamera& camera)
{
	return LLPipeline::calcPixelArea(group->mObjectBounds[0], group->mObjectBounds[1], camera);
}

BOOL LLSpatialGroup::needsUpdate()
{
	return (LLDrawable::getCurrentFrame()%mSpatialPartition->mLODPeriod == mLODHash) ? TRUE : FALSE;
}

BOOL LLSpatialGroup::changeLOD()
{
	if (isState(ALPHA_DIRTY))
	{ ///an alpha sort is going to happen, update distance and LOD
		return TRUE;
	}

	if (mSpatialPartition->mSlopRatio > 0.f)
	{
		F32 ratio = (mDistance - mLastUpdateDistance)/(llmax(mLastUpdateDistance, mRadius));

		if (fabsf(ratio) >= mSpatialPartition->mSlopRatio)
		{
			return TRUE;
		}

		if (mDistance > mRadius)
		{
			return FALSE;
		}
	}
	
	if (needsUpdate())
	{
		return TRUE;
	}
	
	return FALSE;
}

void LLSpatialGroup::handleInsertion(const TreeNode* node, LLDrawable* drawablep)
{
	LLMemType mt(LLMemType::MTYPE_SPACE_PARTITION);
	addObject(drawablep, FALSE, TRUE);
	unbound();
	setState(OBJECT_DIRTY);
}

void LLSpatialGroup::handleRemoval(const TreeNode* node, LLDrawable* drawable)
{
	LLMemType mt(LLMemType::MTYPE_SPACE_PARTITION);
	removeObject(drawable, TRUE);
	setState(OBJECT_DIRTY);
}

void LLSpatialGroup::handleDestruction(const TreeNode* node)
{
	LLMemType mt(LLMemType::MTYPE_SPACE_PARTITION);
	setState(DEAD);
	
	for (element_iter i = getData().begin(); i != getData().end(); ++i)
	{
		LLDrawable* drawable = *i;
		if (drawable->getSpatialGroup() == this)
		{
			drawable->setSpatialGroup(NULL);
		}
	}
	
	clearDrawMap();
	mVertexBuffer = NULL;
	mBufferMap.clear();
	sZombieGroups++;
	mOctreeNode = NULL;
}

void LLSpatialGroup::handleStateChange(const TreeNode* node)
{
	//drop bounding box upon state change
	if (mOctreeNode != node)
	{
		mOctreeNode = (OctreeNode*) node;
	}
	unbound();
}

void LLSpatialGroup::handleChildAddition(const OctreeNode* parent, OctreeNode* child) 
{
	LLMemType mt(LLMemType::MTYPE_SPACE_PARTITION);
	if (child->getListenerCount() == 0)
	{
		LLSpatialGroup* group = new LLSpatialGroup(child, mSpatialPartition);
		group->setState(mState & SG_STATE_INHERIT_MASK);
	}
	else
	{
		OCT_ERRS << "LLSpatialGroup redundancy detected." << llendl;
	}

	unbound();

	assert_states_valid(this);
}

void LLSpatialGroup::handleChildRemoval(const OctreeNode* parent, const OctreeNode* child)
{
	unbound();
}

void LLSpatialGroup::destroyGL() 
{
	setState(LLSpatialGroup::GEOM_DIRTY | LLSpatialGroup::IMAGE_DIRTY);
	mLastUpdateTime = gFrameTimeSeconds;
	mVertexBuffer = NULL;
	mBufferMap.clear();

	clearDrawMap();

	if (mOcclusionQuery)
	{
		sQueryPool.release(mOcclusionQuery);
		mOcclusionQuery = 0;
	}

	delete [] mOcclusionVerts;
	mOcclusionVerts = NULL;

	for (LLSpatialGroup::element_iter i = getData().begin(); i != getData().end(); ++i)
	{
		LLDrawable* drawable = *i;
		for (S32 j = 0; j < drawable->getNumFaces(); j++)
		{
			LLFace* facep = drawable->getFace(j);
			facep->mVertexBuffer = NULL;
			facep->mLastVertexBuffer = NULL;
		}
	}
}

BOOL LLSpatialGroup::rebound()
{
	if (!isState(DIRTY))
	{	//return TRUE if we're not empty
		return TRUE;
	}
	
	if (mOctreeNode->getChildCount() == 1 && mOctreeNode->getElementCount() == 0)
	{
		LLSpatialGroup* group = (LLSpatialGroup*) mOctreeNode->getChild(0)->getListener(0);
		group->rebound();
		
		//copy single child's bounding box
		mBounds[0] = group->mBounds[0];
		mBounds[1] = group->mBounds[1];
		mExtents[0] = group->mExtents[0];
		mExtents[1] = group->mExtents[1];
		
		group->setState(SKIP_FRUSTUM_CHECK);
	}
	else if (mOctreeNode->isLeaf())
	{ //copy object bounding box if this is a leaf
		boundObjects(TRUE, mExtents[0], mExtents[1]);
		mBounds[0] = mObjectBounds[0];
		mBounds[1] = mObjectBounds[1];
	}
	else
	{
		LLVector3& newMin = mExtents[0];
		LLVector3& newMax = mExtents[1];
		LLSpatialGroup* group = (LLSpatialGroup*) mOctreeNode->getChild(0)->getListener(0);
		group->clearState(SKIP_FRUSTUM_CHECK);
		group->rebound();
		//initialize to first child
		newMin = group->mExtents[0];
		newMax = group->mExtents[1];

		//first, rebound children
		for (U32 i = 1; i < mOctreeNode->getChildCount(); i++)
		{
			group = (LLSpatialGroup*) mOctreeNode->getChild(i)->getListener(0);
			group->clearState(SKIP_FRUSTUM_CHECK);
			group->rebound();
			const LLVector3& max = group->mExtents[1];
			const LLVector3& min = group->mExtents[0];

			for (U32 j = 0; j < 3; j++)
			{
				if (max.mV[j] > newMax.mV[j])
				{
					newMax.mV[j] = max.mV[j];
				}
				if (min.mV[j] < newMin.mV[j])
				{
					newMin.mV[j] = min.mV[j];
				}
			}
		}

		boundObjects(FALSE, newMin, newMax);
		
		mBounds[0] = (newMin + newMax)*0.5f;
		mBounds[1] = (newMax - newMin)*0.5f;
	}
	
	setState(OCCLUSION_DIRTY);
	
	clearState(DIRTY);

	return TRUE;
}

void LLSpatialGroup::checkOcclusion()
{
	if (LLPipeline::sUseOcclusion > 1)
	{
		LLSpatialGroup* parent = getParent();
		if (parent && parent->isState(LLSpatialGroup::OCCLUDED))
		{	//if the parent has been marked as occluded, the child is implicitly occluded
			clearState(QUERY_PENDING | DISCARD_QUERY);
		}
		else if (isState(QUERY_PENDING))
		{	//otherwise, if a query is pending, read it back
			LLFastTimer t(LLFastTimer::FTM_OCCLUSION_READBACK);
			GLuint res = 1;
			if (!isState(DISCARD_QUERY) && mOcclusionQuery)
			{
				glGetQueryObjectuivARB(mOcclusionQuery, GL_QUERY_RESULT_ARB, &res);	
			}

			if (res > 0)
			{
				assert_states_valid(this);
				clearState(LLSpatialGroup::OCCLUDED, LLSpatialGroup::STATE_MODE_DIFF);
				assert_states_valid(this);
			}
			else
			{
				assert_states_valid(this);
				setState(LLSpatialGroup::OCCLUDED, LLSpatialGroup::STATE_MODE_DIFF);
				assert_states_valid(this);
			}

			clearState(QUERY_PENDING | DISCARD_QUERY);
		}
		else if (mSpatialPartition->mOcclusionEnabled)
		{
			assert_states_valid(this);
			clearState(LLSpatialGroup::OCCLUDED, LLSpatialGroup::STATE_MODE_DIFF);
			assert_states_valid(this);
		}
	}
}

void LLSpatialGroup::doOcclusion(LLCamera* camera)
{
	if (mSpatialPartition->mOcclusionEnabled && LLPipeline::sUseOcclusion > 1)
	{
		if (earlyFail(camera, this))
		{
			setState(LLSpatialGroup::DISCARD_QUERY);
			assert_states_valid(this);
			clearState(LLSpatialGroup::OCCLUDED, LLSpatialGroup::STATE_MODE_DIFF);
			assert_states_valid(this);
		}
		else
		{
			{
				LLFastTimer t(LLFastTimer::FTM_RENDER_OCCLUSION);

				if (!mOcclusionQuery)
				{
					mOcclusionQuery = sQueryPool.allocate();
				}

				if (!mOcclusionVerts || isState(LLSpatialGroup::OCCLUSION_DIRTY))
				{
					buildOcclusion();
				}

				glBeginQueryARB(GL_SAMPLES_PASSED_ARB, mOcclusionQuery);					
				glVertexPointer(3, GL_FLOAT, 0, mOcclusionVerts);
				glDrawRangeElements(GL_TRIANGLE_FAN, 0, 7, 8,
							GL_UNSIGNED_BYTE, get_occlusion_indices(camera, mBounds[0]));
				glEndQueryARB(GL_SAMPLES_PASSED_ARB);
			}

			setState(LLSpatialGroup::QUERY_PENDING);
			clearState(LLSpatialGroup::DISCARD_QUERY);
		}
	}
}

//==============================================

LLSpatialPartition::LLSpatialPartition(U32 data_mask, U32 buffer_usage)
{
	LLMemType mt(LLMemType::MTYPE_SPACE_PARTITION);
	mOcclusionEnabled = TRUE;
	mDrawableType = 0;
	mPartitionType = LLViewerRegion::PARTITION_NONE;
	mLODSeed = 0;
	mLODPeriod = 1;
	mVertexDataMask = data_mask;
	mBufferUsage = buffer_usage;
	mDepthMask = FALSE;
	mSlopRatio = 0.25f;
	mRenderByGroup = TRUE;
	mInfiniteFarClip = FALSE;

	LLGLNamePool::registerPool(&sQueryPool);

	mOctree = new LLSpatialGroup::OctreeRoot(LLVector3d(0,0,0), 
											LLVector3d(1,1,1), 
											NULL);
	new LLSpatialGroup(mOctree, this);
}


LLSpatialPartition::~LLSpatialPartition()
{
	LLMemType mt(LLMemType::MTYPE_SPACE_PARTITION);
	
	delete mOctree;
	mOctree = NULL;
}


LLSpatialGroup *LLSpatialPartition::put(LLDrawable *drawablep, BOOL was_visible)
{
	LLMemType mt(LLMemType::MTYPE_SPACE_PARTITION);
		
	drawablep->updateSpatialExtents();
	validate_drawable(drawablep);

	//keep drawable from being garbage collected
	LLPointer<LLDrawable> ptr = drawablep;
		
	assert_octree_valid(mOctree);
	mOctree->insert(drawablep);
	assert_octree_valid(mOctree);
	
	LLSpatialGroup* group = drawablep->getSpatialGroup();

	if (group && was_visible && group->isState(LLSpatialGroup::QUERY_PENDING))
	{
		group->setState(LLSpatialGroup::DISCARD_QUERY);
	}

	return group;
}

BOOL LLSpatialPartition::remove(LLDrawable *drawablep, LLSpatialGroup *curp)
{
	LLMemType mt(LLMemType::MTYPE_SPACE_PARTITION);
	
	drawablep->setSpatialGroup(NULL);

	if (!curp->removeObject(drawablep))
	{
		OCT_ERRS << "Failed to remove drawable from octree!" << llendl;
	}

	assert_octree_valid(mOctree);
	
	return TRUE;
}

void LLSpatialPartition::move(LLDrawable *drawablep, LLSpatialGroup *curp, BOOL immediate)
{
	LLMemType mt(LLMemType::MTYPE_SPACE_PARTITION);
		
	// sanity check submitted by open source user bushing Spatula
	// who was seeing crashing here. (See VWR-424 reported by Bunny Mayne)
	if (!drawablep)
	{
		OCT_ERRS << "LLSpatialPartition::move was passed a bad drawable." << llendl;
		return;
	}
		
	BOOL was_visible = curp ? curp->isVisible() : FALSE;

	if (curp && curp->mSpatialPartition != this)
	{
		//keep drawable from being garbage collected
		LLPointer<LLDrawable> ptr = drawablep;
		if (curp->mSpatialPartition->remove(drawablep, curp))
		{
			put(drawablep, was_visible);
			return;
		}
		else
		{
			OCT_ERRS << "Drawable lost between spatial partitions on outbound transition." << llendl;
		}
	}
		
	if (curp && curp->updateInGroup(drawablep, immediate))
	{
		// Already updated, don't need to do anything
		assert_octree_valid(mOctree);
		return;
	}

	//keep drawable from being garbage collected
	LLPointer<LLDrawable> ptr = drawablep;
	if (curp && !remove(drawablep, curp))
	{
		OCT_ERRS << "Move couldn't find existing spatial group!" << llendl;
	}

	put(drawablep, was_visible);
}

class LLSpatialShift : public LLSpatialGroup::OctreeTraveler
{
public:
	LLSpatialShift(LLVector3 offset) : mOffset(offset) { }
	virtual void visit(const LLSpatialGroup::OctreeNode* branch) 
	{ 
		((LLSpatialGroup*) branch->getListener(0))->shift(mOffset); 
	}
		
	LLVector3 mOffset;
};

void LLSpatialPartition::shift(const LLVector3 &offset)
{ //shift octree node bounding boxes by offset
	LLMemType mt(LLMemType::MTYPE_SPACE_PARTITION);
	LLSpatialShift shifter(offset);
	shifter.traverse(mOctree);
}

class LLOctreeCull : public LLSpatialGroup::OctreeTraveler
{
public:
	LLOctreeCull(LLCamera* camera)
		: mCamera(camera), mRes(0) { }

	virtual bool earlyFail(LLSpatialGroup* group)
	{
		group->checkOcclusion();

		if (group->mOctreeNode->getParent() &&	//never occlusion cull the root node
			LLPipeline::sUseOcclusion &&			//ignore occlusion if disabled
			group->isState(LLSpatialGroup::OCCLUDED))
		{
			gPipeline.markOccluder(group);
			return true;
		}
		
		return false;
	}
	
	virtual void traverse(const LLSpatialGroup::TreeNode* n)
	{
		LLSpatialGroup* group = (LLSpatialGroup*) n->getListener(0);

		if (earlyFail(group))
		{
			return;
		}
		
		if (mRes == 2 || 
			(mRes && group->isState(LLSpatialGroup::SKIP_FRUSTUM_CHECK)))
		{	//fully in, just add everything
			LLSpatialGroup::OctreeTraveler::traverse(n);
		}
		else
		{
			mRes = frustumCheck(group);
				
			if (mRes)
			{ //at least partially in, run on down
				LLSpatialGroup::OctreeTraveler::traverse(n);
			}

			mRes = 0;
		}
	}
	
	virtual S32 frustumCheck(const LLSpatialGroup* group)
	{
		S32 res = mCamera->AABBInFrustumNoFarClip(group->mBounds[0], group->mBounds[1]);
		if (res != 0)
		{
			res = llmin(res, AABBSphereIntersect(group->mExtents[0], group->mExtents[1], mCamera->getOrigin(), mCamera->mFrustumCornerDist));
		}
		return res;
	}

	virtual S32 frustumCheckObjects(const LLSpatialGroup* group)
	{
		S32 res = mCamera->AABBInFrustumNoFarClip(group->mObjectBounds[0], group->mObjectBounds[1]);
		if (res != 0)
		{
			res = llmin(res, AABBSphereIntersect(group->mObjectExtents[0], group->mObjectExtents[1], mCamera->getOrigin(), mCamera->mFrustumCornerDist));
		}
		return res;
	}

	virtual bool checkObjects(const LLSpatialGroup::OctreeNode* branch, const LLSpatialGroup* group)
	{
		if (branch->getElementCount() == 0) //no elements
		{
			return false;
		}
		else if (branch->getChildCount() == 0) //leaf state, already checked tightest bounding box
		{
			return true;
		}
		else if (mRes == 1 && !frustumCheckObjects(group)) //no objects in frustum
		{
			return false;
		}
		
		return true;
	}

	virtual void preprocess(LLSpatialGroup* group)
	{
		
	}
	
	virtual void processGroup(LLSpatialGroup* group)
	{
		if (group->needsUpdate() ||
			group->mVisible < LLDrawable::getCurrentFrame() - 1)
		{
			group->doOcclusion(mCamera);
		}
		gPipeline.markNotCulled(group, *mCamera);
	}
	
	virtual void visit(const LLSpatialGroup::OctreeNode* branch) 
	{	
		LLSpatialGroup* group = (LLSpatialGroup*) branch->getListener(0);

		preprocess(group);
		
		if (checkObjects(branch, group))
		{
			processGroup(group);
		}
	}

	LLCamera *mCamera;
	S32 mRes;
};

class LLOctreeCullNoFarClip : public LLOctreeCull
{
public: 
	LLOctreeCullNoFarClip(LLCamera* camera) 
		: LLOctreeCull(camera) { }

	virtual S32 frustumCheck(const LLSpatialGroup* group)
	{
		return mCamera->AABBInFrustumNoFarClip(group->mBounds[0], group->mBounds[1]);
	}

	virtual S32 frustumCheckObjects(const LLSpatialGroup* group)
	{
		S32 res = mCamera->AABBInFrustumNoFarClip(group->mObjectBounds[0], group->mObjectBounds[1]);
		return res;
	}
};

class LLOctreeSelect : public LLOctreeCull
{
public:
	LLOctreeSelect(LLCamera* camera, std::vector<LLDrawable*>* results)
		: LLOctreeCull(camera), mResults(results) { }

	virtual bool earlyFail(LLSpatialGroup* group) { return false; }
	virtual void preprocess(LLSpatialGroup* group) { }

	virtual void processGroup(LLSpatialGroup* group)
	{
		LLSpatialGroup::OctreeNode* branch = group->mOctreeNode;

		for (LLSpatialGroup::OctreeNode::const_element_iter i = branch->getData().begin(); i != branch->getData().end(); ++i)
		{
			LLDrawable* drawable = *i;
			
			if (!drawable->isDead())
			{
				if (drawable->isSpatialBridge())
				{
					drawable->setVisible(*mCamera, mResults, TRUE);
				}
				else
				{
					mResults->push_back(drawable);
				}
			}		
		}
	}
	
	std::vector<LLDrawable*>* mResults;
};

void drawBox(const LLVector3& c, const LLVector3& r)
{
	gGL.begin(LLRender::TRIANGLE_STRIP);
	//left front
	gGL.vertex3fv((c+r.scaledVec(LLVector3(-1,1,-1))).mV);
	gGL.vertex3fv((c+r.scaledVec(LLVector3(-1,1,1))).mV);
	//right front
	gGL.vertex3fv((c+r.scaledVec(LLVector3(1,1,-1))).mV);
	gGL.vertex3fv((c+r.scaledVec(LLVector3(1,1,1))).mV);
	//right back
 	gGL.vertex3fv((c+r.scaledVec(LLVector3(1,-1,-1))).mV);
	gGL.vertex3fv((c+r.scaledVec(LLVector3(1,-1,1))).mV);
	//left back
	gGL.vertex3fv((c+r.scaledVec(LLVector3(-1,-1,-1))).mV);
	gGL.vertex3fv((c+r.scaledVec(LLVector3(-1,-1,1))).mV);
	//left front
	gGL.vertex3fv((c+r.scaledVec(LLVector3(-1,1,-1))).mV);
	gGL.vertex3fv((c+r.scaledVec(LLVector3(-1,1,1))).mV);
	gGL.end();
	
	//bottom
	gGL.begin(LLRender::TRIANGLE_STRIP);
	gGL.vertex3fv((c+r.scaledVec(LLVector3(1,1,-1))).mV);
	gGL.vertex3fv((c+r.scaledVec(LLVector3(1,-1,-1))).mV);
	gGL.vertex3fv((c+r.scaledVec(LLVector3(-1,1,-1))).mV);
	gGL.vertex3fv((c+r.scaledVec(LLVector3(-1,-1,-1))).mV);
	gGL.end();

	//top
	gGL.begin(LLRender::TRIANGLE_STRIP);
	gGL.vertex3fv((c+r.scaledVec(LLVector3(1,1,1))).mV);
	gGL.vertex3fv((c+r.scaledVec(LLVector3(-1,1,1))).mV);
	gGL.vertex3fv((c+r.scaledVec(LLVector3(1,-1,1))).mV);
	gGL.vertex3fv((c+r.scaledVec(LLVector3(-1,-1,1))).mV);
	gGL.end();	
}

void drawBoxOutline(const LLVector3& pos, const LLVector3& size)
{
	LLVector3 v1 = size.scaledVec(LLVector3( 1, 1,1));
	LLVector3 v2 = size.scaledVec(LLVector3(-1, 1,1));
	LLVector3 v3 = size.scaledVec(LLVector3(-1,-1,1));
	LLVector3 v4 = size.scaledVec(LLVector3( 1,-1,1));

	gGL.begin(LLRender::LINES); 
	
	//top
	gGL.vertex3fv((pos+v1).mV);
	gGL.vertex3fv((pos+v2).mV);
	gGL.vertex3fv((pos+v2).mV);
	gGL.vertex3fv((pos+v3).mV);
	gGL.vertex3fv((pos+v3).mV);
	gGL.vertex3fv((pos+v4).mV);
	gGL.vertex3fv((pos+v4).mV);
	gGL.vertex3fv((pos+v1).mV);
	
	//bottom
	gGL.vertex3fv((pos-v1).mV);
	gGL.vertex3fv((pos-v2).mV);
	gGL.vertex3fv((pos-v2).mV);
	gGL.vertex3fv((pos-v3).mV);
	gGL.vertex3fv((pos-v3).mV);
	gGL.vertex3fv((pos-v4).mV);
	gGL.vertex3fv((pos-v4).mV);
	gGL.vertex3fv((pos-v1).mV);
	
	//right
	gGL.vertex3fv((pos+v1).mV);
	gGL.vertex3fv((pos-v3).mV);
			
	gGL.vertex3fv((pos+v4).mV);
	gGL.vertex3fv((pos-v2).mV);

	//left
	gGL.vertex3fv((pos+v2).mV);
	gGL.vertex3fv((pos-v4).mV);

	gGL.vertex3fv((pos+v3).mV);
	gGL.vertex3fv((pos-v1).mV);

	gGL.end();
}

class LLOctreeDirty : public LLOctreeTraveler<LLDrawable>
{
public:
	virtual void visit(const LLOctreeNode<LLDrawable>* state)
	{
		LLSpatialGroup* group = (LLSpatialGroup*) state->getListener(0);
		group->destroyGL();

		for (LLSpatialGroup::element_iter i = group->getData().begin(); i != group->getData().end(); ++i)
		{
			LLDrawable* drawable = *i;
			if (drawable->getVObj().notNull() && !group->mSpatialPartition->mRenderByGroup)
			{
				gPipeline.markRebuild(drawable, LLDrawable::REBUILD_ALL, TRUE);
			}
		}

		for (LLSpatialGroup::bridge_list_t::iterator i = group->mBridgeList.begin(); i != group->mBridgeList.end(); ++i)
		{
			LLSpatialBridge* bridge = *i;
			traverse(bridge->mOctree);
		}
	}
};

void LLSpatialPartition::restoreGL()
{
	LLMemType mt(LLMemType::MTYPE_SPACE_PARTITION);
}

void LLSpatialPartition::resetVertexBuffers()
{
	LLOctreeDirty dirty;
	dirty.traverse(mOctree);
}

S32 LLSpatialPartition::cull(LLCamera &camera, std::vector<LLDrawable *>* results, BOOL for_select)
{
	LLMemType mt(LLMemType::MTYPE_SPACE_PARTITION);
#if LL_OCTREE_PARANOIA_CHECK
	((LLSpatialGroup*)mOctree->getListener(0))->checkStates();
#endif
	{
		BOOL temp = sFreezeState;
		sFreezeState = FALSE;
		LLFastTimer ftm(LLFastTimer::FTM_CULL_REBOUND);		
		LLSpatialGroup* group = (LLSpatialGroup*) mOctree->getListener(0);
		group->rebound();
		sFreezeState = temp;
	}

#if LL_OCTREE_PARANOIA_CHECK
	((LLSpatialGroup*)mOctree->getListener(0))->validate();
#endif

	
	if (for_select)
	{
		LLOctreeSelect selecter(&camera, results);
		selecter.traverse(mOctree);
	}
	else if (mInfiniteFarClip || !LLPipeline::sUseFarClip)
	{
		LLFastTimer ftm(LLFastTimer::FTM_FRUSTUM_CULL);		
		LLOctreeCullNoFarClip culler(&camera);
		culler.traverse(mOctree);
	}
	else
	{
		LLFastTimer ftm(LLFastTimer::FTM_FRUSTUM_CULL);		
		LLOctreeCull culler(&camera);
		culler.traverse(mOctree);
	}
	
	return 0;
}

BOOL earlyFail(LLCamera* camera, LLSpatialGroup* group)
{
	const F32 vel = (LLViewerCamera::getInstance()->getVelocityStat()->getCurrent()+0.2f);
	LLVector3 c = group->mBounds[0];
	LLVector3 r = group->mBounds[1]*SG_OCCLUSION_FUDGE + LLVector3(vel,vel,vel);
    
	if (r.magVecSquared() > 1024.0*1024.0)
	{
		return TRUE;
	}

	LLVector3 e = camera->getOrigin();
	
	LLVector3 min = c - r;
	LLVector3 max = c + r;
	
	for (U32 j = 0; j < 3; j++)
	{
		if (e.mV[j] < min.mV[j] || e.mV[j] > max.mV[j])
		{
			return FALSE;
		}
	}

	return TRUE;
}


void pushVerts(LLDrawInfo* params, U32 mask)
{
	LLRenderPass::applyModelMatrix(*params);
	params->mVertexBuffer->setBuffer(mask);
	params->mVertexBuffer->drawRange(params->mParticle ? LLRender::POINTS : LLRender::TRIANGLES,
								params->mStart, params->mEnd, params->mCount, params->mOffset);
}

void pushVerts(LLSpatialGroup* group, U32 mask)
{
	LLDrawInfo* params = NULL;

	for (LLSpatialGroup::draw_map_t::iterator i = group->mDrawMap.begin(); i != group->mDrawMap.end(); ++i)
	{
		for (LLSpatialGroup::drawmap_elem_t::iterator j = i->second.begin(); j != i->second.end(); ++j) 
		{
			params = *j;
			pushVerts(params, mask);
		}
	}
}

void pushVerts(LLFace* face, U32 mask)
{
	LLVertexBuffer* buffer = face->mVertexBuffer;

	if (buffer)
	{
		buffer->setBuffer(mask);
		U16 start = face->getGeomStart();
		U16 end = start + face->getGeomCount()-1;
		U32 count = face->getIndicesCount();
		U16 offset = face->getIndicesStart();
		buffer->drawRange(LLRender::TRIANGLES, start, end, count, offset);
	}

}

void pushBufferVerts(LLVertexBuffer* buffer, U32 mask)
{
	if (buffer)
	{
		buffer->setBuffer(mask);
		buffer->drawRange(LLRender::TRIANGLES, 0, buffer->getRequestedVerts()-1, buffer->getRequestedIndices(), 0);
	}
}

void pushBufferVerts(LLSpatialGroup* group, U32 mask)
{
	if (!group->mDrawMap.empty())
	{
		LLDrawInfo* params = *(group->mDrawMap.begin()->second.begin());
		LLRenderPass::applyModelMatrix(*params);
	
		pushBufferVerts(group->mVertexBuffer, mask);

		for (LLSpatialGroup::buffer_map_t::iterator i = group->mBufferMap.begin(); i != group->mBufferMap.end(); ++i)
		{
			for (LLSpatialGroup::buffer_list_t::iterator j = i->second.begin(); j != i->second.end(); ++j)
			{
				pushBufferVerts(*j, mask);
			}
		}
	}
}

void pushVertsColorCoded(LLSpatialGroup* group, U32 mask)
{
	LLDrawInfo* params = NULL;

	LLColor4 colors[] = {
		LLColor4::green,
		LLColor4::green1,
		LLColor4::green2,
		LLColor4::green3,
		LLColor4::green4,
		LLColor4::green5,
		LLColor4::green6
	};
		
	static const U32 col_count = sizeof(colors)/sizeof(LLColor4);

	U32 col = 0;

	for (LLSpatialGroup::draw_map_t::iterator i = group->mDrawMap.begin(); i != group->mDrawMap.end(); ++i)
	{
		for (LLSpatialGroup::drawmap_elem_t::iterator j = i->second.begin(); j != i->second.end(); ++j) 
		{
			params = *j;
			LLRenderPass::applyModelMatrix(*params);
			glColor4f(colors[col].mV[0], colors[col].mV[1], colors[col].mV[2], 0.5f);
			params->mVertexBuffer->setBuffer(mask);
			params->mVertexBuffer->drawRange(params->mParticle ? LLRender::POINTS : LLRender::TRIANGLES,
				params->mStart, params->mEnd, params->mCount, params->mOffset);
			col = (col+1)%col_count;
		}
	}
}

void renderOctree(LLSpatialGroup* group)
{
	//render solid object bounding box, color
	//coded by buffer usage and activity
	LLGLDepthTest depth(GL_TRUE, GL_FALSE);
	gGL.setSceneBlendType(LLRender::BT_ADD_WITH_ALPHA);
	LLVector4 col;
	if (group->mBuilt > 0.f)
	{
		group->mBuilt -= 2.f * gFrameIntervalSeconds;
		if (group->mBufferUsage == GL_STATIC_DRAW_ARB)
		{
			col.setVec(1.0f, 0, 0, group->mBuilt*0.5f);
		}
		else 
		{
			col.setVec(0.1f,0.1f,1,0.1f);
			//col.setVec(1.0f, 1.0f, 0, sinf(group->mBuilt*3.14159f)*0.5f);
		}

		if (group->mBufferUsage != GL_STATIC_DRAW_ARB)
		{
			LLGLDepthTest gl_depth(FALSE, FALSE);
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

			gGL.color4f(1,0,0,group->mBuilt);
			gGL.flush();
			glLineWidth(5.f);
			drawBoxOutline(group->mObjectBounds[0], group->mObjectBounds[1]);
			gGL.flush();
			glLineWidth(1.f);
			gGL.flush();
			for (LLSpatialGroup::element_iter i = group->getData().begin(); i != group->getData().end(); ++i)
			{
				LLDrawable* drawable = *i;
				if (!group->mSpatialPartition->isBridge())
				{
					glPushMatrix();
					LLVector3 trans = drawable->getRegion()->getOriginAgent();
					glTranslatef(trans.mV[0], trans.mV[1], trans.mV[2]);
				}
				
				for (S32 j = 0; j < drawable->getNumFaces(); j++)
				{
					LLFace* face = drawable->getFace(j);
					if (face->mVertexBuffer.notNull())
					{
						if (gFrameTimeSeconds - face->mLastUpdateTime < 0.5f)
						{
							glColor4f(0, 1, 0, group->mBuilt);
						}
						else if (gFrameTimeSeconds - face->mLastMoveTime < 0.5f)
						{
							glColor4f(1, 0, 0, group->mBuilt);
						}
						else
						{
							continue;
						}

						face->mVertexBuffer->setBuffer(LLVertexBuffer::MAP_VERTEX);
						//drawBox((face->mExtents[0] + face->mExtents[1])*0.5f,
						//		(face->mExtents[1]-face->mExtents[0])*0.5f);
						face->mVertexBuffer->draw(LLRender::TRIANGLES, face->getIndicesCount(), face->getIndicesStart());
					}
				}

				if (!group->mSpatialPartition->isBridge())
				{
					glPopMatrix();
				}
			}
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			gGL.color4f(1,1,1,1);
		}
	}
	else
	{
		if (group->mBufferUsage == GL_STATIC_DRAW_ARB && !group->getData().empty() 
			&& group->mSpatialPartition->mRenderByGroup)
		{
			col.setVec(0.8f, 0.4f, 0.1f, 0.1f);
		}
		else
		{
			col.setVec(0.1f, 0.1f, 1.f, 0.1f);
		}
	}

	gGL.color4fv(col.mV);
	drawBox(group->mObjectBounds[0], group->mObjectBounds[1]*1.01f+LLVector3(0.001f, 0.001f, 0.001f));
	
	glDepthMask(GL_TRUE);
	gGL.setSceneBlendType(LLRender::BT_ALPHA);

	if (group->mBuilt <= 0.f)
	{
		//draw opaque outline
		gGL.color4f(col.mV[0], col.mV[1], col.mV[2], 1.f);
		drawBoxOutline(group->mObjectBounds[0], group->mObjectBounds[1]);

		if (group->mOctreeNode->isLeaf())
		{
			gGL.color4f(1,1,1,1);
		}
		else
		{
			gGL.color4f(0,1,1,1);
		}
						
		drawBoxOutline(group->mBounds[0],group->mBounds[1]);
	}
	
//	LLSpatialGroup::OctreeNode* node = group->mOctreeNode;
//	gGL.color4f(0,1,0,1);
//	drawBoxOutline(LLVector3(node->getCenter()), LLVector3(node->getSize()));
}

void renderVisibility(LLSpatialGroup* group, LLCamera* camera)
{
	LLGLEnable blend(GL_BLEND);
	gGL.setSceneBlendType(LLRender::BT_ALPHA);
	LLGLEnable cull(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	BOOL render_objects = (!LLPipeline::sUseOcclusion || !group->isState(LLSpatialGroup::OCCLUDED)) && group->isVisible() &&
							!group->getData().empty();
	if (render_objects)
	{
		LLGLDepthTest depth_under(GL_TRUE, GL_FALSE, GL_GREATER);
		glColor4f(0, 0.5f, 0, 0.5f);
		pushBufferVerts(group, LLVertexBuffer::MAP_VERTEX);
	}

	{
		LLGLDepthTest depth_over(GL_TRUE, GL_FALSE, GL_LEQUAL);

		if (render_objects)
		{
			glColor4f(0.f, 0.5f, 0.f,1.f);
			pushBufferVerts(group, LLVertexBuffer::MAP_VERTEX);
		}

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		if (render_objects)
		{
			glColor4f(0.f, 0.75f, 0.f,0.5f);
			pushBufferVerts(group, LLVertexBuffer::MAP_VERTEX);
		}
		else if (camera && group->mOcclusionVerts)
		{
			LLVertexBuffer::unbind();
			glVertexPointer(3, GL_FLOAT, 0, group->mOcclusionVerts);

			glColor4f(1.0f, 0.f, 0.f, 0.5f);
			glDrawRangeElements(GL_TRIANGLE_FAN, 0, 7, 8, GL_UNSIGNED_BYTE, get_occlusion_indices(camera, group->mBounds[0]));
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			
			glColor4f(1.0f, 1.f, 1.f, 1.0f);
			glDrawRangeElements(GL_TRIANGLE_FAN, 0, 7, 8, GL_UNSIGNED_BYTE, get_occlusion_indices(camera, group->mBounds[0]));
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}
	}
}

void renderBoundingBox(LLDrawable* drawable)
{
	if (drawable->isSpatialBridge())
	{
		gGL.color4f(1,0.5f,0,1);
	}
	else if (drawable->getVOVolume())
	{
		if (drawable->isRoot())
		{
			gGL.color4f(1,1,0,1);
		}
		else
		{
			gGL.color4f(0,1,0,1);
		}
	}
	else if (drawable->getVObj())
	{
		switch (drawable->getVObj()->getPCode())
		{
			case LLViewerObject::LL_VO_SURFACE_PATCH:
					gGL.color4f(0,1,1,1);
					break;
			case LLViewerObject::LL_VO_CLOUDS:
					gGL.color4f(0.5f,0.5f,0.5f,1.0f);
					break;
			case LLViewerObject::LL_VO_PART_GROUP:
			case LLViewerObject::LL_VO_HUD_PART_GROUP:
					gGL.color4f(0,0,1,1);
					break;
			case LLViewerObject::LL_VO_WATER:
					gGL.color4f(0,0.5f,1,1);
					break;
			case LL_PCODE_LEGACY_TREE:
					gGL.color4f(0,0.5f,0,1);
			default:
					gGL.color4f(1,0,1,1);
					break;
		}
	}
	else 
	{
		gGL.color4f(1,0,0,1);
	}

	const LLVector3* ext;
	LLVector3 pos, size;

	//render face bounding boxes
	for (S32 i = 0; i < drawable->getNumFaces(); i++)
	{
		LLFace* facep = drawable->getFace(i);

		ext = facep->mExtents;

		if (ext[0].isExactlyZero() && ext[1].isExactlyZero())
		{
			continue;
		}
		pos = (ext[0] + ext[1]) * 0.5f;
		size = (ext[1] - ext[0]) * 0.5f;
		drawBoxOutline(pos,size);
	}

	//render drawable bounding box
	ext = drawable->getSpatialExtents();

	pos = (ext[0] + ext[1]) * 0.5f;
	size = (ext[1] - ext[0]) * 0.5f;
	
	LLViewerObject* vobj = drawable->getVObj();
	if (vobj && vobj->onActiveList())
	{
		gGL.flush();
		glLineWidth(4.f*(sinf(gFrameTimeSeconds*2.f)*0.25f+0.75f));
		stop_glerror();
		drawBoxOutline(pos,size);
		gGL.flush();
		glLineWidth(1.f);
	}
	else
	{
		drawBoxOutline(pos,size);
	}
	
}

void renderTexturePriority(LLDrawable* drawable)
{
	for (int face=0; face<drawable->getNumFaces(); ++face)
	{
		LLFace *facep = drawable->getFace(face);
		
		LLVector4 cold(0,0,0.25f);
		LLVector4 hot(1,0.25f,0.25f);
	
		LLVector4 boost_cold(0,0,0,0);
		LLVector4 boost_hot(0,1,0,1);
		
		LLGLDisable blend(GL_BLEND);
		
		//LLViewerImage* imagep = facep->getTexture();
		//if (imagep)
		{
	
			//F32 vsize = LLVOVolume::getTextureVirtualSize(facep);
			//F32 vsize = imagep->mMaxVirtualSize;
			F32 vsize = facep->getPixelArea();

			if (vsize > sCurMaxTexPriority)
			{
				sCurMaxTexPriority = vsize;
			}
			
			F32 t = vsize/sLastMaxTexPriority;
			
			LLVector4 col = lerp(cold, hot, t);
			gGL.color4fv(col.mV);
		}
		//else
		//{
		//	gGL.color4f(1,0,1,1);
		//}
		
		LLVector3 center = (facep->mExtents[1]+facep->mExtents[0])*0.5f;
		LLVector3 size = (facep->mExtents[1]-facep->mExtents[0])*0.5f + LLVector3(0.01f, 0.01f, 0.01f);
		drawBox(center, size);
		
		/*S32 boost = imagep->getBoostLevel();
		if (boost)
		{
			F32 t = (F32) boost / (F32) (LLViewerImage::BOOST_MAX_LEVEL-1);
			LLVector4 col = lerp(boost_cold, boost_hot, t);
			LLGLEnable blend_on(GL_BLEND);
			gGL.blendFunc(GL_SRC_ALPHA, GL_ONE);
			gGL.color4fv(col.mV);
			drawBox(center, size);
			gGL.blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}*/
	}
}

void renderPoints(LLDrawable* drawablep)
{
	LLGLDepthTest depth(GL_FALSE, GL_FALSE);
	if (drawablep->getNumFaces())
	{
		gGL.begin(LLRender::POINTS);
		gGL.color3f(1,1,1);
		LLVector3 center(drawablep->getPositionGroup());
		for (S32 i = 0; i < drawablep->getNumFaces(); i++)
		{
			gGL.vertex3fv(drawablep->getFace(i)->mCenterLocal.mV);
		}
		gGL.end();
	}
}

void renderTextureAnim(LLDrawInfo* params)
{
	if (!params->mTextureMatrix)
	{
		return;
	}
	
	LLGLEnable blend(GL_BLEND);
	gGL.color4f(1,1,0,0.5f);
	pushVerts(params, LLVertexBuffer::MAP_VERTEX);
}

void renderBatchSize(LLDrawInfo* params)
{
	glColor3ubv((GLubyte*) &(params->mDebugColor));
	pushVerts(params, LLVertexBuffer::MAP_VERTEX);
}

void renderLights(LLDrawable* drawablep)
{
	if (!drawablep->isLight())
	{
		return;
	}

	if (drawablep->getNumFaces())
	{
		LLGLEnable blend(GL_BLEND);
		glColor4f(0,1,1,0.5f);

		for (S32 i = 0; i < drawablep->getNumFaces(); i++)
		{
			pushVerts(drawablep->getFace(i), LLVertexBuffer::MAP_VERTEX);
		}

		const LLVector3* ext = drawablep->getSpatialExtents();

		LLVector3 pos = (ext[0] + ext[1]) * 0.5f;
		LLVector3 size = (ext[1] - ext[0]) * 0.5f;

		{
			LLGLDepthTest depth(GL_FALSE, GL_TRUE);
			gGL.color4f(1,1,1,1);
			drawBoxOutline(pos, size);
		}

		gGL.color4f(1,1,0,1);
		F32 rad = drawablep->getVOVolume()->getLightRadius();
		drawBoxOutline(pos, LLVector3(rad,rad,rad));
	}
}


void renderRaycast(LLDrawable* drawablep)
{
	if (drawablep->getVObj() != gDebugRaycastObject)
	{
		return;
	}
	
	if (drawablep->getNumFaces())
	{
		LLGLEnable blend(GL_BLEND);
		gGL.color4f(0,1,1,0.5f);

		if (drawablep->getVOVolume() && gDebugRaycastFaceHit != -1)
		{
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			pushVerts(drawablep->getFace(gDebugRaycastFaceHit), LLVertexBuffer::MAP_VERTEX);
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}
		else if (drawablep->isAvatar())
		{
			LLGLDepthTest depth(GL_FALSE);
			LLVOAvatar* av = (LLVOAvatar*) drawablep->getVObj().get();
			av->renderCollisionVolumes();
		}

		// draw intersection point
		glPushMatrix();
		glLoadMatrixd(gGLModelView);
		LLVector3 translate = gDebugRaycastIntersection;
		glTranslatef(translate.mV[0], translate.mV[1], translate.mV[2]);
		LLCoordFrame orient;
		orient.lookDir(gDebugRaycastNormal, gDebugRaycastBinormal);
		LLMatrix4 rotation;
		orient.getRotMatrixToParent(rotation);
		glMultMatrixf((float*)rotation.mMatrix);
		
		gGL.color4f(1,0,0,0.5f);
		drawBox(LLVector3(0, 0, 0), LLVector3(0.1f, 0.022f, 0.022f));
		gGL.color4f(0,1,0,0.5f);
		drawBox(LLVector3(0, 0, 0), LLVector3(0.021f, 0.1f, 0.021f));
		gGL.color4f(0,0,1,0.5f);
		drawBox(LLVector3(0, 0, 0), LLVector3(0.02f, 0.02f, 0.1f));
		glPopMatrix();

		// draw bounding box of prim
		const LLVector3* ext = drawablep->getSpatialExtents();

		LLVector3 pos = (ext[0] + ext[1]) * 0.5f;
		LLVector3 size = (ext[1] - ext[0]) * 0.5f;

		LLGLDepthTest depth(GL_FALSE, GL_TRUE);
		gGL.color4f(0,0.5f,0.5f,1);
		drawBoxOutline(pos, size);

	}
}

class LLOctreeRenderNonOccluded : public LLOctreeTraveler<LLDrawable>
{
public:
	LLCamera* mCamera;
	LLOctreeRenderNonOccluded(LLCamera* camera): mCamera(camera) {}
	
	virtual void traverse(const LLSpatialGroup::OctreeNode* node)
	{
		LLSpatialGroup* group = (LLSpatialGroup*) node->getListener(0);
		
		if (!mCamera || mCamera->AABBInFrustumNoFarClip(group->mBounds[0], group->mBounds[1]))
		{
			node->accept(this);
			stop_glerror();

			for (U32 i = 0; i < node->getChildCount(); i++)
			{
				traverse(node->getChild(i));
				stop_glerror();
			}
			
			//draw tight fit bounding boxes for spatial group
			if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_OCTREE))
			{	
				renderOctree(group);
				stop_glerror();
			}

			//render visibility wireframe
			if (group->mSpatialPartition->mRenderByGroup &&
				gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_OCCLUSION))
			{
				gGL.flush();
				glPushMatrix();
				gGLLastMatrix = NULL;
				glLoadMatrixd(gGLModelView);
				renderVisibility(group, mCamera);
				stop_glerror();
				gGLLastMatrix = NULL;
				glPopMatrix();
				gGL.color4f(1,1,1,1);
			}
		}
	}

	virtual void visit(const LLSpatialGroup::OctreeNode* branch)
	{
		LLSpatialGroup* group = (LLSpatialGroup*) branch->getListener(0);

		if (group->isState(LLSpatialGroup::GEOM_DIRTY) || (mCamera && !mCamera->AABBInFrustumNoFarClip(group->mBounds[0], group->mBounds[1])))
		{
			return;
		}

		LLVector3 nodeCenter = group->mBounds[0];
		LLVector3 octCenter = LLVector3(group->mOctreeNode->getCenter());

		for (LLSpatialGroup::OctreeNode::const_element_iter i = branch->getData().begin(); i != branch->getData().end(); ++i)
		{
			LLDrawable* drawable = *i;
						
			if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_BBOXES))
			{
				renderBoundingBox(drawable);			
			}
			
			if (drawable->getVOVolume() && gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_TEXTURE_PRIORITY))
			{
				renderTexturePriority(drawable);
			}

			if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_POINTS))
			{
				renderPoints(drawable);
			}

			if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_LIGHTS))
			{
				renderLights(drawable);
			}

			if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_RAYCAST))
			{
				renderRaycast(drawable);
			}
		}
		
		for (LLSpatialGroup::draw_map_t::iterator i = group->mDrawMap.begin(); i != group->mDrawMap.end(); ++i)
		{
			LLSpatialGroup::drawmap_elem_t& draw_vec = i->second;	
			for (LLSpatialGroup::drawmap_elem_t::iterator j = draw_vec.begin(); j != draw_vec.end(); ++j)	
			{
				LLDrawInfo* draw_info = *j;
				if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_TEXTURE_ANIM))
				{
					renderTextureAnim(draw_info);
				}
				if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_BATCH_SIZE))
				{
					renderBatchSize(draw_info);
				}
			}
		}
	}
};

void LLSpatialPartition::renderDebug()
{
	if (!gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_OCTREE |
									  LLPipeline::RENDER_DEBUG_OCCLUSION |
									  LLPipeline::RENDER_DEBUG_LIGHTS |
									  LLPipeline::RENDER_DEBUG_BATCH_SIZE |
									  LLPipeline::RENDER_DEBUG_BBOXES |
									  LLPipeline::RENDER_DEBUG_POINTS |
									  LLPipeline::RENDER_DEBUG_TEXTURE_PRIORITY |
									  LLPipeline::RENDER_DEBUG_TEXTURE_ANIM |
									  LLPipeline::RENDER_DEBUG_RAYCAST))
	{
		return;
	}
	
	if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_TEXTURE_PRIORITY))
	{
		//sLastMaxTexPriority = lerp(sLastMaxTexPriority, sCurMaxTexPriority, gFrameIntervalSeconds);
		sLastMaxTexPriority = (F32) LLViewerCamera::getInstance()->getScreenPixelArea();
		sCurMaxTexPriority = 0.f;
	}

	LLMemType mt(LLMemType::MTYPE_SPACE_PARTITION);
	
	LLGLDisable cullface(GL_CULL_FACE);
	LLGLEnable blend(GL_BLEND);
	gGL.setSceneBlendType(LLRender::BT_ALPHA);
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	gPipeline.disableLights();

	LLSpatialBridge* bridge = asBridge();
	LLCamera* camera = LLViewerCamera::getInstance();
	
	if (bridge)
	{
		camera = NULL;
	}

	LLOctreeStateCheck checker;
	checker.traverse(mOctree);

	LLOctreeRenderNonOccluded render_debug(camera);
	render_debug.traverse(mOctree);
}


BOOL LLSpatialPartition::isVisible(const LLVector3& v)
{
	if (!LLViewerCamera::getInstance()->sphereInFrustum(v, 4.0f))
	{
		return FALSE;
	}

	return TRUE;
}

class LLOctreeIntersect : public LLSpatialGroup::OctreeTraveler
{
public:
	LLVector3 mStart;
	LLVector3 mEnd;
	S32       *mFaceHit;
	LLVector3 *mIntersection;
	LLVector2 *mTexCoord;
	LLVector3 *mNormal;
	LLVector3 *mBinormal;
	LLDrawable* mHit;
	BOOL mPickTransparent;

	LLOctreeIntersect(LLVector3 start, LLVector3 end, BOOL pick_transparent,
					  S32* face_hit, LLVector3* intersection, LLVector2* tex_coord, LLVector3* normal, LLVector3* binormal)
		: mStart(start),
		  mEnd(end),
		  mFaceHit(face_hit),
		  mIntersection(intersection),
		  mTexCoord(tex_coord),
		  mNormal(normal),
		  mBinormal(binormal),
		  mHit(NULL),
		  mPickTransparent(pick_transparent)
	{
	}
	
	virtual void visit(const LLSpatialGroup::OctreeNode* branch) 
	{	
		for (LLSpatialGroup::OctreeNode::const_element_iter i = branch->getData().begin(); i != branch->getData().end(); ++i)
		{
			check(*i);
		}
	}

	virtual LLDrawable* check(const LLSpatialGroup::OctreeNode* node)
	{
		node->accept(this);
	
		for (U32 i = 0; i < node->getChildCount(); i++)
		{
			const LLSpatialGroup::OctreeNode* child = node->getChild(i);
			LLVector3 res;

			LLSpatialGroup* group = (LLSpatialGroup*) child->getListener(0);
			
			LLVector3 size;
			LLVector3 center;
			
			size = group->mBounds[1];
			center = group->mBounds[0];
			
			LLVector3 local_start = mStart;
			LLVector3 local_end   = mEnd;

			if (group->mSpatialPartition->isBridge())
			{
				LLMatrix4 local_matrix = group->mSpatialPartition->asBridge()->mDrawable->getRenderMatrix();
				local_matrix.invert();
				
				local_start = mStart * local_matrix;
				local_end   = mEnd   * local_matrix;
			}

			if (LLLineSegmentBoxIntersect(local_start, local_end, center, size))
			{
				check(child);
			}
		}	

		return mHit;
	}

	virtual bool check(LLDrawable* drawable)
	{	
		LLVector3 local_start = mStart;
		LLVector3 local_end = mEnd;

		if (!gPipeline.hasRenderType(drawable->getRenderType()) || !drawable->isVisible())
		{
			return false;
		}

		if (drawable->isSpatialBridge())
		{
			LLSpatialPartition *part = drawable->asPartition();
			LLSpatialBridge* bridge = part->asBridge();
			if (bridge && gPipeline.hasRenderType(bridge->mDrawableType))
			{
				check(part->mOctree);
			}
		}
		else
		{
			LLViewerObject* vobj = drawable->getVObj();

			if (vobj)
			{
				LLVector3 intersection;
				if (vobj->lineSegmentIntersect(mStart, mEnd, -1, mPickTransparent, mFaceHit, &intersection, mTexCoord, mNormal, mBinormal))
				{
					mEnd = intersection;  // shorten ray so we only find CLOSER hits
					if (mIntersection)
					{
						*mIntersection = intersection;
					}
					
					mHit = vobj->mDrawable;
				}
			}
		}
				
		return false;
	}
};

LLDrawable* LLSpatialPartition::lineSegmentIntersect(const LLVector3& start, const LLVector3& end,
													 BOOL pick_transparent,													
													 S32* face_hit,                   // return the face hit
													 LLVector3* intersection,         // return the intersection point
													 LLVector2* tex_coord,            // return the texture coordinates of the intersection point
													 LLVector3* normal,               // return the surface normal at the intersection point
													 LLVector3* bi_normal             // return the surface bi-normal at the intersection point
	)

{
	LLOctreeIntersect intersect(start, end, pick_transparent, face_hit, intersection, tex_coord, normal, bi_normal);
	LLDrawable* drawable = intersect.check(mOctree);

	return drawable;
}

LLDrawInfo::LLDrawInfo(U16 start, U16 end, U32 count, U32 offset, 
					   LLViewerImage* texture, LLVertexBuffer* buffer,
					   BOOL fullbright, U8 bump, BOOL particle, F32 part_size)
:
	mVertexBuffer(buffer),
	mTexture(texture),
	mTextureMatrix(NULL),
	mModelMatrix(NULL),
	mStart(start),
	mEnd(end),
	mCount(count),
	mOffset(offset), 
	mFullbright(fullbright),
	mBump(bump),
	mParticle(particle),
	mPartSize(part_size),
	mVSize(0.f),
	mGroup(NULL)
{
	mDebugColor = (rand() << 16) + rand();
}

LLDrawInfo::~LLDrawInfo()	
{
	if (LLSpatialGroup::sNoDelete)
	{
		llerrs << "LLDrawInfo deleted illegally!" << llendl;
	}
}

LLVertexBuffer* LLGeometryManager::createVertexBuffer(U32 type_mask, U32 usage)
{
	return new LLVertexBuffer(type_mask, usage);
}

LLCullResult::LLCullResult() 
{
	clear();
}

void LLCullResult::clear()
{
	mVisibleGroupsSize = 0;
	mAlphaGroupsSize = 0;
	mOcclusionGroupsSize = 0;
	mDrawableGroupsSize = 0;
	mVisibleListSize = 0;
	mVisibleBridgeSize = 0;

	for (U32 i = 0; i < LLRenderPass::NUM_RENDER_TYPES; i++)
	{
		for (U32 j = 0; j < mRenderMapSize[i]; j++)
		{
			mRenderMap[i][j] = 0;
		}
		mRenderMapSize[i] = 0;
	}
}

LLCullResult::sg_list_t::iterator LLCullResult::beginVisibleGroups()
{
	return mVisibleGroups.begin();
}

LLCullResult::sg_list_t::iterator LLCullResult::endVisibleGroups()
{
	return mVisibleGroups.begin() + mVisibleGroupsSize;
}

LLCullResult::sg_list_t::iterator LLCullResult::beginAlphaGroups()
{
	return mAlphaGroups.begin();
}

LLCullResult::sg_list_t::iterator LLCullResult::endAlphaGroups()
{
	return mAlphaGroups.begin() + mAlphaGroupsSize;
}

LLCullResult::sg_list_t::iterator LLCullResult::beginOcclusionGroups()
{
	return mOcclusionGroups.begin();
}

LLCullResult::sg_list_t::iterator LLCullResult::endOcclusionGroups()
{
	return mOcclusionGroups.begin() + mOcclusionGroupsSize;
}

LLCullResult::sg_list_t::iterator LLCullResult::beginDrawableGroups()
{
	return mDrawableGroups.begin();
}

LLCullResult::sg_list_t::iterator LLCullResult::endDrawableGroups()
{
	return mDrawableGroups.begin() + mDrawableGroupsSize;
}

LLCullResult::drawable_list_t::iterator LLCullResult::beginVisibleList()
{
	return mVisibleList.begin();
}

LLCullResult::drawable_list_t::iterator LLCullResult::endVisibleList()
{
	return mVisibleList.begin() + mVisibleListSize;
}

LLCullResult::bridge_list_t::iterator LLCullResult::beginVisibleBridge()
{
	return mVisibleBridge.begin();
}

LLCullResult::bridge_list_t::iterator LLCullResult::endVisibleBridge()
{
	return mVisibleBridge.begin() + mVisibleBridgeSize;
}

LLCullResult::drawinfo_list_t::iterator LLCullResult::beginRenderMap(U32 type)
{
	return mRenderMap[type].begin();
}

LLCullResult::drawinfo_list_t::iterator LLCullResult::endRenderMap(U32 type)
{
	return mRenderMap[type].begin() + mRenderMapSize[type];
}

void LLCullResult::pushVisibleGroup(LLSpatialGroup* group)
{
	if (mVisibleGroupsSize < mVisibleGroups.size())
	{
		mVisibleGroups[mVisibleGroupsSize] = group;
	}
	else
	{
		mVisibleGroups.push_back(group);
	}
	++mVisibleGroupsSize;
}

void LLCullResult::pushAlphaGroup(LLSpatialGroup* group)
{
	if (mAlphaGroupsSize < mAlphaGroups.size())
	{
		mAlphaGroups[mAlphaGroupsSize] = group;
	}
	else
	{
		mAlphaGroups.push_back(group);
	}
	++mAlphaGroupsSize;
}

void LLCullResult::pushOcclusionGroup(LLSpatialGroup* group)
{
	if (mOcclusionGroupsSize < mOcclusionGroups.size())
	{
		mOcclusionGroups[mOcclusionGroupsSize] = group;
	}
	else
	{
		mOcclusionGroups.push_back(group);
	}
	++mOcclusionGroupsSize;
}

void LLCullResult::pushDrawableGroup(LLSpatialGroup* group)
{
	if (mDrawableGroupsSize < mDrawableGroups.size())
	{
		mDrawableGroups[mDrawableGroupsSize] = group;
	}
	else
	{
		mDrawableGroups.push_back(group);
	}
	++mDrawableGroupsSize;
}

void LLCullResult::pushDrawable(LLDrawable* drawable)
{
	if (mVisibleListSize < mVisibleList.size())
	{
		mVisibleList[mVisibleListSize] = drawable;
	}
	else
	{
		mVisibleList.push_back(drawable);
	}
	++mVisibleListSize;
}

void LLCullResult::pushBridge(LLSpatialBridge* bridge)
{
	if (mVisibleBridgeSize < mVisibleBridge.size())
	{
		mVisibleBridge[mVisibleBridgeSize] = bridge;
	}
	else
	{
		mVisibleBridge.push_back(bridge);
	}
	++mVisibleBridgeSize;
}

void LLCullResult::pushDrawInfo(U32 type, LLDrawInfo* draw_info)
{
	if (mRenderMapSize[type] < mRenderMap[type].size())
	{
		mRenderMap[type][mRenderMapSize[type]] = draw_info;
	}
	else
	{
		mRenderMap[type].push_back(draw_info);
	}
	++mRenderMapSize[type];
}


void LLCullResult::assertDrawMapsEmpty()
{
	for (U32 i = 0; i < LLRenderPass::NUM_RENDER_TYPES; i++)
	{
		if (mRenderMapSize[i] != 0)
		{
			llerrs << "Stale LLDrawInfo's in LLCullResult!" << llendl;
		}
	}
}



