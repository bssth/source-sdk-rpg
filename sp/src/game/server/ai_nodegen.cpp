//========= Half-Life: BB ============//
//
// Purpose: ai_generate_nodes - flood-fill the ground a human NPC can walk on,
//			starting where the player stands, and write info_nodes for Hammer.
//
// Walkability uses the same test hull and ground move probe the node graph
// builder uses to link nodes, so whatever this places will connect. Cells on a
// grid (ai_generate_nodes <spacing>) are reached by walking from neighbor to
// neighbor; then every other lattice cell is kept for open ground, plus cells
// with few walkable neighbors (corridors, doorways, corners), no two closer than
// 1.25 * spacing. With "skip existing" (default) spots near the map's own nodes
// are left out, so on a noded map it only fills the gaps.
//
// Output: maps_src/<map>_autonodes.vmf, a VMF with only info_nodes. Point a
// func_instance at it (no offset), or merge the entities into the map with the
// vmf skill. Node ids start after the map's highest Hammer node id.
//
//=============================================================================//

#include "cbase.h"
#include "ai_initutils.h"
#include "ai_moveprobe.h"
#include "ai_hull.h"
#include "ai_node.h"
#include "ai_network.h"
#include "ai_networkmanager.h"
#include "filesystem.h"
#include "ndebugoverlay.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

struct NodeGenCell_t
{
	Vector	pos;
	int		ix;
	int		iy;
	int		nWalkable;
};

static uint64 NodeGenKey( int ix, int iy, int iz )
{
	return ( (uint64)(uint16)( ix + 32768 ) << 32 ) | ( (uint64)(uint16)( iy + 32768 ) << 16 ) | (uint64)(uint16)( iz + 32768 );
}

static bool NodeGenKeyLess( const uint64 &lhs, const uint64 &rhs )
{
	return lhs < rhs;
}

static int NodeGenFloorLayer( float z )
{
	return (int)floorf( z / 48.0f );
}

