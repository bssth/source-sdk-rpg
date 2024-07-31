//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Game rules for Half-Life 2.
//
//=============================================================================//

#ifndef BB_GAMERULES_H
#define BB_GAMERULES_H
#ifdef _WIN32
#pragma once
#endif

#include "gamerules.h"
#include "singleplay_gamerules.h"
#include "hl2_shareddefs.h"

#ifdef CLIENT_DLL
	#define CBBRules C_BBRules
	#define CBBRulesProxy C_BBRulesProxy
#endif


class CBBRulesProxy : public CGameRulesProxy
{
public:
	DECLARE_CLASS( CBBRulesProxy, CGameRulesProxy );
	DECLARE_NETWORKCLASS();
};


class CBBRules : public CSingleplayRules
{
public:
	DECLARE_CLASS( CBBRules, CSingleplayRules );

	// Damage Query Overrides.
	virtual bool			Damage_IsTimeBased( int iDmgType );
	// TEMP:
	virtual int				Damage_GetTimeBased( void );
	
	virtual bool			ShouldCollide( int collisionGroup0, int collisionGroup1 );
	virtual bool			ShouldUseRobustRadiusDamage(CBaseEntity *pEntity);
#ifndef CLIENT_DLL
	virtual float			GetAutoAimScale( CBasePlayer *pPlayer );
	virtual float			GetAmmoQuantityScale( int iAmmoIndex );
#endif

#ifdef CLIENT_DLL

	DECLARE_CLIENTCLASS_NOBASE(); // This makes datatables able to access our private vars.

#else

	DECLARE_SERVERCLASS_NOBASE(); // This makes datatables able to access our private vars.

	CBBRules();
	virtual ~CBBRules() {}

	virtual void			Think( void );

	virtual bool			ClientCommand( CBaseEntity *pEdict, const CCommand &args );
	virtual void			PlayerSpawn( CBasePlayer *pPlayer );

	virtual void			InitDefaultAIRelationships( void );
	virtual const char*		AIClassText(int classType);
	virtual const char *GetGameDescription( void ) { return "Half-Life 2"; }

	// Ammo
	virtual void			PlayerThink( CBasePlayer *pPlayer );
	virtual float			GetAmmoDamage( CBaseEntity *pAttacker, CBaseEntity *pVictim, int nAmmoType );

	virtual bool			ShouldBurningPropsEmitLight();
public:
	bool AllowDamage( CBaseEntity *pVictim, const CTakeDamageInfo &info );

private:

	float	m_flLastHealthDropTime;
	float	m_flLastGrenadeDropTime;

	void AdjustPlayerDamageTaken( CTakeDamageInfo *pInfo );
	float AdjustPlayerDamageInflicted( float damage );

	int	DefaultFOV( void ) { return 75; }
#endif
};


//-----------------------------------------------------------------------------
// Gets us at the Half-Life 2 game rules
//-----------------------------------------------------------------------------
inline CBBRules* BBGameRules()
{
#if ( !defined( HL2_DLL ) && !defined( HL2_CLIENT_DLL ) ) || defined( HL2MP )
	Assert( 0 );	// g_pGameRules is NOT an instance of CBBRules and bad things happen
#endif
	return static_cast<CBBRules*>(g_pGameRules);
}
#endif
