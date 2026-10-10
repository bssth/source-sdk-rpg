//========= Half-Life: BB ============//
//
// Purpose: Scored tactical position searches ("EQS-style") for cover and
//			line-of-fire nodes.
//
// The stock searches walk the node graph by straight-line distance and take the
// first node that passes the trace test, against one threat only. These:
//   1. gather candidates by real path distance (Dijkstra over usable links),
//   2. rate them cheaply, without traces: path length, distance to the threat,
//      room from squadmates, cover hints, and whether other known enemies can see
//      the spot (cached by the threat map),
//   3. run the expensive trace test only on the best few, best first.
// If nothing passes, the stock search runs as before.
//
//=============================================================================//

#include "cbase.h"
#include "ai_tacticalservices.h"
#include "ai_basenpc.h"
#include "ai_node.h"
#include "ai_network.h"
#include "ai_link.h"
#include "ai_pathfinder.h"
#include "ai_navigator.h"
#include "ai_hint.h"
#include "ai_squad.h"
#include "ai_memory.h"
#include "ai_threatmap.h"
#include "ndebugoverlay.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar ai_tactical_query( "ai_tactical_query", "1", FCVAR_NONE, "Rate cover and line-of-fire positions (path length, squad spacing, other enemies) instead of taking the first that passes." );
ConVar ai_tactical_query_tests( "ai_tactical_query_tests", "8", FCVAR_NONE, "How many of the best-rated positions get the expensive trace test." );
ConVar ai_tactical_query_max_visits( "ai_tactical_query_max_visits", "400", FCVAR_NONE, "Node budget for one search." );
ConVar ai_tactical_query_los_dist( "ai_tactical_query_los_dist", "2500", FCVAR_NONE, "How far (path distance) a line-of-fire search looks." );
ConVar ai_tactical_query_debug( "ai_tactical_query_debug", "0", FCVAR_NONE, "1: draw rated positions: chosen (white), passed (green), failed the trace (red), rated only (grey). 2: also print each search to the console." );

struct AI_TacticalCandidate_t
{
	int		iNode;
	float	flPathDist;
	float	flScore;
};

static int CandidateSortFunc( const AI_TacticalCandidate_t *pLeft, const AI_TacticalCandidate_t *pRight )
{
	if ( pLeft->flScore > pRight->flScore )
		return -1;
	if ( pLeft->flScore < pRight->flScore )
		return 1;
	return 0;
}

//-----------------------------------------------------------------------------
// Dijkstra by path length from iStartNode. With pvecThreatPos, like the stock
// cover search, don't expand into nodes that lead toward the threat.
//-----------------------------------------------------------------------------
struct AI_QueryOpen_t
{
	float	dist;
	int		id;
};

static bool QueryOpenIsLowerPriority( const AI_QueryOpen_t &lhs, const AI_QueryOpen_t &rhs )
{
	return lhs.dist > rhs.dist;
}

static CUtlVector<float> s_QueryDist;
static CUtlPriorityQueue<AI_QueryOpen_t> s_QueryOpen( 0, 0, QueryOpenIsLowerPriority );

