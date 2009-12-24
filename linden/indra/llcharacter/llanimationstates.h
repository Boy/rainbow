/** 
 * @file llanimationstates.h
 * @brief Implementation of animation state support.
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

#ifndef LL_LLANIMATIONSTATES_H
#define LL_LLANIMATIONSTATES_H

#include <map>

#include "string_table.h"
#include "lluuid.h"

//-----------------------------------------------------------------------------
// These bit flags are generally used to track the animation state
// of characters.  The simulator and viewer share these flags to interpret
// the Animation name/value attribute on agents.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Agent Animation State
//-----------------------------------------------------------------------------
const S32 MAX_CONCURRENT_ANIMS = 16;


const LLUUID ANIM_AGENT_AFRAID				= LLUUID("6b61c8e8-4747-0d75-12d7-e49ff207a4ca");
const LLUUID ANIM_AGENT_AIM_BAZOOKA_R		= LLUUID("b5b4a67d-0aee-30d2-72cd-77b333e932ef");
const LLUUID ANIM_AGENT_AIM_BOW_L			= LLUUID("46bb4359-de38-4ed8-6a22-f1f52fe8f506");
const LLUUID ANIM_AGENT_AIM_HANDGUN_R		= LLUUID("3147d815-6338-b932-f011-16b56d9ac18b");
const LLUUID ANIM_AGENT_AIM_RIFLE_R			= LLUUID("ea633413-8006-180a-c3ba-96dd1d756720");
const LLUUID ANIM_AGENT_ANGRY				= LLUUID("5747a48e-073e-c331-f6f3-7c2149613d3e");
const LLUUID ANIM_AGENT_AWAY				= LLUUID("fd037134-85d4-f241-72c6-4f42164fedee");
const LLUUID ANIM_AGENT_BACKFLIP			= LLUUID("c4ca6188-9127-4f31-0158-23c4e2f93304");
const LLUUID ANIM_AGENT_BELLY_LAUGH			= LLUUID("18b3a4b5-b463-bd48-e4b6-71eaac76c515");
const LLUUID ANIM_AGENT_BLOW_KISS			= LLUUID("db84829b-462c-ee83-1e27-9bbee66bd624");
const LLUUID ANIM_AGENT_BORED				= LLUUID("b906c4ba-703b-1940-32a3-0c7f7d791510");
const LLUUID ANIM_AGENT_BOW					= LLUUID("82e99230-c906-1403-4d9c-3889dd98daba");
const LLUUID ANIM_AGENT_BRUSH				= LLUUID("349a3801-54f9-bf2c-3bd0-1ac89772af01");
const LLUUID ANIM_AGENT_BUSY				= LLUUID("efcf670c-2d18-8128-973a-034ebc806b67");
const LLUUID ANIM_AGENT_CLAP				= LLUUID("9b0c1c4e-8ac7-7969-1494-28c874c4f668");
const LLUUID ANIM_AGENT_COURTBOW			= LLUUID("9ba1c942-08be-e43a-fb29-16ad440efc50");
const LLUUID ANIM_AGENT_CROUCH				= LLUUID("201f3fdf-cb1f-dbec-201f-7333e328ae7c");
const LLUUID ANIM_AGENT_CROUCHWALK			= LLUUID("47f5f6fb-22e5-ae44-f871-73aaaf4a6022");
const LLUUID ANIM_AGENT_CRY					= LLUUID("92624d3e-1068-f1aa-a5ec-8244585193ed");
const LLUUID ANIM_AGENT_CUSTOMIZE	 		= LLUUID("038fcec9-5ebd-8a8e-0e2e-6e71a0a1ac53");
const LLUUID ANIM_AGENT_CUSTOMIZE_DONE		= LLUUID("6883a61a-b27b-5914-a61e-dda118a9ee2c");
const LLUUID ANIM_AGENT_DANCE1				= LLUUID("b68a3d7c-de9e-fc87-eec8-543d787e5b0d");
const LLUUID ANIM_AGENT_DANCE2				= LLUUID("928cae18-e31d-76fd-9cc9-2f55160ff818");
const LLUUID ANIM_AGENT_DANCE3				= LLUUID("30047778-10ea-1af7-6881-4db7a3a5a114");
const LLUUID ANIM_AGENT_DANCE4				= LLUUID("951469f4-c7b2-c818-9dee-ad7eea8c30b7");
const LLUUID ANIM_AGENT_DANCE5				= LLUUID("4bd69a1d-1114-a0b4-625f-84e0a5237155");
const LLUUID ANIM_AGENT_DANCE6				= LLUUID("cd28b69b-9c95-bb78-3f94-8d605ff1bb12");
const LLUUID ANIM_AGENT_DANCE7				= LLUUID("a54d8ee2-28bb-80a9-7f0c-7afbbe24a5d6");
const LLUUID ANIM_AGENT_DANCE8				= LLUUID("b0dc417c-1f11-af36-2e80-7e7489fa7cdc");
const LLUUID ANIM_AGENT_DEAD				= LLUUID("57abaae6-1d17-7b1b-5f98-6d11a6411276");
const LLUUID ANIM_AGENT_DRINK				= LLUUID("0f86e355-dd31-a61c-fdb0-3a96b9aad05f");
const LLUUID ANIM_AGENT_EMBARRASSED			= LLUUID("514af488-9051-044a-b3fc-d4dbf76377c6");
const LLUUID ANIM_AGENT_EXPRESS_AFRAID		= LLUUID("aa2df84d-cf8f-7218-527b-424a52de766e");
const LLUUID ANIM_AGENT_EXPRESS_ANGER		= LLUUID("1a03b575-9634-b62a-5767-3a679e81f4de");
const LLUUID ANIM_AGENT_EXPRESS_BORED		= LLUUID("214aa6c1-ba6a-4578-f27c-ce7688f61d0d");
const LLUUID ANIM_AGENT_EXPRESS_CRY			= LLUUID("d535471b-85bf-3b4d-a542-93bea4f59d33");
const LLUUID ANIM_AGENT_EXPRESS_DISDAIN		= LLUUID("d4416ff1-09d3-300f-4183-1b68a19b9fc1");
const LLUUID ANIM_AGENT_EXPRESS_EMBARRASSED = LLUUID("0b8c8211-d78c-33e8-fa28-c51a9594e424");
const LLUUID ANIM_AGENT_EXPRESS_FROWN		= LLUUID("fee3df48-fa3d-1015-1e26-a205810e3001");
const LLUUID ANIM_AGENT_EXPRESS_KISS		= LLUUID("1e8d90cc-a84e-e135-884c-7c82c8b03a14");
const LLUUID ANIM_AGENT_EXPRESS_LAUGH		= LLUUID("62570842-0950-96f8-341c-809e65110823");
const LLUUID ANIM_AGENT_EXPRESS_OPEN_MOUTH	= LLUUID("d63bc1f9-fc81-9625-a0c6-007176d82eb7");
const LLUUID ANIM_AGENT_EXPRESS_REPULSED	= LLUUID("f76cda94-41d4-a229-2872-e0296e58afe1");
const LLUUID ANIM_AGENT_EXPRESS_SAD			= LLUUID("eb6ebfb2-a4b3-a19c-d388-4dd5c03823f7");
const LLUUID ANIM_AGENT_EXPRESS_SHRUG		= LLUUID("a351b1bc-cc94-aac2-7bea-a7e6ebad15ef");
const LLUUID ANIM_AGENT_EXPRESS_SMILE		= LLUUID("b7c7c833-e3d3-c4e3-9fc0-131237446312");
const LLUUID ANIM_AGENT_EXPRESS_SURPRISE	= LLUUID("728646d9-cc79-08b2-32d6-937f0a835c24");
const LLUUID ANIM_AGENT_EXPRESS_TONGUE_OUT	= LLUUID("835965c6-7f2f-bda2-5deb-2478737f91bf");
const LLUUID ANIM_AGENT_EXPRESS_TOOTHSMILE	= LLUUID("b92ec1a5-e7ce-a76b-2b05-bcdb9311417e");
const LLUUID ANIM_AGENT_EXPRESS_WINK		= LLUUID("da020525-4d94-59d6-23d7-81fdebf33148");
const LLUUID ANIM_AGENT_EXPRESS_WORRY		= LLUUID("9c05e5c7-6f07-6ca4-ed5a-b230390c3950");
const LLUUID ANIM_AGENT_FALLDOWN			= LLUUID("666307d9-a860-572d-6fd4-c3ab8865c094");
const LLUUID ANIM_AGENT_FEMALE_WALK			= LLUUID("f5fc7433-043d-e819-8298-f519a119b688");
const LLUUID ANIM_AGENT_FINGER_WAG			= LLUUID("c1bc7f36-3ba0-d844-f93c-93be945d644f");
const LLUUID ANIM_AGENT_FIST_PUMP			= LLUUID("7db00ccd-f380-f3ee-439d-61968ec69c8a");
const LLUUID ANIM_AGENT_FLY					= LLUUID("aec4610c-757f-bc4e-c092-c6e9caf18daf");
const LLUUID ANIM_AGENT_FLYSLOW				= LLUUID("2b5a38b2-5e00-3a97-a495-4c826bc443e6");
const LLUUID ANIM_AGENT_HELLO				= LLUUID("9b29cd61-c45b-5689-ded2-91756b8d76a9");
const LLUUID ANIM_AGENT_HOLD_BAZOOKA_R		= LLUUID("ef62d355-c815-4816-2474-b1acc21094a6");
const LLUUID ANIM_AGENT_HOLD_BOW_L			= LLUUID("8b102617-bcba-037b-86c1-b76219f90c88");
const LLUUID ANIM_AGENT_HOLD_HANDGUN_R		= LLUUID("efdc1727-8b8a-c800-4077-975fc27ee2f2");
const LLUUID ANIM_AGENT_HOLD_RIFLE_R		= LLUUID("3d94bad0-c55b-7dcc-8763-033c59405d33");
const LLUUID ANIM_AGENT_HOLD_THROW_R		= LLUUID("7570c7b5-1f22-56dd-56ef-a9168241bbb6");
const LLUUID ANIM_AGENT_HOVER				= LLUUID("4ae8016b-31b9-03bb-c401-b1ea941db41d");
const LLUUID ANIM_AGENT_HOVER_DOWN			= LLUUID("20f063ea-8306-2562-0b07-5c853b37b31e");
const LLUUID ANIM_AGENT_HOVER_UP			= LLUUID("62c5de58-cb33-5743-3d07-9e4cd4352864");
const LLUUID ANIM_AGENT_IMPATIENT			= LLUUID("5ea3991f-c293-392e-6860-91dfa01278a3");
const LLUUID ANIM_AGENT_JUMP				= LLUUID("2305bd75-1ca9-b03b-1faa-b176b8a8c49e");
const LLUUID ANIM_AGENT_JUMP_FOR_JOY		= LLUUID("709ea28e-1573-c023-8bf8-520c8bc637fa");
const LLUUID ANIM_AGENT_KISS_MY_BUTT		= LLUUID("19999406-3a3a-d58c-a2ac-d72e555dcf51");
const LLUUID ANIM_AGENT_LAND				= LLUUID("7a17b059-12b2-41b1-570a-186368b6aa6f");
const LLUUID ANIM_AGENT_LAUGH_SHORT			= LLUUID("ca5b3f14-3194-7a2b-c894-aa699b718d1f");
const LLUUID ANIM_AGENT_MEDIUM_LAND			= LLUUID("f4f00d6e-b9fe-9292-f4cb-0ae06ea58d57");
const LLUUID ANIM_AGENT_MOTORCYCLE_SIT		= LLUUID("08464f78-3a8e-2944-cba5-0c94aff3af29");
const LLUUID ANIM_AGENT_MUSCLE_BEACH		= LLUUID("315c3a41-a5f3-0ba4-27da-f893f769e69b");
const LLUUID ANIM_AGENT_NO					= LLUUID("5a977ed9-7f72-44e9-4c4c-6e913df8ae74");
const LLUUID ANIM_AGENT_NO_UNHAPPY			= LLUUID("d83fa0e5-97ed-7eb2-e798-7bd006215cb4");
const LLUUID ANIM_AGENT_NYAH_NYAH			= LLUUID("f061723d-0a18-754f-66ee-29a44795a32f");
const LLUUID ANIM_AGENT_ONETWO_PUNCH		= LLUUID("eefc79be-daae-a239-8c04-890f5d23654a");
const LLUUID ANIM_AGENT_PEACE				= LLUUID("b312b10e-65ab-a0a4-8b3c-1326ea8e3ed9");
const LLUUID ANIM_AGENT_POINT_ME			= LLUUID("17c024cc-eef2-f6a0-3527-9869876d7752");
const LLUUID ANIM_AGENT_POINT_YOU			= LLUUID("ec952cca-61ef-aa3b-2789-4d1344f016de");
const LLUUID ANIM_AGENT_PRE_JUMP			= LLUUID("7a4e87fe-de39-6fcb-6223-024b00893244");
const LLUUID ANIM_AGENT_PUNCH_LEFT			= LLUUID("f3300ad9-3462-1d07-2044-0fef80062da0");
const LLUUID ANIM_AGENT_PUNCH_RIGHT			= LLUUID("c8e42d32-7310-6906-c903-cab5d4a34656");
const LLUUID ANIM_AGENT_REPULSED			= LLUUID("36f81a92-f076-5893-dc4b-7c3795e487cf");
const LLUUID ANIM_AGENT_ROUNDHOUSE_KICK		= LLUUID("49aea43b-5ac3-8a44-b595-96100af0beda");
const LLUUID ANIM_AGENT_RPS_COUNTDOWN		= LLUUID("35db4f7e-28c2-6679-cea9-3ee108f7fc7f");
const LLUUID ANIM_AGENT_RPS_PAPER			= LLUUID("0836b67f-7f7b-f37b-c00a-460dc1521f5a");
const LLUUID ANIM_AGENT_RPS_ROCK			= LLUUID("42dd95d5-0bc6-6392-f650-777304946c0f");
const LLUUID ANIM_AGENT_RPS_SCISSORS		= LLUUID("16803a9f-5140-e042-4d7b-d28ba247c325");
const LLUUID ANIM_AGENT_RUN					= LLUUID("05ddbff8-aaa9-92a1-2b74-8fe77a29b445");
const LLUUID ANIM_AGENT_SAD					= LLUUID("0eb702e2-cc5a-9a88-56a5-661a55c0676a");
const LLUUID ANIM_AGENT_SALUTE				= LLUUID("cd7668a6-7011-d7e2-ead8-fc69eff1a104");
const LLUUID ANIM_AGENT_SHOOT_BOW_L			= LLUUID("e04d450d-fdb5-0432-fd68-818aaf5935f8");
const LLUUID ANIM_AGENT_SHOUT				= LLUUID("6bd01860-4ebd-127a-bb3d-d1427e8e0c42");
const LLUUID ANIM_AGENT_SHRUG				= LLUUID("70ea714f-3a97-d742-1b01-590a8fcd1db5");
const LLUUID ANIM_AGENT_SIT					= LLUUID("1a5fe8ac-a804-8a5d-7cbd-56bd83184568");
const LLUUID ANIM_AGENT_SIT_FEMALE			= LLUUID("b1709c8d-ecd3-54a1-4f28-d55ac0840782");
const LLUUID ANIM_AGENT_SIT_GENERIC			= LLUUID("245f3c54-f1c0-bf2e-811f-46d8eeb386e7");
const LLUUID ANIM_AGENT_SIT_GROUND 			= LLUUID("1c7600d6-661f-b87b-efe2-d7421eb93c86");
const LLUUID ANIM_AGENT_SIT_GROUND_CONSTRAINED	= LLUUID("1a2bd58e-87ff-0df8-0b4c-53e047b0bb6e");
const LLUUID ANIM_AGENT_SIT_TO_STAND		= LLUUID("a8dee56f-2eae-9e7a-05a2-6fb92b97e21e");
const LLUUID ANIM_AGENT_SLEEP				= LLUUID("f2bed5f9-9d44-39af-b0cd-257b2a17fe40");
const LLUUID ANIM_AGENT_SMOKE_IDLE			= LLUUID("d2f2ee58-8ad1-06c9-d8d3-3827ba31567a");
const LLUUID ANIM_AGENT_SMOKE_INHALE		= LLUUID("6802d553-49da-0778-9f85-1599a2266526");
const LLUUID ANIM_AGENT_SMOKE_THROW_DOWN	= LLUUID("0a9fb970-8b44-9114-d3a9-bf69cfe804d6");
const LLUUID ANIM_AGENT_SNAPSHOT			= LLUUID("eae8905b-271a-99e2-4c0e-31106afd100c");
const LLUUID ANIM_AGENT_STAND				= LLUUID("2408fe9e-df1d-1d7d-f4ff-1384fa7b350f");
const LLUUID ANIM_AGENT_STANDUP				= LLUUID("3da1d753-028a-5446-24f3-9c9b856d9422");
const LLUUID ANIM_AGENT_STAND_1				= LLUUID("15468e00-3400-bb66-cecc-646d7c14458e");
const LLUUID ANIM_AGENT_STAND_2				= LLUUID("370f3a20-6ca6-9971-848c-9a01bc42ae3c");
const LLUUID ANIM_AGENT_STAND_3				= LLUUID("42b46214-4b44-79ae-deb8-0df61424ff4b");
const LLUUID ANIM_AGENT_STAND_4				= LLUUID("f22fed8b-a5ed-2c93-64d5-bdd8b93c889f");
const LLUUID ANIM_AGENT_STRETCH				= LLUUID("80700431-74ec-a008-14f8-77575e73693f");
const LLUUID ANIM_AGENT_STRIDE				= LLUUID("1cb562b0-ba21-2202-efb3-30f82cdf9595");
const LLUUID ANIM_AGENT_SURF				= LLUUID("41426836-7437-7e89-025d-0aa4d10f1d69");
const LLUUID ANIM_AGENT_SURPRISE			= LLUUID("313b9881-4302-73c0-c7d0-0e7a36b6c224");
const LLUUID ANIM_AGENT_SWORD_STRIKE		= LLUUID("85428680-6bf9-3e64-b489-6f81087c24bd");
const LLUUID ANIM_AGENT_TALK				= LLUUID("5c682a95-6da4-a463-0bf6-0f5b7be129d1");
const LLUUID ANIM_AGENT_TANTRUM				= LLUUID("11000694-3f41-adc2-606b-eee1d66f3724");
const LLUUID ANIM_AGENT_THROW_R				= LLUUID("aa134404-7dac-7aca-2cba-435f9db875ca");
const LLUUID ANIM_AGENT_TRYON_SHIRT			= LLUUID("83ff59fe-2346-f236-9009-4e3608af64c1");
const LLUUID ANIM_AGENT_TURNLEFT			= LLUUID("56e0ba0d-4a9f-7f27-6117-32f2ebbf6135");
const LLUUID ANIM_AGENT_TURNRIGHT			= LLUUID("2d6daa51-3192-6794-8e2e-a15f8338ec30");
const LLUUID ANIM_AGENT_TYPE				= LLUUID("c541c47f-e0c0-058b-ad1a-d6ae3a4584d9");
const LLUUID ANIM_AGENT_WALK				= LLUUID("6ed24bd8-91aa-4b12-ccc7-c97c857ab4e0");
const LLUUID ANIM_AGENT_WHISPER				= LLUUID("7693f268-06c7-ea71-fa21-2b30d6533f8f");
const LLUUID ANIM_AGENT_WHISTLE				= LLUUID("b1ed7982-c68e-a982-7561-52a88a5298c0");
const LLUUID ANIM_AGENT_WINK				= LLUUID("869ecdad-a44b-671e-3266-56aef2e3ac2e");
const LLUUID ANIM_AGENT_WINK_HOLLYWOOD		= LLUUID("c0c4030f-c02b-49de-24ba-2331f43fe41c");
const LLUUID ANIM_AGENT_WORRY				= LLUUID("9f496bd2-589a-709f-16cc-69bf7df1d36c");
const LLUUID ANIM_AGENT_YES					= LLUUID("15dd911d-be82-2856-26db-27659b142875");
const LLUUID ANIM_AGENT_YES_HAPPY			= LLUUID("b8c8b2a3-9008-1771-3bfc-90924955ab2d");
const LLUUID ANIM_AGENT_YOGA_FLOAT			= LLUUID("42ecd00b-9947-a97c-400a-bbc9174c7aeb");

extern LLUUID AGENT_WALK_ANIMS[];
extern S32 NUM_AGENT_WALK_ANIMS;

extern LLUUID AGENT_GUN_HOLD_ANIMS[];
extern S32 NUM_AGENT_GUN_HOLD_ANIMS;

extern LLUUID AGENT_GUN_AIM_ANIMS[];
extern S32 NUM_AGENT_GUN_AIM_ANIMS;

extern LLUUID AGENT_NO_ROTATE_ANIMS[];
extern S32 NUM_AGENT_NO_ROTATE_ANIMS;

extern LLUUID AGENT_STAND_ANIMS[];
extern S32 NUM_AGENT_STAND_ANIMS;

class LLAnimationLibrary
{
private:
	LLStringTable mAnimStringTable;

	typedef std::map<LLUUID, char *> anim_map_t;
	anim_map_t mAnimMap;

public:
	LLAnimationLibrary();
	~LLAnimationLibrary();

	//-----------------------------------------------------------------------------
	// Return the text name of a single animation state,
	// Return NULL if the state is invalid
	//-----------------------------------------------------------------------------
	const char *animStateToString( const LLUUID& state );

	//-----------------------------------------------------------------------------
	// Return the animation state for the given name.
	// Retun NULL if the name is invalid.
	//-----------------------------------------------------------------------------
	LLUUID stringToAnimState( const std::string& name, BOOL allow_ids = TRUE );
};

struct LLAnimStateEntry
{
	LLAnimStateEntry(const char* name, const LLUUID& id) :
		mName(name),
		mID(id)
	{ 
		// LABELS:
		// Look to newview/LLAnimStateLabels.* for how to get the labels.
		// The labels should no longer be stored in this structure. The server
		// shouldn't care about the local friendly name of an animation, and
		// this is common code.
	}


	const char* mName;
	const LLUUID mID;
};

// Animation states that the user can trigger
extern const LLAnimStateEntry gUserAnimStates[];
extern const S32 gUserAnimStatesCount;
extern LLAnimationLibrary gAnimLibrary;


#endif // LL_LLANIMATIONSTATES_H



