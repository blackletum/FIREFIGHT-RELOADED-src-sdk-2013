//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: This is the soldier version of the combine, analogous to the HL1 grunt.
//
//=============================================================================//

#include "cbase.h"
#include "ai_hull.h"
#include "ai_motor.h"
#include "npc_player.h"
#include "bitstring.h"
#include "engine/IEngineSound.h"
#include "soundent.h"
#include "ndebugoverlay.h"
#include "npcevent.h"
#include "hl2/hl2_player.h"
#include "game.h"
#include "ammodef.h"
#include "explode.h"
#include "ai_memory.h"
#include "Sprite.h"
#include "soundenvelope.h"
#include "weapon_physcannon.h"
#include "hl2_gamerules.h"
#include "gameweaponmanager.h"
#include "vehicle_base.h"
#include "gib.h"
#include "filesystem.h"
// TODO: add npc_citizen17 features, like player squads!!!
#include "ai_squad.h"

/*
#include <time.h>
#include <vector>
#include "mathlib/vector.h"
*/

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//TODO: GIVE PLAYERS A HINT WHEN ALLY SPAWN

extern ConVar sk_combine_guard_kick;
extern ConVar sk_plr_dmg_buckshot;	
extern ConVar sk_plr_num_shotgun_pellets;
extern ConVar sv_player_hardcoremode;
extern ConVar fr_new_normspeed;
extern ConVar sv_regeneration;
extern ConVar sv_regeneration_wait_time;
extern ConVar sv_regeneration_rate_default;
extern ConVar sv_regeneration_rate;
extern ConVar sv_regen_interval;

LINK_ENTITY_TO_CLASS( npc_playerbot, CNPC_Player );

BEGIN_DATADESC(CNPC_Player)
DEFINE_FIELD(m_flSoonestWeaponSwitch, FIELD_TIME),
DEFINE_FIELD(m_fTimeLastHurt, FIELD_TIME),
DEFINE_FIELD(m_fTimeLastHealed, FIELD_TIME),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST(CNPC_Player, DT_NPC_Player)
END_SEND_TABLE()

#define	PLAYERNPC_FASTEST_SWITCH_TIME 5.0f
const int MAX_PLAYER_SQUAD = 4;

const char* g_charNPCMidRangeWeapons[] =
{
	"weapon_smg1",
	"weapon_ar2"
};

const char* g_charNPCShortRangeWeapons[] =
{
	"weapon_shotgun",
	"weapon_pistol",
	"weapon_crowbar"
};

const char* g_charAvailableModels[] =
{
	"models/player/playermodels/gordon.mdl",
	"models/player/playermodels/gordon_old.mdl",
	"models/player/playermodels/male_01.mdl",
	"models/player/playermodels/male_02.mdl",
	"models/player/playermodels/male_03.mdl",
	"models/player/playermodels/male_04.mdl",
	"models/player/playermodels/male_05.mdl",
	"models/player/playermodels/male_06.mdl",
	"models/player/playermodels/male_07.mdl",
	"models/player/playermodels/male_08.mdl",
	"models/player/playermodels/male_09.mdl",
	"models/player/playermodels/female_01.mdl",
	"models/player/playermodels/female_02.mdl",
	"models/player/playermodels/female_03.mdl",
	"models/player/playermodels/female_04.mdl",
	"models/player/playermodels/female_06.mdl",
	"models/player/playermodels/female_07.mdl"
};

CNPC_Player::CNPC_Player()
{
	m_flSoonestWeaponSwitch = gpGlobals->curtime;
	m_fRegenRemander = 0;
}

