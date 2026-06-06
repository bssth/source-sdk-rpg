//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose: This file contains brand new VScript singletons and singletons replicated from API
//			documentation in other games.
//
//			See vscript_funcs_shared.cpp for more information.
//
//			NOTE (port): split out of vscript_funcs_shared.cpp following Mapbase eb014cce
//			(v6.0). For now this only holds CScriptNetPropManager (moved here verbatim).
//			The brand-new v6.0 singletons (CScriptLocalize, CScriptGameEventListener,
//			CScriptSaveRestoreUtil, CScriptReadWriteFile, CNetMsgScriptHelper, plus the
//			AmmoDef/Squads instances) are deferred to follow-up port sub-commits because
//			they pull heavier dependencies (vgui ILocalize, usermessages, ai_squad,
//			filesystem, igameevents).
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#ifndef CLIENT_DLL
#include "ai_squad.h"
#endif // !CLIENT_DLL

#include "filesystem.h"
#include "tier1/utlbuffer.h"

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
// Save/Restore Utility
// Based on Source 2 API
//=============================================================================
class CScriptSaveRestoreUtil : public CAutoGameSystem
{
public:
	static void SaveTable( const char *szId, HSCRIPT hTable );
	static void RestoreTable( const char *szId, HSCRIPT hTable );
	static void ClearSavedTable( const char *szId );

// IGameSystem interface
public:
	void OnSave()
	{
		if ( g_pScriptVM )
		{
			HSCRIPT hFunc = g_pScriptVM->LookupFunction( "OnSave" );
			if ( hFunc )
			{
				g_pScriptVM->Call( hFunc );
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
			}
		}
	}

	void Shutdown()
	{
		FOR_EACH_VEC( m_aKeyValues, i )
			m_aKeyValues[i]->deleteThis();
		m_aKeyValues.Purge();
		m_aContext.PurgeAndDeleteElements();
	}

private:
	static int GetIndexForContext( const char *szId );

	// indices must match, always remove keeping order
	static CUtlStringList m_aContext;
	static CUtlVector<KeyValues*> m_aKeyValues;

} g_ScriptSaveRestoreUtil;

CUtlStringList CScriptSaveRestoreUtil::m_aContext;
CUtlVector<KeyValues*> CScriptSaveRestoreUtil::m_aKeyValues;

int CScriptSaveRestoreUtil::GetIndexForContext( const char *szId )
{
	int idx = -1;
	FOR_EACH_VEC( m_aContext, i )
	{
		if ( !V_stricmp( szId, m_aContext[i] ) )
		{
			idx = i;
			break;
		}
	}
	return idx;
}

