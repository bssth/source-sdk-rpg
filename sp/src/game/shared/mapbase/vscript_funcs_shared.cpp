//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Due to this being a custom integration of VScript based on the Alien Swarm SDK, we don't have access to
//			some of the code normally available in games like L4D2 or Valve's original VScript DLL.
//			Instead, that code is recreated here, shared between server and client.
//
//			It also contains other functions unique to Mapbase.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "matchers.h"
#include "takedamageinfo.h"
#include "vscript_singletons.h"

#ifndef CLIENT_DLL
#include "globalstate.h"
#include "vscript_server.h"
#include "enginecallback.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// NOTE (port): CScriptNetPropManager moved to vscript_singletons.cpp, and
// CScriptConvarLookup ("CConvars"/"Convars") moved into the VScript library itself
// (vscript_bindings_base.cpp, registered by RegisterBaseBindings) following Mapbase
// eb014cce (v6.0). Our prebuilt vscript.lib already provides and auto-registers it.

#ifndef CLIENT_DLL
void AddThinkToEnt( HSCRIPT entity, const char *pszFuncName )
{
	CBaseEntity *pEntity = ToEnt( entity );
	if (!pEntity)
		return;

	if (pszFuncName == NULL || pszFuncName[0] == '\0')
		pEntity->m_iszScriptThinkFunction = NULL_STRING;
	else
		pEntity->m_iszScriptThinkFunction = AllocPooledString(pszFuncName);

	pEntity->SetContextThink( &CBaseEntity::ScriptThink, gpGlobals->curtime, "ScriptThink" );
}

HSCRIPT EntIndexToHScript( int index )
{
	return ToHScript( UTIL_EntityByIndex( index ) );
}

void ParseScriptTableKeyValues( CBaseEntity *pEntity, HSCRIPT hKV )
{
	int nIterator = -1;
	ScriptVariant_t varKey, varValue;
	while ((nIterator = g_pScriptVM->GetKeyValue( hKV, nIterator, &varKey, &varValue )) != -1)
	{
		switch (varValue.m_type)
		{
			case FIELD_CSTRING:		pEntity->KeyValue( varKey.m_pszString, varValue.m_pszString ); break;
			case FIELD_FLOAT:		pEntity->KeyValue( varKey.m_pszString, varValue.m_float ); break;
			case FIELD_VECTOR:		pEntity->KeyValue( varKey.m_pszString, *varValue.m_pVector ); break;
		}

		g_pScriptVM->ReleaseValue( varKey );
		g_pScriptVM->ReleaseValue( varValue );
	}
}

void PrecacheEntityFromTable( const char *pszClassname, HSCRIPT hKV )
{
	if ( IsEntityCreationAllowedInScripts() == false )
	{
		Warning( "VScript error: A script attempted to create an entity mid-game. Due to the server's settings, entity creation from scripts is only allowed during map init.\n" );
		return;
	}

	// This is similar to UTIL_PrecacheOther(), but we can't check if we can only precache it once.
	// Probably for the best anyway, as similar classes can still have different precachable properties.
	CBaseEntity *pEntity = CreateEntityByName( pszClassname );
	if (!pEntity)
	{
		Assert( !"PrecacheEntityFromTable: only works for CBaseEntities" );
		return;
	}

	ParseScriptTableKeyValues( pEntity, hKV );

	pEntity->Precache();

	UTIL_RemoveImmediate( pEntity );
}

HSCRIPT SpawnEntityFromTable( const char *pszClassname, HSCRIPT hKV )
{
	if ( IsEntityCreationAllowedInScripts() == false )
	{
		Warning( "VScript error: A script attempted to create an entity mid-game. Due to the server's settings, entity creation from scripts is only allowed during map init.\n" );
		return NULL;
	}

	CBaseEntity *pEntity = CreateEntityByName( pszClassname );
	if ( !pEntity )
	{
		Assert( !"SpawnEntityFromTable: only works for CBaseEntities" );
		return NULL;
	}

	gEntList.NotifyCreateEntity( pEntity );

	ParseScriptTableKeyValues( pEntity, hKV );

	DispatchSpawn( pEntity );
	pEntity->Activate();

	return ToHScript( pEntity );
}
#endif

//-----------------------------------------------------------------------------
// Mapbase-specific functions start here
//-----------------------------------------------------------------------------

static void ScriptColorPrint( int r, int g, int b, const char *pszMsg )
{
	const Color clr(r, g, b, 255);
	ConColorMsg( clr, "%s", pszMsg );
}

