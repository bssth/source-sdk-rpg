//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 =================
//
// Purpose: General matching functions for things like wildcards and !=.
//
//          Ported from Mapbase. Regex path ('@/pattern/') removed to avoid
//          pulling <regex> into the binary.
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "matchers.h"
#include "fmtstr.h"
#include "convar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar mapbase_wildcards_enabled( "mapbase_wildcards_enabled", "1", FCVAR_NONE,
	"Toggles Mapbase's '?' wildcard and true '*' features. Useful for maps that have '?' in their targetnames." );
ConVar mapbase_wildcards_lazy_hack( "mapbase_wildcards_lazy_hack", "1", FCVAR_NONE,
	"Toggles a hack which prevents Mapbase's lazy '?' wildcards from picking up \"???\", the default instance parameter." );

#ifdef CLIENT_DLL
// FIXME: There is no clientside equivalent to the RS code
static bool ResponseSystemCompare( const char *criterion, const char *value ) { return Matcher_NamesMatch( criterion, value ); }
#else
extern bool ResponseSystemCompare( const char *criterion, const char *value );
#endif

//=============================================================================
// These are the "matchers" that compare with wildcards ("any*" for text
// starting with "any") and operators (<3 for numbers less than 3).
//=============================================================================

// The recursive part of the modified NamesMatch(). Model based on ASW SDK.
static bool Matcher_RunCharCompare( const char *pszQuery, const char *szValue )
{
	while ( *szValue && *pszQuery )
	{
		char cName = *szValue;
		char cQuery = *pszQuery;
		if ( cName != cQuery && tolower( cName ) != tolower( cQuery ) )
		{
			switch ( *pszQuery )
			{
				case '*':
				{
					// Classic trailing '*' — always matches
					if ( *( pszQuery + 1 ) == 0 )
						return true;

					if ( mapbase_wildcards_enabled.GetBool() )
					{
						// There's text after this '*' which we need to test.
						// Recursive to allow multiple wildcards.
						int vlen = Q_strlen( szValue );
						++pszQuery;
						for ( int i = 0; i < vlen; i++ )
						{
							if ( Matcher_RunCharCompare( pszQuery, szValue + i ) )
								return true;
						}
					}
					return false;
				}
				break;
				case '?':
					// Single-char wildcard — consume one char from value and continue
					if ( mapbase_wildcards_enabled.GetBool() )
						break;
				default:
					return false;
			}
		}
		++szValue;
		++pszQuery;
	}

	// Include a classic trailing '*' check for when pszQuery is "value*"
	return ( ( *pszQuery == 0 && *szValue == 0 ) || *pszQuery == '*' );
}

// Entry point for the modified NamesMatch.
bool Matcher_NamesMatch( const char *pszQuery, const char *szValue )
{
	if ( szValue == NULL )
		return ( *pszQuery == 0 || *pszQuery == '*' );

	if ( szValue == pszQuery )
		return true;

	// '@/pattern/' regex prefix intentionally not supported (stripped to avoid <regex>).
	// We still honor the lazy-wildcard "???" hack:
	if ( pszQuery[0] == '?' && pszQuery[1] == '?' && pszQuery[2] == '?' && mapbase_wildcards_lazy_hack.GetBool() )
	{
		// Instances with blank parameters use "???". Don't match those as 3 chars.
		return false;
	}

	return Matcher_RunCharCompare( pszQuery, szValue );
}

bool Matcher_NamesMatch_Classic( const char *pszQuery, const char *szValue )
{
	if ( szValue == NULL )
		return ( !pszQuery || *pszQuery == 0 || *pszQuery == '*' );

	if ( szValue == pszQuery )
		return true;

	while ( *szValue && *pszQuery )
	{
		unsigned char cName = *szValue;
		unsigned char cQuery = *pszQuery;
		if ( cName == cQuery )
			;
		else if ( cName - 'A' <= (unsigned char)'Z' - 'A' && cName - 'A' + 'a' == cQuery )
			;
		else if ( cName - 'a' <= (unsigned char)'z' - 'a' && cName - 'a' + 'A' == cQuery )
			;
		else
			break;
		++szValue;
		++pszQuery;
	}

	if ( *pszQuery == 0 && *szValue == 0 )
		return true;

	if ( *pszQuery == '*' )
		return true;

	return false;
}

bool Matcher_NamesMatch_MutualWildcard( const char *pszQuery, const char *szValue )
{
	if ( szValue == NULL )
		return ( !pszQuery || *pszQuery == 0 || *pszQuery == '*' );

	if ( pszQuery == NULL )
		return ( !szValue || *szValue == 0 || *szValue == '*' );

	if ( szValue == pszQuery )
		return true;

	while ( *szValue && *pszQuery )
	{
		unsigned char cName = *szValue;
		unsigned char cQuery = *pszQuery;
		if ( cName == cQuery )
			;
		else if ( cName - 'A' <= (unsigned char)'Z' - 'A' && cName - 'A' + 'a' == cQuery )
			;
		else if ( cName - 'a' <= (unsigned char)'z' - 'a' && cName - 'a' + 'A' == cQuery )
			;
		else
			break;
		++szValue;
		++pszQuery;
	}

	if ( *pszQuery == 0 && *szValue == 0 )
		return true;

	if ( *pszQuery == '*' || *szValue == '*' )
		return true;

	return false;
}

bool Matcher_ContainsWildcard( const char *pszQuery )
{
	if ( pszQuery == NULL )
		return false;

	while ( *pszQuery )
	{
		unsigned char cQuery = *pszQuery;
		if ( cQuery == '*' || cQuery == '?' )
			return true;
		++pszQuery;
	}

	return false;
}

//-----------------------------------------------------------------------------
// High-level Matcher_Match — uses Response System comparison path.
//-----------------------------------------------------------------------------
bool Matcher_Match( const char *pszQuery, const char *szValue )
{
	return ResponseSystemCompare( pszQuery, szValue );
}

bool Matcher_Match( const char *pszQuery, int iValue ) { return Matcher_Match( pszQuery, CNumStr( iValue ) ); }
bool Matcher_Match( const char *pszQuery, float flValue ) { return Matcher_Match( pszQuery, CNumStr( flValue ) ); }
