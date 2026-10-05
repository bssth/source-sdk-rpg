//========= VScript port (Mapbase commit d840c57b4) =========================//
//
// Purpose: Out-of-line CBaseEntity VScript surface (SCRIPTDESC + script method
//			bodies + FindNamedOutput), extracted from baseentity.cpp so they don't
//			bloat the giant translation unit. CBaseEntity:: members can be defined
//			in any translation unit.
//
//===========================================================================//

#include "cbase.h"
#include "vscript_server.h"
#include "eventqueue.h"
#include "tier1/fmtstr.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar sv_script_think_interval("sv_script_think_interval", "0.1");

// #define VMPROFILE	// define to profile vscript calls

#ifdef VMPROFILE
float g_debugCumulativeTime = 0.0;
float g_debugCounter = 0;

#define START_VMPROFILE float debugStartTime = Plat_FloatTime();
#define UPDATE_VMPROFILE \
	g_debugCumulativeTime += Plat_FloatTime() - debugStartTime; \
	g_debugCounter++; \
	if ( g_debugCounter >= 500 ) \
	{ \
		DevMsg("***VSCRIPT PROFILE***: %s %s: %6.4f milliseconds\n", "500 vscript function calls", "", g_debugCumulativeTime*1000.0 ); \
		g_debugCounter = 0; \
		g_debugCumulativeTime = 0.0; \
	} \

#else

#define START_VMPROFILE
#define UPDATE_VMPROFILE

#endif // VMPROFILE


#ifdef MAPBASE_VSCRIPT
ScriptHook_t	CBaseEntity::g_Hook_UpdateOnRemove;
ScriptHook_t	CBaseEntity::g_Hook_VPhysicsCollision;
ScriptHook_t	CBaseEntity::g_Hook_FireBullets;
ScriptHook_t	CBaseEntity::g_Hook_OnDeath;
#endif

BEGIN_ENT_SCRIPTDESC_ROOT_WITH_HELPER( CBaseEntity, "Root class of all server-side entities", &g_BaseEntityScriptInstanceHelper )
	DEFINE_SCRIPTFUNC_NAMED( ConnectOutputToScript, "ConnectOutput", "Adds an I/O connection that will call the named function when the specified output fires"  )
	DEFINE_SCRIPTFUNC_NAMED( DisconnectOutputFromScript, "DisconnectOutput", "Removes a connected script function from an I/O event."  )

	DEFINE_SCRIPTFUNC( GetHealth, "" )
	DEFINE_SCRIPTFUNC( SetHealth, "" )
	DEFINE_SCRIPTFUNC( GetMaxHealth, "" )
	DEFINE_SCRIPTFUNC( SetMaxHealth, "" )

	DEFINE_SCRIPTFUNC( SetModel, "" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetModelName, "GetModelName", "Returns the name of the model" )

	DEFINE_SCRIPTFUNC_NAMED( ScriptEmitSound, "EmitSound", "Plays a sound from this entity." )
	DEFINE_SCRIPTFUNC_NAMED( VScriptPrecacheScriptSound, "PrecacheSoundScript", "Precache a sound for later playing." )
	DEFINE_SCRIPTFUNC_NAMED( ScriptSoundDuration, "GetSoundDuration", "Returns float duration of the sound. Takes soundname and optional actormodelname.")


	DEFINE_SCRIPTFUNC( GetClassname, "" )
	DEFINE_SCRIPTFUNC_NAMED( GetEntityNameAsCStr, "GetName", "" )
#ifdef MAPBASE_VSCRIPT
	DEFINE_SCRIPTFUNC( GetDebugName, "If name exists returns name, otherwise returns classname" )
	DEFINE_SCRIPTFUNC_NAMED( SetNameAsCStr, "SetName", "" )
#endif
	DEFINE_SCRIPTFUNC( GetPreTemplateName, "Get the entity name stripped of template unique decoration" )

	DEFINE_SCRIPTFUNC_NAMED( GetAbsOrigin, "GetOrigin", ""  )
	DEFINE_SCRIPTFUNC( SetAbsOrigin, "SetAbsOrigin" )
#ifdef MAPBASE_VSCRIPT
	DEFINE_SCRIPTFUNC( SetAbsAngles, "SetAbsAngles" )

	DEFINE_SCRIPTFUNC( GetLocalOrigin, "GetLocalOrigin" )
	DEFINE_SCRIPTFUNC( SetLocalOrigin, "SetLocalOrigin" )
	DEFINE_SCRIPTFUNC( GetLocalAngles, "GetLocalAngles" )
	DEFINE_SCRIPTFUNC( SetLocalAngles, "SetLocalAngles" )
#endif

	DEFINE_SCRIPTFUNC_NAMED( ScriptSetOrigin, "SetOrigin", ""  )
	DEFINE_SCRIPTFUNC_NAMED( ScriptSetForward, "SetForwardVector", "Set the orientation of the entity to have this forward vector" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetForward, "GetForwardVector", "Get the forward vector of the entity"  )
#ifdef MAPBASE_VSCRIPT
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetRight, "GetRightVector", "Get the right vector of the entity" )
#endif
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetLeft, "GetLeftVector", SCRIPT_HIDE )

	DEFINE_SCRIPTFUNC_NAMED( ScriptGetUp, "GetUpVector", "Get the up vector of the entity"  )

#ifdef MAPBASE_VSCRIPT
	DEFINE_SCRIPTFUNC_NAMED( ScriptSetOriginAngles, "SetOriginAngles", "Set both the origin and the angles" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptSetOriginAnglesVelocity, "SetOriginAnglesVelocity", "Set the origin, the angles, and the velocity" )

	DEFINE_SCRIPTFUNC_NAMED( ScriptEntityToWorldTransform, "EntityToWorldTransform", "Get the entity's transform" )

	DEFINE_SCRIPTFUNC_NAMED( ScriptGetPhysicsObject, "GetPhysicsObject", "Get the entity's physics object if it has one" )

	DEFINE_SCRIPTFUNC( ApplyAbsVelocityImpulse, "" )
	DEFINE_SCRIPTFUNC( ApplyLocalAngularVelocityImpulse, "" )
#endif

	DEFINE_SCRIPTFUNC_NAMED( GetAbsVelocity, "GetVelocity", ""  )
	DEFINE_SCRIPTFUNC_NAMED( SetAbsVelocity, "SetVelocity", ""  )

	DEFINE_SCRIPTFUNC_NAMED( ScriptSetLocalAngularVelocity, "SetAngularVelocity", "Set the local angular velocity - takes float pitch,yaw,roll velocities"  )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetLocalAngularVelocity, "GetAngularVelocity", "Get the local angular velocity - returns a vector of pitch,yaw,roll"  )

	DEFINE_SCRIPTFUNC_NAMED( WorldSpaceCenter, "GetCenter", "Get vector to center of object - absolute coords")
	DEFINE_SCRIPTFUNC_NAMED( ScriptEyePosition, "EyePosition", "Get vector to eye position - absolute coords")