static void ScriptColorPrintL( int r, int g, int b, const char *pszMsg )
{
	const Color clr(r, g, b, 255);
	ConColorMsg( clr, "%s\n", pszMsg );
}

#ifndef CLIENT_DLL
// Since IScriptVM::GetKeyValue() is unsupported, Mapbase uses this CScriptKeyValues version.
// NOTE (port): the final vscript.lib owns CScriptKeyValues; obtain its KeyValues via the
// script manager instead of the game-side wrapper that earlier Mapbase commits used.
HSCRIPT SpawnEntityFromKeyValues( const char *pszClassname, HSCRIPT hKV )
{
	if ( IsEntityCreationAllowedInScripts() == false )
	{
		Warning( "VScript error: A script attempted to create an entity mid-game. Due to the server's settings, entity creation from scripts is only allowed during map init.\n" );
		return NULL;
	}

	CBaseEntity *pEntity = CreateEntityByName( pszClassname );
	if ( !pEntity )
	{
		Assert( !"SpawnEntityFromKeyValues: only works for CBaseEntities" );
		return NULL;
	}

	gEntList.NotifyCreateEntity( pEntity );

	KeyValues *pKV = scriptmanager->GetKeyValuesFromScriptKV( g_pScriptVM, hKV );
	if (pKV)
	{
		for (pKV = pKV->GetFirstSubKey(); pKV != NULL; pKV = pKV->GetNextKey())
		{
			pEntity->KeyValue( pKV->GetName(), pKV->GetString() );
		}
	}

	DispatchSpawn( pEntity );
	pEntity->Activate();

	return ToHScript( pEntity );
}

void ScriptDispatchSpawn( HSCRIPT hEntity )
{
	CBaseEntity *pEntity = ToEnt( hEntity );
	if (pEntity)
	{
		DispatchSpawn( pEntity );
	}
}
#endif

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static HSCRIPT CreateDamageInfo( HSCRIPT hInflictor, HSCRIPT hAttacker, const Vector &vecForce, const Vector &vecDamagePos, float flDamage, int iDamageType )
{
	// The script is responsible for deleting this via DestroyDamageInfo().
	CTakeDamageInfo *damageInfo = new CTakeDamageInfo(ToEnt(hInflictor), ToEnt(hAttacker), flDamage, iDamageType);
	HSCRIPT hScript = g_pScriptVM->RegisterInstance( damageInfo );

	damageInfo->SetDamagePosition( vecDamagePos );
	damageInfo->SetDamageForce( vecForce );

	return hScript;
}

static void DestroyDamageInfo( HSCRIPT hDamageInfo )
{
	if (hDamageInfo)
	{
		CTakeDamageInfo *pInfo = (CTakeDamageInfo*)g_pScriptVM->GetInstanceValue( hDamageInfo, GetScriptDescForClass( CTakeDamageInfo ) );
		if (pInfo)
		{
			g_pScriptVM->RemoveInstance( hDamageInfo );
			delete pInfo;
		}
	}
}

