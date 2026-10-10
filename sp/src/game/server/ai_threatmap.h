//========= Half-Life: BB ============//
//
// Purpose: Which nav nodes the player can see ("threat map").
//
// While some NPC is fighting the player, a few nodes around him are re-tested
// every frame (ai_threatmap_traces_per_frame) and the results are cached per
// node. The pathfinder uses it to make NPCs prefer routes the player can't see,
// and tactical queries use it to rate positions against more than one enemy
// without paying for the traces again. Other threats are traced on demand.
//
//=============================================================================//

#ifndef AI_THREATMAP_H
#define AI_THREATMAP_H
#ifdef _WIN32
#pragma once
#endif

#include "igamesystem.h"

enum AI_Exposure_t
{
	AI_EXPOSURE_UNKNOWN = -1,
	AI_EXPOSURE_HIDDEN = 0,
	AI_EXPOSURE_VISIBLE = 1,
};

class CAI_ThreatMap : public CAutoGameSystemPerFrame
{
public:
	CAI_ThreatMap() : CAutoGameSystemPerFrame( "CAI_ThreatMap" ) { m_iNextNode = 0; }

	// Cached answer only, never traces. UNKNOWN for anything but the player or a stale entry.
	AI_Exposure_t	GetCachedExposure( int iNode, CBaseEntity *pThreat );

	// Cached answer if there is one, otherwise a trace (cached when the threat is the player).
	bool			IsNodeExposed( int iNode, CBaseEntity *pThreat );

	// Where a standing human at this node would have his eyes
	static Vector	GetNodeEyePosition( int iNode );

	virtual void	FrameUpdatePostEntityThink();
	virtual void	LevelShutdownPostEntity();

private:
	struct Entry_t
	{
		float	flTime;
		Vector	vecThreatEye;	// where the player's eyes were for this test
		bool	bVisible;
	};

	bool			EnsureSize();
	bool			IsEntryFresh( const Entry_t &entry, const Vector &vecThreatEye ) const;
	bool			TraceExposure( int iNode, const Vector &vecThreatEye, CBaseEntity *pThreat );

	CUtlVector<Entry_t>	m_PlayerEntries;
	int					m_iNextNode;
};

extern CAI_ThreatMap g_AI_ThreatMap;

#endif // AI_THREATMAP_H