#ifdef MAPBASE_VSCRIPT
	DEFINE_SCRIPTFUNC_NAMED( ScriptEyeAngles, "EyeAngles", "Get eye pitch, yaw, roll as a vector" )
#endif
	DEFINE_SCRIPTFUNC_NAMED( ScriptSetAngles, "SetAngles", "Set entity pitch, yaw, roll")
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetAngles, "GetAngles", "Get entity pitch, yaw, roll as a vector")

	DEFINE_SCRIPTFUNC_NAMED( ScriptSetSize, "SetSize", ""  )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetBoundingMins, "GetBoundingMins", "Get a vector containing min bounds, centered on object")
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetBoundingMaxs, "GetBoundingMaxs", "Get a vector containing max bounds, centered on object")

	DEFINE_SCRIPTFUNC_NAMED( ScriptUtilRemove, "Destroy", ""  )
	DEFINE_SCRIPTFUNC_NAMED( ScriptSetOwner, "SetOwner", ""  )
	DEFINE_SCRIPTFUNC_NAMED( GetTeamNumber, "GetTeam", ""  )
	DEFINE_SCRIPTFUNC_NAMED( ChangeTeam, "SetTeam", ""  )

#ifdef MAPBASE_VSCRIPT
	DEFINE_SCRIPTFUNC_NAMED( ScriptSetParent, "SetParent", "" )
#endif
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetMoveParent, "GetMoveParent", "If in hierarchy, retrieves the entity's parent" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetRootMoveParent, "GetRootMoveParent", "If in hierarchy, walks up the hierarchy to find the root parent" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptFirstMoveChild,  "FirstMoveChild", "" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptNextMovePeer, "NextMovePeer", "" )

	DEFINE_SCRIPTFUNC_NAMED( KeyValueFromString, "__KeyValueFromString", SCRIPT_HIDE )
	DEFINE_SCRIPTFUNC_NAMED( KeyValueFromFloat, "__KeyValueFromFloat", SCRIPT_HIDE )
	DEFINE_SCRIPTFUNC_NAMED( KeyValueFromInt, "__KeyValueFromInt", SCRIPT_HIDE )
	DEFINE_SCRIPTFUNC_NAMED( KeyValueFromVector, "__KeyValueFromVector", SCRIPT_HIDE )

#ifdef MAPBASE_VSCRIPT
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetModelKeyValues, "GetModelKeyValues", "Get a KeyValue class instance on this entity's model")
#endif

