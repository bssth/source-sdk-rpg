//========== Copyright (c) 2008, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#include "cbase.h"
#include "vscript_shared.h"
#include "icommandline.h"
#include "tier1/utlbuffer.h"
#include "tier1/fmtstr.h"
#include "filesystem.h"
#include "characterset.h"
#include "isaverestore.h"
#include "gamerules.h"

IScriptVM * g_pScriptVM;
extern ScriptClassDesc_t * GetScriptDesc( CBaseEntity * );

// #define VMPROFILE 1

#ifdef VMPROFILE

#define VMPROF_START float debugStartTime = Plat_FloatTime();
#define VMPROF_SHOW( funcname, funcdesc  ) DevMsg("***VSCRIPT PROFILE***: %s %s: %6.4f milliseconds\n", (##funcname), (##funcdesc), (Plat_FloatTime() - debugStartTime)*1000.0 );

#else // !VMPROFILE

#define VMPROF_START
#define VMPROF_SHOW

#endif // VMPROFILE

#ifdef MAPBASE_VSCRIPT
// This is to ensure a dependency exists between the vscript library and the game DLLs
extern int vscript_token;
int vscript_token_hack = vscript_token;
#endif



HSCRIPT VScriptCompileScript( const char *pszScriptName, bool bWarnMissing )
{
	if ( !g_pScriptVM )
	{
		return NULL;
	}

	static const char *pszExtensions[] =
	{
		"",		// SL_NONE
		".gm",	// SL_GAMEMONKEY
		".nut",	// SL_SQUIRREL
		".lua", // SL_LUA
		".py",  // SL_PYTHON
	};

	const char *pszVMExtension = pszExtensions[g_pScriptVM->GetLanguage()];
	const char *pszIncomingExtension = V_strrchr( pszScriptName , '.' );
	if ( pszIncomingExtension && V_strcmp( pszIncomingExtension, pszVMExtension ) != 0 )
	{
		CGWarning( 0, CON_GROUP_VSCRIPT, "Script file type does not match VM type\n" );
		return NULL;
	}

	CFmtStr scriptPath;
	if ( pszIncomingExtension )
	{
		scriptPath.sprintf( "scripts/vscripts/%s", pszScriptName );
	}
	else
	{
		scriptPath.sprintf( "scripts/vscripts/%s%s", pszScriptName,  pszVMExtension );
	}

	const char *pBase;
	CUtlBuffer bufferScript;

	if ( g_pScriptVM->GetLanguage() == SL_PYTHON )
	{
		// python auto-loads raw or precompiled modules - don't load data here
		pBase = NULL;
	}
	else
	{
		bool bResult = filesystem->ReadFile( scriptPath, "GAME", bufferScript );

#ifdef MAPBASE_VSCRIPT
		if ( !bResult && bWarnMissing )
#else
		if( !bResult )
#endif
		{
			CGWarning( 0, CON_GROUP_VSCRIPT, "Script not found (%s) \n", scriptPath.operator const char *() );
			Assert( "Error running script" );
		}

		pBase = (const char *) bufferScript.Base();

		if ( !pBase || !*pBase )
		{
			return NULL;
		}
	}


	const char *pszFilename = V_strrchr( scriptPath, '/' );
	pszFilename++;
	HSCRIPT hScript = g_pScriptVM->CompileScript( pBase, pszFilename );
	if ( !hScript )
	{
		CGWarning( 0, CON_GROUP_VSCRIPT, "FAILED to compile and execute script file named %s\n", scriptPath.operator const char *() );
		Assert( "Error running script" );
	}
	return hScript;
}

static int g_ScriptServerRunScriptDepth;

bool VScriptRunScript( const char *pszScriptName, HSCRIPT hScope, bool bWarnMissing )
{
	if ( !g_pScriptVM )
	{
		return false;
	}

	if ( !pszScriptName || !*pszScriptName )
	{
		CGWarning( 0, CON_GROUP_VSCRIPT, "Cannot run script: NULL script name\n" );
		return false;
	}

	// Prevent infinite recursion in VM
	if ( g_ScriptServerRunScriptDepth > 16 )
	{
		CGWarning( 0, CON_GROUP_VSCRIPT, "IncludeScript stack overflow\n" );
		return false;
	}

	g_ScriptServerRunScriptDepth++;
	HSCRIPT	hScript = VScriptCompileScript( pszScriptName, bWarnMissing );
	bool bSuccess = false;
	if ( hScript )
	{
		// player is not yet spawned, this block is always skipped.
		// It is registered in CBasePlayer instead.
#ifndef MAPBASE
#ifdef GAME_DLL
		if ( gpGlobals->maxClients == 1 )
		{
			CBaseEntity *pPlayer = UTIL_GetLocalPlayer();
			if ( pPlayer )
			{
				g_pScriptVM->SetValue( "player", pPlayer->GetScriptInstance() );
			}
		}
#endif
#endif
		bSuccess = ( g_pScriptVM->Run( hScript, hScope ) != SCRIPT_ERROR );
		if ( !bSuccess )
		{
			CGWarning( 0, CON_GROUP_VSCRIPT, "Error running script named %s\n", pszScriptName );
			Assert( "Error running script" );
		}
	}
	g_ScriptServerRunScriptDepth--;
	return bSuccess;
}


