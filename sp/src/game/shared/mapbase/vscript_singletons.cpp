//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose: This file contains brand new VScript singletons and singletons replicated from API
//			documentation in other games.
//
//			See vscript_funcs_shared.cpp for more information.
//
//			NOTE (port): server-only for now (not in the client project). Not yet ported:
//			CScriptLocalize ("Localize": vgui ILocalize, which server.dll does not link) and
//			the ef7d9ccc0 CNetMsgScriptHelper redesign (script-to-script messages over a new
//			"ScriptMsg" usermessage and CUserCmd), which needs client VScript. "NetMsg" stays
//			at its eb014cce server-only form below.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "ammodef.h"
#include "tier1/utlcommon.h"

#ifndef CLIENT_DLL
#include "ai_squad.h"
#include "usermessages.h"
#include "recipientfilter.h"
#endif // !CLIENT_DLL

#include "filesystem.h"
#include "tier1/utlbuffer.h"
#include "igameevents.h"
#include "engine/ivdebugoverlay.h"
#include "tier1/mapbase_con_groups.h"

#include "vscript_singletons.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IScriptManager *scriptmanager;

//=============================================================================
// Net Prop Manager
// Based on L4D2 API
//=============================================================================
class CScriptNetPropManager
{
public:

#ifdef CLIENT_DLL
	RecvProp *RecurseTable( RecvTable *pTable, const char *pszPropName )
#else
	SendProp *RecurseTable( SendTable *pTable, const char *pszPropName )
#endif
	{
#ifdef CLIENT_DLL
		RecvProp *pProp = NULL;
#else
		SendProp *pProp = NULL;
#endif
		for (int i = 0; i < pTable->GetNumProps(); i++)
		{
			pProp = pTable->GetProp( i );
			if (pProp->GetType() == DPT_DataTable)
			{
				pProp = RecurseTable(pProp->GetDataTable(), pszPropName);
				if (pProp)
					return pProp;
			}
			else
			{
				if (FStrEq( pProp->GetName(), pszPropName ))
					return pProp;
			}
		}

		return NULL;
	}

#ifdef CLIENT_DLL
	RecvProp *RecurseNetworkClass( ClientClass *pClass, const char *pszPropName )
#else
	SendProp *RecurseNetworkClass( ServerClass *pClass, const char *pszPropName )
#endif
	{
#ifdef CLIENT_DLL
		RecvProp *pProp = RecurseTable( pClass->m_pRecvTable, pszPropName );
#else
		SendProp *pProp = RecurseTable( pClass->m_pTable, pszPropName );
#endif
		if (pProp)
			return pProp;

		if (pClass->m_pNext)
			return RecurseNetworkClass( pClass->m_pNext, pszPropName );
		else
			return NULL;
	}

#ifdef CLIENT_DLL
	RecvProp *GetPropByName( CBaseEntity *pEnt, const char *pszPropName )
	{
		if (pEnt)
		{
			return RecurseNetworkClass( pEnt->GetClientClass(), pszPropName );
		}

		return NULL;
	}
#else
	SendProp *GetPropByName( CBaseEntity *pEnt, const char *pszPropName )
	{
		if (pEnt)
		{
			return RecurseNetworkClass( pEnt->GetServerClass(), pszPropName );
		}

		return NULL;
	}
#endif

	int GetPropArraySize( HSCRIPT hEnt, const char *pszPropName )
	{
		CBaseEntity *pEnt = ToEnt( hEnt );
		auto *pProp = GetPropByName( pEnt, pszPropName );
		if (pProp)
		{
			// TODO: Is this what this function wants?
			return pProp->GetNumElements();
		}

		return -1;
	}

	#define GetPropFunc( name, varType, propType, defaultval ) \
	varType name( HSCRIPT hEnt, const char *pszPropName ) \
	{ \
		CBaseEntity *pEnt = ToEnt( hEnt ); \
		auto *pProp = GetPropByName( pEnt, pszPropName ); \
		if (pProp && pProp->GetType() == propType) \
		{ \
			return *(varType*)((char *)pEnt + pProp->GetOffset()); \
		} \
		return defaultval; \
	} \

	#define GetPropFuncArray( name, varType, propType, defaultval ) \
	varType name( HSCRIPT hEnt, const char *pszPropName, int iArrayElement ) \
	{ \
		CBaseEntity *pEnt = ToEnt( hEnt ); \
		auto *pProp = GetPropByName( pEnt, pszPropName ); \
		if (pProp && pProp->GetType() == propType) \
		{ \
			return ((varType*)((char *)pEnt + pProp->GetOffset()))[iArrayElement]; \
		} \
		return defaultval; \
	} \

	GetPropFunc( GetPropFloat, float, DPT_Float, -1 );
	GetPropFuncArray( GetPropFloatArray, float, DPT_Float, -1 );
	GetPropFunc( GetPropInt, int, DPT_Int, -1 );
	GetPropFuncArray( GetPropIntArray, int, DPT_Int, -1 );
	GetPropFunc( GetPropVector, Vector, DPT_Vector, vec3_invalid );
	GetPropFuncArray( GetPropVectorArray, Vector, DPT_Vector, vec3_invalid );

	HSCRIPT GetPropEntity( HSCRIPT hEnt, const char *pszPropName )
	{
		CBaseEntity *pEnt = ToEnt( hEnt );
		auto *pProp = GetPropByName( pEnt, pszPropName );
		if (pProp && pProp->GetType() == DPT_Int)
		{
			return ToHScript( *(CHandle<CBaseEntity>*)((char *)pEnt + pProp->GetOffset()) );
		}

		return NULL;
	}

	HSCRIPT GetPropEntityArray( HSCRIPT hEnt, const char *pszPropName, int iArrayElement )
	{
		CBaseEntity *pEnt = ToEnt( hEnt );
		auto *pProp = GetPropByName( pEnt, pszPropName );
		if (pProp && pProp->GetType() == DPT_Int)
		{
			return ToHScript( ((CHandle<CBaseEntity>*)((char *)pEnt + pProp->GetOffset()))[iArrayElement] );
		}

		return NULL;
	}

	const char *GetPropString( HSCRIPT hEnt, const char *pszPropName )
	{
		CBaseEntity *pEnt = ToEnt( hEnt );
		auto *pProp = GetPropByName( pEnt, pszPropName );
		if (pProp && pProp->GetType() == DPT_Int)
		{
			return (const char*)((char *)pEnt + pProp->GetOffset());
		}

		return NULL;
	}

	const char *GetPropStringArray( HSCRIPT hEnt, const char *pszPropName, int iArrayElement )
	{
		CBaseEntity *pEnt = ToEnt( hEnt );
		auto *pProp = GetPropByName( pEnt, pszPropName );
		if (pProp && pProp->GetType() == DPT_Int)
		{
			return ((const char**)((char *)pEnt + pProp->GetOffset()))[iArrayElement];
		}

		return NULL;
	}

	const char *GetPropType( HSCRIPT hEnt, const char *pszPropName )
	{
		CBaseEntity *pEnt = ToEnt( hEnt );
		auto *pProp = GetPropByName( pEnt, pszPropName );
		if (pProp)
		{
			switch (pProp->GetType())
			{
			case DPT_Int:		return "integer";
			case DPT_Float:		return "float";
			case DPT_Vector:	return "vector";
			case DPT_VectorXY:	return "vector2d";
			case DPT_String:	return "string";
			case DPT_Array:		return "array";
			case DPT_DataTable:	return "datatable";
			}
		}

		return NULL;
	}

	bool HasProp( HSCRIPT hEnt, const char *pszPropName )
	{
		CBaseEntity *pEnt = ToEnt( hEnt );
		return GetPropByName( pEnt, pszPropName ) != NULL;
	}

	#define SetPropFunc( name, varType, propType ) \
	void name( HSCRIPT hEnt, const char *pszPropName, varType value ) \
	{ \
		CBaseEntity *pEnt = ToEnt( hEnt ); \
		auto *pProp = GetPropByName( pEnt, pszPropName ); \
		if (pProp && pProp->GetType() == propType) \
		{ \
			*(varType*)((char *)pEnt + pProp->GetOffset()) = value; \
		} \
	} \

	#define SetPropFuncArray( name, varType, propType ) \
	void name( HSCRIPT hEnt, const char *pszPropName, varType value, int iArrayElement ) \
	{ \
		CBaseEntity *pEnt = ToEnt( hEnt ); \
		auto *pProp = GetPropByName( pEnt, pszPropName ); \
		if (pProp && pProp->GetType() == propType) \
		{ \
			((varType*)((char *)pEnt + pProp->GetOffset()))[iArrayElement] = value; \
		} \
	} \

