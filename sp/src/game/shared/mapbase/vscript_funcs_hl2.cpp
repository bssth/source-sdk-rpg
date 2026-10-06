//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: VScript functions for Half-Life 2.
//
//			NOTE (port): the canon Mapbase file targets CHalfLife2 / HL2GameRules(). This fork's
//			game rules class is CBBRules (CSingleplayRules), so the override lives there instead.
//			MegaPhyscannonActive() does not exist on CBBRules, so that binding is omitted.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#include "bb_gamerules.h"
#ifndef CLIENT_DLL
#include "eventqueue.h"
#include "weapon_physcannon.h"
#include "player_pickup.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#ifndef CLIENT_DLL
extern CBaseEntity *CreatePlayerLoadSave( Vector vOrigin, float flDuration, float flHoldTime, float flLoadTime );

HSCRIPT ScriptGameOver( const char *pszMessage, float flDelay, float flFadeTime, float flLoadTime, int r, int g, int b )
{
	CBaseEntity *pPlayer = AI_GetSinglePlayer();
	if (pPlayer)
	{
		UTIL_ShowMessage( pszMessage, ToBasePlayer( pPlayer ) );
		ToBasePlayer( pPlayer )->NotifySinglePlayerGameEnding();
	}
	else
	{
		// TODO: How should MP handle this?
		return NULL;
	}

	CBaseEntity *pReload = CreatePlayerLoadSave( vec3_origin, flFadeTime, flLoadTime + 1.0f, flLoadTime );
	if (pReload)
	{
		pReload->SetRenderColor( r, g, b, 255 );
		g_EventQueue.AddEvent( pReload, "Reload", flDelay, pReload, pReload );
	}

	return ToHScript( pReload );
}

void ScriptPickup_ForcePlayerToDropThisObject( HSCRIPT hTarget )
{
	Pickup_ForcePlayerToDropThisObject( ToEnt( hTarget ) );
}

float ScriptPlayerPickupGetHeldObjectMass( HSCRIPT hPickupControllerEntity, HSCRIPT hHeldObject )
{
	IPhysicsObject *pPhysObj = HScriptToClass<IPhysicsObject>( hHeldObject );
	if (!pPhysObj)
	{
		CBaseEntity *pEnt = ToEnt( hHeldObject );
		if (pEnt)
			pPhysObj = pEnt->VPhysicsGetObject();
	}

	if (!pPhysObj)
	{
		Warning( "PlayerPickupGetHeldObjectMass: Invalid physics object\n" );
		return 0.0f;
	}

	return PlayerPickupGetHeldObjectMass( ToEnt( hPickupControllerEntity ), pPhysObj );
}

HSCRIPT ScriptGetPlayerHeldEntity( HSCRIPT hPlayer )
{
	CBasePlayer *pPlayer = ToBasePlayer( ToEnt( hPlayer ) );
	if (!pPlayer)
		return NULL;

	return ToHScript( GetPlayerHeldEntity( pPlayer ) );
}

HSCRIPT ScriptPhysCannonGetHeldEntity( HSCRIPT hWeapon )
{
	CBaseEntity *pEnt = ToEnt( hWeapon );
	if (!pEnt)
		return NULL;

	return ToHScript( PhysCannonGetHeldEntity( pEnt->MyCombatWeaponPointer() ) );
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Registers the game-rules-specific VScript functions for this mod.
//-----------------------------------------------------------------------------
void CBBRules::RegisterScriptFunctions( void )
{
	BaseClass::RegisterScriptFunctions();

#ifndef CLIENT_DLL
	ScriptRegisterFunctionNamed( g_pScriptVM, ScriptGameOver, "GameOver", "Ends the game and reloads the last save." );

	ScriptRegisterFunctionNamed( g_pScriptVM, ScriptPickup_ForcePlayerToDropThisObject, "Pickup_ForcePlayerToDropThisObject", "If the specified entity is being carried, instantly drops it." );
	ScriptRegisterFunctionNamed( g_pScriptVM, ScriptPlayerPickupGetHeldObjectMass, "PlayerPickupGetHeldObjectMass", "Gets the mass of the specified player_pickup controller, with the second parameter the held object's physics object." );
	ScriptRegisterFunctionNamed( g_pScriptVM, ScriptGetPlayerHeldEntity, "GetPlayerHeldEntity", "Gets the specified player's currently held entity." );
	ScriptRegisterFunctionNamed( g_pScriptVM, ScriptPhysCannonGetHeldEntity, "PhysCannonGetHeldEntity", "Gets the specified gravity gun's currently held entity." );
#endif
}