void CAI_TacticalServices::GatherCandidateNodes( int iStartNode, const Vector &vNearPos, float flMaxPathDist, const Vector *pvecThreatPos, CUtlVector<AI_TacticalCandidate_t> &candidates )
{
	const int nNodes = GetNetwork()->NumNodes();
	const Hull_t hull = GetHullType();

	s_QueryDist.SetCount( nNodes );
	for ( int i = 0; i < nNodes; i++ )
		s_QueryDist[i] = FLT_MAX;
	s_QueryOpen.RemoveAll();

	s_QueryDist[iStartNode] = 0;
	AI_QueryOpen_t start = { 0, iStartNode };
	s_QueryOpen.Insert( start );

	int nVisits = 0;
	const int nMaxVisits = ai_tactical_query_max_visits.GetInt();

	while ( s_QueryOpen.Count() && nVisits < nMaxVisits )
	{
		AI_QueryOpen_t entry = s_QueryOpen.ElementAtHead();
		s_QueryOpen.RemoveAtHead();

		if ( entry.dist != s_QueryDist[entry.id] )
			continue; // superseded

		nVisits++;

		AI_TacticalCandidate_t candidate = { entry.id, entry.dist, 0 };
		candidates.AddToTail( candidate );

		CAI_Node *pNode = GetNetwork()->GetNode( entry.id );
		const Vector vecNodePos = pNode->GetPosition( hull );

		for ( int link = 0; link < pNode->NumLinks(); link++ )
		{
			CAI_Link *pLink = pNode->GetLinkByIndex( link );
			if ( !m_pPathfinder->IsLinkUsable( pLink, entry.id ) )
				continue;

			const int iDest = pLink->DestNodeID( entry.id );
			CAI_Node *pDest = GetNetwork()->GetNode( iDest );

			// Don't accept climb nodes or nodes that aren't ready to use yet
			if ( pDest->GetType() == NODE_CLIMB || pDest->IsLocked() )
				continue;

			const Vector vecDestPos = pDest->GetPosition( hull );
			const float flDist = entry.dist + ( vecDestPos - vecNodePos ).Length();
			if ( flDist > flMaxPathDist || flDist >= s_QueryDist[iDest] )
				continue;

			// Same rule as the stock cover search: keep away from the threat while looking for cover
			if ( pvecThreatPos && ( vNearPos - vecDestPos ).LengthSqr() >= ( *pvecThreatPos - vecDestPos ).LengthSqr() * 1.5f )
				continue;

			s_QueryDist[iDest] = flDist;
			AI_QueryOpen_t open = { flDist, iDest };
			s_QueryOpen.Insert( open );
		}
	}
}

//-----------------------------------------------------------------------------
// 0 when nobody from the squad is near the spot or heading there, down to -1.5
//-----------------------------------------------------------------------------
float CAI_TacticalServices::ScoreSquadSpacing( const Vector &vecPos )
{
	CAI_Squad *pSquad = GetOuter()->GetSquad();
	if ( !pSquad )
		return 0.0f;

	float flScore = 0.0f;
	AISquadIter_t iter;
	for ( CAI_BaseNPC *pMember = pSquad->GetFirstMember( &iter ); pMember; pMember = pSquad->GetNextMember( &iter ) )
	{
		if ( pMember == GetOuter() )
			continue;

		float flDistSqr = ( pMember->GetAbsOrigin().AsVector2D() - vecPos.AsVector2D() ).LengthSqr();
		if ( pMember->GetNavigator()->IsGoalActive() )
			flDistSqr = MIN( flDistSqr, ( pMember->GetNavigator()->GetGoalPos().AsVector2D() - vecPos.AsVector2D() ).LengthSqr() );

		if ( flDistSqr < Square( 96.0f ) )
			flScore -= 1.0f;
		else if ( flDistSqr < Square( 192.0f ) )
			flScore -= 0.35f;
	}

	return MAX( flScore, -1.5f );
}