//-----------------------------------------------------------------------------
// Store a table with primitive values that will persist across level transitions and save loads.
//-----------------------------------------------------------------------------
void CScriptSaveRestoreUtil::SaveTable( const char *szId, HSCRIPT hTable )
{
	int idx = GetIndexForContext(szId);

	KeyValues *pKV;

	if ( idx == -1 )
	{
		pKV = new KeyValues("ScriptSavedTable");
		m_aKeyValues.AddToTail(pKV);

		if ( V_strlen(szId) > 255 ) // arbitrary clamp
		{
			char c[256];
			V_strncpy( c, szId, sizeof(c) );
			m_aContext.CopyAndAddToTail(c);
		}
		else
		{
			m_aContext.CopyAndAddToTail(szId);
		}
	}
	else
	{
		pKV = m_aKeyValues[idx];
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
	int idx = GetIndexForContext(szId);

	KeyValues *pKV;

	if ( idx == -1 )
	{
		// DevWarning( 2, "RestoreTable could not find saved table with context '%s'\n", szId );
		return;
	}
	else
	{
		pKV = m_aKeyValues[idx];
	}

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
	int idx = GetIndexForContext(szId);

	if ( idx == -1 )
	{
		// DevWarning( 2, "ClearSavedTable could not find saved table with context '%s'\n", szId );
		return;
	}

	m_aKeyValues[idx]->deleteThis();
	m_aKeyValues.Remove(idx);

	delete[] m_aContext[idx];
	m_aContext.Remove(idx);
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
public:
	static bool ScriptFileWrite( const char *szFile, const char *szInput );
	static const char *ScriptFileRead( const char *szFile );
	//static const char *CRC32_Checksum( const char *szFilename );

	// NOTE: These two functions are new with Mapbase and have no Valve equivalent
	static bool ScriptKeyValuesWrite( const char *szFile, HSCRIPT hInput );
	static HSCRIPT ScriptKeyValuesRead( const char *szFile );

	void LevelShutdownPostEntity()
	{
		if ( m_pszReturnReadFile )
		{
			delete[] m_pszReturnReadFile;
			m_pszReturnReadFile = NULL;
		}

		//if ( m_pszReturnCRC32 )
		//{
		//	delete[] m_pszReturnCRC32;
		//	m_pszReturnCRC32 = NULL;
		//}
	}

private:
	static const char *m_pszReturnReadFile;
	//static const char *m_pszReturnCRC32;

} g_ScriptReadWrite;

const char *CScriptReadWriteFile::m_pszReturnReadFile = NULL;
//const char *CScriptReadWriteFile::m_pszReturnCRC32 = NULL;

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CScriptReadWriteFile::ScriptFileWrite( const char *szFile, const char *szInput )
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
const char *CScriptReadWriteFile::ScriptFileRead( const char *szFile )
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
//
//-----------------------------------------------------------------------------
bool CScriptReadWriteFile::ScriptKeyValuesWrite( const char *szFile, HSCRIPT hInput )
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
		DevWarning( 2, "Input is too large for a ScriptFileWrite ( %s / %d MB )\n", V_pretifymem(buf.Size(),2,true), (SCRIPT_MAX_FILE_WRITE_SIZE >> 20) );
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
HSCRIPT CScriptReadWriteFile::ScriptKeyValuesRead( const char *szFile )
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

	KeyValues *pKV = new KeyValues( szFile );
	if ( !pKV->LoadFromFile( g_pFullFileSystem, pszFullName, SCRIPT_RW_PATH_ID ) )
	{
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
//=============================================================================

void RegisterScriptSingletons()
{
	// NOTE (port): partial port of Mapbase eb014cce (v6.0) RegisterScriptSingletons.
	// Brought over: NetProps (moved), the SaveRestore table funcs, the file R/W funcs,
	// and the Squads instance. Still DEFERRED to follow-up sub-commits:
	// CScriptLocalize ("Localize", needs vgui which server.dll does not link), the
	// game-event listener funcs (ListenToGameEvent/StopListening/FireGameEvent),
	// CNetMsgScriptHelper ("NetMsg"), and the "AmmoDef" instance (CAmmoDef scriptdesc
	// is a pre-v6.0 surface this tree never ported).
	ScriptRegisterFunctionNamed( g_pScriptVM, CScriptSaveRestoreUtil::SaveTable, "SaveTable", "Store a table with primitive values that will persist across level transitions and save loads." );
	ScriptRegisterFunctionNamed( g_pScriptVM, CScriptSaveRestoreUtil::RestoreTable, "RestoreTable", "Retrieves a table from storage. Write into input table." );
	ScriptRegisterFunctionNamed( g_pScriptVM, CScriptSaveRestoreUtil::ClearSavedTable, "ClearSavedTable", "Removes the table with the given context." );
	ScriptRegisterFunctionNamed( g_pScriptVM, CScriptReadWriteFile::ScriptFileWrite, "StringToFile", "Stores the string into the file" );
	ScriptRegisterFunctionNamed( g_pScriptVM, CScriptReadWriteFile::ScriptFileRead, "FileToString", "Returns the string from the file, null if no file or file is too big." );
	ScriptRegisterFunctionNamed( g_pScriptVM, CScriptReadWriteFile::ScriptKeyValuesWrite, "KeyValuesToFile", "Stores the CScriptKeyValues into the file" );
	ScriptRegisterFunctionNamed( g_pScriptVM, CScriptReadWriteFile::ScriptKeyValuesRead, "KeyValuesToString", "Returns the CScriptKeyValues from the file, null if no file or file is too big." );

	g_pScriptVM->RegisterInstance( &g_ScriptNetPropManager, "NetProps" );

	// Singletons not unique to VScript (not declared or defined here)
#ifndef CLIENT_DLL
	g_pScriptVM->RegisterInstance( &g_AI_SquadManager, "Squads" );
#endif
}
