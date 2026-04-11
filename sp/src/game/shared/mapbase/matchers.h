//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 =================
//
// Purpose: General matching functions for things like wildcards and !=.
//
//          Ported from Mapbase. Regex support stripped to avoid pulling <regex>
//          into the binary; the '@/' regex prefix falls back to wildcard match.
//
// $NoKeywords: $
//=============================================================================

#ifndef MAPBASE_MATCHERS_H
#define MAPBASE_MATCHERS_H
#ifdef _WIN32
#pragma once
#endif

#include <math.h>

#define MAPBASE_MATCHERS 1

// Compares two strings with support for wildcards. Expanded version of
// baseentity.cpp's classic NamesMatch() — supports '*' in any position and '?'
// as a single-character wildcard.
//  pszQuery = The value that should have the wildcard.
//  szValue  = The value tested against the query.
// Use Matcher_Match if you want <, !=, etc. as well.
bool Matcher_NamesMatch( const char *pszQuery, const char *szValue );

// Identical to baseentity.cpp's original NamesMatch(). Supports only trailing '*'.
bool Matcher_NamesMatch_Classic( const char *pszQuery, const char *szValue );

// Identical to Matcher_NamesMatch_Classic(), but either value may contain a trailing '*'.
bool Matcher_NamesMatch_MutualWildcard( const char *pszQuery, const char *szValue );

// Returns true if the specified string contains a wildcard character ('*' or '?').
bool Matcher_ContainsWildcard( const char *pszQuery );

// Checks if the specified string appears to be a number (from Response System).
static bool AppearsToBeANumber( char const *token )
{
	if ( atof( token ) != 0.0f )
		return true;

	char const *p = token;
	while ( *p )
	{
		if ( *p != '0' )
			return false;

		p++;
	}

	return true;
}

// Compares with !=, <, >, etc. — hijacks the Response System matching path and
// loops back into Matcher_NamesMatch for wildcard handling.
//  pszQuery = The value that should have the operator(s) at the beginning.
//  szValue  = The value tested against the criterion.
bool Matcher_Match( const char *pszQuery, const char *szValue );
bool Matcher_Match( const char *pszQuery, int iValue );
bool Matcher_Match( const char *pszQuery, float flValue );

#endif // MAPBASE_MATCHERS_H