//-----------------------------------------------------------------------------
// -0.5 for every other known enemy that can see a man standing at the node.
// Uses the threat map's cache; traces at most nMaxTraces unknown ones.
//-----------------------------------------------------------------------------
float CAI_TacticalServices::ScoreOtherEnemies( int iNode, const Vector &vThreatPos, int nMaxTraces )
{
	CAI_Enemies *pEnemies = GetOuter()->GetEnemies();
	if ( !pEnemies )
		return 0.0f;

	float flScore = 0.0f;
	AIEnemiesIter_t iter;
	for ( AI_EnemyInfo_t *pInfo = pEnemies->GetFirst( &iter ); pInfo; pInfo = pEnemies->GetNext( &iter ) )
	{
		CBaseEntity *pEnemy = pInfo->hEnemy;
		if ( !pEnemy || !pEnemy->IsAlive() )
			continue;

		// The threat we're searching against is handled by the caller
		if ( ( pEnemy->GetAbsOrigin() - vThreatPos ).LengthSqr() < Square( 64.0f ) )
			continue;

		if ( gpGlobals->curtime - pInfo->timeLastSeen > 10.0f || GetOuter()->IRelationType( pEnemy ) != D_HT )
			continue;

		AI_Exposure_t exposure = g_AI_ThreatMap.GetCachedExposure( iNode, pEnemy );
		if ( exposure == AI_EXPOSURE_UNKNOWN && nMaxTraces > 0 )
		{
			nMaxTraces--;
			exposure = g_AI_ThreatMap.IsNodeExposed( iNode, pEnemy ) ? AI_EXPOSURE_VISIBLE : AI_EXPOSURE_HIDDEN;
		}

		if ( exposure == AI_EXPOSURE_VISIBLE )
			flScore -= 0.5f;
	}

	return flScore;
}

static void DebugQueryNode( int iNode, int r, int g, int b )
{
	if ( !ai_tactical_query_debug.GetBool() )
		return;

	const Vector vecPos = g_pBigAINet->GetNode( iNode )->GetPosition( HULL_HUMAN );
	NDebugOverlay::Box( vecPos, Vector( -8, -8, 0 ), Vector( 8, 8, 16 ), r, g, b, 200, 3.0f );
}