void ScriptCalculateExplosiveDamageForce( HSCRIPT info, const Vector &vecDir, const Vector &vecForceOrigin, float flScale ) { CalculateExplosiveDamageForce( HScriptToClass<CTakeDamageInfo>(info), vecDir, vecForceOrigin, flScale ); }
void ScriptCalculateBulletDamageForce( HSCRIPT info, int iBulletType, const Vector &vecBulletDir, const Vector &vecForceOrigin, float flScale ) { CalculateBulletDamageForce( HScriptToClass<CTakeDamageInfo>(info), iBulletType, vecBulletDir, vecForceOrigin, flScale ); }
void ScriptCalculateMeleeDamageForce( HSCRIPT info, const Vector &vecMeleeDir, const Vector &vecForceOrigin, float flScale ) { CalculateMeleeDamageForce( HScriptToClass<CTakeDamageInfo>( info ), vecMeleeDir, vecForceOrigin, flScale ); }
void ScriptGuessDamageForce( HSCRIPT info, const Vector &vecForceDir, const Vector &vecForceOrigin, float flScale ) { GuessDamageForce( HScriptToClass<CTakeDamageInfo>( info ), vecForceDir, vecForceOrigin, flScale ); }

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CTraceInfoAccessor
{
public:

	// CGrameTrace stuff
	bool DidHitWorld() const { return m_tr.DidHitWorld(); }
	bool DidHitNonWorldEntity() const { return m_tr.DidHitNonWorldEntity(); }
	int GetEntityIndex() const { return m_tr.GetEntityIndex(); }
	bool DidHit() const { return m_tr.DidHit(); }

	float FractionLeftSolid() const { return m_tr.fractionleftsolid; }
	int HitGroup() const { return m_tr.hitgroup; }
	int PhysicsBone() const { return m_tr.physicsbone; }

	HSCRIPT Entity() const { return ToHScript(m_tr.m_pEnt); }

	int HitBox() const { return m_tr.hitbox; }

	// CBaseTrace stuff
	bool IsDispSurface() { return m_tr.IsDispSurface(); }
	bool IsDispSurfaceWalkable() { return m_tr.IsDispSurfaceWalkable(); }
	bool IsDispSurfaceBuildable() { return m_tr.IsDispSurfaceBuildable(); }
	bool IsDispSurfaceProp1() { return m_tr.IsDispSurfaceProp1(); }
	bool IsDispSurfaceProp2() { return m_tr.IsDispSurfaceProp2(); }

	Vector StartPos() const { return m_tr.startpos; }
	Vector EndPos() const { return m_tr.endpos; }

	float Fraction() const { return m_tr.fraction; }

	int Contents() const { return m_tr.contents; }
	int DispFlags() const { return m_tr.dispFlags; }

	bool AllSolid() const { return m_tr.allsolid; }
	bool StartSolid() const { return m_tr.startsolid; }

	trace_t &GetTrace() { return m_tr; }
	void Destroy() { delete this; }

private:
	trace_t m_tr;

};

BEGIN_SCRIPTDESC_ROOT_NAMED( CTraceInfoAccessor, "CGameTrace", "Handle for accessing trace_t info." )
	DEFINE_SCRIPTFUNC( DidHitWorld, "Returns whether the trace hit the world entity or not." )
	DEFINE_SCRIPTFUNC( DidHitNonWorldEntity, "Returns whether the trace hit something other than the world entity." )
	DEFINE_SCRIPTFUNC( GetEntityIndex, "Returns the index of whatever entity this trace hit." )
	DEFINE_SCRIPTFUNC( DidHit, "Returns whether the trace hit anything." )

	DEFINE_SCRIPTFUNC( FractionLeftSolid, "If this trace started within a solid, this is the point in the trace's fraction at which it left that solid." )
	DEFINE_SCRIPTFUNC( HitGroup, "Returns the specific hit group this trace hit if it hit an entity." )
	DEFINE_SCRIPTFUNC( PhysicsBone, "Returns the physics bone this trace hit if it hit an entity." )
	DEFINE_SCRIPTFUNC( Entity, "Returns the entity this trace has hit." )
	DEFINE_SCRIPTFUNC( HitBox, "Returns the hitbox of the entity this trace has hit. If it hit the world entity, this returns the static prop index." )

	DEFINE_SCRIPTFUNC( IsDispSurface, "Returns whether this trace hit a displacement." )
	DEFINE_SCRIPTFUNC( IsDispSurfaceWalkable, "Returns whether DISPSURF_FLAG_WALKABLE is ticked on the displacement this trace hit." )
	DEFINE_SCRIPTFUNC( IsDispSurfaceBuildable, "Returns whether DISPSURF_FLAG_BUILDABLE is ticked on the displacement this trace hit." )
	DEFINE_SCRIPTFUNC( IsDispSurfaceProp1, "Returns whether DISPSURF_FLAG_SURFPROP1 is ticked on the displacement this trace hit." )
	DEFINE_SCRIPTFUNC( IsDispSurfaceProp2, "Returns whether DISPSURF_FLAG_SURFPROP2 is ticked on the displacement this trace hit." )

	DEFINE_SCRIPTFUNC( StartPos, "Gets the trace's start position." )
	DEFINE_SCRIPTFUNC( EndPos, "Gets the trace's end position." )

	DEFINE_SCRIPTFUNC( Fraction, "Gets the fraction of the trace completed. For example, if the trace stopped exactly halfway to the end position, this would be 0.5." )
	DEFINE_SCRIPTFUNC( Contents, "Gets the contents of the surface the trace has hit." )
	DEFINE_SCRIPTFUNC( DispFlags, "Gets the displacement flags of the surface the trace has hit." )

	DEFINE_SCRIPTFUNC( AllSolid, "Returns whether the trace is completely within a solid." )
	DEFINE_SCRIPTFUNC( StartSolid, "Returns whether the trace started within a solid." )

	DEFINE_SCRIPTFUNC( Destroy, "Deletes this instance. Important for preventing memory leaks." )