#ifdef MAPBASE_VSCRIPT
	DEFINE_SCRIPTFUNC_NAMED( ScriptIsVisible, "IsVisible", "Check if the specified position can be visible to this entity." )
	DEFINE_SCRIPTFUNC_NAMED( ScriptIsEntVisible, "IsEntVisible", "Check if the specified entity can be visible to this entity." )
	DEFINE_SCRIPTFUNC_NAMED( ScriptIsVisibleWithMask, "IsVisibleWithMask", "Check if the specified position can be visible to this entity with a specific trace mask." )

	DEFINE_SCRIPTFUNC_NAMED( ScriptTakeDamage, "TakeDamage", "Apply damage to this entity with a given info handle" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptFireBullets, "FireBullets", "Fire bullets from entity with a given info handle" )

	DEFINE_SCRIPTFUNC( TakeHealth, "Give this entity health" )
	DEFINE_SCRIPTFUNC( IsAlive, "Return true if this entity is alive" )

	DEFINE_SCRIPTFUNC( GetWaterLevel, "Get current level of water submergence" )

	DEFINE_SCRIPTFUNC_NAMED( ScriptGetContext, "GetContext", "Get a response context value" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptAddContext, "AddContext", "Add a response context value" )
	DEFINE_SCRIPTFUNC( GetContextExpireTime, "Get a response context's expiration time" )
	DEFINE_SCRIPTFUNC( GetContextCount, "Get the number of response contexts" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetContextIndex, "GetContextIndex", "Get a response context at a specific index in the form of a table" )

	DEFINE_SCRIPTFUNC_NAMED( ScriptFollowEntity, "FollowEntity", "Begin following the specified entity. This makes this entity non-solid, parents it to the target entity, and teleports it to the specified entity's origin. The second parameter is whether or not to use bonemerging while following." )
	DEFINE_SCRIPTFUNC( StopFollowingEntity, "Stops following an entity if we're following one." )
	DEFINE_SCRIPTFUNC( IsFollowingEntity, "Returns true if this entity is following another entity." )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetFollowedEntity, "GetFollowedEntity", "Get the entity we're following." )

	DEFINE_SCRIPTFUNC_NAMED( ScriptClassify, "Classify", "Get Class_T class ID (corresponds to the CLASS_ set of constants)" )

	DEFINE_SCRIPTFUNC_NAMED( ScriptAcceptInput, "AcceptInput", "" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptFireOutput, "FireOutput", "Fire an entity output" )
	DEFINE_SCRIPTFUNC( GetMaxOutputDelay, "Get the longest delay for all events attached to an output" )
	DEFINE_SCRIPTFUNC( CancelEventsByInput, "Cancel all I/O events for this entity, match input" )

	DEFINE_SCRIPTFUNC_NAMED( ScriptAddOutput, "AddOutput", "Add an output" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetKeyValue, "GetKeyValue", "Get a keyvalue" )

	DEFINE_SCRIPTFUNC_NAMED( ScriptGetColorVector, "GetRenderColorVector", "Get the render color as a vector" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetColorR, "GetRenderColorR", "Get the render color's R value" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetColorG, "GetRenderColorG", "Get the render color's G value" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetColorB, "GetRenderColorB", "Get the render color's B value" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetAlpha, "GetRenderAlpha", "Get the render color's alpha value" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptSetColorVector, "SetRenderColorVector", "Set the render color as a vector" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptSetColor, "SetRenderColor", "Set the render color" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptSetColorR, "SetRenderColorR", "Set the render color's R value" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptSetColorG, "SetRenderColorG", "Set the render color's G value" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptSetColorB, "SetRenderColorB", "Set the render color's B value" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptSetAlpha, "SetRenderAlpha", "Set the render color's alpha value" )

	DEFINE_SCRIPTFUNC_NAMED( ScriptGetColorVector, "GetColorVector", SCRIPT_HIDE )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetColorR, "GetColorR", SCRIPT_HIDE )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetColorG, "GetColorG", SCRIPT_HIDE )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetColorB, "GetColorB", SCRIPT_HIDE )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetAlpha, "GetAlpha", SCRIPT_HIDE )
	DEFINE_SCRIPTFUNC_NAMED( ScriptSetColorVector, "SetColorVector", SCRIPT_HIDE )
	DEFINE_SCRIPTFUNC_NAMED( ScriptSetColorR, "SetColorR", SCRIPT_HIDE )
	DEFINE_SCRIPTFUNC_NAMED( ScriptSetColorG, "SetColorG", SCRIPT_HIDE )
	DEFINE_SCRIPTFUNC_NAMED( ScriptSetColorB, "SetColorB", SCRIPT_HIDE )
	DEFINE_SCRIPTFUNC_NAMED( ScriptSetAlpha, "SetAlpha", SCRIPT_HIDE )

	DEFINE_SCRIPTFUNC_NAMED( ScriptGetRenderMode, "GetRenderMode", "Get render mode" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptSetRenderMode, "SetRenderMode", "Set render mode" )

	DEFINE_SCRIPTFUNC( GetSpawnFlags, "Get spawnflags" )
	DEFINE_SCRIPTFUNC( AddSpawnFlags, "Add spawnflag(s)" )
	DEFINE_SCRIPTFUNC( RemoveSpawnFlags, "Remove spawnflag(s)" )
	DEFINE_SCRIPTFUNC( ClearSpawnFlags, "Clear spawnflag(s)" )
	DEFINE_SCRIPTFUNC( HasSpawnFlags, "Check if the entity has specific spawnflag(s) ticked" )

	DEFINE_SCRIPTFUNC( GetEffects, "Get effects" )
	DEFINE_SCRIPTFUNC( AddEffects, "Add effect(s)" )
	DEFINE_SCRIPTFUNC( RemoveEffects, "Remove effect(s)" )
	DEFINE_SCRIPTFUNC( ClearEffects, "Clear effect(s)" )
	DEFINE_SCRIPTFUNC( SetEffects, "Set effect(s)" )
	DEFINE_SCRIPTFUNC( IsEffectActive, "Check if an effect is active" )

	DEFINE_SCRIPTFUNC( GetFlags, "Get flags" )
	DEFINE_SCRIPTFUNC( AddFlag, "Add flag" )
	DEFINE_SCRIPTFUNC( RemoveFlag, "Remove flag" )

	DEFINE_SCRIPTFUNC( GetEFlags, "Get Eflags" )
	DEFINE_SCRIPTFUNC( AddEFlags, "Add Eflags" )
	DEFINE_SCRIPTFUNC( RemoveEFlags, "Remove Eflags" )

	DEFINE_SCRIPTFUNC( GetTransmitState, "" )
	DEFINE_SCRIPTFUNC( SetTransmitState, "" )

	DEFINE_SCRIPTFUNC_NAMED( ScriptGetMoveType, "GetMoveType", "Get the move type" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptSetMoveType, "SetMoveType", "Set the move type" )

	DEFINE_SCRIPTFUNC( GetCollisionGroup, "Get the collision group" )
	DEFINE_SCRIPTFUNC( SetCollisionGroup, "Set the collision group" )

	DEFINE_SCRIPTFUNC( GetGravity, "" )
	DEFINE_SCRIPTFUNC( SetGravity, "" )
	DEFINE_SCRIPTFUNC( GetFriction, "" )
	DEFINE_SCRIPTFUNC( SetFriction, "" )
	DEFINE_SCRIPTFUNC( GetMass, "" )
	DEFINE_SCRIPTFUNC( SetMass, "" )

	DEFINE_SCRIPTFUNC( GetSolidFlags, "Get solid flags" )
	DEFINE_SCRIPTFUNC( AddSolidFlags, "Add solid flags" )
	DEFINE_SCRIPTFUNC( RemoveSolidFlags, "Remove solid flags" )

	DEFINE_SCRIPTFUNC( IsPlayer, "Returns true if this entity is a player." )
	DEFINE_SCRIPTFUNC( IsNPC, "Returns true if this entity is a NPC." )
	DEFINE_SCRIPTFUNC( IsCombatCharacter, "Returns true if this entity is a combat character (player or NPC)." )
	DEFINE_SCRIPTFUNC_NAMED( IsBaseCombatWeapon, "IsWeapon", "Returns true if this entity is a weapon." )
	DEFINE_SCRIPTFUNC( IsWorld, "Returns true if this entity is the world." )

	DEFINE_SCRIPTFUNC_NAMED( ScriptGetTakeDamage, "GetTakeDamage", "Gets this entity's m_takedamage value. (DAMAGE_YES, DAMAGE_NO, etc.)" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptSetTakeDamage, "SetTakeDamage", "Sets this entity's m_takedamage value. (DAMAGE_YES, DAMAGE_NO, etc.)" )

	// DEFINE_SCRIPTFUNC( IsMarkedForDeletion, "Returns true if the entity is valid and marked for deletion." )
#endif

	DEFINE_SCRIPTFUNC( ValidateScriptScope, "Ensure that an entity's script scope has been created" )
	DEFINE_SCRIPTFUNC( GetScriptScope, "Retrieve the script-side data associated with an entity" )
#ifdef MAPBASE_VSCRIPT
	DEFINE_SCRIPTFUNC( GetOrCreatePrivateScriptScope, "Create and retrieve the script-side data associated with an entity" )
#endif
	DEFINE_SCRIPTFUNC( GetScriptId, "Retrieve the unique identifier used to refer to the entity within the scripting system" )
	DEFINE_SCRIPTFUNC_NAMED( GetScriptOwnerEntity, "GetOwner", "Gets this entity's owner" )
	DEFINE_SCRIPTFUNC_NAMED( SetScriptOwnerEntity, "SetOwner", "Sets this entity's owner" )
	DEFINE_SCRIPTFUNC( entindex, "" )