//-----------------------------------------------------------------------------
// Cover from vThreatEyePos near vNearPos
//-----------------------------------------------------------------------------
int CAI_TacticalServices::FindCoverNodeScored( const Vector &vNearPos, const Vector &vThreatPos, const Vector &vThreatEyePos, float flMinDist, float flMaxDist )
{
	AI_PROFILE_SCOPE( CAI_TacticalServices_FindCoverNodeScored );

	const int iMyNode = GetPathfinder()->NearestNodeToPoint( vNearPos );
	if ( iMyNode == NO_NODE )
		return NO_NODE;

	// Same defaults as the stock search
	if ( !flMaxDist )
		flMaxDist = 784;
	if ( flMinDist > 0.5 * flMaxDist )
		flMinDist = 0.5 * flMaxDist;

	CUtlVector<AI_TacticalCandidate_t> candidates;
	GatherCandidateNodes( iMyNode, vNearPos, flMaxDist * 1.5f, &vThreatPos, candidates );

	const Hull_t hull = GetHullType();
	const float flMyThreatDist = ( vNearPos - vThreatPos ).Length();

	// Cheap ratings, no traces
	for ( int i = candidates.Count() - 1; i >= 0; i-- )
	{
		AI_TacticalCandidate_t &candidate = candidates[i];
		CAI_Node *pNode = GetNetwork()->GetNode( candidate.iNode );
		const Vector vecPos = pNode->GetPosition( hull );

		const float flDistSqr = ( vNearPos - vecPos ).LengthSqr();
		if ( flDistSqr < Square( flMinDist ) || flDistSqr >= Square( flMaxDist ) || !GetOuter()->IsValidCover( vecPos, pNode->GetHint() ) )
		{
			candidates.FastRemove( i );
			continue;
		}

		float flScore = -candidate.flPathDist / flMaxDist;
		flScore += 0.25f * clamp( ( ( vecPos - vThreatPos ).Length() - flMyThreatDist ) / 512.0f, -1.0f, 1.0f );
		flScore += ScoreSquadSpacing( vecPos );
		flScore += ScoreOtherEnemies( candidate.iNode, vThreatPos, 0 );

		// Mapper-placed cover is probably good cover
		if ( pNode->GetHint() && ( pNode->GetHint()->HintType() == HINT_TACTICAL_COVER_MED || pNode->GetHint()->HintType() == HINT_TACTICAL_COVER_LOW ) )
			flScore += 0.4f;

		candidate.flScore = flScore;
		DebugQueryNode( candidate.iNode, 128, 128, 128 );
	}

	candidates.Sort( CandidateSortFunc );

	// Expensive test on the best few, best first
	int iBest = NO_NODE;
	float flBestScore = -FLT_MAX;
	const int nTests = MIN( candidates.Count(), ai_tactical_query_tests.GetInt() );
	for ( int i = 0; i < nTests; i++ )
	{
		const AI_TacticalCandidate_t &candidate = candidates[i];
		if ( candidate.flScore <= flBestScore )
			break; // can't beat what we have

		CAI_Node *pNode = GetNetwork()->GetNode( candidate.iNode );
		const Vector vecPos = pNode->GetPosition( hull );
		const Vector vecEyePos = vecPos + GetOuter()->EyeOffset( GetOuter()->GetCoverActivity( pNode->GetHint() ) );

		if ( !GetOuter()->IsCoverPosition( vThreatEyePos, vecEyePos ) )
		{
			DebugQueryNode( candidate.iNode, 255, 0, 0 );
			continue;
		}

		DebugQueryNode( candidate.iNode, 0, 255, 0 );

		// Now that it's real cover, check the other enemies properly (a couple of traces)
		const float flCached = ScoreOtherEnemies( candidate.iNode, vThreatPos, 0 );
		const float flScore = candidate.flScore - flCached + ScoreOtherEnemies( candidate.iNode, vThreatPos, 2 );

		if ( flScore > flBestScore )
		{
			flBestScore = flScore;
			iBest = candidate.iNode;
		}

		// Nobody else can see it either: nothing further down the list can do better
		if ( flScore >= candidate.flScore )
			break;
	}

	if ( ai_tactical_query_debug.GetInt() >= 2 )
	{
		if ( iBest != NO_NODE )
			Msg( "ai_tactical_query: %.1f %s cover: %d rated, chose node %d (%.2f)\n", gpGlobals->curtime, GetOuter()->GetDebugName(), candidates.Count(), iBest, flBestScore );
		else
			Msg( "ai_tactical_query: %.1f %s cover: %d rated, none, stock search\n", gpGlobals->curtime, GetOuter()->GetDebugName(), candidates.Count() );
	}

	if ( iBest == NO_NODE )
		return NO_NODE;

	DebugQueryNode( iBest, 255, 255, 255 );

	// Same bookkeeping as the stock search
	CAI_Node *pBest = GetNetwork()->GetNode( iBest );
	pBest->Lock( 1.0 );

	if ( pBest->GetHint() && ( pBest->GetHint()->HintType() == HINT_TACTICAL_COVER_MED || pBest->GetHint()->HintType() == HINT_TACTICAL_COVER_LOW ) )
	{
		if ( GetOuter()->GetHintNode() )
		{
			GetOuter()->GetHintNode()->Unlock( GetOuter()->GetHintDelay( GetOuter()->GetHintNode()->HintType() ) );
			GetOuter()->SetHintNode( NULL );
		}

		GetOuter()->SetHintNode( pBest->GetHint() );
	}

	return iBest;
}

