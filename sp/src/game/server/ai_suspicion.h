//========= Half-Life: BB ============//
//
// Purpose: Local suspicion of the pre-criminal player.
//
// While the "gordon_precriminal" global is on, Combine NPCs that carry a
// CAI_Suspicion don't treat the player as an enemy until they've personally
// grown suspicious enough. Each NPC has its own level (0..1):
//
//   calm (< ai_suspicion_threshold) -> suspicious -> hostile (1.0)
//
// Suspicion rises while the NPC sees the player holding a weapon or standing in
// a trigger_restricted_area, and jumps when the player hurts it. It decays when
// nothing suspicious happens. Going hostile is shared with the NPC's squad at
// once and with other participants in radio range after a short delay. A
// hostile NPC that hasn't seen the player for ai_suspicion_forget_time (and
// hasn't heard about him on the radio) cools down, so the player can escape.
//
// The global turned off means the classic behavior: everyone hates the player.
//
//=============================================================================//

#ifndef AI_SUSPICION_H
#define AI_SUSPICION_H
#ifdef _WIN32
#pragma once
#endif

#include "triggers.h"

class CAI_BaseNPC;
class CBasePlayer;

enum SuspicionState_t
{
	SUSPICION_CALM = 0,
	SUSPICION_SUSPICIOUS,
	SUSPICION_HOSTILE,
};

//-----------------------------------------------------------------------------
// Per-NPC suspicion of the player. Embed in an NPC, call Init() from its
// constructor, Update() from PrescheduleThink() and OnDamaged() from
// OnTakeDamage_Alive(), and return it from CAI_BaseNPC::GetSuspicion().
//-----------------------------------------------------------------------------
class CAI_Suspicion
{
public:
	DECLARE_SIMPLE_DATADESC();

	CAI_Suspicion();

	void				Init( CAI_BaseNPC *pOuter )		{ m_pOuter = pOuter; }

	// True while the system runs: precriminal global on, ai_suspicion on, not disabled by a manager
	static bool			IsSystemActive();

	void				Update();
	void				OnDamaged( const CTakeDamageInfo &info );

	SuspicionState_t	GetState() const				{ return (SuspicionState_t)m_iState; }
	float				GetLevel() const				{ return m_flLevel; }

	// The player counts as an enemy for this NPC. Always false while the system is inactive,
	// callers keep their own pre-criminal handling for that case.
	bool				IsPlayerCriminal() const		{ return m_iState == SUSPICION_HOSTILE; }

	void				AddSuspicion( float flAmount );
	void				BecomeHostile( CBaseEntity *pInformer, const Vector &vecPlayerPos, bool bBroadcast );
	void				QueueHostile( float flDelay, const Vector &vecPlayerPos );
	void				Calm();

	// Someone on the radio still sees the player: keep hunting
	void				ReportSighting( CBaseEntity *pInformer, const Vector &vecPlayerPos );

private:
	void				SetLevel( float flLevel );
	void				SetState( SuspicionState_t newState );
	void				CoolDown();
	void				BroadcastHostile( const Vector &vecPlayerPos );
	void				ShareSuspicionWithSquad();
	void				RadioSighting( const Vector &vecPlayerPos );
	void				DrawDebug( bool bSeesPlayer, float flRate );

	CAI_BaseNPC			*m_pOuter;

	float				m_flLevel;
	int					m_iState;
	float				m_flLastUpdateTime;
	float				m_flLastStimulusTime;	// last time something raised the level
	float				m_flLastSightingTime;	// hostile: last time we (or the radio) saw the player
	float				m_flNextRadioTime;		// hostile: next time we report a sighting
	float				m_flPendingHostileTime;	// radio call queued for this time (0 = none)
	Vector				m_vecPendingPlayerPos;
};

//-----------------------------------------------------------------------------
// Optional map entity: switches the system and fires outputs for the whole map.
//-----------------------------------------------------------------------------
class CAI_SuspicionManager : public CLogicalEntity
{
	DECLARE_CLASS( CAI_SuspicionManager, CLogicalEntity );
	DECLARE_DATADESC();

public:
	static CAI_SuspicionManager *GetManager();

	virtual void	Activate();
	virtual void	UpdateOnRemove();

	bool			IsEnabled() const	{ return !m_bDisabled; }

	// Called by CAI_Suspicion on state changes
	void			OnNPCStateChanged( CAI_BaseNPC *pNPC, SuspicionState_t oldState, SuspicionState_t newState );

private:
	void			InputEnable( inputdata_t &inputdata );
	void			InputDisable( inputdata_t &inputdata );
	void			InputMakePlayerHostile( inputdata_t &inputdata );
	void			InputCalmAll( inputdata_t &inputdata );

	bool			m_bDisabled;
	int				m_iAlertLevel;	// highest state among participants, for the outputs

	COutputEvent	m_OnPlayerSuspicious;
	COutputEvent	m_OnPlayerHostile;
	COutputEvent	m_OnAllCalm;
};

//-----------------------------------------------------------------------------
// Brush entity: Combine who see the player inside grow suspicious (or turn
// hostile at once with the Hostile key).
//-----------------------------------------------------------------------------
class CTriggerRestrictedArea : public CBaseTrigger
{
	DECLARE_CLASS( CTriggerRestrictedArea, CBaseTrigger );
	DECLARE_DATADESC();

public:
	CTriggerRestrictedArea();
	~CTriggerRestrictedArea();

	void			Spawn();

	// Finds an enabled area the player is in. Returns NULL if none.
	static CTriggerRestrictedArea *FindAreaContaining( CBasePlayer *pPlayer );

	float			GetSuspicionRate() const	{ return m_flSuspicionRate; }
	bool			IsInstantHostile() const	{ return m_bHostile; }

	void			OnSpotted( CAI_BaseNPC *pNPC );

private:
	float			m_flSuspicionRate;
	bool			m_bHostile;
	float			m_flNextSpottedOutputTime;

	COutputEvent	m_OnPlayerSpotted;
};

#endif // AI_SUSPICION_H