#ifdef GAME_DLL
#define IsCommandIssuedByServerAdmin() UTIL_IsCommandIssuedByServerAdmin()
#else
#define IsCommandIssuedByServerAdmin() true
#endif

#ifdef CLIENT_DLL
CON_COMMAND_F( script_client, "Run the text as a script", FCVAR_CHEAT )
#else
CON_COMMAND_F( script, "Run the text as a script", FCVAR_CHEAT )
#endif
{
	if ( !IsCommandIssuedByServerAdmin() )
		return;

	if ( !*args[1] )
	{
		CGWarning( 0, CON_GROUP_VSCRIPT, "No function name specified\n" );
		return;
	}

	if ( !g_pScriptVM )
	{
		CGWarning( 0, CON_GROUP_VSCRIPT, "Scripting disabled or no server running\n" );
		return;
	}

	const char *pszScript = args.GetCommandString();

#ifdef CLIENT_DLL
	pszScript += 13;
#else
	pszScript += 6;
#endif

	while ( *pszScript == ' ' )
	{
		pszScript++;
	}

	if ( !*pszScript )
	{
		return;
	}

	if ( *pszScript != '\"' )
	{
		g_pScriptVM->Run( pszScript );
	}
	else
	{
		pszScript++;
		const char *pszEndQuote = pszScript;
		while ( *pszEndQuote !=  '\"' )
		{
			pszEndQuote++;
		}
		if ( !*pszEndQuote )
		{
			return;
		}
		*((char *)pszEndQuote) = 0;
		g_pScriptVM->Run( pszScript );
		*((char *)pszEndQuote) = '\"';
	}
}

#ifdef CLIENT_DLL
CON_COMMAND_F( script_execute_client, "Run a vscript file", FCVAR_CHEAT )
#else
CON_COMMAND_F( script_execute, "Run a vscript file", FCVAR_CHEAT )
#endif
{
	if ( !IsCommandIssuedByServerAdmin() )
		return;

	if ( !*args[1] )
	{
		CGWarning( 0, CON_GROUP_VSCRIPT, "No script specified\n" );
		return;
	}

	if ( !g_pScriptVM )
	{
		CGWarning( 0, CON_GROUP_VSCRIPT, "Scripting disabled or no server running\n" );
		return;
	}

	VScriptRunScript( args[1], true );
}

#ifdef CLIENT_DLL
CON_COMMAND_F( script_debug_client, "Connect the vscript VM to the script debugger", FCVAR_CHEAT )
#else
CON_COMMAND_F( script_debug, "Connect the vscript VM to the script debugger", FCVAR_CHEAT )
#endif
{
	if ( !IsCommandIssuedByServerAdmin() )
		return;

	if ( !g_pScriptVM )
	{
		CGWarning( 0, CON_GROUP_VSCRIPT, "Scripting disabled or no server running\n" );
		return;
	}
	g_pScriptVM->ConnectDebugger();
}

#ifdef CLIENT_DLL
CON_COMMAND_F( script_help_client, "Output help for script functions, optionally with a search string", FCVAR_CHEAT )
#else
CON_COMMAND_F( script_help, "Output help for script functions, optionally with a search string", FCVAR_CHEAT )
#endif
{
	if ( !IsCommandIssuedByServerAdmin() )
		return;

	if ( !g_pScriptVM )
	{
		CGWarning( 0, CON_GROUP_VSCRIPT, "Scripting disabled or no server running\n" );
		return;
	}
	const char *pszArg1 = "*";
	if ( *args[1] )
	{
		pszArg1 = args[1];
	}

	g_pScriptVM->Run( CFmtStr( "__Documentation.PrintHelp( \"%s\" );", pszArg1 ) );
}

#ifdef CLIENT_DLL
CON_COMMAND_F( script_dump_all_client, "Dump the state of the VM to the console", FCVAR_CHEAT )
#else
CON_COMMAND_F( script_dump_all, "Dump the state of the VM to the console", FCVAR_CHEAT )
#endif
{
	if ( !IsCommandIssuedByServerAdmin() )
		return;

	if ( !g_pScriptVM )
	{
		CGWarning( 0, CON_GROUP_VSCRIPT, "Scripting disabled or no server running\n" );
		return;
	}
	g_pScriptVM->DumpState();
}