#ifdef MAPBASE_VSCRIPT
	DEFINE_SCRIPTFUNC_NAMED( ScriptSetThinkFunction, "SetThinkFunction", "" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptStopThinkFunction, "StopThinkFunction", "" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptSetContextThink, "SetContextThink", "Set a think function on this entity." )
	DEFINE_SCRIPTFUNC_NAMED( ScriptSetThink, "SetThink", "" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptStopThink, "StopThink", "" )

	//
	// Hooks
	//
	DEFINE_SIMPLE_SCRIPTHOOK( CBaseEntity::g_Hook_UpdateOnRemove, "UpdateOnRemove", FIELD_VOID, "Called when the entity is being removed." )

	BEGIN_SCRIPTHOOK( CBaseEntity::g_Hook_VPhysicsCollision, "VPhysicsCollision", FIELD_VOID, "Called for every single VPhysics-related collision experienced by this entity." )
		DEFINE_SCRIPTHOOK_PARAM( "entity", FIELD_HSCRIPT )
		DEFINE_SCRIPTHOOK_PARAM( "speed", FIELD_FLOAT )
		DEFINE_SCRIPTHOOK_PARAM( "point", FIELD_VECTOR )
		DEFINE_SCRIPTHOOK_PARAM( "normal", FIELD_VECTOR )
	END_SCRIPTHOOK()

	// NOTE (port): eb014cce copy-pasted VPhysicsCollision's description/parameters here, which
	// would hand the FireBulletsInfo_t to legacy scripts as "entity"; uses the later Mapbase desc.
	BEGIN_SCRIPTHOOK( CBaseEntity::g_Hook_FireBullets, "FireBullets", FIELD_VOID, "Called when the entity fires bullets from itself or from a weapon. The parameter is the associated FireBulletsInfo_t handle. Return false to cancel bullet firing." )
		DEFINE_SCRIPTHOOK_PARAM( "info", FIELD_HSCRIPT )
	END_SCRIPTHOOK()

	BEGIN_SCRIPTHOOK( CBaseEntity::g_Hook_OnDeath, "OnDeath", FIELD_BOOLEAN, "Called when the entity dies (Event_Killed). Returning false makes the entity cancel death, although this could have unforeseen consequences. For hooking any damage instead of just death, see filter_script and PassesFinalDamageFilter." )
		DEFINE_SCRIPTHOOK_PARAM( "info", FIELD_HSCRIPT )
	END_SCRIPTHOOK()
#endif
END_SCRIPTDESC();


//-----------------------------------------------------------------------------
#ifdef MAPBASE_VSCRIPT
void CBaseEntity::ScriptFireOutput( const char *pszOutput, HSCRIPT hActivator, HSCRIPT hCaller, const char *szValue, float flDelay )
{
	variant_t value;
	value.SetString( MAKE_STRING(szValue) );

	FireNamedOutput( pszOutput, value, ToEnt(hActivator), ToEnt(hCaller), flDelay );
}

float CBaseEntity::GetMaxOutputDelay( const char *pszOutput )
{
	CBaseEntityOutput *pOutput = FindNamedOutput( pszOutput );
	if ( pOutput )
	{
		return pOutput->GetMaxDelay();
	}
	return 0;
}

void CBaseEntity::CancelEventsByInput( const char *szInput )
{
	g_EventQueue.CancelEventsByInput( this, szInput );
}

bool CBaseEntity::ScriptAcceptInput( const char *szInputName, const char *szValue, HSCRIPT hActivator, HSCRIPT hCaller )
{
	variant_t value;
	value.SetString( MAKE_STRING(szValue) );

	return AcceptInput( szInputName, ToEnt(hActivator), ToEnt(hCaller), value, 0 );
}
#endif // MAPBASE_VSCRIPT

CBaseEntityOutput *CBaseEntity::FindNamedOutput( const char *pszOutput )
{
	if ( pszOutput == NULL )
		return NULL;

	datamap_t *dmap = GetDataDescMap();
	while ( dmap )
	{
		int fields = dmap->dataNumFields;
		for ( int i = 0; i < fields; i++ )
		{
			typedescription_t *dataDesc = &dmap->dataDesc[i];
			if ( ( dataDesc->fieldType == FIELD_CUSTOM ) && ( dataDesc->flags & FTYPEDESC_OUTPUT ) )
			{
				CBaseEntityOutput *pOutput = ( CBaseEntityOutput * )( ( int )this + ( int )dataDesc->fieldOffset[0] );
				if ( !Q_stricmp( dataDesc->externalName, pszOutput ) )
				{
					return pOutput;
				}
			}
		}
		dmap = dmap->baseMap;
	}
	return NULL;
}

//---------------------------------------------------------
// Use the string as the filename of a script file
// that should be loaded from disk, compiled, and run.
//---------------------------------------------------------
void CBaseEntity::InputRunScriptFile(inputdata_t& inputdata)
{
	RunScriptFile(inputdata.value.String());
}

//---------------------------------------------------------
// Send the string to the VM as source code and execute it
//---------------------------------------------------------
void CBaseEntity::InputRunScript(inputdata_t& inputdata)
{
	RunScript(inputdata.value.String(), "InputRunScript");
}

//---------------------------------------------------------
// Make an explicit function call.
//---------------------------------------------------------
void CBaseEntity::InputCallScriptFunction(inputdata_t& inputdata)
{
	CallScriptFunction(inputdata.value.String(), NULL);
}

#ifdef MAPBASE_VSCRIPT
//---------------------------------------------------------
// Send the string to the VM as source code and execute it
//---------------------------------------------------------
void CBaseEntity::InputRunScriptQuotable(inputdata_t& inputdata)
{
	char szQuotableCode[1024];
	if (V_StrSubst( inputdata.value.String(), "''", "\"", szQuotableCode, sizeof( szQuotableCode ), false ))
	{
		RunScript( szQuotableCode, "InputRunScriptQuotable" );
	}
	else
	{
		RunScript( inputdata.value.String(), "InputRunScriptQuotable" );
	}
}

//---------------------------------------------------------
// Clear this entity's script scope
//---------------------------------------------------------
void CBaseEntity::InputClearScriptScope(inputdata_t& inputdata)
{
	m_ScriptScope.Term();
}
#endif

//-----------------------------------------------------------------------------
// Returns true if the function was located and called. false otherwise.
// NOTE:	Assumes the function takes no parameters at the moment.
//-----------------------------------------------------------------------------
bool CBaseEntity::CallScriptFunction(const char* pFunctionName, ScriptVariant_t* pFunctionReturn)
{
	START_VMPROFILE

		if (!ValidateScriptScope())
		{
			DevMsg("\n***\nFAILED to create private ScriptScope. ABORTING script\n***\n");
			return false;
		}


	HSCRIPT hFunc = m_ScriptScope.LookupFunction(pFunctionName);

	if (hFunc)
	{
		m_ScriptScope.Call(hFunc, pFunctionReturn);
		m_ScriptScope.ReleaseFunction(hFunc);

		UPDATE_VMPROFILE

			return true;
	}

	return false;
}