	SetPropFunc( SetPropFloat, float, DPT_Float );
	SetPropFuncArray( SetPropFloatArray, float, DPT_Float );
	SetPropFunc( SetPropInt, int, DPT_Int );
	SetPropFuncArray( SetPropIntArray, int, DPT_Int );
	SetPropFunc( SetPropVector, Vector, DPT_Vector );
	SetPropFuncArray( SetPropVectorArray, Vector, DPT_Vector );
	SetPropFunc( SetPropString, const char*, DPT_String );
	SetPropFuncArray( SetPropStringArray, const char*, DPT_String );

	void SetPropEntity( HSCRIPT hEnt, const char *pszPropName, HSCRIPT value )
	{
		CBaseEntity *pEnt = ToEnt( hEnt );
		auto *pProp = GetPropByName( pEnt, pszPropName );
		if (pProp && pProp->GetType() == DPT_Int)
		{
			*((CHandle<CBaseEntity>*)((char *)pEnt + pProp->GetOffset())) = ToEnt(value);
		}
	}

	HSCRIPT SetPropEntityArray( HSCRIPT hEnt, const char *pszPropName, HSCRIPT value, int iArrayElement )
	{
		CBaseEntity *pEnt = ToEnt( hEnt );
		auto *pProp = GetPropByName( pEnt, pszPropName );
		if (pProp && pProp->GetType() == DPT_Int)
		{
			((CHandle<CBaseEntity>*)((char *)pEnt + pProp->GetOffset()))[iArrayElement] = ToEnt(value);
		}

		return NULL;
	}

private:
} g_ScriptNetPropManager;

BEGIN_SCRIPTDESC_ROOT_NAMED( CScriptNetPropManager, "CNetPropManager", SCRIPT_SINGLETON "Allows reading and updating the network properties of an entity." )
	DEFINE_SCRIPTFUNC( GetPropArraySize, "Returns the size of an netprop array, or -1." )
	DEFINE_SCRIPTFUNC( GetPropEntity, "Reads an EHANDLE valued netprop (21 bit integer). Returns the script handle of the entity." )
	DEFINE_SCRIPTFUNC( GetPropEntityArray, "Reads an EHANDLE valued netprop (21 bit integer) from an array. Returns the script handle of the entity." )
	DEFINE_SCRIPTFUNC( GetPropFloat, "Reads a float valued netprop." )
	DEFINE_SCRIPTFUNC( GetPropFloatArray, "Reads a float valued netprop from an array." )
	DEFINE_SCRIPTFUNC( GetPropInt, "Reads an integer valued netprop." )
	DEFINE_SCRIPTFUNC( GetPropIntArray, "Reads an integer valued netprop from an array." )
	DEFINE_SCRIPTFUNC( GetPropString, "Reads a string valued netprop." )
	DEFINE_SCRIPTFUNC( GetPropStringArray, "Reads a string valued netprop from an array." )
	DEFINE_SCRIPTFUNC( GetPropVector, "Reads a 3D vector valued netprop." )
	DEFINE_SCRIPTFUNC( GetPropVectorArray, "Reads a 3D vector valued netprop from an array." )
	DEFINE_SCRIPTFUNC( GetPropType, "Returns the name of the netprop type as a string." )
	DEFINE_SCRIPTFUNC( HasProp, "Checks if a netprop exists." )
	DEFINE_SCRIPTFUNC( SetPropEntity, "Sets an EHANDLE valued netprop (21 bit integer) to reference the specified entity." )
	DEFINE_SCRIPTFUNC( SetPropEntityArray, "Sets an EHANDLE valued netprop (21 bit integer) from an array to reference the specified entity." )
	DEFINE_SCRIPTFUNC( SetPropFloat, "Sets a netprop to the specified float." )
	DEFINE_SCRIPTFUNC( SetPropFloatArray, "Sets a netprop from an array to the specified float." )
	DEFINE_SCRIPTFUNC( SetPropInt, "Sets a netprop to the specified integer." )
	DEFINE_SCRIPTFUNC( SetPropIntArray, "Sets a netprop from an array to the specified integer." )
	DEFINE_SCRIPTFUNC( SetPropString, "Sets a netprop to the specified string." )
	DEFINE_SCRIPTFUNC( SetPropStringArray, "Sets a netprop from an array to the specified string." )
	DEFINE_SCRIPTFUNC( SetPropVector, "Sets a netprop to the specified vector." )
	DEFINE_SCRIPTFUNC( SetPropVectorArray, "Sets a netprop from an array to the specified vector." )
END_SCRIPTDESC();

//=============================================================================
// Game Event Listener
// Based on Source 2 API
//=============================================================================
class CScriptGameEventListener : public IGameEventListener2, public CAutoGameSystem
{
public:
	CScriptGameEventListener() : m_bActive(false) {}
	~CScriptGameEventListener()
	{
		StopListeningForEvent();
	}

	intptr_t ListenToGameEvent( const char* szEvent, HSCRIPT hFunc, const char* szContext );
	void StopListeningForEvent();

public:
	static bool StopListeningToGameEvent( intptr_t listener );
	static void StopListeningToAllGameEvents( const char* szContext );

public:
	void FireGameEvent( IGameEvent *event );
	void LevelShutdownPreEntity();

private:
	bool m_bActive;
	unsigned int m_iContextHash;
	HSCRIPT m_hCallback;

	static StringHashFunctor Hash;
	static inline unsigned int HashContext( const char* c ) { return (c && *c) ? Hash(c) : 0; }

public:
	static void DumpEventListeners();
#ifndef CLIENT_DLL
	static void LoadAllEvents();
	static void LoadEventsFromFile( const char *filename, const char *pathID = NULL );
	static void WriteEventData( IGameEvent *event, HSCRIPT hTable );
#endif // !CLIENT_DLL

public:
#ifndef CLIENT_DLL
	static CUtlMap< unsigned int, KeyValues* > s_GameEvents;
#endif
	static CUtlVectorAutoPurge< CScriptGameEventListener* > s_GameEventListeners;
	static CUtlVector< KeyValues* > s_LoadedFiles;
};

#ifndef CLIENT_DLL
CUtlMap< unsigned int, KeyValues* > CScriptGameEventListener::s_GameEvents( DefLessFunc(unsigned int) );
#endif
CUtlVectorAutoPurge< CScriptGameEventListener* > CScriptGameEventListener::s_GameEventListeners;
CUtlVector< KeyValues* > CScriptGameEventListener::s_LoadedFiles;
StringHashFunctor CScriptGameEventListener::Hash;

#if _DEBUG
#ifdef CLIENT_DLL
CON_COMMAND_F( cl_dump_script_game_event_listeners, "Dump all game event listeners created from script.", FCVAR_CHEAT )
{
	CScriptGameEventListener::DumpEventListeners();
}
#else // GAME_DLL
CON_COMMAND_F( dump_script_game_event_listeners, "Dump all game event listeners created from script.", FCVAR_CHEAT )
{
	CScriptGameEventListener::DumpEventListeners();
}
#endif // CLIENT_DLL
#endif

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CScriptGameEventListener::DumpEventListeners()
{
	CGMsg( 0, CON_GROUP_VSCRIPT, "--- Script game event listener dump start\n" );
	FOR_EACH_VEC( s_GameEventListeners, i )
	{
		CGMsg( 0, CON_GROUP_VSCRIPT, " %d   (0x%p) %d : %d\n", i,s_GameEventListeners[i],
										s_GameEventListeners[i],
										s_GameEventListeners[i]->m_iContextHash );
	}
	CGMsg( 0, CON_GROUP_VSCRIPT, "--- Script game event listener dump end\n" );
}

void CScriptGameEventListener::FireGameEvent( IGameEvent *event )
{
	ScriptVariant_t hTable;
	g_pScriptVM->CreateTable( hTable );
	// TODO: pass event data on client
#ifdef GAME_DLL
	WriteEventData( event, hTable );
#endif
	g_pScriptVM->SetValue( hTable, "game_event_listener", reinterpret_cast<intptr_t>(this) ); // POINTER_TO_INT
	// g_pScriptVM->SetValue( hTable, "game_event_name", event->GetName() );
	g_pScriptVM->ExecuteFunction( m_hCallback, &hTable, 1, NULL, NULL, true );
	g_pScriptVM->ReleaseScript( hTable );
}

void CScriptGameEventListener::LevelShutdownPreEntity()
{
	s_GameEventListeners.FindAndFastRemove(this);
	delete this;
}

//-----------------------------------------------------------------------------
// Executed in LevelInitPreEntity
//-----------------------------------------------------------------------------
#ifndef CLIENT_DLL
void CScriptGameEventListener::LoadAllEvents()
{
	// Listed in the same order they are loaded in GameEventManager
	const char *filenames[] =
	{
		"resource/serverevents.res",
		"resource/gameevents.res",
		"resource/mapbaseevents.res",
		"resource/modevents.res"
	};

	const char *pathlist[] =
	{
		"GAME",
		"MOD"
	};

	// Destroy old KeyValues
	if ( s_LoadedFiles.Count() )
	{
		for ( int i = s_LoadedFiles.Count(); i--; )
			s_LoadedFiles[i]->deleteThis();
		s_LoadedFiles.Purge();
		s_GameEvents.Purge();
	}

	for ( int j = 0; j < ARRAYSIZE(pathlist); ++j )
		for ( int i = 0; i < ARRAYSIZE(filenames); ++i )
		{
			LoadEventsFromFile( filenames[i], pathlist[j] );
		}
}

