//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose: Mapbase's self-contained CBaseEntity I/O extension handlers, ported into
//          the vanilla-based tree.
//
//          Method bodies live here; the declarations are added to baseentity.h and
//          the datadesc registration is added to baseentity.cpp's BEGIN_DATADESC.
//
//          The networked view-hide / flashlight inputs are intentionally omitted
//          (they need SendTable/RecvTable + client changes).
//
//=============================================================================//

#include "cbase.h"
#include "entitylist.h"
#include "eventqueue.h"
#include "mapbase/matchers.h"
#include "mapbase/datadesc_mod.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// --- PassUser / FireRandomUser / PassRandomUser ---
void CBaseEntity::InputPassUser1( inputdata_t& inputdata )
{
	m_OutUser1.Set( inputdata.value, inputdata.pActivator, this );
}

void CBaseEntity::InputPassUser2( inputdata_t& inputdata )
{
	m_OutUser2.Set( inputdata.value, inputdata.pActivator, this );
}

void CBaseEntity::InputPassUser3( inputdata_t& inputdata )
{
	m_OutUser3.Set( inputdata.value, inputdata.pActivator, this );
}

void CBaseEntity::InputPassUser4( inputdata_t& inputdata )
{
	m_OutUser4.Set( inputdata.value, inputdata.pActivator, this );
}


void CBaseEntity::InputFireRandomUser( inputdata_t& inputdata )
{
	switch (RandomInt(1, 4))
	{
		case 1:		m_OnUser1.FireOutput( inputdata.pActivator, this ); break;
		case 2:		m_OnUser2.FireOutput( inputdata.pActivator, this ); break;
		case 3:		m_OnUser3.FireOutput( inputdata.pActivator, this ); break;
		case 4:		m_OnUser4.FireOutput( inputdata.pActivator, this ); break;
	}
}

void CBaseEntity::InputPassRandomUser( inputdata_t& inputdata )
{
	switch (RandomInt(1, 4))
	{
		case 1:		m_OutUser1.Set( inputdata.value, inputdata.pActivator, this ); break;
		case 2:		m_OutUser2.Set( inputdata.value, inputdata.pActivator, this ); break;
		case 3:		m_OutUser3.Set( inputdata.value, inputdata.pActivator, this ); break;
		case 4:		m_OutUser4.Set( inputdata.value, inputdata.pActivator, this ); break;
	}
}

