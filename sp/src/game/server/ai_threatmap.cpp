//========= Half-Life: BB ============//
//
// Purpose: Which nav nodes the player can see. See ai_threatmap.h.
//
//=============================================================================//

#include "cbase.h"
#include "ai_threatmap.h"
#include "ai_basenpc.h"
#include "ai_network.h"
#include "ai_node.h"
#include "ai_debug.h"
#include "ndebugoverlay.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar ai_threatmap( "ai_threatmap", "1", FCVAR_NONE, "Track which nav nodes the player can see while NPCs fight him." );
ConVar ai_threatmap_traces_per_frame( "ai_threatmap_traces_per_frame", "16", FCVAR_NONE, "Node visibility tests per frame for the threat map." );
ConVar ai_threatmap_radius( "ai_threatmap_radius", "2500", FCVAR_NONE, "Only nodes this close to the player are kept up to date." );
ConVar ai_threatmap_ttl( "ai_threatmap_ttl", "1.5", FCVAR_NONE, "Seconds a node visibility test stays valid." );
ConVar ai_threatmap_move_tolerance( "ai_threatmap_move_tolerance", "48", FCVAR_NONE, "A test stays valid while the player's eyes are within this distance of where they were." );
ConVar ai_threatmap_debug( "ai_threatmap_debug", "0", FCVAR_NONE, "Draw nodes the player can see (red) or can't (green)." );

CAI_ThreatMap g_AI_ThreatMap;

//-----------------------------------------------------------------------------

bool CAI_ThreatMap::EnsureSize()
{
	if ( !g_pBigAINet || g_pBigAINet->NumNodes() <= 0 )
		return false;

	const int nNodes = g_pBigAINet->NumNodes();
	if ( m_PlayerEntries.Count() != nNodes )
	{
		// New or rebuilt graph: forget everything
		m_PlayerEntries.SetCount( nNodes );
		for ( int i = 0; i < nNodes; i++ )
		{
			m_PlayerEntries[i].flTime = -1.0f;
			m_PlayerEntries[i].vecThreatEye = vec3_origin;
			m_PlayerEntries[i].bVisible = false;
		}
		m_iNextNode = 0;
	}

	return true;
}

Vector CAI_ThreatMap::GetNodeEyePosition( int iNode )
{
	return g_pBigAINet->GetNode( iNode )->GetPosition( HULL_HUMAN ) + Vector( 0, 0, 64 );
}

bool CAI_ThreatMap::IsEntryFresh( const Entry_t &entry, const Vector &vecThreatEye ) const
{
	if ( entry.flTime < 0.0f || gpGlobals->curtime - entry.flTime > ai_threatmap_ttl.GetFloat() )
		return false;

	return ( entry.vecThreatEye - vecThreatEye ).LengthSqr() <= Square( ai_threatmap_move_tolerance.GetFloat() );
}

bool CAI_ThreatMap::TraceExposure( int iNode, const Vector &vecThreatEye, CBaseEntity *pThreat )
{
	trace_t tr;
	AI_TraceLOS( vecThreatEye, GetNodeEyePosition( iNode ), pThreat, &tr );
	return ( tr.fraction == 1.0f );
}

//-----------------------------------------------------------------------------

AI_Exposure_t CAI_ThreatMap::GetCachedExposure( int iNode, CBaseEntity *pThreat )
{
	if ( !ai_threatmap.GetBool() || !pThreat || !pThreat->IsPlayer() || !EnsureSize() || iNode < 0 || iNode >= m_PlayerEntries.Count() )
		return AI_EXPOSURE_UNKNOWN;

	const Entry_t &entry = m_PlayerEntries[iNode];
	if ( !IsEntryFresh( entry, pThreat->EyePosition() ) )
		return AI_EXPOSURE_UNKNOWN;

	return entry.bVisible ? AI_EXPOSURE_VISIBLE : AI_EXPOSURE_HIDDEN;
}