//-----------------------------------------------------------------------------
// Load event files into a lookup array to be able to return the event data to the VM.
//-----------------------------------------------------------------------------
void CScriptGameEventListener::LoadEventsFromFile( const char *filename, const char *pathID )
{
	KeyValues *pKV = new KeyValues("GameEvents");

	if ( !pKV->LoadFromFile( filesystem, filename, pathID ) )
	{
		// CGMsg( 1, CON_GROUP_VSCRIPT, "CScriptGameEventListener::LoadEventsFromFile: Failed to load file [%s]%s\n", pathID, filename );
		pKV->deleteThis();
		return;
	}

	// Set the key value types to what they are from their string description values to read the correct data type in WriteEventData.
	// There might be a better way of doing this, but this is okay since it's only done on file load.
	for ( KeyValues *key = pKV->GetFirstSubKey(); key; key = key->GetNextKey() )
	{
		for ( KeyValues *sub = key->GetFirstSubKey(); sub; sub = sub->GetNextKey() )
		{
			if ( sub->GetDataType() == KeyValues::TYPE_STRING )
			{
				const char *szVal = sub->GetString();
				if ( !V_stricmp( szVal, "byte" ) || !V_stricmp( szVal, "short" ) || !V_stricmp( szVal, "long" ) || !V_stricmp( szVal, "bool" ) )
				{
					sub->SetInt( NULL, 0 );
				}
				else if ( !V_stricmp( szVal, "float" ) )
				{
					sub->SetFloat( NULL, 0.0f );
				}
			}
			// none   : value is not networked
			// string : a zero terminated string
			// bool   : unsigned int, 1 bit
			// byte   : unsigned int, 8 bit
			// short  : signed int, 16 bit
			// long   : signed int, 32 bit
			// float  : float, 32 bit
		}

		// Store event subkeys
		// Replace key so modevents can overwrite gameevents.
		// It does not check for hash collisions, however.
		s_GameEvents.InsertOrReplace( Hash( key->GetName() ), key );
	}

	// Store files (allocated KV)
	s_LoadedFiles.AddToTail( pKV );

	CGMsg( 2, CON_GROUP_VSCRIPT, "CScriptGameEventListener::LoadEventsFromFile: Loaded [%s]%s\n", pathID, filename );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CScriptGameEventListener::WriteEventData( IGameEvent *event, HSCRIPT hTable )
{
	int i = s_GameEvents.Find( Hash( event->GetName() ) );
	if ( i != s_GameEvents.InvalidIndex() )
	{
		KeyValues *pKV = s_GameEvents[i];
		for ( KeyValues *sub = pKV->GetFirstSubKey(); sub; sub = sub->GetNextKey() )
		{
			const char *szKey = sub->GetName();
			switch ( sub->GetDataType() )
			{
				case KeyValues::TYPE_STRING: g_pScriptVM->SetValue( hTable, szKey, event->GetString( szKey ) ); break;
				case KeyValues::TYPE_INT:    g_pScriptVM->SetValue( hTable, szKey, event->GetInt   ( szKey ) ); break;
				case KeyValues::TYPE_FLOAT:  g_pScriptVM->SetValue( hTable, szKey, event->GetFloat ( szKey ) ); break;
				// default: DevWarning( 2, "CScriptGameEventListener::WriteEventData: unknown data type '%d' on key '%s' in event '%s'\n", sub->GetDataType(), szKey, szEvent );
			}
		}
	}
}
#endif // !CLIENT_DLL

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
intptr_t CScriptGameEventListener::ListenToGameEvent( const char* szEvent, HSCRIPT hFunc, const char* szContext )
{
	bool bValid;

	if ( gameeventmanager && hFunc )
#ifdef CLIENT_DLL
		bValid = gameeventmanager->AddListener( this, szEvent, false );
#else
		bValid = gameeventmanager->AddListener( this, szEvent, true );
#endif
	else bValid = false;

	if ( bValid )
	{
		m_iContextHash = HashContext( szContext );
		// NOTE (port): final Mapbase form; our final vscript.lib only lends HSCRIPT parameters
		// for the duration of the call, so the callback has to be copied to be kept.
		m_hCallback = g_pScriptVM->CopyObject( hFunc );
		m_bActive = true;

		s_GameEventListeners.AddToTail( this );

		return reinterpret_cast<intptr_t>( this ); // POINTER_TO_INT
	}
	else
	{
		delete this;
		return 0x0;
	}
}

//-----------------------------------------------------------------------------
// Free stuff. Called from the destructor, does not remove itself from the listener list.
//-----------------------------------------------------------------------------
void CScriptGameEventListener::StopListeningForEvent()
{
	if ( !m_bActive )
		return;

	if ( g_pScriptVM )
	{
		g_pScriptVM->ReleaseScript( m_hCallback );
	}
	else
	{
		// AssertMsg( !m_hCallback, "LEAK (0x%p)\n", (void*)m_hCallback );
	}

	m_hCallback = NULL;
	m_bActive = false;

	if ( gameeventmanager )
		gameeventmanager->RemoveListener( this );
}

//-----------------------------------------------------------------------------
// Stop the specified event listener.
//-----------------------------------------------------------------------------
bool CScriptGameEventListener::StopListeningToGameEvent( intptr_t listener )
{
	CScriptGameEventListener *p = reinterpret_cast<CScriptGameEventListener*>(listener); // INT_TO_POINTER	

	bool bRemoved = s_GameEventListeners.FindAndFastRemove(p);
	if ( bRemoved )
	{
		delete p;
	}

	return bRemoved;
}

//-----------------------------------------------------------------------------
// Stops listening to all events within a context.
//-----------------------------------------------------------------------------
void CScriptGameEventListener::StopListeningToAllGameEvents( const char* szContext )
{
	unsigned int hash = HashContext( szContext );

	// Iterate from the end so they can be safely removed as they are deleted
	for ( int i = s_GameEventListeners.Count(); i--; )
	{
		CScriptGameEventListener *pCur = s_GameEventListeners[i];
		if ( pCur->m_iContextHash == hash )
		{
			s_GameEventListeners.Remove(i); // keep list order
			delete pCur;
		}
	}
}

//=============================================================================
//=============================================================================

static int ListenToGameEvent( const char* szEvent, HSCRIPT hFunc, const char* szContext )
{
	CScriptGameEventListener *p = new CScriptGameEventListener();
	return p->ListenToGameEvent( szEvent, hFunc, szContext );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static void FireGameEvent( const char* szEvent, HSCRIPT hTable )
{
	IGameEvent *event = gameeventmanager->CreateEvent( szEvent );
	if ( event )
	{
		ScriptVariant_t key, val;
		int nIterator = -1;
		while ( ( nIterator = g_pScriptVM->GetKeyValue( hTable, nIterator, &key, &val ) ) != -1 )
		{
			switch ( val.m_type )
			{
				case FIELD_FLOAT:   event->SetFloat ( key.m_pszString, val.m_float     ); break;
				case FIELD_INTEGER: event->SetInt   ( key.m_pszString, val.m_int       ); break;
				case FIELD_BOOLEAN: event->SetBool  ( key.m_pszString, val.m_bool      ); break;
				case FIELD_CSTRING: event->SetString( key.m_pszString, val.m_pszString ); break;
			}

			g_pScriptVM->ReleaseValue(key);
			g_pScriptVM->ReleaseValue(val);
		}

#ifdef CLIENT_DLL
		gameeventmanager->FireEventClientSide(event);
#else
		gameeventmanager->FireEvent(event);
#endif
	}
}

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Copy of FireGameEvent, server only with no broadcast to clients.
//-----------------------------------------------------------------------------
static void FireGameEventLocal( const char* szEvent, HSCRIPT hTable )
{
	IGameEvent *event = gameeventmanager->CreateEvent( szEvent );
	if ( event )
	{
		ScriptVariant_t key, val;
		int nIterator = -1;
		while ( ( nIterator = g_pScriptVM->GetKeyValue( hTable, nIterator, &key, &val ) ) != -1 )
		{
			switch ( val.m_type )
			{
				case FIELD_FLOAT:   event->SetFloat ( key.m_pszString, val.m_float     ); break;
				case FIELD_INTEGER: event->SetInt   ( key.m_pszString, val.m_int       ); break;
				case FIELD_BOOLEAN: event->SetBool  ( key.m_pszString, val.m_bool      ); break;
				case FIELD_CSTRING: event->SetString( key.m_pszString, val.m_pszString ); break;
			}

			g_pScriptVM->ReleaseValue(key);
			g_pScriptVM->ReleaseValue(val);
		}

		gameeventmanager->FireEvent(event,true);
	}
}
#endif // !CLIENT_DLL

//=============================================================================
// Save/Restore Utility
// Based on L4D2 API
//=============================================================================
class CScriptSaveRestoreUtil : public CAutoGameSystem
{
public:
	static void SaveTable( const char *szId, HSCRIPT hTable );
	static void RestoreTable( const char *szId, HSCRIPT hTable );
	static void ClearSavedTable( const char *szId );

public: // IGameSystem

	void OnSave()
	{
		if ( g_pScriptVM )
		{
			HSCRIPT hFunc = g_pScriptVM->LookupFunction( "OnSave" );
			if ( hFunc )
			{
				g_pScriptVM->Call( hFunc );
				g_pScriptVM->ReleaseScript( hFunc );
			}
		}
	}

	void OnRestore()
	{
		if ( g_pScriptVM )
		{
			HSCRIPT hFunc = g_pScriptVM->LookupFunction( "OnRestore" );
			if ( hFunc )
			{
				g_pScriptVM->Call( hFunc );
				g_pScriptVM->ReleaseScript( hFunc );
			}
		}
	}

	void Shutdown()
	{
		FOR_EACH_MAP_FAST( m_Lookup, i )
			m_Lookup[i]->deleteThis();
		m_Lookup.Purge();
	}

private:
	static StringHashFunctor Hash;
	static CUtlMap< unsigned int, KeyValues* > m_Lookup;

} g_ScriptSaveRestoreUtil;

CUtlMap< unsigned int, KeyValues* > CScriptSaveRestoreUtil::m_Lookup( DefLessFunc(unsigned int) );
StringHashFunctor CScriptSaveRestoreUtil::Hash;

//-----------------------------------------------------------------------------
// Store a table with primitive values that will persist across level transitions and save loads.
// Case sensitive
//-----------------------------------------------------------------------------
void CScriptSaveRestoreUtil::SaveTable( const char *szId, HSCRIPT hTable )
{
	KeyValues *pKV;
	int idx = m_Lookup.Find( Hash(szId) );
	if ( idx == m_Lookup.InvalidIndex() )
	{
		pKV = new KeyValues("ScriptSavedTable");
		m_Lookup.Insert( Hash(szId), pKV );
	}
	else
	{
		pKV = m_Lookup[idx];
		pKV->Clear();
	}

	ScriptVariant_t key, val;
	int nIterator = -1;
	while ( ( nIterator = g_pScriptVM->GetKeyValue( hTable, nIterator, &key, &val ) ) != -1 )
	{
		switch ( val.m_type )
		{
			case FIELD_FLOAT:   pKV->SetFloat ( key.m_pszString, val.m_float     ); break;
			case FIELD_INTEGER: pKV->SetInt   ( key.m_pszString, val.m_int       ); break;
			case FIELD_BOOLEAN: pKV->SetBool  ( key.m_pszString, val.m_bool      ); break;
			case FIELD_CSTRING: pKV->SetString( key.m_pszString, val.m_pszString ); break;
		}

		g_pScriptVM->ReleaseValue(key);
		g_pScriptVM->ReleaseValue(val);
	}
}

//-----------------------------------------------------------------------------
// Retrieves a table from storage. Write into input table.
//-----------------------------------------------------------------------------
void CScriptSaveRestoreUtil::RestoreTable( const char *szId, HSCRIPT hTable )
{
	int idx = m_Lookup.Find( Hash(szId) );
	if ( idx == m_Lookup.InvalidIndex() )
	{
		// DevWarning( 2, "RestoreTable could not find saved table with context '%s'\n", szId );
		return;
	}

	KeyValues *pKV = m_Lookup[idx];
	FOR_EACH_SUBKEY( pKV, key )
	{
		switch ( key->GetDataType() )
		{
			case KeyValues::TYPE_STRING: g_pScriptVM->SetValue( hTable, key->GetName(), key->GetString() ); break;
			case KeyValues::TYPE_INT:    g_pScriptVM->SetValue( hTable, key->GetName(), key->GetInt()    ); break;
			case KeyValues::TYPE_FLOAT:  g_pScriptVM->SetValue( hTable, key->GetName(), key->GetFloat()  ); break;
		}
	}
}

//-----------------------------------------------------------------------------
// Remove a saved table.
//-----------------------------------------------------------------------------
void CScriptSaveRestoreUtil::ClearSavedTable( const char *szId )
{
	int idx = m_Lookup.Find( Hash(szId) );
	if ( idx != m_Lookup.InvalidIndex() )
	{
		m_Lookup[idx]->deleteThis();
		m_Lookup.RemoveAt( idx );
	}
	else
	{
		// DevWarning( 2, "ClearSavedTable could not find saved table with context '%s'\n", szId );
	}
}

//=============================================================================
// Read/Write to File
// Based on L4D2/Source 2 API
//=============================================================================
#define SCRIPT_MAX_FILE_READ_SIZE  (16 * 1024)			// 16KB
#define SCRIPT_MAX_FILE_WRITE_SIZE (64 * 1024 * 1024)	// 64MB
#define SCRIPT_RW_PATH_ID "MOD"
#define SCRIPT_RW_FULL_PATH_FMT "vscript_io/%s"

class CScriptReadWriteFile : public CAutoGameSystem
{
	// A singleton class with all static members is used to be able to free the read string on level shutdown,
	// and register script funcs directly. Same reason applies to CScriptSaveRestoreUtil
public:
	static bool FileWrite( const char *szFile, const char *szInput );
	static const char *FileRead( const char *szFile );

	// NOTE: These two functions are new with Mapbase and have no Valve equivalent
	static bool KeyValuesWrite( const char *szFile, HSCRIPT hInput );
	static HSCRIPT KeyValuesRead( const char *szFile );

	void LevelShutdownPostEntity()
	{
		if ( m_pszReturnReadFile )
		{
			delete[] m_pszReturnReadFile;
			m_pszReturnReadFile = NULL;
		}
	}

private:
	static const char *m_pszReturnReadFile;

} g_ScriptReadWrite;

const char *CScriptReadWriteFile::m_pszReturnReadFile = NULL;

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CScriptReadWriteFile::FileWrite( const char *szFile, const char *szInput )
{
	size_t len = strlen(szInput);
	if ( len > SCRIPT_MAX_FILE_WRITE_SIZE )
	{
		DevWarning( 2, "Input is too large for a ScriptFileWrite ( %s / %d MB )\n", V_pretifymem(len,2,true), (SCRIPT_MAX_FILE_WRITE_SIZE >> 20) );
		return false;
	}

	char pszFullName[MAX_PATH];
	V_snprintf( pszFullName, sizeof(pszFullName), SCRIPT_RW_FULL_PATH_FMT, szFile );

	if ( !V_RemoveDotSlashes( pszFullName, CORRECT_PATH_SEPARATOR, true ) )
	{
		DevWarning( 2, "Invalid file location : %s\n", szFile );
		return false;
	}

	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	buf.PutString(szInput);

	int nSize = V_strlen(pszFullName) + 1;
	char *pszDir = (char*)stackalloc(nSize);
	V_memcpy( pszDir, pszFullName, nSize );
	V_StripFilename( pszDir );

	g_pFullFileSystem->CreateDirHierarchy( pszDir, SCRIPT_RW_PATH_ID );
	bool res = g_pFullFileSystem->WriteFile( pszFullName, SCRIPT_RW_PATH_ID, buf );
	buf.Purge();
	return res;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *CScriptReadWriteFile::FileRead( const char *szFile )
{
	char pszFullName[MAX_PATH];
	V_snprintf( pszFullName, sizeof(pszFullName), SCRIPT_RW_FULL_PATH_FMT, szFile );

	if ( !V_RemoveDotSlashes( pszFullName, CORRECT_PATH_SEPARATOR, true ) )
	{
		DevWarning( 2, "Invalid file location : %s\n", szFile );
		return NULL;
	}

	unsigned int size = g_pFullFileSystem->Size( pszFullName, SCRIPT_RW_PATH_ID );
	if ( size >= SCRIPT_MAX_FILE_READ_SIZE )
	{
		DevWarning( 2, "File '%s' (from '%s') is too large for a ScriptFileRead ( %s / %u bytes )\n", pszFullName, szFile, V_pretifymem(size,2,true), SCRIPT_MAX_FILE_READ_SIZE );
		return NULL;
	}

	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( !g_pFullFileSystem->ReadFile( pszFullName, SCRIPT_RW_PATH_ID, buf, SCRIPT_MAX_FILE_READ_SIZE ) )
	{
		return NULL;
	}

	// first time calling, allocate
	if ( !m_pszReturnReadFile )
		m_pszReturnReadFile = new char[SCRIPT_MAX_FILE_READ_SIZE];

	V_strncpy( const_cast<char*>(m_pszReturnReadFile), (const char*)buf.Base(), buf.Size() );
	buf.Purge();
	return m_pszReturnReadFile;
}

//-----------------------------------------------------------------------------
// Get the checksum of any file. Can be used to check the existence or validity of a file.
// Returns unsigned int as hex string.
//-----------------------------------------------------------------------------
/*
const char *CScriptReadWriteFile::CRC32_Checksum( const char *szFilename )
{
	CUtlBuffer buf( 0, 0, CUtlBuffer::READ_ONLY );
	if ( !g_pFullFileSystem->ReadFile( szFilename, NULL, buf ) )
		return NULL;

	// first time calling, allocate
	if ( !m_pszReturnCRC32 )
		m_pszReturnCRC32 = new char[9]; // 'FFFFFFFF\0'

	V_snprintf( const_cast<char*>(m_pszReturnCRC32), 9, "%X", CRC32_ProcessSingleBuffer( buf.Base(), buf.Size()-1 ) );
	buf.Purge();

	return m_pszReturnCRC32;
}
*/

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CScriptReadWriteFile::KeyValuesWrite( const char *szFile, HSCRIPT hInput )
{
	KeyValues *pKV = scriptmanager->GetKeyValuesFromScriptKV( g_pScriptVM, hInput );
	if (!pKV)
	{
		return false;
	}

	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	pKV->RecursiveSaveToFile( buf, 0 );

	if ( buf.Size() > SCRIPT_MAX_FILE_WRITE_SIZE )
	{
		DevWarning( 2, "Input is too large for a ScriptKeyValuesWrite ( %s / %d MB )\n", V_pretifymem(buf.Size(),2,true), (SCRIPT_MAX_FILE_WRITE_SIZE >> 20) );
		buf.Purge();
		return false;
	}

	char pszFullName[MAX_PATH];
	V_snprintf( pszFullName, sizeof(pszFullName), SCRIPT_RW_FULL_PATH_FMT, szFile );

	if ( !V_RemoveDotSlashes( pszFullName, CORRECT_PATH_SEPARATOR, true ) )
	{
		DevWarning( 2, "Invalid file location : %s\n", szFile );
		buf.Purge();
		return false;
	}

	int nSize = V_strlen(pszFullName) + 1;
	char *pszDir = (char*)stackalloc(nSize);
	V_memcpy( pszDir, pszFullName, nSize );
	V_StripFilename( pszDir );

	g_pFullFileSystem->CreateDirHierarchy( pszDir, SCRIPT_RW_PATH_ID );
	bool res = g_pFullFileSystem->WriteFile( pszFullName, SCRIPT_RW_PATH_ID, buf );
	buf.Purge();
	return res;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
HSCRIPT CScriptReadWriteFile::KeyValuesRead( const char *szFile )
{
	char pszFullName[MAX_PATH];
	V_snprintf( pszFullName, sizeof(pszFullName), SCRIPT_RW_FULL_PATH_FMT, szFile );

	if ( !V_RemoveDotSlashes( pszFullName, CORRECT_PATH_SEPARATOR, true ) )
	{
		DevWarning( 2, "Invalid file location : %s\n", szFile );
		return NULL;
	}

	unsigned int size = g_pFullFileSystem->Size( pszFullName, SCRIPT_RW_PATH_ID );
	if ( size >= SCRIPT_MAX_FILE_READ_SIZE )
	{
		DevWarning( 2, "File '%s' (from '%s') is too large for a ScriptKeyValuesRead ( %s / %u bytes )\n", pszFullName, szFile, V_pretifymem(size,2,true), SCRIPT_MAX_FILE_READ_SIZE );
		return NULL;
	}

	KeyValues *pKV = new KeyValues( szFile );
	if ( !pKV->LoadFromFile( g_pFullFileSystem, pszFullName, SCRIPT_RW_PATH_ID ) )
	{
		pKV->deleteThis();
		return NULL;
	}

	HSCRIPT hScript = scriptmanager->CreateScriptKeyValues( g_pScriptVM, pKV, true ); // bAllowDestruct is supposed to automatically remove the involved KV

	return hScript;
}
#undef SCRIPT_MAX_FILE_READ_SIZE
#undef SCRIPT_MAX_FILE_WRITE_SIZE
#undef SCRIPT_RW_PATH_ID
#undef SCRIPT_RW_FULL_PATH_FMT

//=============================================================================
// User Message Helper
// Based on Source 2 API
//
// NOTE (port): server-only. SendUserMessage only dispatches usermessages that are
// ALREADY registered (it raises a script exception for unknown ones) and never
// registers new ones, so this does NOT alter the network protocol / usermessage
// table. The ef7d9ccc0 redesign (Start/Send/Recieve over "ScriptMsg") is deferred until
// client VScript is ported.
//=============================================================================
#ifndef CLIENT_DLL
class CNetMsgScriptHelper
{
private:
	CRecipientFilter filter;
	bf_write message;
	byte data_msg[ MAX_USER_MSG_DATA ];

	inline void SendMsg( bf_write *bf )
	{
		bf_read buffer = bf_read();
		buffer.StartReading( message.GetData(), message.m_nDataBytes );
		bf->WriteBitsFromBuffer( &buffer, message.GetNumBitsWritten() );
		engine->MessageEnd();
	}

public:
	inline void Reset()
	{
		message.StartWriting( data_msg, sizeof(data_msg) );
		filter.Reset();
	}

	void SendUserMessage( HSCRIPT player, const char *msg, bool bReliable )
	{
		int msg_type = usermessages->LookupUserMessage(msg);
		if ( msg_type == -1 )
		{
			g_pScriptVM->RaiseException("UserMessageBegin: Unregistered message");
			return;
		}

		CBaseEntity *pPlayer = ToEnt(player);
		if ( pPlayer )
		{
			filter.AddRecipient( (CBasePlayer*)pPlayer );
		}

		if ( bReliable )
		{
			filter.MakeReliable();
		}

		SendMsg( engine->UserMessageBegin( &filter, msg_type ) );
	}

	void SendEntityMessage( HSCRIPT hEnt, bool bReliable )
	{
		CBaseEntity *entity = ToEnt(hEnt);
		if ( !entity )
		{
			g_pScriptVM->RaiseException("EntityMessageBegin: invalid entity");
			return;
		}

		SendMsg( engine->EntityMessageBegin( entity->entindex(), entity->GetServerClass(), bReliable ) );
	}

public:
	void AddRecipient( HSCRIPT player )
	{
		CBaseEntity *pPlayer = ToEnt(player);
		if ( pPlayer )
		{
			filter.AddRecipient( (CBasePlayer*)pPlayer );
		}
	}

	void AddRecipientsByPVS( const Vector &pos )
	{
		filter.AddRecipientsByPVS(pos);
	}

	void AddAllPlayers()
	{
		filter.AddAllPlayers();
	}

public:
	void WriteByte( int iValue )                    { message.WriteByte( iValue );             }
	void WriteChar( int iValue )                    { message.WriteChar( iValue );             }
	void WriteShort( int iValue )                   { message.WriteShort( iValue );            }
	void WriteWord( int iValue )                    { message.WriteWord( iValue );             }
	void WriteLong( int iValue )                    { message.WriteLong( iValue );             }
	void WriteFloat( float flValue )                { message.WriteFloat( flValue );           }
	void WriteAngle( float flValue )                { message.WriteBitAngle( flValue, 8 );     }
	void WriteCoord( float flValue )                { message.WriteBitCoord( flValue );        }
	void WriteVec3Coord( const Vector& rgflValue )  { message.WriteBitVec3Coord( rgflValue );  }
	void WriteVec3Normal( const Vector& rgflValue ) { message.WriteBitVec3Normal( rgflValue ); }
	void WriteAngles( const QAngle& rgflValue )     { message.WriteBitAngles( rgflValue );     }
	void WriteString( const char *sz )              { message.WriteString( sz );               }
	void WriteEntity( int iValue )                  { message.WriteShort( iValue );            }
	void WriteBool( bool bValue )                   { message.WriteOneBit( bValue ? 1 : 0 );   }
	void WriteEHandle( HSCRIPT hEnt )
	{
		CBaseEntity *pEnt = ToEnt( hEnt );
		long iEncodedEHandle;
		if ( pEnt )
		{
			EHANDLE hEnt = pEnt;
			int iSerialNum = hEnt.GetSerialNumber() & (1 << NUM_NETWORKED_EHANDLE_SERIAL_NUMBER_BITS) - 1;
			iEncodedEHandle = hEnt.GetEntryIndex() | (iSerialNum << MAX_EDICT_BITS);
		}
		else
		{
			iEncodedEHandle = INVALID_NETWORKED_EHANDLE_VALUE;
		}
		message.WriteLong( iEncodedEHandle );
	}

} g_ScriptNetMsg;

BEGIN_SCRIPTDESC_ROOT_NAMED( CNetMsgScriptHelper, "CNetMsg", SCRIPT_SINGLETON "NetworkMessages" )
	DEFINE_SCRIPTFUNC( Reset, "" )
	DEFINE_SCRIPTFUNC( SendUserMessage, "" )
	DEFINE_SCRIPTFUNC( SendEntityMessage, "" )
	DEFINE_SCRIPTFUNC( AddRecipient, "" )
	DEFINE_SCRIPTFUNC( AddRecipientsByPVS, "" )
	DEFINE_SCRIPTFUNC( AddAllPlayers, "" )
	DEFINE_SCRIPTFUNC( WriteByte, "" )
	DEFINE_SCRIPTFUNC( WriteChar, "" )
	DEFINE_SCRIPTFUNC( WriteShort, "" )
	DEFINE_SCRIPTFUNC( WriteWord, "" )
	DEFINE_SCRIPTFUNC( WriteLong, "" )
	DEFINE_SCRIPTFUNC( WriteFloat, "" )
	DEFINE_SCRIPTFUNC( WriteAngle, "" )
	DEFINE_SCRIPTFUNC( WriteCoord, "" )
	DEFINE_SCRIPTFUNC( WriteVec3Coord, "" )
	DEFINE_SCRIPTFUNC( WriteVec3Normal, "" )
	DEFINE_SCRIPTFUNC( WriteAngles, "" )
	DEFINE_SCRIPTFUNC( WriteString, "" )
	DEFINE_SCRIPTFUNC( WriteEntity, "" )
	DEFINE_SCRIPTFUNC( WriteEHandle, "" )
	DEFINE_SCRIPTFUNC( WriteBool, "" )
END_SCRIPTDESC();

#endif // !CLIENT_DLL

//=============================================================================
// Convar Lookup
//
// NOTE (port): Mapbase eb014cce moved this into the vscript library
// (vscript_bindings_base.cpp) and final Mapbase moved it back here as
// CScriptConvarAccessor. Our prebuilt vscript.lib is the final version, so it no
// longer provides "Convars" at all; this is the eb014cce-era library class,
// verbatim, so the script API and vscript_server.nut's
// CConvars::GetClientConvarValue work as they did at this point in history.
//=============================================================================
class CScriptConvarLookup
{
public:

	float GetFloat( const char *pszConVar )
	{
		ConVarRef cvar( pszConVar );
		return cvar.GetFloat();
	}

	int GetInt( const char *pszConVar )
	{
		ConVarRef cvar( pszConVar );
		return cvar.GetInt();
	}

	bool GetBool( const char *pszConVar )
	{
		ConVarRef cvar( pszConVar );
		return cvar.GetBool();
	}

	const char *GetStr( const char *pszConVar )
	{
		ConVarRef cvar( pszConVar );
		return cvar.GetString();
	}

	const char *GetDefaultValue( const char *pszConVar )
	{
		ConVarRef cvar( pszConVar );
		return cvar.GetDefault();
	}

	bool IsFlagSet( const char *pszConVar, int nFlags )
	{
		ConVarRef cvar( pszConVar );
		return cvar.IsFlagSet( nFlags );
	}

	void SetFloat( const char *pszConVar, float value )
	{
		SetValue( pszConVar, value );
	}

	void SetInt( const char *pszConVar, int value )
	{
		SetValue( pszConVar, value );
	}

	void SetBool( const char *pszConVar, bool value )
	{
		SetValue( pszConVar, value );
	}

	void SetStr( const char *pszConVar, const char *value )
	{
		SetValue( pszConVar, value );
	}

	template <typename T>
	void SetValue( const char *pszConVar, T value )
	{
		ConVarRef cvar( pszConVar );
		if (!cvar.IsValid())
			return;

		// FCVAR_NOT_CONNECTED can be used to protect specific convars from nefarious interference
		if (cvar.IsFlagSet(FCVAR_NOT_CONNECTED))
			return;

		cvar.SetValue( value );
	}

private:
} g_ScriptConvarLookup;

BEGIN_SCRIPTDESC_ROOT_NAMED( CScriptConvarLookup, "CConvars", SCRIPT_SINGLETON "Provides an interface for getting and setting convars." )
	DEFINE_SCRIPTFUNC( GetFloat, "Returns the convar as a float. May return null if no such convar." )
	DEFINE_SCRIPTFUNC( GetInt, "Returns the convar as an int. May return null if no such convar." )
	DEFINE_SCRIPTFUNC( GetBool, "Returns the convar as a bool. May return null if no such convar." )
	DEFINE_SCRIPTFUNC( GetStr, "Returns the convar as a string. May return null if no such convar." )
	DEFINE_SCRIPTFUNC( GetDefaultValue, "Returns the convar's default value as a string. May return null if no such convar." )
	DEFINE_SCRIPTFUNC( IsFlagSet, "Returns the convar's flags. May return null if no such convar." )
	DEFINE_SCRIPTFUNC( SetFloat, "Sets the value of the convar as a float." )
	DEFINE_SCRIPTFUNC( SetInt, "Sets the value of the convar as an int." )
	DEFINE_SCRIPTFUNC( SetBool, "Sets the value of the convar as a bool." )
	DEFINE_SCRIPTFUNC( SetStr, "Sets the value of the convar as a string." )
END_SCRIPTDESC();

#define RETURN_IF_CANNOT_DRAW_OVERLAY\
	if (engine->IsPaused())\
	{\
		CGWarning( 1, CON_GROUP_VSCRIPT, "debugoverlay: cannot draw while the game is paused!\n");\
		return;\
	}
class CDebugOverlayScriptHelper
{
public:

	void Box( const Vector &origin, const Vector &mins, const Vector &maxs, int r, int g, int b, int a, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		debugoverlay->AddBoxOverlay(origin, mins, maxs, vec3_angle, r, g, b, a, flDuration);
	}
	void BoxDirection( const Vector &origin, const Vector &mins, const Vector &maxs, const Vector &forward, int r, int g, int b, int a, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		QAngle f_angles = vec3_angle;
		f_angles.y = UTIL_VecToYaw(forward);

		debugoverlay->AddBoxOverlay(origin, mins, maxs, f_angles, r, g, b, a, flDuration);
	}
	void BoxAngles( const Vector &origin, const Vector &mins, const Vector &maxs, const QAngle &angles, int r, int g, int b, int a, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		debugoverlay->AddBoxOverlay(origin, mins, maxs, angles, r, g, b, a, flDuration);
	}
	void SweptBox( const Vector& start, const Vector& end, const Vector& mins, const Vector& maxs, const QAngle & angles, int r, int g, int b, int a, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		debugoverlay->AddSweptBoxOverlay(start, end, mins, maxs, angles, r, g, b, a, flDuration);
	}
	void EntityBounds( HSCRIPT pEntity, int r, int g, int b, int a, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		CBaseEntity *pEnt = ToEnt(pEntity);
		if (!pEnt)
			return;

		const CCollisionProperty *pCollide = pEnt->CollisionProp();
		debugoverlay->AddBoxOverlay(pCollide->GetCollisionOrigin(), pCollide->OBBMins(), pCollide->OBBMaxs(), pCollide->GetCollisionAngles(), r, g, b, a, flDuration);
	}
	void Line( const Vector &origin, const Vector &target, int r, int g, int b, bool noDepthTest, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		debugoverlay->AddLineOverlay(origin, target, r, g, b, noDepthTest, flDuration);
	}
	void Triangle( const Vector &p1, const Vector &p2, const Vector &p3, int r, int g, int b, int a, bool noDepthTest, float duration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		debugoverlay->AddTriangleOverlay(p1, p2, p3, r, g, b, a, noDepthTest, duration);
	}
	void EntityText( int entityID, int text_offset, const char *text, float flDuration, int r, int g, int b, int a )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		debugoverlay->AddEntityTextOverlay(entityID, text_offset, flDuration,
				(int)clamp(r * 255.f, 0.f, 255.f), (int)clamp(g * 255.f, 0.f, 255.f), (int)clamp(b * 255.f, 0.f, 255.f),
				(int)clamp(a * 255.f, 0.f, 255.f), text);
	}
	void EntityTextAtPosition( const Vector &origin, int text_offset, const char *text, float flDuration, int r, int g, int b, int a )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		debugoverlay->AddTextOverlayRGB(origin, text_offset, flDuration, r, g, b, a, "%s", text);
	}
	void Grid( const Vector &vPosition )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		debugoverlay->AddGridOverlay(vPosition);
	}
	void Text( const Vector &origin, const char *text, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		debugoverlay->AddTextOverlay(origin, flDuration, "%s", text);
	}
	void ScreenText( float fXpos, float fYpos, const char *text, int r, int g, int b, int a, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		debugoverlay->AddScreenTextOverlay(fXpos, fYpos, flDuration, r, g, b, a, text);
	}
	void Cross3D( const Vector &position, float size, int r, int g, int b, bool noDepthTest, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		Line( position + Vector(size,0,0), position - Vector(size,0,0), r, g, b, noDepthTest, flDuration );
		Line( position + Vector(0,size,0), position - Vector(0,size,0), r, g, b, noDepthTest, flDuration );
		Line( position + Vector(0,0,size), position - Vector(0,0,size), r, g, b, noDepthTest, flDuration );
	}
	void Cross3DOriented( const Vector &position, const QAngle &angles, float size, int r, int g, int b, bool noDepthTest, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		Vector forward, right, up;
		AngleVectors( angles, &forward, &right, &up );

		forward *= size;
		right *= size;
		up *= size;

		Line( position + right, position - right, r, g, b, noDepthTest, flDuration );
		Line( position + forward, position - forward, r, g, b, noDepthTest, flDuration );
		Line( position + up, position - up, r, g, b, noDepthTest, flDuration );
	}
	void DrawTickMarkedLine( const Vector &startPos, const Vector &endPos, float tickDist, int tickTextDist, int r, int g, int b, bool noDepthTest, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		Vector	lineDir = (endPos - startPos);
		float	lineDist = VectorNormalize(lineDir);
		int		numTicks = lineDist / tickDist;

		Vector  upVec = Vector(0,0,4);
		Vector	sideDir;
		Vector	tickPos = startPos;
		int		tickTextCnt = 0;

		CrossProduct(lineDir, upVec, sideDir);

		Line(startPos, endPos, r, g, b, noDepthTest, flDuration);

		for (int i = 0; i<numTicks + 1; i++)
		{
			Vector tickLeft = tickPos - sideDir;
			Vector tickRight = tickPos + sideDir;

			if (tickTextCnt == tickTextDist)
			{
				char text[25];
				Q_snprintf(text, sizeof(text), "%i", i);
				Vector textPos = tickLeft + Vector(0, 0, 8);
				Line(tickLeft, tickRight, 255, 255, 255, noDepthTest, flDuration);
				Text(textPos, text, flDuration);
				tickTextCnt = 0;
			}
			else
			{
				Line(tickLeft, tickRight, r, g, b, noDepthTest, flDuration);
			}

			tickTextCnt++;

			tickPos = tickPos + (tickDist * lineDir);
		}
	}
	void HorzArrow( const Vector &startPos, const Vector &endPos, float width, int r, int g, int b, int a, bool noDepthTest, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		Vector	lineDir		= (endPos - startPos);
		VectorNormalize( lineDir );
		Vector  upVec		= Vector( 0, 0, 1 );
		Vector	sideDir;
		float   radius		= width / 2.0;

		CrossProduct(lineDir, upVec, sideDir);

		Vector p1 =	startPos - sideDir * radius;
		Vector p2 = endPos - lineDir * width - sideDir * radius;
		Vector p3 = endPos - lineDir * width - sideDir * width;
		Vector p4 = endPos;
		Vector p5 = endPos - lineDir * width + sideDir * width;
		Vector p6 = endPos - lineDir * width + sideDir * radius;
		Vector p7 =	startPos + sideDir * radius;

		Line(p1, p2, r,g,b,noDepthTest,flDuration);
		Line(p2, p3, r,g,b,noDepthTest,flDuration);
		Line(p3, p4, r,g,b,noDepthTest,flDuration);
		Line(p4, p5, r,g,b,noDepthTest,flDuration);
		Line(p5, p6, r,g,b,noDepthTest,flDuration);
		Line(p6, p7, r,g,b,noDepthTest,flDuration);

		if ( a > 0 )
		{
			Triangle( p5, p4, p3, r, g, b, a, noDepthTest, flDuration );
			Triangle( p1, p7, p6, r, g, b, a, noDepthTest, flDuration );
			Triangle( p6, p2, p1, r, g, b, a, noDepthTest, flDuration );

			Triangle( p3, p4, p5, r, g, b, a, noDepthTest, flDuration );
			Triangle( p6, p7, p1, r, g, b, a, noDepthTest, flDuration );
			Triangle( p1, p2, p6, r, g, b, a, noDepthTest, flDuration );
		}
	}
	void YawArrow( const Vector &startPos, float yaw, float length, float width, int r, int g, int b, int a, bool noDepthTest, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		Vector forward = UTIL_YawToVector( yaw );
		HorzArrow( startPos, startPos + forward * length, width, r, g, b, a, noDepthTest, flDuration );
	}
	void VertArrow( const Vector &startPos, const Vector &endPos, float width, int r, int g, int b, int a, bool noDepthTest, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		Vector	lineDir		= (endPos - startPos);
		VectorNormalize( lineDir );
		Vector  upVec;
		Vector	sideDir;
		float   radius		= width / 2.0;

		VectorVectors( lineDir, sideDir, upVec );

		Vector p1 =	startPos - upVec * radius;
		Vector p2 = endPos - lineDir * width - upVec * radius;
		Vector p3 = endPos - lineDir * width - upVec * width;
		Vector p4 = endPos;
		Vector p5 = endPos - lineDir * width + upVec * width;
		Vector p6 = endPos - lineDir * width + upVec * radius;
		Vector p7 =	startPos + upVec * radius;

		Line(p1, p2, r,g,b,noDepthTest,flDuration);
		Line(p2, p3, r,g,b,noDepthTest,flDuration);
		Line(p3, p4, r,g,b,noDepthTest,flDuration);
		Line(p4, p5, r,g,b,noDepthTest,flDuration);
		Line(p5, p6, r,g,b,noDepthTest,flDuration);
		Line(p6, p7, r,g,b,noDepthTest,flDuration);

		if ( a > 0 )
		{
			Triangle( p5, p4, p3, r, g, b, a, noDepthTest, flDuration );
			Triangle( p1, p7, p6, r, g, b, a, noDepthTest, flDuration );
			Triangle( p6, p2, p1, r, g, b, a, noDepthTest, flDuration );

			Triangle( p3, p4, p5, r, g, b, a, noDepthTest, flDuration );
			Triangle( p6, p7, p1, r, g, b, a, noDepthTest, flDuration );
			Triangle( p1, p2, p6, r, g, b, a, noDepthTest, flDuration );
		}
	}
	void Axis( const Vector &position, const QAngle &angles, float size, bool noDepthTest, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		Vector xvec, yvec, zvec;
		AngleVectors( angles, &xvec, &yvec, &zvec );

		xvec = position + (size * xvec);
		yvec = position - (size * yvec);
		zvec = position + (size * zvec);

		Line( position, xvec, 255, 0, 0, noDepthTest, flDuration );
		Line( position, yvec, 0, 255, 0, noDepthTest, flDuration );
		Line( position, zvec, 0, 0, 255, noDepthTest, flDuration );
	}
	void Sphere( const Vector &center, float radius, int r, int g, int b, bool noDepthTest, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		Vector edge, lastEdge;

		float axisSize = radius;
		Line( center + Vector( 0, 0, -axisSize ), center + Vector( 0, 0, axisSize ), r, g, b, noDepthTest, flDuration );
		Line( center + Vector( 0, -axisSize, 0 ), center + Vector( 0, axisSize, 0 ), r, g, b, noDepthTest, flDuration );
		Line( center + Vector( -axisSize, 0, 0 ), center + Vector( axisSize, 0, 0 ), r, g, b, noDepthTest, flDuration );

		lastEdge = Vector( radius + center.x, center.y, center.z );
		float angle;
		for( angle=0.0f; angle <= 360.0f; angle += 22.5f )
		{
			edge.x = radius * cosf( angle / 180.0f * M_PI ) + center.x;
			edge.y = center.y;
			edge.z = radius * sinf( angle / 180.0f * M_PI ) + center.z;

			Line( edge, lastEdge, r, g, b, noDepthTest, flDuration );

			lastEdge = edge;
		}

		lastEdge = Vector( center.x, radius + center.y, center.z );
		for( angle=0.0f; angle <= 360.0f; angle += 22.5f )
		{
			edge.x = center.x;
			edge.y = radius * cosf( angle / 180.0f * M_PI ) + center.y;
			edge.z = radius * sinf( angle / 180.0f * M_PI ) + center.z;

			Line( edge, lastEdge, r, g, b, noDepthTest, flDuration );

			lastEdge = edge;
		}

		lastEdge = Vector( center.x, radius + center.y, center.z );
		for( angle=0.0f; angle <= 360.0f; angle += 22.5f )
		{
			edge.x = radius * cosf( angle / 180.0f * M_PI ) + center.x;
			edge.y = radius * sinf( angle / 180.0f * M_PI ) + center.y;
			edge.z = center.z;

			Line( edge, lastEdge, r, g, b, noDepthTest, flDuration );

			lastEdge = edge;
		}
	}
	void CircleOriented( const Vector &position, const QAngle &angles, float radius, int r, int g, int b, int a, bool bNoDepthTest, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		matrix3x4_t xform;
		AngleMatrix(angles, position, xform);
		Vector xAxis, yAxis;
		MatrixGetColumn(xform, 2, xAxis);
		MatrixGetColumn(xform, 1, yAxis);
		Circle(position, xAxis, yAxis, radius, r, g, b, a, bNoDepthTest, flDuration);
	}
	void Circle( const Vector &position, const Vector &xAxis, const Vector &yAxis, float radius, int r, int g, int b, int a, bool bNoDepthTest, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		const unsigned int nSegments = 16;
		const float flRadStep = (M_PI*2.0f) / (float) nSegments;

		Vector vecLastPosition;
		Vector vecStart = position + xAxis * radius;
		Vector vecPosition = vecStart;

		for ( int i = 1; i <= nSegments; i++ )
		{
			vecLastPosition = vecPosition;

			float flSin, flCos;
			SinCos( flRadStep*i, &flSin, &flCos );
			vecPosition = position + (xAxis * flCos * radius) + (yAxis * flSin * radius);

			Line( vecLastPosition, vecPosition, r, g, b, bNoDepthTest, flDuration );

			if ( a && i > 1 )
			{		
				debugoverlay->AddTriangleOverlay( vecStart, vecLastPosition, vecPosition, r, g, b, a, bNoDepthTest, flDuration );
			}
		}
	}
#ifndef CLIENT_DLL
	void SetDebugBits( HSCRIPT hEntity, int bit ) // DebugOverlayBits_t
	{
		CBaseEntity *pEnt = ToEnt(hEntity);
		if (!pEnt)
			return;

		if (pEnt->m_debugOverlays & bit)
		{
			pEnt->m_debugOverlays &= ~bit;
		}
		else
		{
			pEnt->m_debugOverlays |= bit;

#ifdef AI_MONITOR_FOR_OSCILLATION
			if (pEnt->IsNPC())
			{
				pEnt->MyNPCPointer()->m_ScheduleHistory.RemoveAll();
			}
#endif//AI_MONITOR_FOR_OSCILLATION
		}
	}
#endif
	void ClearAllOverlays()
	{
#ifndef CLIENT_DLL
		// Clear all entities of their debug overlays
		for (CBaseEntity *pEntity = gEntList.FirstEnt(); pEntity; pEntity = gEntList.NextEnt(pEntity))
		{
			pEntity->m_debugOverlays = 0;
		}
#endif

		debugoverlay->ClearAllOverlays();
	}

private:
} g_ScriptDebugOverlay;

BEGIN_SCRIPTDESC_ROOT( CDebugOverlayScriptHelper, SCRIPT_SINGLETON "CDebugOverlayScriptHelper" )
	DEFINE_SCRIPTFUNC( Box, "Draws a world-space axis-aligned box. Specify bounds in world space." )
	DEFINE_SCRIPTFUNC( BoxDirection, "Draw box oriented to a Vector direction" )
	DEFINE_SCRIPTFUNC( BoxAngles, "Draws an oriented box at the origin. Specify bounds in local space." )
	DEFINE_SCRIPTFUNC( SweptBox, "Draws a swept box. Specify endpoints in world space and the bounds in local space." )
	DEFINE_SCRIPTFUNC( EntityBounds, "Draws bounds of an entity" )
	DEFINE_SCRIPTFUNC( Line, "Draws a line between two points" )
	DEFINE_SCRIPTFUNC( Triangle, "Draws a filled triangle. Specify vertices in world space." )
	DEFINE_SCRIPTFUNC( EntityText, "Draws text on an entity" )
	DEFINE_SCRIPTFUNC( EntityTextAtPosition, "Draw entity text overlay at a specific position" )
	DEFINE_SCRIPTFUNC( Grid, "Add grid overlay" )
	DEFINE_SCRIPTFUNC( Text, "Draws 2D text. Specify origin in world space." )
	DEFINE_SCRIPTFUNC( ScreenText, "Draws 2D text. Specify coordinates in screen space." )
	DEFINE_SCRIPTFUNC( Cross3D, "Draws a world-aligned cross. Specify origin in world space." )
	DEFINE_SCRIPTFUNC( Cross3DOriented, "Draws an oriented cross. Specify origin in world space." )
	DEFINE_SCRIPTFUNC( DrawTickMarkedLine, "Draws a dashed line. Specify endpoints in world space." )
	DEFINE_SCRIPTFUNC( HorzArrow, "Draws a horizontal arrow. Specify endpoints in world space." )
	DEFINE_SCRIPTFUNC( YawArrow, "Draws a arrow associated with a specific yaw. Specify endpoints in world space." )
	DEFINE_SCRIPTFUNC( VertArrow, "Draws a vertical arrow. Specify endpoints in world space." )
	DEFINE_SCRIPTFUNC( Axis, "Draws an axis. Specify origin + orientation in world space." )
	DEFINE_SCRIPTFUNC( Sphere, "Draws a wireframe sphere. Specify center in world space." )
	DEFINE_SCRIPTFUNC( CircleOriented, "Draws a circle oriented. Specify center in world space." )
	DEFINE_SCRIPTFUNC( Circle, "Draws a circle. Specify center in world space." )
#ifndef CLIENT_DLL
	DEFINE_SCRIPTFUNC( SetDebugBits, "Set debug bits on entity" )
#endif
	DEFINE_SCRIPTFUNC( ClearAllOverlays, "Clear all debug overlays at once" )
END_SCRIPTDESC();



void RegisterScriptSingletons()
{
	ScriptRegisterFunctionNamed( g_pScriptVM, CScriptSaveRestoreUtil::SaveTable, "SaveTable", "Store a table with primitive values that will persist across level transitions and save loads." );
	ScriptRegisterFunctionNamed( g_pScriptVM, CScriptSaveRestoreUtil::RestoreTable, "RestoreTable", "Retrieves a table from storage. Write into input table." );
	ScriptRegisterFunctionNamed( g_pScriptVM, CScriptSaveRestoreUtil::ClearSavedTable, "ClearSavedTable", "Removes the table with the given context." );
	ScriptRegisterFunctionNamed( g_pScriptVM, CScriptReadWriteFile::FileWrite, "StringToFile", "Stores the string into the file" );
	ScriptRegisterFunctionNamed( g_pScriptVM, CScriptReadWriteFile::FileRead, "FileToString", "Returns the string from the file, null if no file or file is too big." );
	ScriptRegisterFunctionNamed( g_pScriptVM, CScriptReadWriteFile::KeyValuesWrite, "KeyValuesToFile", "Stores the CScriptKeyValues into the file" );
	ScriptRegisterFunctionNamed( g_pScriptVM, CScriptReadWriteFile::KeyValuesRead, "FileToKeyValues", "Returns the CScriptKeyValues from the file, null if no file or file is too big." );

	ScriptRegisterFunction( g_pScriptVM, ListenToGameEvent, "Register as a listener for a game event from script." );
	ScriptRegisterFunctionNamed( g_pScriptVM, CScriptGameEventListener::StopListeningToGameEvent, "StopListeningToGameEvent", "Stop the specified event listener." );
	ScriptRegisterFunctionNamed( g_pScriptVM, CScriptGameEventListener::StopListeningToAllGameEvents, "StopListeningToAllGameEvents", "Stop listening to all game events within a specific context." );
	ScriptRegisterFunction( g_pScriptVM, FireGameEvent, "Fire a game event." );
#ifndef CLIENT_DLL
	ScriptRegisterFunction( g_pScriptVM, FireGameEventLocal, "Fire a game event without broadcasting to the client." );
#endif

	g_pScriptVM->RegisterInstance( &g_ScriptNetPropManager, "NetProps" );
	g_pScriptVM->RegisterInstance( &g_ScriptConvarLookup, "Convars" );
#ifndef CLIENT_DLL
	g_pScriptVM->RegisterInstance( &g_ScriptNetMsg, "NetMsg" );
#endif
	g_pScriptVM->RegisterInstance( &g_ScriptDebugOverlay, "debugoverlay" );

	// Singletons not unique to VScript (not declared or defined here)
	g_pScriptVM->RegisterInstance( GameRules(), "GameRules" );
	g_pScriptVM->RegisterInstance( GetAmmoDef(), "AmmoDef" );
#ifndef CLIENT_DLL
	g_pScriptVM->RegisterInstance( &g_AI_SquadManager, "Squads" );
#endif

#ifndef CLIENT_DLL
	CScriptGameEventListener::LoadAllEvents();
#endif // !CLIENT_DLL
}