CON_COMMAND( ai_generate_nodes, "Flood-fill the ground a human NPC can walk on from where you stand and write info_nodes to maps_src/<map>_autonodes.vmf. Args: [spacing=128] [radius=3000] [skip near existing nodes=1]" )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if ( !pPlayer )
		pPlayer = UTIL_GetLocalPlayer();
	if ( !pPlayer )
	{
		Warning( "ai_generate_nodes: no player\n" );
		return;
	}

	const float flSpacing = ( args.ArgC() > 1 ) ? clamp( (float)atof( args[1] ), 64.0f, 512.0f ) : 128.0f;
	const float flRadius = ( args.ArgC() > 2 ) ? MAX( (float)atof( args[2] ), flSpacing ) : 3000.0f;
	const bool bSkipExisting = ( args.ArgC() > 3 ) ? ( atoi( args[3] ) != 0 ) : true;
	const int nMaxCells = 20000;
	const double flStartTime = Plat_FloatTime();

	const Vector vecMins = NAI_Hull::Mins( HULL_HUMAN );
	const Vector vecMaxs = NAI_Hull::Maxs( HULL_HUMAN );

	// Start on the ground under the player
	trace_t tr;
	UTIL_TraceHull( pPlayer->GetAbsOrigin() + Vector( 0, 0, 8 ), pPlayer->GetAbsOrigin() - Vector( 0, 0, 256 ),
		vecMins, vecMaxs, MASK_NPCSOLID_BRUSHONLY, pPlayer, COLLISION_GROUP_NONE, &tr );
	if ( tr.startsolid || tr.fraction == 1.0f )
	{
		Warning( "ai_generate_nodes: stand somewhere a human NPC fits\n" );
		return;
	}

	CAI_TestHull *pHull = CAI_TestHull::GetTestHull();
	if ( !pHull )
	{
		Warning( "ai_generate_nodes: no test hull\n" );
		return;
	}
	pHull->SetHullType( HULL_HUMAN );
	pHull->SetHullSizeNormal( true );

	// ---------------------------------------------------------------
	// Flood fill
	// ---------------------------------------------------------------
	CUtlVector<NodeGenCell_t> cells;
	CUtlRBTree<uint64> visited( 0, 0, NodeGenKeyLess );

	NodeGenCell_t start;
	start.pos = tr.endpos;
	start.ix = RoundFloatToInt( start.pos.x / flSpacing );
	start.iy = RoundFloatToInt( start.pos.y / flSpacing );
	start.nWalkable = 0;
	cells.AddToTail( start );
	visited.Insert( NodeGenKey( start.ix, start.iy, NodeGenFloorLayer( start.pos.z ) ) );

	static const int s_dx[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
	static const int s_dy[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };

	bool bHitCellLimit = false;
	for ( int iHead = 0; iHead < cells.Count(); iHead++ )
	{
		const Vector vecFrom = cells[iHead].pos;
		const int ix = cells[iHead].ix;
		const int iy = cells[iHead].iy;
		int nWalkable = 0;

		for ( int d = 0; d < 8; d++ )
		{
			const int nx = ix + s_dx[d];
			const int ny = iy + s_dy[d];
			const Vector vecTo( nx * flSpacing, ny * flSpacing, vecFrom.z );

			if ( ( vecTo - start.pos ).Length2DSqr() > Square( flRadius ) )
				continue;

			AIMoveTrace_t moveTrace;
			if ( !pHull->GetMoveProbe()->TestGroundMove( vecFrom, vecTo, MASK_NPCSOLID_BRUSHONLY, AITGM_2D | AITGM_IGNORE_INITIAL_STAND_POS, &moveTrace ) )
				continue;

			const Vector vecEnd = moveTrace.vEndPosition;
			if ( !pHull->GetMoveProbe()->CheckStandPosition( vecEnd, MASK_NPCSOLID_BRUSHONLY ) )
				continue;

			nWalkable++;

			const uint64 key = NodeGenKey( nx, ny, NodeGenFloorLayer( vecEnd.z ) );
			if ( visited.Find( key ) != visited.InvalidIndex() )
				continue;

			if ( cells.Count() >= nMaxCells )
			{
				bHitCellLimit = true;
				continue;
			}

			visited.Insert( key );

			NodeGenCell_t cell;
			cell.pos = vecEnd;
			cell.ix = nx;
			cell.iy = ny;
			cell.nWalkable = 0;
			cells.AddToTail( cell );
		}

		cells[iHead].nWalkable = nWalkable;
	}

	CAI_TestHull::ReturnTestHull();

	// ---------------------------------------------------------------
	// Thin out: lattice for open ground, plus tight spots
	// ---------------------------------------------------------------
	const float flMinKeptDistSqr = Square( flSpacing * 1.25f );
	CUtlVector<int> kept;
	int nSkippedExisting = 0;

	for ( int nPass = 0; nPass < 2; nPass++ )
	{
		for ( int i = 0; i < cells.Count(); i++ )
		{
			const NodeGenCell_t &cell = cells[i];
			const bool bLattice = ( ( cell.ix & 1 ) == 0 && ( cell.iy & 1 ) == 0 );
			const bool bTight = ( cell.nWalkable <= 5 );

			if ( nPass == 0 ? !bLattice : ( bLattice || !bTight ) )
				continue;

			bool bTooClose = false;
			for ( int k = 0; k < kept.Count() && !bTooClose; k++ )
			{
				bTooClose = ( cells[kept[k]].pos - cell.pos ).LengthSqr() < flMinKeptDistSqr;
			}
			if ( bTooClose )
				continue;

			if ( bSkipExisting && g_pBigAINet )
			{
				bool bNearExisting = false;
				for ( int n = 0; n < g_pBigAINet->NumNodes() && !bNearExisting; n++ )
				{
					CAI_Node *pNode = g_pBigAINet->GetNode( n );
					bNearExisting = ( pNode->GetType() == NODE_GROUND && ( pNode->GetOrigin() - cell.pos ).LengthSqr() < Square( flSpacing ) );
				}
				if ( bNearExisting )
				{
					nSkippedExisting++;
					continue;
				}
			}

			kept.AddToTail( i );
		}
	}

	// ---------------------------------------------------------------
	// Write the VMF
	// ---------------------------------------------------------------
	char szPath[MAX_PATH];
	V_snprintf( szPath, sizeof( szPath ), "maps_src/%s_autonodes.vmf", STRING( gpGlobals->mapname ) );

	FileHandle_t hFile = filesystem->Open( szPath, "w", "MOD" );
	if ( !hFile )
	{
		Warning( "ai_generate_nodes: can't write %s\n", szPath );
		return;
	}

	// After the map's highest Hammer node id. m_nNextWCIndex is only kept up to date when the
	// graph is built from the map's info_nodes, not when it's loaded from the .ain, so scan too.
	int nFirstNodeId = 1;
	if ( g_pAINetworkManager && g_pAINetworkManager->GetEditOps() )
	{
		CAI_NetworkEditTools *pEditOps = g_pAINetworkManager->GetEditOps();
		nFirstNodeId = MAX( 1, pEditOps->m_nNextWCIndex );
		for ( int n = 0; g_pBigAINet && n < g_pBigAINet->NumNodes(); n++ )
			nFirstNodeId = MAX( nFirstNodeId, pEditOps->GetWCIdFromNodeId( n ) + 1 );
	}

	filesystem->FPrintf( hFile, "versioninfo\n{\n\t\"editorversion\" \"400\"\n\t\"editorbuild\" \"8000\"\n\t\"mapversion\" \"1\"\n\t\"formatversion\" \"100\"\n\t\"prefab\" \"0\"\n}\n" );
	filesystem->FPrintf( hFile, "world\n{\n\t\"id\" \"1\"\n\t\"mapversion\" \"1\"\n\t\"classname\" \"worldspawn\"\n}\n" );

	for ( int k = 0; k < kept.Count(); k++ )
	{
		const Vector &vecPos = cells[kept[k]].pos;
		filesystem->FPrintf( hFile, "entity\n{\n\t\"id\" \"%d\"\n\t\"classname\" \"info_node\"\n\t\"nodeid\" \"%d\"\n\t\"origin\" \"%.0f %.0f %.0f\"\n"
			"\teditor\n\t{\n\t\t\"color\" \"255 255 0\"\n\t\t\"visgroupshown\" \"1\"\n\t\t\"visgroupautoshown\" \"1\"\n\t\t\"logicalpos\" \"[0 0]\"\n\t}\n}\n",
			k + 2, nFirstNodeId + k, vecPos.x, vecPos.y, vecPos.z + 8.0f );

		NDebugOverlay::Box( vecPos, Vector( -8, -8, 0 ), Vector( 8, 8, 16 ), 255, 255, 0, 200, 20.0f );
	}

	filesystem->FPrintf( hFile, "cameras\n{\n\t\"activecamera\" \"-1\"\n}\ncordons\n{\n\t\"active\" \"0\"\n}\n" );
	filesystem->Close( hFile );

	Msg( "ai_generate_nodes: %d walkable cells%s, %d nodes written to %s (ids %d..%d), %d skipped near existing nodes, %.1f s\n",
		cells.Count(), bHitCellLimit ? " (cell limit hit, use a smaller radius)" : "", kept.Count(), szPath,
		nFirstNodeId, nFirstNodeId + kept.Count() - 1, nSkippedExisting, Plat_FloatTime() - flStartTime );

	if ( g_pBigAINet && g_pBigAINet->NumNodes() + kept.Count() > MAX_NODES )
		Warning( "ai_generate_nodes: with the map's %d nodes that's more than MAX_NODES (%d)\n", g_pBigAINet->NumNodes(), MAX_NODES );
}