#ifdef MAPBASE_VSCRIPT
//-----------------------------------------------------------------------------
// Gets a function handle
//-----------------------------------------------------------------------------
HSCRIPT CBaseEntity::LookupScriptFunction(const char* pFunctionName)
{
	START_VMPROFILE

	if (!m_ScriptScope.IsInitialized())
	{
		return NULL;
	}

	return m_ScriptScope.LookupFunction(pFunctionName);
}

//-----------------------------------------------------------------------------
// Calls and releases a function handle (ASSUMES SCRIPT SCOPE AND FUNCTION ARE VALID!)
//-----------------------------------------------------------------------------
bool CBaseEntity::CallScriptFunctionHandle(HSCRIPT hFunc, ScriptVariant_t* pFunctionReturn)
{
	m_ScriptScope.Call(hFunc, pFunctionReturn);
	m_ScriptScope.ReleaseFunction(hFunc);

	UPDATE_VMPROFILE

	return true;
}
#endif


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CBaseEntity::ConnectOutputToScript(const char* pszOutput, const char* pszScriptFunc)
{
	CBaseEntityOutput* pOutput = FindNamedOutput(pszOutput);
	if (!pOutput)
	{
		DevMsg(2, "Script failed to find output \"%s\"\n", pszOutput);
		return;
	}

	string_t iszSelf = AllocPooledString("!self"); // @TODO: cache this [4/25/2008 tom]
	CEventAction* pAction = pOutput->GetActionList();
	while (pAction)
	{
		if (pAction->m_iTarget == iszSelf &&
			pAction->m_flDelay == 0 &&
			pAction->m_nTimesToFire == EVENT_FIRE_ALWAYS &&
			V_strcmp(STRING(pAction->m_iTargetInput), "CallScriptFunction") == 0 &&
			V_strcmp(STRING(pAction->m_iParameter), pszScriptFunc) == 0)
		{
			return;
		}
		pAction = pAction->m_pNext;
	}

	pAction = new CEventAction(NULL);
	pAction->m_iTarget = iszSelf;
	pAction->m_iTargetInput = AllocPooledString("CallScriptFunction");
	pAction->m_iParameter = AllocPooledString(pszScriptFunc);
	pOutput->AddEventAction(pAction);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CBaseEntity::DisconnectOutputFromScript(const char* pszOutput, const char* pszScriptFunc)
{
	CBaseEntityOutput* pOutput = FindNamedOutput(pszOutput);
	if (!pOutput)
	{
		DevMsg(2, "Script failed to find output \"%s\"\n", pszOutput);
		return;
	}

	string_t iszSelf = AllocPooledString("!self"); // @TODO: cache this [4/25/2008 tom]
	CEventAction* pAction = pOutput->GetActionList();
	while (pAction)
	{
		if (pAction->m_iTarget == iszSelf &&
			pAction->m_flDelay == 0 &&
			pAction->m_nTimesToFire == EVENT_FIRE_ALWAYS &&
			V_strcmp(STRING(pAction->m_iTargetInput), "CallScriptFunction") == 0 &&
			V_strcmp(STRING(pAction->m_iParameter), pszScriptFunc) == 0)
		{
			pOutput->RemoveEventAction(pAction);
			delete pAction;
			return;
		}
		pAction = pAction->m_pNext;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CBaseEntity::ScriptThink(void)
{
	ScriptVariant_t varThinkRetVal;
	if (CallScriptFunction(m_iszScriptThinkFunction.ToCStr(), &varThinkRetVal))
	{
		float flThinkFrequency = 0.0f;
		if (!varThinkRetVal.AssignTo(&flThinkFrequency))
		{
			// use default think interval if script think function doesn't provide one
			flThinkFrequency = sv_script_think_interval.GetFloat();
		}

		SetNextThink( gpGlobals->curtime + flThinkFrequency, "ScriptThink" );
	}
	else
	{
		DevWarning("%s FAILED to call script think function %s!\n", GetDebugName(), STRING(m_iszScriptThinkFunction));
	}
}


#ifdef MAPBASE_VSCRIPT
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CBaseEntity::ScriptSetThinkFunction( const char *szFunc, float flTime )
{
	// Empty string stops thinking
	if (!szFunc || szFunc[0] == '\0')
	{
		ScriptStopThinkFunction();
	}
	else
	{
		m_iszScriptThinkFunction = AllocPooledString(szFunc);
		flTime = max( 0, flTime );
		SetContextThink( &CBaseEntity::ScriptThink, gpGlobals->curtime + flTime, "ScriptThink" );
	}
}

void CBaseEntity::ScriptStopThinkFunction()
{
	m_iszScriptThinkFunction = NULL_STRING;
	SetContextThink( NULL, TICK_NEVER_THINK, "ScriptThink" );
}


static inline void ScriptStopContextThink( scriptthinkfunc_t *context )
{
	g_pScriptVM->ReleaseScript( context->m_hfnThink );
	context->m_hfnThink = NULL;
	context->m_nNextThinkTick = TICK_NEVER_THINK;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CBaseEntity::ScriptContextThink()
{
	float flNextThink = FLT_MAX;
	int nScheduledTick = 0;

	for ( int i = m_ScriptThinkFuncs.Count(); i--; )
	{
		scriptthinkfunc_t *cur = &m_ScriptThinkFuncs[i];

		if ( cur->m_nNextThinkTick == TICK_NEVER_THINK )
			continue;

		if ( cur->m_nNextThinkTick > gpGlobals->tickcount )
		{
			// There is more to execute, don't stop thinking if the rest are done.

			// also find the shortest schedule
			if ( !nScheduledTick || nScheduledTick > cur->m_nNextThinkTick )
			{
				nScheduledTick = cur->m_nNextThinkTick;
			}
			continue;
		}

		ScriptVariant_t varReturn;

		if ( cur->m_bNoParam )
		{
			if ( g_pScriptVM->Call( cur->m_hfnThink, NULL, true, &varReturn ) == SCRIPT_ERROR )
			{
				ScriptStopContextThink(cur);
				m_ScriptThinkFuncs.Remove(i);
				continue;
			}
		}
		else
		{
			if ( g_pScriptVM->Call( cur->m_hfnThink, NULL, true, &varReturn, m_hScriptInstance ) == SCRIPT_ERROR )
			{
				ScriptStopContextThink(cur);
				m_ScriptThinkFuncs.Remove(i);
				continue;
			}
		}

		float flReturn;
		if ( !varReturn.AssignTo( &flReturn ) )
		{
			ScriptStopContextThink(cur);
			m_ScriptThinkFuncs.Remove(i);
			continue;
		}

		if ( flReturn < 0.0f )
		{
			ScriptStopContextThink(cur);
			m_ScriptThinkFuncs.Remove(i);
			continue;
		}

		// find the shortest delay
		if ( flReturn < flNextThink )
		{
			flNextThink = flReturn;
		}

		cur->m_nNextThinkTick = TIME_TO_TICKS( gpGlobals->curtime + flReturn );
	}

	if ( flNextThink < FLT_MAX )
	{
		SetNextThink( gpGlobals->curtime + flNextThink, "ScriptContextThink" );
	}
	else if ( nScheduledTick )
	{
		SetNextThink( TICKS_TO_TIME( nScheduledTick ), "ScriptContextThink" );
	}
	else
	{
		SetNextThink( TICK_NEVER_THINK, "ScriptContextThink" );
	}
}

// see ScriptSetThink
static bool s_bScriptContextThinkNoParam = false;

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CBaseEntity::ScriptSetContextThink( const char* szContext, HSCRIPT hFunc, float flTime )
{
	scriptthinkfunc_t th;
	V_memset( &th, 0x0, sizeof(scriptthinkfunc_t) );
	unsigned short hash = ( szContext && *szContext ) ? HashString( szContext ) : 0;
	bool bFound = false;

	FOR_EACH_VEC( m_ScriptThinkFuncs, i )
	{
		scriptthinkfunc_t f = m_ScriptThinkFuncs[i];
		if ( hash == f.m_iContextHash )
		{
			th = f;
			m_ScriptThinkFuncs.Remove(i); // reorder
			bFound = true;
			break;
		}
	}

	if ( hFunc )
	{
		float nextthink = gpGlobals->curtime + flTime;

		th.m_bNoParam = s_bScriptContextThinkNoParam;
		th.m_hfnThink = hFunc;
		th.m_iContextHash = hash;
		th.m_nNextThinkTick = TIME_TO_TICKS( nextthink );

		m_ScriptThinkFuncs.AddToHead( th );

		int nexttick = GetNextThinkTick( RegisterThinkContext( "ScriptContextThink" ) );

		// sooner than next think
		if ( nexttick <= 0 || nexttick > th.m_nNextThinkTick )
		{
			SetContextThink( &CBaseEntity::ScriptContextThink, nextthink, "ScriptContextThink" );
		}
	}
	// null func input, think exists
	else if ( bFound )
	{
		ScriptStopContextThink( &th );
	}
}

//-----------------------------------------------------------------------------
// m_bNoParam and s_bScriptContextThinkNoParam exist only to keep backwards compatibility
// and are an alternative to this script closure:
//
//	function CBaseEntity::SetThink( func, time )
//	{
//		SetContextThink( "", function(_){ return func() }, time )
//	}
//-----------------------------------------------------------------------------
void CBaseEntity::ScriptSetThink( HSCRIPT hFunc, float time )
{
	s_bScriptContextThinkNoParam = true;
	ScriptSetContextThink( NULL, hFunc, time );
	s_bScriptContextThinkNoParam = false;
}

void CBaseEntity::ScriptStopThink()
{
	ScriptSetContextThink( NULL, NULL, 0.0f );
}
#endif // MAPBASE_VSCRIPT

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char* CBaseEntity::GetScriptId()
{
#ifdef MAPBASE_VSCRIPT
	return STRING(m_iszScriptId);
#else
	return STRING(m_iszScriptThinkFunction);
#endif
}

//-----------------------------------------------------------------------------
// Recreate the old behaviour of GetScriptId under a new function
//-----------------------------------------------------------------------------
// const char* CBaseEntity::GetScriptThinkFunction()
// {
// 	return STRING(m_iszScriptThinkFunction);
// }

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
HSCRIPT CBaseEntity::GetScriptScope()
{
	return m_ScriptScope;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#ifdef MAPBASE_VSCRIPT
HSCRIPT CBaseEntity::GetOrCreatePrivateScriptScope()
{
	ValidateScriptScope();
	return m_ScriptScope;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CBaseEntity::ScriptSetParent(HSCRIPT hParent, const char *szAttachment)
{
	CBaseEntity *pParent = ToEnt(hParent);
	if ( !pParent )
	{
		SetParent(NULL);
		return;
	}

	// if an attachment is specified, the parent needs to be CBaseAnimating
	if ( szAttachment && szAttachment[0] != '\0' )
	{
		CBaseAnimating *pAnimating = pParent->GetBaseAnimating();
		if ( !pAnimating )
		{
			Warning("ERROR: Tried to set parent for entity %s (%s), but its parent has no model.\n", GetClassname(), GetDebugName());
			return;
		}

		int iAttachment = pAnimating->LookupAttachment(szAttachment);
		if ( iAttachment <= 0 )
		{
			Warning("ERROR: Tried to set parent for entity %s (%s), but it has no attachment named %s.\n", GetClassname(), GetDebugName(), szAttachment);
			return;
		}

		SetParent(pParent, iAttachment);
		SetMoveType(MOVETYPE_NONE);
		return;
	}

	SetParent(pParent);
}
#endif

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
HSCRIPT CBaseEntity::ScriptGetMoveParent(void)
{
	return ToHScript(GetMoveParent());
}
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
HSCRIPT CBaseEntity::ScriptGetRootMoveParent()
{
	return ToHScript(GetRootMoveParent());
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
HSCRIPT CBaseEntity::ScriptFirstMoveChild(void)
{
	return ToHScript(FirstMoveChild());
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
HSCRIPT CBaseEntity::ScriptNextMovePeer(void)
{
	return ToHScript(NextMovePeer());
}

//-----------------------------------------------------------------------------
// Purpose: Load, compile, and run a script file from disk.
//-----------------------------------------------------------------------------
bool CBaseEntity::RunScriptFile(const char* pScriptFile, bool bUseRootScope)
{
	if (!ValidateScriptScope())
	{
		DevMsg("\n***\nFAILED to create private ScriptScope. ABORTING script\n***\n");
		return false;
	}

	if (bUseRootScope)
	{
		return VScriptRunScript(pScriptFile);
	}
	else
	{
		return VScriptRunScript(pScriptFile, m_ScriptScope, true);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Compile and execute a discrete string of script source code
//-----------------------------------------------------------------------------
bool CBaseEntity::RunScript(const char* pScriptText, const char* pDebugFilename)
{
	if (!ValidateScriptScope())
	{
		DevMsg("\n***\nFAILED to create private ScriptScope. ABORTING script\n***\n");
		return false;
	}

	if (m_ScriptScope.Run(pScriptText, pDebugFilename) == SCRIPT_ERROR)
	{
		DevWarning(" Entity %s encountered an error in RunScript()\n", GetDebugName());
	}

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
HSCRIPT CBaseEntity::GetScriptInstance()
{
	if (!m_hScriptInstance)
	{
		if (m_iszScriptId == NULL_STRING)
		{
			char* szName = (char*)stackalloc(1024);
			g_pScriptVM->GenerateUniqueKey((m_iName != NULL_STRING) ? STRING(GetEntityName()) : GetClassname(), szName, 1024);
			m_iszScriptId = AllocPooledString(szName);
		}

		m_hScriptInstance = g_pScriptVM->RegisterInstance(GetScriptDesc(), this);
		g_pScriptVM->SetInstanceUniqeId(m_hScriptInstance, STRING(m_iszScriptId));
	}
	return m_hScriptInstance;
}

//-----------------------------------------------------------------------------
// Using my edict, cook up a unique VScript scope that's private to me, and
// persistent.
//-----------------------------------------------------------------------------
bool CBaseEntity::ValidateScriptScope()
{
	if (!m_ScriptScope.IsInitialized())
	{
		if (scriptmanager == NULL)
		{
			ExecuteOnce(DevMsg("Cannot execute script because scripting is disabled (-scripting)\n"));
			return false;
		}

		if (g_pScriptVM == NULL)
		{
			ExecuteOnce(DevMsg(" Cannot execute script because there is no available VM\n"));
			return false;
		}

		// Force instance creation
		GetScriptInstance();

		EHANDLE hThis;
		hThis.Set(this);

		bool bResult = m_ScriptScope.Init(STRING(m_iszScriptId));

		if (!bResult)
		{
			DevMsg("%s couldn't create ScriptScope!\n", GetDebugName());
			return false;
		}
		g_pScriptVM->SetValue(m_ScriptScope, "self", GetScriptInstance());
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose:	Run all of the vscript files that are set in this entity's VSCRIPTS
//			field in Hammer. The list is space-delimited.
//-----------------------------------------------------------------------------
void CBaseEntity::RunVScripts()
{
	if (m_iszVScripts == NULL_STRING)
	{
		return;
	}

#ifdef MAPBASE_VSCRIPT
	if (g_pScriptVM == NULL)
	{
		return;
	}
#endif

	ValidateScriptScope();

	// All functions we want to have call chained instead of overwritten
	// by other scripts in this entities list.
	static const char* sCallChainFunctions[] =
	{
		"OnPostSpawn",
		"Precache"
	};

	ScriptLanguage_t language = g_pScriptVM->GetLanguage();

	// Make a call chainer for each in this entities scope
	for (int j = 0; j < ARRAYSIZE(sCallChainFunctions); ++j)
	{

		if (language == SL_PYTHON)
		{
			// UNDONE - handle call chaining in python
			;
		}
		else if (language == SL_SQUIRREL)
		{
			//TODO: For perf, this should be precompiled and the %s should be passed as a parameter
			HSCRIPT hCreateChainScript = g_pScriptVM->CompileScript(CFmtStr("%sCallChain <- CSimpleCallChainer(\"%s\", self.GetScriptScope(), true)", sCallChainFunctions[j], sCallChainFunctions[j]));
			g_pScriptVM->Run(hCreateChainScript, (HSCRIPT)m_ScriptScope);
		}
	}

	char szScriptsList[255];
	Q_strcpy(szScriptsList, STRING(m_iszVScripts));
	CUtlStringList szScripts;

	V_SplitString(szScriptsList, " ", szScripts);

	for (int i = 0; i < szScripts.Count(); i++)
	{
		Log( "%s executing script: %s\n", GetDebugName(), szScripts[i]);

		RunScriptFile(szScripts[i], IsWorld());

		for (int j = 0; j < ARRAYSIZE(sCallChainFunctions); ++j)
		{
			if (language == SL_PYTHON)
			{
				// UNDONE - handle call chaining in python
				;
			}
			else if (language == SL_SQUIRREL)
			{
				//TODO: For perf, this should be precompiled and the %s should be passed as a parameter.
				HSCRIPT hRunPostScriptExecute = g_pScriptVM->CompileScript(CFmtStr("%sCallChain.PostScriptExecute()", sCallChainFunctions[j]));
				g_pScriptVM->Run(hRunPostScriptExecute, (HSCRIPT)m_ScriptScope);
			}
		}
	}

	if (m_iszScriptThinkFunction != NULL_STRING)
	{
		SetContextThink(&CBaseEntity::ScriptThink, gpGlobals->curtime + sv_script_think_interval.GetFloat(), "ScriptThink");
	}
}


//--------------------------------------------------------------------------------------------------
// This is called during entity spawning and after restore to allow scripts to precache any
// resources they need.
//--------------------------------------------------------------------------------------------------
void CBaseEntity::RunPrecacheScripts(void)
{
	if (m_iszVScripts == NULL_STRING)
	{
		return;
	}

#ifdef MAPBASE_VSCRIPT
	if (g_pScriptVM == NULL)
	{
		return;
	}
#endif

	HSCRIPT hScriptPrecache = m_ScriptScope.LookupFunction("DispatchPrecache");
	if (hScriptPrecache)
	{
		g_pScriptVM->Call(hScriptPrecache, m_ScriptScope);
		m_ScriptScope.ReleaseFunction(hScriptPrecache);
	}
}

void CBaseEntity::RunOnPostSpawnScripts(void)
{
	if (m_iszVScripts == NULL_STRING)
	{
		return;
	}

#ifdef MAPBASE_VSCRIPT
	if (g_pScriptVM == NULL)
	{
		return;
	}
#endif

	HSCRIPT hFuncConnect = g_pScriptVM->LookupFunction("ConnectOutputs");
	if (hFuncConnect)
	{
		g_pScriptVM->Call(hFuncConnect, NULL, true, NULL, (HSCRIPT)m_ScriptScope);
		g_pScriptVM->ReleaseFunction(hFuncConnect);
	}

	HSCRIPT hFuncDisp = m_ScriptScope.LookupFunction("DispatchOnPostSpawn");
	if (hFuncDisp)
	{
		variant_t variant;
		variant.SetString(MAKE_STRING("DispatchOnPostSpawn"));
		g_EventQueue.AddEvent(this, "CallScriptFunction", variant, 0, this, this);
		m_ScriptScope.ReleaseFunction(hFuncDisp);

	}
}

HSCRIPT	CBaseEntity::GetScriptOwnerEntity()
{
	return ToHScript(GetOwnerEntity());
}

void CBaseEntity::SetScriptOwnerEntity(HSCRIPT pOwner)
{
	SetOwnerEntity(ToEnt(pOwner));
}

void CBaseEntity::ScriptSetLocalAngularVelocity(float pitchVel, float yawVel, float rollVel)
{
	QAngle qa;
	qa.Init(pitchVel, yawVel, rollVel);
	SetLocalAngularVelocity(qa);
}

const Vector& CBaseEntity::ScriptGetLocalAngularVelocity(void)
{
	QAngle qa = GetLocalAngularVelocity();
	static Vector v;
	v.x = qa.x;
	v.y = qa.y;
	v.z = qa.z;
	return v;
}

//-----------------------------------------------------------------------------
// Vscript: Gets the min collision bounds, centered on object
//-----------------------------------------------------------------------------
const Vector& CBaseEntity::ScriptGetBoundingMins(void)
{
	return m_Collision.OBBMins();
}

//-----------------------------------------------------------------------------
// Vscript: Gets the max collision bounds, centered on object
//-----------------------------------------------------------------------------
const Vector& CBaseEntity::ScriptGetBoundingMaxs(void)
{
	return m_Collision.OBBMaxs();
}

#ifdef MAPBASE_VSCRIPT
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CBaseEntity::ScriptTakeDamage( HSCRIPT pInfo )
{
	if (pInfo)
	{
		CTakeDamageInfo *info = HScriptToClass<CTakeDamageInfo>( pInfo );
		if (info)
		{
			return OnTakeDamage( *info );
		}
	}

	return 0;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CBaseEntity::ScriptFireBullets( HSCRIPT pInfo )
{
	if (pInfo)
	{
		extern FireBulletsInfo_t *GetFireBulletsInfoFromInfo( HSCRIPT hBulletsInfo );
		FireBulletsInfo_t *info = GetFireBulletsInfoFromInfo( pInfo );
		if (info)
		{
			FireBullets( *info );
		}
	}
}

//-----------------------------------------------------------------------------
// NOTE (port): Mapbase 66a44d07 wrapped this in ScriptCreateMatrixInstance() from
// vscript_funcs_math.cpp; that moved into the VScript library (matrix3x4_t is a
// lib-registered class in our prebuilt vscript.lib), so register the instance
// directly the way later Mapbase does.
//-----------------------------------------------------------------------------
HSCRIPT CBaseEntity::ScriptEntityToWorldTransform( void )
{
	return g_pScriptVM->RegisterInstance( &EntityToWorldTransform() );
}

//-----------------------------------------------------------------------------
// VScript access to model's key values
// for iteration and value access, use:
//	ScriptFindKey, ScriptGetFirstSubKey, ScriptGetString,
//	ScriptGetInt, ScriptGetFloat, ScriptGetNextKey
// NOTE (port): deferred from the d840 port; written against our vscript.lib, which owns
// CScriptKeyValues, as a ref-counted handle (the form later Mapbase uses).
//-----------------------------------------------------------------------------
HSCRIPT_RC CBaseEntity::ScriptGetModelKeyValues( void )
{
	KeyValues *pModelKeyValues = new KeyValues("");
	HSCRIPT hScript = NULL;
	const char *pszModelName = modelinfo->GetModelName( GetModel() );
	const char *pBuffer = modelinfo->GetModelKeyValueText( GetModel() ) ;

	if ( pModelKeyValues->LoadFromBuffer( pszModelName, pBuffer ) )
	{
		hScript = scriptmanager->CreateScriptKeyValues( g_pScriptVM, pModelKeyValues );
	}
	else
	{
		pModelKeyValues->deleteThis();
	}

	return hScript;
}

//-----------------------------------------------------------------------------
// Vscript: Gets the entity's physics object if it has one
//-----------------------------------------------------------------------------
HSCRIPT CBaseEntity::ScriptGetPhysicsObject( void )
{
	if (VPhysicsGetObject())
		return g_pScriptVM->RegisterInstance( VPhysicsGetObject() );
	else
		return NULL;
}

//-----------------------------------------------------------------------------
// NOTE (port): this base lacks Mapbase's 3-arg AddContext / by-name GetContextValue,
// so these adapt to the vanilla single-string AddContext ("name:value:duration")
// and the index-based context accessors.
//-----------------------------------------------------------------------------
void CBaseEntity::ScriptAddContext( const char *name, const char *value, float duration )
{
	if ( duration > 0.0f )
		AddContext( CFmtStr( "%s:%s:%f", name, value, duration ) );
	else
		AddContext( CFmtStr( "%s:%s", name, value ) );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char *CBaseEntity::ScriptGetContext( const char *name )
{
	int idx = FindContextByName( name );
	if ( idx == -1 )
		return NULL;
	return GetContextValue( idx );
}

//-----------------------------------------------------------------------------
// NOTE (port): Mapbase defines this next to its by-name GetContextValue (MAPBASE-only
// in this tree), so it lives here with the other script context accessors.
//-----------------------------------------------------------------------------
float CBaseEntity::GetContextExpireTime( const char *name )
{
	int idx = FindContextByName( name );
	if ( idx == -1 )
		return 0.0f;

	return m_ResponseContexts[ idx ].m_fExpirationTime;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
HSCRIPT CBaseEntity::ScriptGetContextIndex( int index )
{
	if (index >= m_ResponseContexts.Count())
		return NULL;

	ScriptVariant_t varTable;
	g_pScriptVM->CreateTable( varTable );

	g_pScriptVM->SetValue( varTable, "name", STRING( m_ResponseContexts[index].m_iszName ) );
	g_pScriptVM->SetValue( varTable, "value", STRING( m_ResponseContexts[index].m_iszValue ) );
	g_pScriptVM->SetValue( varTable, "expiration_time", m_ResponseContexts[index].m_fExpirationTime );

	return varTable.m_hScript;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CBaseEntity::ScriptClassify( void )
{
	return (int)Classify();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CBaseEntity::ScriptAddOutput( const char *pszOutputName, const char *pszTarget, const char *pszAction, const char *pszParameter, float flDelay, int iMaxTimes )
{
	const char *pszValue = UTIL_VarArgs("%s,%s,%s,%f,%i", pszTarget, pszAction, pszParameter, flDelay, iMaxTimes);
	return KeyValue( pszOutputName, pszValue );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char *CBaseEntity::ScriptGetKeyValue( const char *pszKeyName )
{
	static char szValue[128];
	GetKeyValue( pszKeyName, szValue, sizeof(szValue) );
	return szValue;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const Vector& CBaseEntity::ScriptGetColorVector()
{
	static Vector vecColor;
	vecColor.Init( m_clrRender.GetR(), m_clrRender.GetG(), m_clrRender.GetB() );
	return vecColor;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CBaseEntity::ScriptSetColorVector( const Vector& vecColor )
{
	SetRenderColor( vecColor.x, vecColor.y, vecColor.z );
}

void CBaseEntity::ScriptSetColor( int r, int g, int b )
{
	SetRenderColor( r, g, b );
}
#endif