// --- SetEntityName / SetTarget / SetOwnerEntity / Health / FireOutput / RemoveOutput / ReplaceOutput / AcceptInput / CancelPending / FreeChildren / transforms / flags / render / effects / move / Touch / KilledNPC / Kill*Visible / SetThinkNull ---
//-----------------------------------------------------------------------------
// Purpose: Sets the entity's targetname.
//-----------------------------------------------------------------------------
void CBaseEntity::InputSetEntityName( inputdata_t& inputdata )
{
	SetName( inputdata.value.StringID() );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the generic target field.
//-----------------------------------------------------------------------------
void CBaseEntity::InputSetTarget( inputdata_t& inputdata )
{
	m_target = inputdata.value.StringID();
	Activate();
}

//-----------------------------------------------------------------------------
// Purpose: Sets our owner entity.
//-----------------------------------------------------------------------------
void CBaseEntity::InputSetOwnerEntity( inputdata_t& inputdata )
{
	SetOwnerEntity(inputdata.value.Entity());
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for adding to the entity's health.
// Input  : Integer health points to add.
//-----------------------------------------------------------------------------
void CBaseEntity::InputAddHealth( inputdata_t &inputdata )
{
	TakeHealth( abs(inputdata.value.Int()), DMG_GENERIC );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for removing health from the entity.
// Input  : Integer health points to remove.
//-----------------------------------------------------------------------------
void CBaseEntity::InputRemoveHealth( inputdata_t &inputdata )
{
	TakeDamage( CTakeDamageInfo( this, this, abs(inputdata.value.Int()), DMG_GENERIC ) );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseEntity::InputSetHealth( inputdata_t &inputdata )
{
	int iNewHealth = inputdata.value.Int();
	int iDelta = abs(GetHealth() - iNewHealth);
	if ( iNewHealth > GetHealth() )
	{
		TakeHealth( iDelta, DMG_GENERIC );
	}
	else if ( iNewHealth < GetHealth() )
	{
		TakeDamage( CTakeDamageInfo( this, this, iDelta, DMG_GENERIC ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseEntity::InputSetMaxHealth( inputdata_t &inputdata )
{
	int iNewMaxHealth = inputdata.value.Int();
	SetMaxHealth(iNewMaxHealth);

	if (GetHealth() > iNewMaxHealth)
	{
		SetHealth(iNewMaxHealth);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Forces the named output to fire.
//			In addition to the output itself, parameter may include !activator, !caller, and what to pass.
//-----------------------------------------------------------------------------
void CBaseEntity::InputFireOutput( inputdata_t& inputdata )
{
	char sParameter[MAX_PATH];
	Q_strncpy( sParameter, inputdata.value.String(), sizeof(sParameter) );
	if ( sParameter )
	{
		int iter = 0;
		char *data[5] = {sParameter};
		char *sToken = strtok( sParameter, ":" );
		while ( sToken && iter < 5 )
		{
			data[iter] = sToken;
			iter++;
			sToken = strtok( NULL, ":" );
		}

		//DevMsg("data[0]: %s\ndata[1]: %s\ndata[2]: %s\ndata[3]: %s\ndata[4]: %s\n", data[0], data[1], data[2], data[3], data[4]);

		// Format: <output name>:<activator>:<caller>:<parameter>:<delay>
		// 
		// data[0] = Output Name
		// data[1] = Activator
		// data[2] = Caller
		// data[3] = Parameter
		// data[4] = Delay
		// 
		CBaseEntity *pActivator = inputdata.pActivator;
		if (data[1])
			pActivator = gEntList.FindEntityByName(NULL, data[1], this, inputdata.pActivator, inputdata.pCaller);

		CBaseEntity *pCaller = this;
		if (data[2])
			pCaller = gEntList.FindEntityByName(NULL, data[2], this, inputdata.pActivator, inputdata.pCaller);

		variant_t parameter;
		if (data[3])
		{
			parameter.SetString(MAKE_STRING(data[3]));
		}

		float flDelay = 0.0f;
		if (data[4])
			flDelay = atof(data[4]);

		FireNamedOutput(data[0], parameter, pActivator, pCaller, flDelay);
		//Msg("Output Name: %s, Activator: %s, Caller: %s, Data: %s, Delay: %f\n", data[0], pActivator->GetDebugName(), pCaller->GetDebugName(), parameter.String(), flDelay);
	}
	else
	{
		Warning("FireOutput input fired with bad parameter. Format: <output name>:<activator>:<caller>:<parameter>:<delay>\n");
	}
}

//-----------------------------------------------------------------------------
// Purpose: Removes all outputs of the specified name.
//-----------------------------------------------------------------------------
void CBaseEntity::InputRemoveOutput( inputdata_t& inputdata )
{
	const char *szOutput = inputdata.value.String();
	datamap_t *dmap = GetDataDescMap();
	while ( dmap )
	{
		int fields = dmap->dataNumFields;
		for ( int i = 0; i < fields; i++ )
		{
			typedescription_t *dataDesc = &dmap->dataDesc[i];
			if ( ( dataDesc->fieldType == FIELD_CUSTOM ) && ( dataDesc->flags & FTYPEDESC_OUTPUT ) )
			{
				// If our names match, remove
				if (Matcher_NamesMatch(szOutput, dataDesc->externalName))
				{
					CBaseEntityOutput *pOutput = (CBaseEntityOutput *)((int)this + (int)dataDesc->fieldOffset[0]);
					pOutput->DeleteAllElements();
				}
			}
		}

		dmap = dmap->baseMap;
	}
}

// Find a way to implement this
/*
//------------------------------------------------------------------------------
// Purpose: Cancels all I/O events of a specific output.
//------------------------------------------------------------------------------
void CBaseEntity::InputCancelOutput( inputdata_t &inputdata )
{ 

}
*/

//-----------------------------------------------------------------------------
// Purpose: Replaces all outputs of the specified name.
//-----------------------------------------------------------------------------
void CBaseEntity::InputReplaceOutput( inputdata_t& inputdata )
{
	char sParameter[128];
	Q_strncpy( sParameter, inputdata.value.String(), sizeof(sParameter) );
	if (!sParameter)
		return;

	int iter = 0;
	char *data[2];
	char *sToken = strtok( sParameter, ": " );
	while ( sToken && iter < 2 )
	{
		data[iter] = sToken;
		iter++;
		sToken = strtok( NULL, ": " );
	}

	const char *szOutput = data[0];
	const char *szNewOutput = data[1];
	if (!szOutput || !szNewOutput)
	{
		Warning("ReplaceOutput input fired with bad parameter. Format: <old output name>:<new output name>\n");
		return;
	}

	int iOutputsReplaced = 0;
	datamap_t *dmap = GetDataDescMap();
	while ( dmap )
	{
		int fields = dmap->dataNumFields;
		for ( int i = 0; i < fields; i++ )
		{
			typedescription_t *dataDesc = &dmap->dataDesc[i];
			if ( ( dataDesc->fieldType == FIELD_CUSTOM ) && ( dataDesc->flags & FTYPEDESC_OUTPUT ) )
			{
				// If our names match, replace
				if (Matcher_NamesMatch(szOutput, dataDesc->externalName))
				{
					CBaseEntityOutput *pOutput = (CBaseEntityOutput *)((int)this + (int)dataDesc->fieldOffset[0]);
					const char *szTarget;
					const char *szInputName;
					const char *szParam;
					float flDelay;
					int iNumTimes;
					char szData[256];
					for ( CEventAction *ev = pOutput->GetActionList(); ev != NULL; ev = ev->m_pNext )
					{
						// This is the only way I think we could do this. Accomplishes the job more or less anyway
						szTarget = STRING(ev->m_iTarget);
						szInputName = STRING(ev->m_iTargetInput);
						szParam = ev->m_iParameter == NULL_STRING ? "" : STRING(ev->m_iParameter);
						flDelay = ev->m_flDelay;
						iNumTimes = ev->m_nTimesToFire;
						Q_snprintf(szData, sizeof(szData), "%s,%s,%s,%f,%i", szTarget, szInputName, szParam, flDelay, iNumTimes);

						KeyValue(szNewOutput, szData);

						DevMsg("ReplaceOutput: %s %s\n", szNewOutput, szData);

						iOutputsReplaced++;
					}
					pOutput->DeleteAllElements();
				}
			}
		}

		dmap = dmap->baseMap;
	}

	if (iOutputsReplaced == 0)
	{
		Warning("ReplaceOutput unable to find %s on %s\n", szOutput, GetDebugName());
	}
	else
	{
		DevMsg("Replaced %i instances of %s with %s on %s\n", iOutputsReplaced, szOutput, szNewOutput, GetDebugName());
	}
}

//-----------------------------------------------------------------------------
// Purpose: Forces the named input to fire...what?
//			Inputception...or is it just "Inception"? Whatever.
//			True inception would be using this input to fire AcceptInput. 
//			(it would probably crash, I haven't tested it)
//			
//			In addition to the input itself, parameter may include !activator, !caller, and what to pass.
//-----------------------------------------------------------------------------
void CBaseEntity::InputAcceptInput( inputdata_t& inputdata )
{
	char sParameter[MAX_PATH];
	Q_strncpy( sParameter, inputdata.value.String(), sizeof(sParameter) );
	if ( sParameter )
	{
		int iter = 0;
		char *data[5] = {sParameter};
		char *sToken = strtok( sParameter, ":" );
		while ( sToken && iter < 5 )
		{
			data[iter] = sToken;
			iter++;
			sToken = strtok( NULL, ":" );
		}

		//DevMsg("data[0]: %s\ndata[1]: %s\ndata[2]: %s\ndata[3]: %s\ndata[4]: %s\n", data[0], data[1], data[2], data[3], data[4]);

		// Format: <input name>:<parameter>:<activator>:<caller>:<output ID>
		// 
		// data[0] = Input Name
		// data[1] = Parameter
		// data[2] = Activator
		// data[3] = Caller
		// data[4] = Output ID
		// 
		variant_t parameter;
		if (data[1])
		{
			parameter.SetString(MAKE_STRING(data[1]));
		}

		CBaseEntity *pActivator = inputdata.pActivator;
		if (data[2])
			pActivator = gEntList.FindEntityByName(NULL, data[2], this, inputdata.pActivator, inputdata.pCaller);

		CBaseEntity *pCaller = this;
		if (data[3])
			pCaller = gEntList.FindEntityByName(NULL, data[3], this, inputdata.pActivator, inputdata.pCaller);

		int iOutputID = -1;
		if (data[4])
			iOutputID = atoi(data[4]);

		AcceptInput(data[0], pActivator, pCaller, parameter, iOutputID);
		Msg("Input Name: %s, Activator: %s, Caller: %s, Data: %s, Output ID: %i\n", data[0], pActivator ? pActivator->GetDebugName() : "None", pCaller ? pCaller->GetDebugName() : "None", parameter.String(), iOutputID);
	}
	else
	{
		Warning("AcceptInput input fired with bad parameter. Format: <input name>:<parameter>:<activator>:<caller>:<output ID>\n");
	}
}

//------------------------------------------------------------------------------
// Purpose: Cancels any I/O events in the queue that were fired by this entity.
//------------------------------------------------------------------------------
void CBaseEntity::InputCancelPending( inputdata_t &inputdata )
{ 
	g_EventQueue.CancelEvents( this );
}

//-----------------------------------------------------------------------------
// Purpose: Frees all of our children, entities parented to this entity.
//-----------------------------------------------------------------------------
void CBaseEntity::InputFreeChildren( inputdata_t& inputdata )
{
	UnlinkAllChildren( this );
}

//-----------------------------------------------------------------------------
// Purpose: Sets our origin.
//-----------------------------------------------------------------------------
void CBaseEntity::InputSetLocalOrigin( inputdata_t& inputdata )
{
	Vector vec;
	inputdata.value.Vector3D(vec);
	SetLocalOrigin(vec);
}

//-----------------------------------------------------------------------------
// Purpose: Sets our angles.
//-----------------------------------------------------------------------------
void CBaseEntity::InputSetLocalAngles( inputdata_t& inputdata )
{
	Vector vec;
	inputdata.value.Vector3D(vec);
	SetLocalAngles( QAngle( vec.x, vec.y, vec.z ) );
}

//-----------------------------------------------------------------------------
// Purpose: Sets our origin.
//-----------------------------------------------------------------------------
void CBaseEntity::InputSetAbsOrigin( inputdata_t& inputdata )
{
	Vector vec;
	inputdata.value.Vector3D(vec);
	SetAbsOrigin(vec);
}

//-----------------------------------------------------------------------------
// Purpose: Sets our angles.
//-----------------------------------------------------------------------------
void CBaseEntity::InputSetAbsAngles( inputdata_t& inputdata )
{
	Vector vec;
	inputdata.value.Vector3D(vec);
	SetAbsAngles( QAngle( vec.x, vec.y, vec.z ) );
}

//-----------------------------------------------------------------------------
// Purpose: Sets our velocity.
//-----------------------------------------------------------------------------
void CBaseEntity::InputSetLocalVelocity( inputdata_t& inputdata )
{
	Vector vec;
	inputdata.value.Vector3D(vec);
	SetLocalVelocity(vec);
}

//-----------------------------------------------------------------------------
// Purpose: Sets our angular velocity.
//-----------------------------------------------------------------------------
void CBaseEntity::InputSetLocalAngularVelocity( inputdata_t& inputdata )
{
	Vector vec;
	inputdata.value.Vector3D(vec);
	SetLocalAngularVelocity(QAngle(vec.x, vec.y, vec.z));
}

//-----------------------------------------------------------------------------
// Purpose: Adds spawn flags.
//-----------------------------------------------------------------------------
void CBaseEntity::InputAddSpawnFlags( inputdata_t& inputdata )
{
	AddSpawnFlags(inputdata.value.Int());
}

//-----------------------------------------------------------------------------
// Purpose: Removes spawn flags.
//-----------------------------------------------------------------------------
void CBaseEntity::InputRemoveSpawnFlags( inputdata_t& inputdata )
{
	RemoveSpawnFlags(inputdata.value.Int());
}

//-----------------------------------------------------------------------------
// Purpose: Sets our render mode.
//-----------------------------------------------------------------------------
void CBaseEntity::InputSetRenderMode( inputdata_t& inputdata )
{
	SetRenderMode((RenderMode_t)inputdata.value.Int());
}

//-----------------------------------------------------------------------------
// Purpose: Sets our render FX.
//-----------------------------------------------------------------------------
void CBaseEntity::InputSetRenderFX( inputdata_t& inputdata )
{
	m_nRenderFX = inputdata.value.Int();
}

//-----------------------------------------------------------------------------
// Purpose: Adds effects.
//-----------------------------------------------------------------------------
void CBaseEntity::InputAddEffects( inputdata_t& inputdata )
{
	AddEffects(inputdata.value.Int());
}

//-----------------------------------------------------------------------------
// Purpose: Removes effects.
//-----------------------------------------------------------------------------
void CBaseEntity::InputRemoveEffects( inputdata_t& inputdata )
{
	RemoveEffects(inputdata.value.Int());
}

//-----------------------------------------------------------------------------
// Purpose: Shortcut for removing nodraw.
//-----------------------------------------------------------------------------
void CBaseEntity::InputDrawEntity( inputdata_t& inputdata )
{
	RemoveEffects(EF_NODRAW);
}

//-----------------------------------------------------------------------------
// Purpose: Shortcut to adding nodraw.
//-----------------------------------------------------------------------------
void CBaseEntity::InputUndrawEntity( inputdata_t& inputdata )
{
	AddEffects(EF_NODRAW);
}

//-----------------------------------------------------------------------------
// Purpose: Adds eflags.
//-----------------------------------------------------------------------------
void CBaseEntity::InputAddEFlags( inputdata_t& inputdata )
{
	AddEFlags(inputdata.value.Int());
}

//-----------------------------------------------------------------------------
// Purpose: Removes eflags.
//-----------------------------------------------------------------------------
void CBaseEntity::InputRemoveEFlags( inputdata_t& inputdata )
{
	RemoveEFlags(inputdata.value.Int());
}

//-----------------------------------------------------------------------------
// Purpose: Adds solid flags.
//-----------------------------------------------------------------------------
void CBaseEntity::InputAddSolidFlags( inputdata_t& inputdata )
{
	AddSolidFlags(inputdata.value.Int());
}

//-----------------------------------------------------------------------------
// Purpose: Removes solid flags.
//-----------------------------------------------------------------------------
void CBaseEntity::InputRemoveSolidFlags( inputdata_t& inputdata )
{
	RemoveSolidFlags(inputdata.value.Int());
}

//-----------------------------------------------------------------------------
// Purpose: Sets the movetype.
//-----------------------------------------------------------------------------
void CBaseEntity::InputSetMoveType( inputdata_t& inputdata )
{
	SetMoveType((MoveType_t)inputdata.value.Int());
}

//-----------------------------------------------------------------------------
// Purpose: Sets the collision group.
//-----------------------------------------------------------------------------
void CBaseEntity::InputSetCollisionGroup( inputdata_t& inputdata )
{
	SetCollisionGroup(inputdata.value.Int());
}

//-----------------------------------------------------------------------------
// Purpose: Touch touch :)
//-----------------------------------------------------------------------------
void CBaseEntity::InputTouch( inputdata_t& inputdata )
{
	if (inputdata.value.Entity())
		Touch( inputdata.value.Entity() );
	else
		Warning( "%s InputTouch: Can't touch null entity", GetDebugName() );
}

//-----------------------------------------------------------------------------
// Purpose: Passes KilledNPC to our possibly more capable parents.
//-----------------------------------------------------------------------------
void CBaseEntity::InputKilledNPC( inputdata_t &inputdata )
{
	// Don't get stuck in an endless loop
	if (inputdata.value.Int() > 16)
		return;
	else
		inputdata.value.SetInt(inputdata.value.Int() + 1);

	if (GetOwnerEntity())
	{
		GetOwnerEntity()->AcceptInput("KilledNPC", inputdata.pActivator, inputdata.pCaller, inputdata.value, inputdata.nOutputID);
	}
	else if (HasPhysicsAttacker(4.0f))
	{
		HasPhysicsAttacker(4.0f)->AcceptInput("KilledNPC", inputdata.pActivator, inputdata.pCaller, inputdata.value, inputdata.nOutputID);
	}
	else if (GetMoveParent())
	{
		GetMoveParent()->AcceptInput("KilledNPC", inputdata.pActivator, inputdata.pCaller, inputdata.value, inputdata.nOutputID);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Remove if not visible by any players
//-----------------------------------------------------------------------------
void CBaseEntity::InputKillIfNotVisible( inputdata_t& inputdata )
{
#ifdef MAPBASE_MP
	// Go through each client and check if we're in their viewcone.
	// If we're in someone's viewcone, return immediately.
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer && pPlayer->FInViewCone( this ) )
			return;
	}
#else
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if ( !pPlayer || !pPlayer->FInViewCone( this ) )
#endif
	{
		m_OnKilled.FireOutput(inputdata.pActivator, this);
		UTIL_Remove(this);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Remove when not visible by any players
//-----------------------------------------------------------------------------
void CBaseEntity::InputKillWhenNotVisible( inputdata_t& inputdata )
{
	SetContextThink( &CBaseEntity::SUB_RemoveWhenNotVisible, gpGlobals->curtime + inputdata.value.Float(), "SUB_RemoveWhenNotVisible" );
	//SetRenderColorA( 255 );
	//m_nRenderMode = kRenderNormal;
}

//-----------------------------------------------------------------------------
// Purpose: Stop thinking
//-----------------------------------------------------------------------------
void CBaseEntity::InputSetThinkNull( inputdata_t& inputdata )
{
	const char *szContext = inputdata.value.String();
	if (szContext && szContext[0] != '\0')
	{
		SetContextThink( NULL, TICK_NEVER_THINK, szContext );
	}
	else
	{
		SetThink( NULL );
		SetNextThink( TICK_NEVER_THINK );
	}
}

// --- SUB_RemoveWhenNotVisible (think used by KillWhenNotVisible) ---
void CBaseEntity::SUB_RemoveWhenNotVisible( void )
{
	if ( SUB_AllowedToFade() == false )
	{
		SetNextThink( gpGlobals->curtime + 1, "SUB_RemoveWhenNotVisible" );
		SetRenderColorA( 255 );
		return;
	}
    
	SetRenderColorA( m_clrRender->a - 1 );

	if ( m_clrRender->a == 0 )
	{
		m_OnKilled.FireOutput(this, this);
		UTIL_Remove(this);
	}
	else
	{
		SetNextThink( gpGlobals->curtime, "SUB_RemoveWhenNotVisible" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Changes a datadesc field by name (uses Datadesc_SetFieldString).
//-----------------------------------------------------------------------------
void CBaseEntity::InputChangeVariable( inputdata_t &inputdata )
{
	const char *szKeyName = NULL;
	const char *szValue = NULL;

	char sOutputName[MAX_PATH];
	Q_strncpy( sOutputName, inputdata.value.String(), sizeof(sOutputName) );
	char *sChar = strchr( sOutputName, ' ' );
	if ( sChar )
	{
		*sChar = '\0';
		// Now replace all the :'s in the string with ,'s.
		// Has to be done this way because Hammer doesn't allow ,'s inside parameters.
		char *sColon = strchr( sChar+1, ':' );
		while ( sColon )
		{
			*sColon = ',';
			sColon = strchr( sChar+1, ':' );
		}

		szKeyName = sOutputName;
		szValue = sChar + 1;
	}
	else
	{
		Warning("ChangeVariable input fired with bad string. Format: <output name> <targetname>,<inputname>,<parameter>,<delay>,<max times to fire (-1 == infinite)>\n");
		return;
	}

	if (szKeyName == NULL)
		return;

	for ( datamap_t *dmap = GetDataDescMap(); dmap != NULL; dmap = dmap->baseMap )
	{
		// search through all the readable fields in the data description, looking for a match
		for ( int i = 0; i < dmap->dataNumFields; i++ )
		{
			if ( dmap->dataDesc[i].flags & (FTYPEDESC_SAVE | FTYPEDESC_KEY) )
			{
				if ( Matcher_NamesMatch(szKeyName, dmap->dataDesc[i].fieldName) )
				{
					// Copied from ::ParseKeyvalue...or technically logic_datadesc_accessor...
					typedescription_t *pField = &dmap->dataDesc[i];
					char *data = Datadesc_SetFieldString( szValue, this, pField, NULL );

					if (!data)
					{
						Warning( "%s cannot set field of type %i.\n", GetDebugName(), dmap->dataDesc[i].fieldType );
					}
				}
			}
		}
	}
}