END_SCRIPTDESC();

static HSCRIPT ScriptTraceLineComplex( const Vector &vecStart, const Vector &vecEnd, HSCRIPT entIgnore, int iMask, int iCollisionGroup )
{
	// The script is responsible for deleting this via Destroy().
	CTraceInfoAccessor *traceInfo = new CTraceInfoAccessor();
	HSCRIPT hScript = g_pScriptVM->RegisterInstance( traceInfo );

	CBaseEntity *pLooker = ToEnt(entIgnore);
	UTIL_TraceLine( vecStart, vecEnd, iMask, pLooker, iCollisionGroup, &traceInfo->GetTrace());
	return hScript;
}

static HSCRIPT ScriptTraceHullComplex( const Vector &vecStart, const Vector &vecEnd, const Vector &hullMin, const Vector &hullMax,
	HSCRIPT entIgnore, int iMask, int iCollisionGroup )
{
	// The script is responsible for deleting this via Destroy().
	CTraceInfoAccessor *traceInfo = new CTraceInfoAccessor();
	HSCRIPT hScript = g_pScriptVM->RegisterInstance( traceInfo );

	CBaseEntity *pLooker = ToEnt(entIgnore);
	UTIL_TraceHull( vecStart, vecEnd, hullMin, hullMax, iMask, pLooker, iCollisionGroup, &traceInfo->GetTrace());
	return hScript;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
BEGIN_SCRIPTDESC_ROOT_NAMED( CFireBulletsInfoAccessor, "FireBulletsInfo_t", "Handle for accessing FireBulletsInfo_t info." )
	DEFINE_SCRIPTFUNC( GetShots, "Gets the number of shots which should be fired." )
	DEFINE_SCRIPTFUNC( SetShots, "Sets the number of shots which should be fired." )

	DEFINE_SCRIPTFUNC( GetSource, "Gets the source of the bullets." )
	DEFINE_SCRIPTFUNC( SetSource, "Sets the source of the bullets." )
	DEFINE_SCRIPTFUNC( GetDirShooting, "Gets the direction of the bullets." )
	DEFINE_SCRIPTFUNC( SetDirShooting, "Sets the direction of the bullets." )
	DEFINE_SCRIPTFUNC( GetSpread, "Gets the spread of the bullets." )
	DEFINE_SCRIPTFUNC( SetSpread, "Sets the spread of the bullets." )

	DEFINE_SCRIPTFUNC( GetDistance, "Gets the distance the bullets should travel." )
	DEFINE_SCRIPTFUNC( SetDistance, "Sets the distance the bullets should travel." )

	DEFINE_SCRIPTFUNC( GetAmmoType, "Gets the ammo type the bullets should use." )
	DEFINE_SCRIPTFUNC( SetAmmoType, "Sets the ammo type the bullets should use." )

	DEFINE_SCRIPTFUNC( GetTracerFreq, "Gets the tracer frequency." )
	DEFINE_SCRIPTFUNC( SetTracerFreq, "Sets the tracer frequency." )

	DEFINE_SCRIPTFUNC( GetDamage, "Gets the damage the bullets should deal. 0 = use ammo type" )
	DEFINE_SCRIPTFUNC( SetDamage, "Sets the damage the bullets should deal. 0 = use ammo type" )
	DEFINE_SCRIPTFUNC( GetPlayerDamage, "Gets the damage the bullets should deal when hitting the player. 0 = use regular damage" )
	DEFINE_SCRIPTFUNC( SetPlayerDamage, "Sets the damage the bullets should deal when hitting the player. 0 = use regular damage" )

	DEFINE_SCRIPTFUNC( GetFlags, "Gets the flags the bullets should use." )
	DEFINE_SCRIPTFUNC( SetFlags, "Sets the flags the bullets should use." )

	DEFINE_SCRIPTFUNC( GetDamageForceScale, "Gets the scale of the damage force applied by the bullets." )
	DEFINE_SCRIPTFUNC( SetDamageForceScale, "Sets the scale of the damage force applied by the bullets." )

	DEFINE_SCRIPTFUNC( GetAttacker, "Gets the entity considered to be the one who fired the bullets." )
	DEFINE_SCRIPTFUNC( SetAttacker, "Sets the entity considered to be the one who fired the bullets." )
	DEFINE_SCRIPTFUNC( GetAdditionalIgnoreEnt, "Gets the optional entity which the bullets should ignore." )
	DEFINE_SCRIPTFUNC( SetAdditionalIgnoreEnt, "Sets the optional entity which the bullets should ignore." )

	DEFINE_SCRIPTFUNC( GetPrimaryAttack, "Gets whether the bullets came from a primary attack." )
	DEFINE_SCRIPTFUNC( SetPrimaryAttack, "Sets whether the bullets came from a primary attack." )

	//DEFINE_SCRIPTFUNC( Destroy, "Deletes this instance. Important for preventing memory leaks." )
END_SCRIPTDESC();

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
HSCRIPT CFireBulletsInfoAccessor::GetAttacker()
{
	return ToHScript( m_info->m_pAttacker );
}

void CFireBulletsInfoAccessor::SetAttacker( HSCRIPT value )
{
	m_info->m_pAttacker = ToEnt( value );
}

HSCRIPT CFireBulletsInfoAccessor::GetAdditionalIgnoreEnt()
{
	return ToHScript( m_info->m_pAdditionalIgnoreEnt );
}

void CFireBulletsInfoAccessor::SetAdditionalIgnoreEnt( HSCRIPT value )
{
	m_info->m_pAdditionalIgnoreEnt = ToEnt( value );
}

static HSCRIPT CreateFireBulletsInfo( int cShots, const Vector &vecSrc, const Vector &vecDirShooting,
	const Vector &vecSpread, float iDamage, HSCRIPT pAttacker )
{
	// The script is responsible for deleting this via DestroyFireBulletsInfo().
	FireBulletsInfo_t *info = new FireBulletsInfo_t();
	CFireBulletsInfoAccessor *bulletsInfo = new CFireBulletsInfoAccessor( info );

	HSCRIPT hScript = g_pScriptVM->RegisterInstance( bulletsInfo );

	bulletsInfo->SetShots( cShots );
	bulletsInfo->SetSource( vecSrc );
	bulletsInfo->SetDirShooting( vecDirShooting );
	bulletsInfo->SetSpread( vecSpread );
	bulletsInfo->SetDamage( iDamage );
	bulletsInfo->SetAttacker( pAttacker );

	return hScript;
}

static void DestroyFireBulletsInfo( HSCRIPT hBulletsInfo )
{
	if (hBulletsInfo)
	{
		CFireBulletsInfoAccessor *pInfo = (CFireBulletsInfoAccessor*)g_pScriptVM->GetInstanceValue( hBulletsInfo, GetScriptDescForClass( CFireBulletsInfoAccessor ) );
		if (pInfo)
		{
			g_pScriptVM->RemoveInstance( hBulletsInfo );
			pInfo->Destroy();
		}
	}
}

// For the function in baseentity.cpp
FireBulletsInfo_t *GetFireBulletsInfoFromInfo( HSCRIPT hBulletsInfo )
{
	if (hBulletsInfo)
	{
		CFireBulletsInfoAccessor *pInfo = (CFireBulletsInfoAccessor*)g_pScriptVM->GetInstanceValue( hBulletsInfo, GetScriptDescForClass( CFireBulletsInfoAccessor ) );
		if (pInfo)
		{
			return pInfo->GetInfo();
		}
	}

	return NULL;
}

//=============================================================================
//=============================================================================

#ifndef CLIENT_DLL
// NOTE (port): Mapbase 66a44d07 put this in vscript_funcs_math.cpp. Everything else in that
// file (matrix3x4_t, the math wrappers, RandomFloat/RandomInt/AngleDiff) moved into the
// VScript library in eb014cce (v6.0) and our prebuilt vscript.lib already registers it,
// so only this entity-dependent helper stays game-side (where v6.0 puts it too).
const Vector& ScriptPredictedPosition( HSCRIPT hTarget, float flTimeDelta )
{
	static Vector predicted;
	UTIL_PredictedPosition( ToEnt(hTarget), flTimeDelta, &predicted );
	return predicted;
}
#endif

//=============================================================================
//=============================================================================

bool ScriptMatcherMatch( const char *pszQuery, const char *szValue ) { return Matcher_Match( pszQuery, szValue ); }

void RegisterSharedScriptFunctions()
{
	//
	// Due to this being a custom integration of VScript based on the Alien Swarm SDK, we don't have access to
	// some of the code normally available in games like L4D2 or Valve's original VScript DLL.
	// Instead, that code is recreated here, shared between server and client.
	//

#ifndef CLIENT_DLL
	ScriptRegisterFunctionNamed( g_pScriptVM, NDebugOverlay::BoxDirection, "DebugDrawBoxDirection", "Draw a debug forward box" );
	ScriptRegisterFunctionNamed( g_pScriptVM, NDebugOverlay::Text, "DebugDrawText", "Draw a debug overlay text" );

	ScriptRegisterFunction( g_pScriptVM, AddThinkToEnt, "This will put a think function onto an entity, or pass null to remove it. This is NOT chained, so be careful." );
	ScriptRegisterFunction( g_pScriptVM, EntIndexToHScript, "Returns the script handle for the given entity index." );
	ScriptRegisterFunction( g_pScriptVM, PrecacheEntityFromTable, "Precache an entity from KeyValues in a table." );
	ScriptRegisterFunction( g_pScriptVM, SpawnEntityFromTable, "Native function for entity spawning." );
#endif

	// Functions unique to Mapbase
	ScriptRegisterFunctionNamed( g_pScriptVM, ScriptColorPrint, "printc", "Version of print() which takes a color before the message." );
	ScriptRegisterFunctionNamed( g_pScriptVM, ScriptColorPrintL, "printcl", "Version of printl() which takes a color before the message." );

#ifndef CLIENT_DLL
	ScriptRegisterFunction( g_pScriptVM, SpawnEntityFromKeyValues, "Spawns an entity with the keyvalues in a CScriptKeyValues handle." );
	ScriptRegisterFunctionNamed( g_pScriptVM, ScriptDispatchSpawn, "DispatchSpawn", "Spawns an unspawned entity." );
#endif

	ScriptRegisterFunction( g_pScriptVM, CreateDamageInfo, "Creates damage info." );
	ScriptRegisterFunction( g_pScriptVM, DestroyDamageInfo, "Destroys damage info." );
	ScriptRegisterFunction( g_pScriptVM, ImpulseScale, "Returns an impulse scale required to push an object." );
	ScriptRegisterFunctionNamed( g_pScriptVM, ScriptCalculateExplosiveDamageForce, "CalculateExplosiveDamageForce", "Fill out a damage info handle with a damage force for an explosive." );
	ScriptRegisterFunctionNamed( g_pScriptVM, ScriptCalculateBulletDamageForce, "CalculateBulletDamageForce", "Fill out a damage info handle with a damage force for a bullet impact." );
	ScriptRegisterFunctionNamed( g_pScriptVM, ScriptCalculateMeleeDamageForce, "CalculateMeleeDamageForce", "Fill out a damage info handle with a damage force for a melee impact." );
	ScriptRegisterFunctionNamed( g_pScriptVM, ScriptGuessDamageForce, "GuessDamageForce", "Try and guess the physics force to use." );

	ScriptRegisterFunction( g_pScriptVM, CreateFireBulletsInfo, "Creates FireBullets info." );
	ScriptRegisterFunction( g_pScriptVM, DestroyFireBulletsInfo, "Destroys FireBullets info." );

	ScriptRegisterFunctionNamed( g_pScriptVM, ScriptTraceLineComplex, "TraceLineComplex", "Complex version of TraceLine which takes 2 points, an ent to ignore, a trace mask, and a collision group. Returns a handle which can access all trace info." );
	ScriptRegisterFunctionNamed( g_pScriptVM, ScriptTraceHullComplex, "TraceHullComplex", "Takes 2 points, min/max hull bounds, an ent to ignore, a trace mask, and a collision group to trace to a point using a hull. Returns a handle which can access all trace info." );

	ScriptRegisterFunctionNamed( g_pScriptVM, ScriptMatcherMatch, "Matcher_Match", "Compares a string to a query using Mapbase's matcher system, supporting wildcards, RS matchers, etc." );
	ScriptRegisterFunction( g_pScriptVM, Matcher_NamesMatch, "Compares a string to a query using Mapbase's matcher system using wildcards only." );
	ScriptRegisterFunction( g_pScriptVM, AppearsToBeANumber, "Checks if the given string appears to be a number." );

#ifndef CLIENT_DLL
	ScriptRegisterFunctionNamed( g_pScriptVM, ScriptPredictedPosition, "PredictedPosition", "Predicts what an entity's position will be in a given amount of time." );
#endif

	RegisterScriptSingletons();

#ifdef CLIENT_DLL
	VScriptRunScript( "vscript_client", true );
#else
	VScriptRunScript( "vscript_server", true );
#endif
}