CNPC_Player::~CNPC_Player(void)
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_Player::Spawn( void )
{
	Precache();

	// Stronger, tougher.
	SetHealth(200);
	SetMaxHealth(200);
	SetKickDamage(sk_combine_guard_kick.GetFloat());

	int nModels = ARRAYSIZE(g_charAvailableModels);
	int randomChoiceModels = rand() % nModels;
	const char* pRandomName = g_charAvailableModels[randomChoiceModels];

	SetModel(pRandomName);

	//Give him a random amount of grenades on spawn
	m_iNumGrenades = random->RandomInt(3, 5);
	AddGlowEffect();

	m_fIsPlayer = true;

	CapabilitiesAdd( bits_CAP_ANIMATEDFACE );
	CapabilitiesAdd( bits_CAP_MOVE_SHOOT );
	CapabilitiesAdd(bits_CAP_MOVE_JUMP);
	CapabilitiesAdd( bits_CAP_DOORS_GROUP );

	GiveWeapons();

	BaseClass::Spawn();
}

void CNPC_Player::GiveWeapons(void)
{
	int nWeaponsMid = ARRAYSIZE(g_charNPCMidRangeWeapons);
	int randomChoiceMid = rand() % nWeaponsMid;
	const char* pRandomNameMid = g_charNPCMidRangeWeapons[randomChoiceMid];
	GiveWeapon(pRandomNameMid);
	DevMsg("PLAYER: GIVING MID RANGE WEAPON %s.\n", pRandomNameMid);

	int nWeaponsShort = ARRAYSIZE(g_charNPCShortRangeWeapons);
	int randomChoiceShort = rand() % nWeaponsShort;
	const char* pRandomNameShort = g_charNPCShortRangeWeapons[randomChoiceShort];
	GiveWeapon(pRandomNameShort);
	DevMsg("PLAYER: GIVING SHORT RANGE WEAPON %s.\n", pRandomNameShort);
}