bool CAI_ThreatMap::IsNodeExposed( int iNode, CBaseEntity *pThreat )
{
	if ( !pThreat || !g_pBigAINet || iNode < 0 || iNode >= g_pBigAINet->NumNodes() )
		return false;

	AI_Exposure_t cached = GetCachedExposure( iNode, pThreat );
	if ( cached != AI_EXPOSURE_UNKNOWN )
		return ( cached == AI_EXPOSURE_VISIBLE );

	const Vector vecThreatEye = pThreat->EyePosition();
	const bool bVisible = TraceExposure( iNode, vecThreatEye, pThreat );

	if ( ai_threatmap.GetBool() && pThreat->IsPlayer() && iNode < m_PlayerEntries.Count() )
	{
		Entry_t &entry = m_PlayerEntries[iNode];
		entry.flTime = gpGlobals->curtime;
		entry.vecThreatEye = vecThreatEye;
		entry.bVisible = bVisible;
	}

	return bVisible;
}

//-----------------------------------------------------------------------------
// Re-test a few stale nodes around the player each frame, but only while
// somebody is actually fighting him
//-----------------------------------------------------------------------------
void CAI_ThreatMap::FrameUpdatePostEntityThink()
{
	if ( !ai_threatmap.GetBool() )
		return;

	CBasePlayer *pPlayer = AI_GetSinglePlayer();
	if ( !pPlayer || !pPlayer->IsAlive() || !EnsureSize() )
		return;

	bool bEngaged = false;
	CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
	for ( int i = 0; i < g_AI_Manager.NumAIs(); i++ )
	{
		if ( ppAIs[i]->GetEnemy() == pPlayer && ppAIs[i]->IsAlive() )
		{
			bEngaged = true;
			break;
		}
	}

	if ( !bEngaged && !ai_threatmap_debug.GetBool() )
		return;

	const Vector vecEye = pPlayer->EyePosition();
	const float flRadiusSqr = Square( ai_threatmap_radius.GetFloat() );
	const int nNodes = m_PlayerEntries.Count();
	int nBudget = ai_threatmap_traces_per_frame.GetInt();

	for ( int nScanned = 0; nBudget > 0 && nScanned < nNodes; nScanned++ )
	{
		const int iNode = m_iNextNode;
		m_iNextNode = ( m_iNextNode + 1 ) % nNodes;

		Entry_t &entry = m_PlayerEntries[iNode];
		if ( IsEntryFresh( entry, vecEye ) )
			continue;

		if ( g_pBigAINet->GetNode( iNode )->GetType() != NODE_GROUND )
			continue;

		if ( ( GetNodeEyePosition( iNode ) - vecEye ).LengthSqr() > flRadiusSqr )
			continue;

		entry.bVisible = TraceExposure( iNode, vecEye, pPlayer );
		entry.flTime = gpGlobals->curtime;
		entry.vecThreatEye = vecEye;
		nBudget--;
	}

	if ( ai_threatmap_debug.GetBool() )
	{
		static float s_flNextDraw = 0.0f;
		if ( gpGlobals->curtime >= s_flNextDraw || s_flNextDraw - gpGlobals->curtime > 1.0f )
		{
			s_flNextDraw = gpGlobals->curtime + 0.25f;
			for ( int iNode = 0; iNode < nNodes; iNode++ )
			{
				const Entry_t &entry = m_PlayerEntries[iNode];
				if ( !IsEntryFresh( entry, vecEye ) )
					continue;

				const Vector vecPos = g_pBigAINet->GetNode( iNode )->GetPosition( HULL_HUMAN );
				if ( entry.bVisible )
					NDebugOverlay::Box( vecPos, Vector( -6, -6, 0 ), Vector( 6, 6, 12 ), 255, 0, 0, 160, 0.25f );
				else
					NDebugOverlay::Box( vecPos, Vector( -6, -6, 0 ), Vector( 6, 6, 12 ), 0, 255, 0, 160, 0.25f );
			}
		}
	}
}

void CAI_ThreatMap::LevelShutdownPostEntity()
{
	m_PlayerEntries.Purge();
	m_iNextNode = 0;
}