//-----------------------------------------------------------------------------
// A place to shoot vThreatEyePos from
//-----------------------------------------------------------------------------
int CAI_TacticalServices::FindLosNodeScored( const Vector &vThreatPos, const Vector &vThreatEyePos, float flMinThreatDist, float flMaxThreatDist, float flBlockTime, FlankType_t eFlankType, const Vector &vecFlankRefPos, float flFlankParam )
{
	AI_PROFILE_SCOPE( CAI_TacticalServices_FindLosNodeScored );

	const int iMyNode = GetPathfinder()->NearestNodeToNPC();
	if ( iMyNode == NO_NODE )
		return NO_NODE;

	CUtlVector<AI_TacticalCandidate_t> candidates;
	GatherCandidateNodes( iMyNode, GetLocalOrigin(), ai_tactical_query_los_dist.GetFloat(), NULL, candidates );

	const Hull_t hull = GetHullType();

	Vector vecEnemyToRef = vecFlankRefPos - vThreatPos;
	VectorNormalize( vecEnemyToRef );

	for ( int i = candidates.Count() - 1; i >= 0; i-- )
	{
		AI_TacticalCandidate_t &candidate = candidates[i];

		// Like the stock search: the node we're at is why we're searching
		if ( candidate.iNode == iMyNode )
		{
			candidates.FastRemove( i );
			continue;
		}

		CAI_Node *pNode = GetNetwork()->GetNode( candidate.iNode );
		const Vector vecPos = pNode->GetPosition( hull );

		float flFlankBonus = 0.0f;
		bool bSkip = false;
		switch ( eFlankType )
		{
		case FLANKTYPE_NONE:
			break;

		case FLANKTYPE_RADIUS:
			bSkip = ( ( vecPos - vecFlankRefPos ).Length() < flFlankParam );
			break;

		case FLANKTYPE_ARC:
			{
				Vector vecEnemyToNode = vecPos - vThreatPos;
				VectorNormalize( vecEnemyToNode );
				const float flDegrees = RAD2DEG( acos( clamp( DotProduct( vecEnemyToRef, vecEnemyToNode ), -1.0f, 1.0f ) ) );
				bSkip = ( flDegrees < flFlankParam );
				// The wider the angle, the better the flank
				flFlankBonus = 0.3f * clamp( ( flDegrees - flFlankParam ) / 90.0f, 0.0f, 1.0f );
			}
			break;
		}

		const float flThreatDist = ( vecPos - vThreatPos ).Length();
		if ( bSkip || flThreatDist >= flMaxThreatDist || flThreatDist <= flMinThreatDist ||
			 !GetOuter()->IsValidShootPosition( vecPos, pNode, pNode->GetHint() ) )
		{
			candidates.FastRemove( i );
			continue;
		}

		float flScore = -candidate.flPathDist / 1000.0f + flFlankBonus;
		flScore += ScoreSquadSpacing( vecPos );
		flScore += ScoreOtherEnemies( candidate.iNode, vThreatPos, 0 );

		// Shooting from a cover spot beats shooting from the open
		if ( pNode->GetHint() && ( pNode->GetHint()->HintType() == HINT_TACTICAL_COVER_MED || pNode->GetHint()->HintType() == HINT_TACTICAL_COVER_LOW ) )
			flScore += 0.3f;

		candidate.flScore = flScore;
		DebugQueryNode( candidate.iNode, 128, 128, 128 );
	}

	candidates.Sort( CandidateSortFunc );

	const int nTests = MIN( candidates.Count(), ai_tactical_query_tests.GetInt() );
	for ( int i = 0; i < nTests; i++ )
	{
		const int iNode = candidates[i].iNode;
		const Vector vecPos = GetNetwork()->GetNode( iNode )->GetPosition( hull );

		if ( !GetOuter()->TestShootPosition( vecPos, vThreatEyePos ) )
		{
			DebugQueryNode( iNode, 255, 0, 0 );
			continue;
		}

		DebugQueryNode( iNode, 255, 255, 255 );

		if ( ai_tactical_query_debug.GetInt() >= 2 )
		{
			Msg( "ai_tactical_query: %.1f %s los%s: %d rated, chose node %d (rank %d)\n", gpGlobals->curtime, GetOuter()->GetDebugName(),
				( eFlankType != FLANKTYPE_NONE ) ? " (flank)" : "", candidates.Count(), iNode, i + 1 );
		}

		// Note when this node was used, so we don't try to use it again right away.
		GetNetwork()->GetNode( iNode )->Lock( flBlockTime );
		return iNode;
	}

	if ( ai_tactical_query_debug.GetInt() >= 2 )
	{
		Msg( "ai_tactical_query: %.1f %s los%s: %d rated, none, stock search\n", gpGlobals->curtime, GetOuter()->GetDebugName(),
			( eFlankType != FLANKTYPE_NONE ) ? " (flank)" : "", candidates.Count() );
	}

	return NO_NODE;
}