void CNPC_Player::GiveWeapon(const char* iszWeaponName)
{
	CBaseCombatWeapon* pWeapon = Weapon_Create(iszWeaponName);
	if (!pWeapon)
	{
		Warning("Couldn't create weapon %s to give NPC %s.\n", iszWeaponName, GetEntityName());
		return;
	}

	// If I have a weapon already, drop it
	if (GetActiveWeapon())
	{
		GetActiveWeapon()->DestroyItem();
	}

	// If I have a name, make my weapon match it with "_weapon" appended
	if (GetEntityName() != NULL_STRING)
	{
		pWeapon->SetName(AllocPooledString(UTIL_VarArgs("%s_weapon", GetEntityName())));
	}

	Weapon_Equip(pWeapon);

	// Handle this case
	OnGivenWeapon(pWeapon);
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_Player::Precache()
{
	for (const char* i : g_charAvailableModels)
	{
		CBaseEntity::PrecacheModel(i);
	}

	for (const char* i : g_charNPCShortRangeWeapons)
	{
		UTIL_PrecacheOther(i);
	}

	for (const char* i : g_charNPCMidRangeWeapons)
	{
		UTIL_PrecacheOther(i);
	}

	UTIL_PrecacheOther( "item_healthvial" );
	UTIL_PrecacheOther( "weapon_frag" );
	UTIL_PrecacheOther( "item_ammo_ar2_altfire" );
	UTIL_PrecacheOther( "item_ammo_smg1_grenade" );
	UTIL_PrecacheOther( "item_oicw_grenade" );

	BaseClass::Precache();
}

Class_T	CNPC_Player::Classify(void)
{
	return CLASS_PLAYER_NPC;
}

float CNPC_Player::GetSequenceGroundSpeed(CStudioHdr* pStudioHdr, int iSequence)
{
	return fr_new_normspeed.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose: Soldiers use CAN_RANGE_ATTACK2 to indicate whether they can throw
//			a grenade. Because they check only every half-second or so, this
//			condition must persist until it is updated again by the code
//			that determines whether a grenade can be thrown, so prevent the 
//			base class from clearing it out. (sjb)
//-----------------------------------------------------------------------------
void CNPC_Player::ClearAttackConditions( )
{
	bool fCanRangeAttack2 = HasCondition( COND_CAN_RANGE_ATTACK2 );

	// Call the base class.
	BaseClass::ClearAttackConditions();

	if( fCanRangeAttack2 )
	{
		// We don't allow the base class to clear this condition because we
		// don't sense for it every frame.
		SetCondition( COND_CAN_RANGE_ATTACK2 );
	}
}
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_Player::OnTakeDamage_Alive(const CTakeDamageInfo& info)
{
	CTakeDamageInfo subInfo = info;

	if (subInfo.GetDamageType() != DMG_GENERIC)
	{
		if (info.GetAttacker()->IsPlayer())
		{
			// no friendly fire.
			subInfo.SetDamage(0);
		}
		else
		{
			//only take half of the damage so we can be around for a bit longer.
			float flDamage = subInfo.GetDamage();
			float flNewDmg = (flDamage * 0.5);
			if (flDamage > flNewDmg)
			{
				flDamage = flNewDmg;
				subInfo.SetDamage(flDamage);
			}
		}
	}

	return BaseClass::OnTakeDamage_Alive(subInfo);
}

void CNPC_Player::NPCThink( void )
{
	// EXCEPTIONS
	if (GetEnemy() && (m_flSoonestWeaponSwitch < gpGlobals->curtime))
	{
		CBaseCombatWeapon* pActiveWeapon = GetActiveWeapon();
		if (pActiveWeapon)
		{
			DevMsg("PLAYER: SWITCHING.\n");
			if (SwitchToNextBestWeaponBot(pActiveWeapon))
			{
				m_flSoonestWeaponSwitch = gpGlobals->curtime + PLAYERNPC_FASTEST_SWITCH_TIME;
			}
		}
	}

	// regeneration
	if (IsAlive() && GetHealth() < GetMaxHealth() && (sv_regeneration.GetInt() == 1))
	{
		// Color to overlay on the screen while the player is taking damage

		if (gpGlobals->curtime > m_fTimeLastHurt + sv_regeneration_wait_time.GetFloat())
		{
			//Regenerate based on rate, and scale it by the frametime
			m_fRegenRemander += sv_regeneration_rate.GetFloat() * gpGlobals->frametime;

			if (m_fRegenRemander >= 1)
			{
				//If the regen interval is set, and the health is evenly divisible by that interval, don't regen.
				if (sv_regen_interval.GetFloat() > 0 && floor(m_iHealth / sv_regen_interval.GetFloat()) == m_iHealth / sv_regen_interval.GetFloat())
				{
					m_fRegenRemander = 0;
					DevMsg("PLAYER: Player %s health is at %i\n", GetModelName(), GetHealth());
				}
				else
				{
					TakeHealth(m_fRegenRemander, DMG_GENERIC);
					m_fRegenRemander = 0;
					DevMsg("PLAYER: Player %s health is at %i\n", GetModelName(), GetHealth());
				}
			}
		}
	}

	BaseClass::NPCThink();
}

//ported bot funcs
float CNPC_Player::BotWeaponRangeDetermine(CBaseCombatWeapon* pActiveWeapon)
{
	if (pActiveWeapon == NULL)
		DevWarning("PLAYER: DETERMINED MAX RANGE FOR INVALID WEAPON\n");
		return SKILL_MAX_RANGE;

	if (GetEnemy() == NULL)
		DevMsg("PLAYER: DETERMINED MID RANGE\n");
		return SKILL_MID_RANGE;
	
	for (const char* i : g_charNPCShortRangeWeapons)
	{
		if (FClassnameIs(pActiveWeapon, i))
		{
			DevMsg("PLAYER: DETERMINED SHORT RANGE\n");
			return SKILL_SHORT_RANGE;
		}
	}

	for (const char* i : g_charNPCMidRangeWeapons)
	{
		if (FClassnameIs(pActiveWeapon, i))
		{
			DevMsg("PLAYER: DETERMINED MID RANGE\n");
			return SKILL_MID_RANGE;
		}
	}

	DevMsg("PLAYER: DETERMINED MAX RANGE\n");
	return SKILL_MAX_RANGE;
}

CBaseCombatWeapon* CNPC_Player::GetNextBestWeaponBot(CBaseCombatWeapon* pCurrentWeapon)
{
	CBaseCombatWeapon* pCheck;
	CBaseCombatWeapon* pBest;// this will be used in the event that we don't find a weapon in the same category.

	pBest = NULL;

	for (int i = 0; i < WeaponCount(); ++i)
	{
		pCheck = GetWeapon(i);
		if (!pCheck)
			continue;

		if (pCheck->HasAnyAmmo())
		{
			if (GetEnemy())
			{
				float flDist;
				flDist = (GetLocalOrigin() - GetEnemy()->GetLocalOrigin()).Length();
				DevMsg("PLAYER: TARGET AT %i\n", (int)flDist);

				if (BotWeaponRangeDetermine(pCheck) >= flDist)
				{
					DevMsg("PLAYER: SETTING %s AS BEST.\n", pCheck->GetClassname());
					// if this weapon is useable, flag it as the best
					pBest = pCheck;
				}
			}
		}

		if (!pCheck->HasAnyAmmo())
		{
			CBaseCombatWeapon* pActiveWeapon = GetActiveWeapon();
			DevMsg("PLAYER: SETTING CURRENT WEAPON AS BEST.\n");
			return pActiveWeapon;
		}
	}

	// if we make it here, we've checked all the weapons and found no useable 
	// weapon in the same catagory as the current weapon. 

	// if pBest is null, we didn't find ANYTHING. Shouldn't be possible- should always 
	// at least get the crowbar, but ya never know.
	return pBest;
}

bool CNPC_Player::SwitchToNextBestWeaponBot(CBaseCombatWeapon* pCurrent)
{
	CBaseCombatWeapon* pNewWeapon = GetNextBestWeaponBot(pCurrent);

	if ((pNewWeapon != NULL))
	{
		return Weapon_Switch(pNewWeapon);
	}

	return false;
}

bool CNPC_Player::Weapon_Switch(CBaseCombatWeapon* pWeapon)
{
	if (!Weapon_OwnsThisType(pWeapon->GetClassname()))
	{
		GiveWeapon(pWeapon->GetClassname());
	}

	// Already have it out?
	if (m_hActiveWeapon.Get() == pWeapon)
	{
		if (!m_hActiveWeapon->IsWeaponVisible() || m_hActiveWeapon->IsHolstered())
			return m_hActiveWeapon->Deploy();
		return false;
	}

	if (!Weapon_CanSwitchTo(pWeapon))
	{
		return false;
	}

	if (m_hActiveWeapon)
	{
		if (!m_hActiveWeapon->Holster(pWeapon))
		{
			return false;
		}
		else
		{
			m_hActiveWeapon->AddEffects(EF_NODRAW);
		}
	}

	pWeapon->RemoveEffects(EF_NODRAW);
	m_hActiveWeapon = pWeapon;

	DevMsg("PLAYER: SWITCHED WEAPON TO: %s\n", pWeapon->GetClassname());

	return pWeapon->Deploy();
}

//-----------------------------------------------------------------------------
// Purpose: Allows for modification of the interrupt mask for the current schedule.
//			In the most cases the base implementation should be called first.
//-----------------------------------------------------------------------------
void CNPC_Player::BuildScheduleTestBits( void )
{
	//Interrupt any schedule with physics danger (as long as I'm not moving or already trying to block)
	if ( m_flGroundSpeed == 0.0 && !IsCurSchedule( SCHED_FLINCH_PHYSICS ) )
	{
		SetCustomInterruptCondition( COND_HEAR_PHYSICS_DANGER );
	}

	BaseClass::BuildScheduleTestBits();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_Player::SelectSchedule ( void )
{
	m_FollowBehavior.SetFollowTarget(UTIL_GetNearestPlayer(GetAbsOrigin()));
	m_FollowBehavior.SetParameters(AIF_SIMPLE);

	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
float CNPC_Player::GetHitgroupDamageMultiplier( int iHitGroup, const CTakeDamageInfo &info )
{
	return BaseClass::GetHitgroupDamageMultiplier( iHitGroup, info );
}

void CNPC_Player::OnListened()
{
	BaseClass::OnListened();

	if ( HasCondition( COND_HEAR_DANGER ) && HasCondition( COND_HEAR_PHYSICS_DANGER ) )
	{
		if ( HasInterruptCondition( COND_HEAR_DANGER ) )
		{
			ClearCondition( COND_HEAR_PHYSICS_DANGER );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CNPC_Player::Event_Killed( const CTakeDamageInfo &info )
{
	RemoveGlowEffect();

	if (m_hActiveWeapon)
	{
		CBaseEntity* pItem = NULL;

		if (FClassnameIs(GetActiveWeapon(), "weapon_ar2"))
		{
			pItem = DropItem("item_ammo_ar2_altfire", WorldSpaceCenter() + RandomVector(-4, 4), RandomAngle(0, 360));
		}
		else if (FClassnameIs(GetActiveWeapon(), "weapon_smg1"))
		{
			pItem = DropItem("item_ammo_smg1_grenade", WorldSpaceCenter() + RandomVector(-4, 4), RandomAngle(0, 360));
		}
		else if (FClassnameIs(GetActiveWeapon(), "weapon_oicw"))
		{
			pItem = DropItem("item_oicw_grenade", WorldSpaceCenter() + RandomVector(-4, 4), RandomAngle(0, 360));
		}

		if (pItem)
		{
			IPhysicsObject* pObj = pItem->VPhysicsGetObject();

			if (pObj)
			{
				Vector			vel = RandomVector(-64.0f, 64.0f);
				AngularImpulse	angImp = RandomAngularImpulse(-300.0f, 300.0f);

				vel[2] = 0.0f;
				pObj->AddVelocity(&vel, &angImp);
			}

			if (info.GetDamageType() & DMG_DISSOLVE)
			{
				CBaseAnimating* pAnimating = dynamic_cast<CBaseAnimating*>(pItem);

				if (pAnimating)
				{
					pAnimating->Dissolve(NULL, gpGlobals->curtime, false, ENTITY_DISSOLVE_NORMAL);
				}
			}
			else
			{
				WeaponManager_AddManaged(pItem);
			}
		}
	}

	BaseClass::Event_Killed( info );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Player::IsLightDamage( const CTakeDamageInfo &info )
{
	return BaseClass::IsLightDamage( info );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_Player::IsHeavyDamage( const CTakeDamageInfo &info )
{
	// Combine considers AR2 fire to be heavy damage
	if ( info.GetAmmoType() == GetAmmoDef()->Index("AR2") )
		return true;

	// 357 rounds are heavy damage
	if ( info.GetAmmoType() == GetAmmoDef()->Index("357") )
		return true;

	// Shotgun blasts where at least half the pellets hit me are heavy damage
	if ( info.GetDamageType() & DMG_BUCKSHOT )
	{
		int iHalfMax = sk_plr_dmg_buckshot.GetFloat() * sk_plr_num_shotgun_pellets.GetInt() * 0.5;
		if ( info.GetDamage() >= iHalfMax )
			return true;
	}

	// Rollermine shocks
	if( (info.GetDamageType() & DMG_SHOCK) && hl2_episodic.GetBool() )
	{
		return true;
	}

	return BaseClass::IsHeavyDamage( info );
}

Activity CNPC_Player::NPC_TranslateActivity(Activity eNewActivity)
{
	return BaseClass::NPC_TranslateActivity(eNewActivity);
}