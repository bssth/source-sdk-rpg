//========= Half-Life: BB ============//
//
// Purpose: Local suspicion of the pre-criminal player. See ai_suspicion.h.
//
//=============================================================================//

#include "cbase.h"
#include "ai_suspicion.h"
#include "ai_basenpc.h"
#include "ai_senses.h"
#include "ai_memory.h"
#include "ai_squad.h"
#include "ndebugoverlay.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar ai_suspicion( "ai_suspicion", "1", FCVAR_NONE, "Combine judge the player by what they see. 0 = they always treat him as an enemy (classic HL2)." );
ConVar ai_suspicion_debug( "ai_suspicion_debug", "0", FCVAR_NONE, "Show each NPC's suspicion of the player above its head." );
ConVar ai_suspicion_threshold( "ai_suspicion_threshold", "0.35", FCVAR_NONE, "Level (0..1) at which an NPC becomes suspicious. 1 is hostile." );
ConVar ai_suspicion_rate_weapon( "ai_suspicion_rate_weapon", "0.4", FCVAR_NONE, "Suspicion per second while an NPC sees the player holding a weapon up close." );
ConVar ai_suspicion_near_dist( "ai_suspicion_near_dist", "256", FCVAR_NONE, "Within this distance suspicion rises at the full rate." );
ConVar ai_suspicion_far_scale( "ai_suspicion_far_scale", "0.25", FCVAR_NONE, "Rate multiplier at the edge of the NPC's sight (linear from ai_suspicion_near_dist)." );
ConVar ai_suspicion_decay( "ai_suspicion_decay", "0.08", FCVAR_NONE, "Suspicion lost per second once nothing suspicious has happened for ai_suspicion_decay_delay." );
ConVar ai_suspicion_decay_delay( "ai_suspicion_decay_delay", "4", FCVAR_NONE );
ConVar ai_suspicion_damage( "ai_suspicion_damage", "0.5", FCVAR_NONE, "Suspicion added when the player hurts an NPC with a club or a physics object. Bullets, blasts and fire turn it hostile at once." );
ConVar ai_suspicion_forget_time( "ai_suspicion_forget_time", "30", FCVAR_NONE, "A hostile NPC calms down after this long without itself or the radio seeing the player." );
ConVar ai_suspicion_radio_radius( "ai_suspicion_radio_radius", "1500", FCVAR_NONE, "Other participants within this distance hear radio calls." );
ConVar ai_suspicion_radio_delay_min( "ai_suspicion_radio_delay_min", "1.0", FCVAR_NONE );
ConVar ai_suspicion_radio_delay_max( "ai_suspicion_radio_delay_max", "2.5", FCVAR_NONE );
ConVar ai_suspicion_radio_interval( "ai_suspicion_radio_interval", "2.0", FCVAR_NONE, "How often a hostile NPC that sees the player reports his position on the radio." );
ConVar ai_suspicion_squad_share( "ai_suspicion_squad_share", "0.5", FCVAR_NONE, "A newly suspicious NPC raises its squadmates to at least this fraction of its level." );
ConVar ai_suspicion_weapon_ignore( "ai_suspicion_weapon_ignore", "weapon_citizenpackage weapon_citizensuitcase weapon_cubemap", FCVAR_NONE, "Space-separated weapons that don't look suspicious." );

// Difficulty: multiplier on how fast suspicion rises (_1 easy, _2 normal, _3 hard)
ConVar sk_ai_suspicion_scale_1( "sk_ai_suspicion_scale_1", "0.7" );
ConVar sk_ai_suspicion_scale_2( "sk_ai_suspicion_scale_2", "1.0" );
ConVar sk_ai_suspicion_scale_3( "sk_ai_suspicion_scale_3", "1.4" );

// Damage that can't be mistaken for an accident
#define SUSPICION_LETHAL_DAMAGE ( DMG_BULLET | DMG_BUCKSHOT | DMG_BLAST | DMG_BURN | DMG_SLASH | DMG_ENERGYBEAM | DMG_PLASMA | DMG_DISSOLVE | DMG_SHOCK | DMG_ACID | DMG_POISON )

static float SuspicionSkillScale()
{
	switch ( g_pGameRules ? g_pGameRules->GetSkillLevel() : SKILL_MEDIUM )
	{
	case SKILL_EASY:	return sk_ai_suspicion_scale_1.GetFloat();
	case SKILL_HARD:	return sk_ai_suspicion_scale_3.GetFloat();
	default:			return sk_ai_suspicion_scale_2.GetFloat();
	}
}

static bool IsWeaponIgnored( const char *pszClassname )
{
	const char *pszList = ai_suspicion_weapon_ignore.GetString();
	int nNameLen = V_strlen( pszClassname );

	while ( *pszList )
	{
		while ( *pszList == ' ' )
			pszList++;

		const char *pszEnd = pszList;
		while ( *pszEnd && *pszEnd != ' ' )
			pszEnd++;

		if ( pszEnd - pszList == nNameLen && !V_strnicmp( pszList, pszClassname, nNameLen ) )
			return true;

		pszList = pszEnd;
	}

	return false;
}

static bool IsPlayerVisiblyArmed( CBasePlayer *pPlayer )
{
	CBaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();
	if ( !pWeapon || !pWeapon->IsWeaponVisible() )
		return false;

	return !IsWeaponIgnored( pWeapon->GetClassname() );
}

//=============================================================================
// CAI_Suspicion
//=============================================================================

BEGIN_SIMPLE_DATADESC( CAI_Suspicion )
	DEFINE_FIELD( m_flLevel,				FIELD_FLOAT ),
	DEFINE_FIELD( m_iState,					FIELD_INTEGER ),
	DEFINE_FIELD( m_flLastUpdateTime,		FIELD_TIME ),
	DEFINE_FIELD( m_flLastStimulusTime,		FIELD_TIME ),
	DEFINE_FIELD( m_flLastSightingTime,		FIELD_TIME ),
	DEFINE_FIELD( m_flNextRadioTime,		FIELD_TIME ),
	DEFINE_FIELD( m_flPendingHostileTime,	FIELD_TIME ),
	DEFINE_FIELD( m_vecPendingPlayerPos,	FIELD_POSITION_VECTOR ),
END_DATADESC()

CAI_Suspicion::CAI_Suspicion()
{
	m_pOuter = NULL;
	m_flLevel = 0.0f;
	m_iState = SUSPICION_CALM;
	m_flLastUpdateTime = 0.0f;
	m_flLastStimulusTime = 0.0f;
	m_flLastSightingTime = 0.0f;
	m_flNextRadioTime = 0.0f;
	m_flPendingHostileTime = 0.0f;
	m_vecPendingPlayerPos = vec3_origin;
}

bool CAI_Suspicion::IsSystemActive()
{
	if ( !ai_suspicion.GetBool() )
		return false;

	CAI_SuspicionManager *pManager = CAI_SuspicionManager::GetManager();
	return !pManager || pManager->IsEnabled();
}

bool CAI_Suspicion::IsPlayerWantedNear( const Vector &vecPos )
{
	const float flRadiusSqr = Square( ai_suspicion_radio_radius.GetFloat() );
	CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
	for ( int i = 0; i < g_AI_Manager.NumAIs(); i++ )
	{
		CAI_BaseNPC *pNPC = ppAIs[i];
		CAI_Suspicion *pSuspicion = pNPC->GetSuspicion();
		if ( pSuspicion && pSuspicion->IsPlayerCriminal() && pNPC->IsAlive() &&
			 ( pNPC->GetAbsOrigin() - vecPos ).LengthSqr() <= flRadiusSqr )
		{
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Called once per NPC think
//-----------------------------------------------------------------------------
void CAI_Suspicion::Update()
{
	if ( !m_pOuter )
		return;

	const float flNow = gpGlobals->curtime;
	const float flDelta = clamp( flNow - m_flLastUpdateTime, 0.0f, 1.0f );
	m_flLastUpdateTime = flNow;

	if ( !IsSystemActive() )
	{
		// Switched off: everyone hates the player anyway, just forget quietly
		m_flLevel = 0.0f;
		m_iState = SUSPICION_CALM;
		m_flPendingHostileTime = 0.0f;
		return;
	}

	CBasePlayer *pPlayer = AI_GetSinglePlayer();
	if ( !pPlayer || !pPlayer->IsAlive() || !m_pOuter->IsAlive() )
		return;

	// A radio call reached us
	if ( m_flPendingHostileTime != 0.0f && flNow >= m_flPendingHostileTime )
	{
		m_flPendingHostileTime = 0.0f;
		if ( m_iState != SUSPICION_HOSTILE )
			BecomeHostile( NULL, m_vecPendingPlayerPos, false );
	}

	// Scripts are in charge of scripted NPCs
	if ( m_pOuter->IsInAScript() || m_pOuter->GetState() == NPC_STATE_SCRIPT )
		return;

	const bool bSeesPlayer = m_pOuter->GetSenses() && m_pOuter->GetSenses()->DidSeeEntity( pPlayer );

	if ( m_iState == SUSPICION_HOSTILE )
	{
		if ( bSeesPlayer )
		{
			m_flLastSightingTime = flNow;
			if ( flNow >= m_flNextRadioTime )
			{
				m_flNextRadioTime = flNow + ai_suspicion_radio_interval.GetFloat();
				RadioSighting( pPlayer->GetAbsOrigin() );
			}
		}
		else if ( flNow - m_flLastSightingTime > ai_suspicion_forget_time.GetFloat() )
		{
			CoolDown();
		}

		DrawDebug( bSeesPlayer, 0.0f );
		return;
	}

	float flRate = 0.0f;
	if ( bSeesPlayer )
	{
		const float flNear = ai_suspicion_near_dist.GetFloat();
		const float flFar = MAX( m_pOuter->GetSenses()->GetDistLook(), flNear + 1.0f );
		const float flDist = ( pPlayer->WorldSpaceCenter() - m_pOuter->EyePosition() ).Length();
		const float flDistScale = RemapValClamped( flDist, flNear, flFar, 1.0f, ai_suspicion_far_scale.GetFloat() );

		if ( IsPlayerVisiblyArmed( pPlayer ) )
			flRate += ai_suspicion_rate_weapon.GetFloat();

		CTriggerRestrictedArea *pArea = CTriggerRestrictedArea::FindAreaContaining( pPlayer );
		if ( pArea )
		{
			pArea->OnSpotted( m_pOuter );

			if ( pArea->IsInstantHostile() )
			{
				BecomeHostile( m_pOuter, pPlayer->GetAbsOrigin(), true );
				DrawDebug( bSeesPlayer, 0.0f );
				return;
			}

			flRate += pArea->GetSuspicionRate();
		}

		flRate *= flDistScale * SuspicionSkillScale();
	}

	if ( flRate > 0.0f )
	{
		m_flLastStimulusTime = flNow;
		AddSuspicion( flRate * flDelta );
	}
	else if ( m_flLevel > 0.0f && flNow - m_flLastStimulusTime > ai_suspicion_decay_delay.GetFloat() )
	{
		SetLevel( m_flLevel - ai_suspicion_decay.GetFloat() * flDelta );
	}

	DrawDebug( bSeesPlayer, flRate );
}

//-----------------------------------------------------------------------------
void CAI_Suspicion::OnDamaged( const CTakeDamageInfo &info )
{
	if ( !m_pOuter || !IsSystemActive() )
		return;

	CBaseEntity *pAttacker = info.GetAttacker();
	if ( !pAttacker || !pAttacker->IsPlayer() )
		return;

	if ( info.GetDamageType() & SUSPICION_LETHAL_DAMAGE )
	{
		BecomeHostile( m_pOuter, pAttacker->GetAbsOrigin(), true );
	}
	else
	{
		m_flLastStimulusTime = gpGlobals->curtime;
		AddSuspicion( ai_suspicion_damage.GetFloat() );
	}
}

//-----------------------------------------------------------------------------
void CAI_Suspicion::AddSuspicion( float flAmount )
{
	if ( m_iState == SUSPICION_HOSTILE )
		return;

	SetLevel( m_flLevel + flAmount );
}

void CAI_Suspicion::SetLevel( float flLevel )
{
	m_flLevel = clamp( flLevel, 0.0f, 1.0f );

	if ( m_iState == SUSPICION_HOSTILE )
		return;

	const float flThreshold = ai_suspicion_threshold.GetFloat();

	if ( m_flLevel >= 1.0f )
	{
		CBasePlayer *pPlayer = AI_GetSinglePlayer();
		BecomeHostile( m_pOuter, pPlayer ? pPlayer->GetAbsOrigin() : m_pOuter->GetAbsOrigin(), true );
	}
	else if ( m_flLevel >= flThreshold )
	{
		SetState( SUSPICION_SUSPICIOUS );
	}
	else if ( m_iState == SUSPICION_CALM || m_flLevel < flThreshold * 0.5f )
	{
		// Some hysteresis so a suspicious NPC doesn't flicker back and forth
		SetState( SUSPICION_CALM );
	}
}

void CAI_Suspicion::SetState( SuspicionState_t newState )
{
	if ( m_iState == newState )
		return;

	SuspicionState_t oldState = (SuspicionState_t)m_iState;
	m_iState = newState;

	m_pOuter->OnSuspicionStateChanged( oldState, newState );

	if ( oldState == SUSPICION_CALM && newState == SUSPICION_SUSPICIOUS )
		ShareSuspicionWithSquad();

	CAI_SuspicionManager *pManager = CAI_SuspicionManager::GetManager();
	if ( pManager )
		pManager->OnNPCStateChanged( m_pOuter, oldState, newState );
}

//-----------------------------------------------------------------------------
void CAI_Suspicion::BecomeHostile( CBaseEntity *pInformer, const Vector &vecPlayerPos, bool bBroadcast )
{
	const bool bWasHostile = ( m_iState == SUSPICION_HOSTILE );

	m_flLevel = 1.0f;
	m_flPendingHostileTime = 0.0f;
	m_flLastSightingTime = gpGlobals->curtime;
	m_flNextRadioTime = gpGlobals->curtime + ai_suspicion_radio_interval.GetFloat();

	// State first: the relationship has to say "hate" before we learn where the enemy is
	SetState( SUSPICION_HOSTILE );

	CBasePlayer *pPlayer = AI_GetSinglePlayer();
	if ( pPlayer )
		m_pOuter->UpdateEnemyMemory( pPlayer, vecPlayerPos, pInformer );

	if ( bBroadcast && !bWasHostile )
		BroadcastHostile( vecPlayerPos );
}

void CAI_Suspicion::QueueHostile( float flDelay, const Vector &vecPlayerPos )
{
	if ( m_iState == SUSPICION_HOSTILE )
		return;

	float flTime = gpGlobals->curtime + flDelay;
	if ( m_flPendingHostileTime == 0.0f || flTime < m_flPendingHostileTime )
		m_flPendingHostileTime = flTime;

	m_vecPendingPlayerPos = vecPlayerPos;
}

void CAI_Suspicion::ReportSighting( CBaseEntity *pInformer, const Vector &vecPlayerPos )
{
	if ( m_iState != SUSPICION_HOSTILE )
		return;

	m_flLastSightingTime = gpGlobals->curtime;

	CBasePlayer *pPlayer = AI_GetSinglePlayer();
	if ( pPlayer )
		m_pOuter->UpdateEnemyMemory( pPlayer, vecPlayerPos, pInformer );
}

//-----------------------------------------------------------------------------
// Hostile -> suspicious after losing the player for a while
//-----------------------------------------------------------------------------
void CAI_Suspicion::CoolDown()
{
	const float flThreshold = ai_suspicion_threshold.GetFloat();

	m_flLevel = flThreshold + ( 1.0f - flThreshold ) * 0.5f;
	m_flLastStimulusTime = gpGlobals->curtime;
	m_flPendingHostileTime = 0.0f;

	SetState( SUSPICION_SUSPICIOUS );

	CBasePlayer *pPlayer = AI_GetSinglePlayer();
	if ( pPlayer )
	{
		if ( m_pOuter->GetEnemy() == pPlayer )
			m_pOuter->SetEnemy( NULL );
		if ( m_pOuter->GetEnemies() )
			m_pOuter->GetEnemies()->ClearMemory( pPlayer );
	}

	if ( m_pOuter->GetState() == NPC_STATE_COMBAT )
		m_pOuter->SetIdealState( NPC_STATE_ALERT );
}

void CAI_Suspicion::Calm()
{
	const bool bWasHostile = ( m_iState == SUSPICION_HOSTILE );

	m_flLevel = 0.0f;
	m_flPendingHostileTime = 0.0f;
	SetState( SUSPICION_CALM );

	if ( bWasHostile )
	{
		CBasePlayer *pPlayer = AI_GetSinglePlayer();
		if ( pPlayer )
		{
			if ( m_pOuter->GetEnemy() == pPlayer )
				m_pOuter->SetEnemy( NULL );
			if ( m_pOuter->GetEnemies() )
				m_pOuter->GetEnemies()->ClearMemory( pPlayer );
		}

		if ( m_pOuter->GetState() == NPC_STATE_COMBAT )
			m_pOuter->SetIdealState( NPC_STATE_ALERT );
	}
}

//-----------------------------------------------------------------------------
// Squad hears it at once, everyone else in radio range a moment later
//-----------------------------------------------------------------------------
void CAI_Suspicion::BroadcastHostile( const Vector &vecPlayerPos )
{
	CAI_Squad *pSquad = m_pOuter->GetSquad();
	if ( pSquad )
	{
		AISquadIter_t iter;
		for ( CAI_BaseNPC *pMember = pSquad->GetFirstMember( &iter, false ); pMember; pMember = pSquad->GetNextMember( &iter, false ) )
		{
			if ( pMember == m_pOuter )
				continue;

			CAI_Suspicion *pSuspicion = pMember->GetSuspicion();
			if ( pSuspicion && pSuspicion->GetState() != SUSPICION_HOSTILE )
				pSuspicion->BecomeHostile( m_pOuter, vecPlayerPos, false );
		}
	}

	const float flRadiusSqr = Square( ai_suspicion_radio_radius.GetFloat() );
	CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
	for ( int i = 0; i < g_AI_Manager.NumAIs(); i++ )
	{
		CAI_BaseNPC *pNPC = ppAIs[i];
		if ( pNPC == m_pOuter || !pNPC->IsAlive() || ( pSquad && pNPC->GetSquad() == pSquad ) )
			continue;

		CAI_Suspicion *pSuspicion = pNPC->GetSuspicion();
		if ( !pSuspicion || pSuspicion->GetState() == SUSPICION_HOSTILE )
			continue;

		if ( ( pNPC->GetAbsOrigin() - m_pOuter->GetAbsOrigin() ).LengthSqr() > flRadiusSqr )
			continue;

		pSuspicion->QueueHostile( RandomFloat( ai_suspicion_radio_delay_min.GetFloat(), ai_suspicion_radio_delay_max.GetFloat() ), vecPlayerPos );
	}
}

void CAI_Suspicion::RadioSighting( const Vector &vecPlayerPos )
{
	const float flRadiusSqr = Square( ai_suspicion_radio_radius.GetFloat() );
	CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
	for ( int i = 0; i < g_AI_Manager.NumAIs(); i++ )
	{
		CAI_BaseNPC *pNPC = ppAIs[i];
		if ( pNPC == m_pOuter || !pNPC->IsAlive() )
			continue;

		CAI_Suspicion *pSuspicion = pNPC->GetSuspicion();
		if ( !pSuspicion || pSuspicion->GetState() != SUSPICION_HOSTILE )
			continue;

		if ( ( pNPC->GetAbsOrigin() - m_pOuter->GetAbsOrigin() ).LengthSqr() > flRadiusSqr )
			continue;

		pSuspicion->ReportSighting( m_pOuter, vecPlayerPos );
	}
}

void CAI_Suspicion::ShareSuspicionWithSquad()
{
	CAI_Squad *pSquad = m_pOuter->GetSquad();
	if ( !pSquad )
		return;

	const float flShared = m_flLevel * ai_suspicion_squad_share.GetFloat();

	AISquadIter_t iter;
	for ( CAI_BaseNPC *pMember = pSquad->GetFirstMember( &iter, false ); pMember; pMember = pSquad->GetNextMember( &iter, false ) )
	{
		if ( pMember == m_pOuter )
			continue;

		// Only raise calm members, so this can't bounce back and forth
		CAI_Suspicion *pSuspicion = pMember->GetSuspicion();
		if ( pSuspicion && pSuspicion->GetState() == SUSPICION_CALM && pSuspicion->GetLevel() < flShared )
		{
			pSuspicion->m_flLastStimulusTime = gpGlobals->curtime;
			pSuspicion->SetLevel( flShared );
		}
	}
}

void CAI_Suspicion::DrawDebug( bool bSeesPlayer, float flRate )
{
	if ( !ai_suspicion_debug.GetBool() )
		return;

	static const char *s_pszStates[] = { "calm", "SUSPICIOUS", "HOSTILE" };

	int r = 0, g = 255, b = 0;
	if ( m_iState == SUSPICION_SUSPICIOUS )
		r = 255;
	else if ( m_iState == SUSPICION_HOSTILE )
		g = 0, r = 255;

	char szText[128];
	if ( m_iState == SUSPICION_HOSTILE )
	{
		V_snprintf( szText, sizeof( szText ), "%s  forget in %.0fs%s", s_pszStates[m_iState],
			MAX( 0.0f, ai_suspicion_forget_time.GetFloat() - ( gpGlobals->curtime - m_flLastSightingTime ) ),
			bSeesPlayer ? "  [sees]" : "" );
	}
	else
	{
		V_snprintf( szText, sizeof( szText ), "%s  %.2f%s%s", s_pszStates[m_iState], m_flLevel,
			flRate > 0.0f ? "  +" : "", bSeesPlayer ? "  [sees]" : "" );
	}

	NDebugOverlay::EntityText( m_pOuter->entindex(), 0, szText, 0.25f, r, g, b, 255 );
}

//=============================================================================
// ai_suspicion_manager
//=============================================================================

LINK_ENTITY_TO_CLASS( ai_suspicion_manager, CAI_SuspicionManager );

BEGIN_DATADESC( CAI_SuspicionManager )
	DEFINE_KEYFIELD( m_bDisabled, FIELD_BOOLEAN, "StartDisabled" ),
	DEFINE_FIELD( m_iAlertLevel, FIELD_INTEGER ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "MakePlayerHostile", InputMakePlayerHostile ),
	DEFINE_INPUTFUNC( FIELD_VOID, "CalmAll", InputCalmAll ),

	DEFINE_OUTPUT( m_OnPlayerSuspicious, "OnPlayerSuspicious" ),
	DEFINE_OUTPUT( m_OnPlayerHostile, "OnPlayerHostile" ),
	DEFINE_OUTPUT( m_OnAllCalm, "OnAllCalm" ),
END_DATADESC()

static CHandle<CAI_SuspicionManager> s_hSuspicionManager;

CAI_SuspicionManager *CAI_SuspicionManager::GetManager()
{
	return s_hSuspicionManager.Get();
}

void CAI_SuspicionManager::Activate()
{
	BaseClass::Activate();

	if ( s_hSuspicionManager.Get() && s_hSuspicionManager.Get() != this )
		Warning( "More than one ai_suspicion_manager in the map, only %s is used\n", GetDebugName() );

	s_hSuspicionManager = this;
}

void CAI_SuspicionManager::UpdateOnRemove()
{
	if ( s_hSuspicionManager.Get() == this )
		s_hSuspicionManager = NULL;

	BaseClass::UpdateOnRemove();
}

void CAI_SuspicionManager::OnNPCStateChanged( CAI_BaseNPC *pNPC, SuspicionState_t oldState, SuspicionState_t newState )
{
	int iHighest = SUSPICION_CALM;
	CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
	for ( int i = 0; i < g_AI_Manager.NumAIs(); i++ )
	{
		CAI_Suspicion *pSuspicion = ppAIs[i]->IsAlive() ? ppAIs[i]->GetSuspicion() : NULL;
		if ( pSuspicion )
			iHighest = MAX( iHighest, (int)pSuspicion->GetState() );
	}

	if ( iHighest >= SUSPICION_SUSPICIOUS && m_iAlertLevel == SUSPICION_CALM )
		m_OnPlayerSuspicious.FireOutput( pNPC, this );

	if ( iHighest == SUSPICION_HOSTILE && m_iAlertLevel != SUSPICION_HOSTILE )
		m_OnPlayerHostile.FireOutput( pNPC, this );

	if ( iHighest == SUSPICION_CALM && m_iAlertLevel != SUSPICION_CALM )
		m_OnAllCalm.FireOutput( pNPC, this );

	m_iAlertLevel = iHighest;
}

void CAI_SuspicionManager::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;
}

void CAI_SuspicionManager::InputDisable( inputdata_t &inputdata )
{
	m_bDisabled = true;
}

void CAI_SuspicionManager::InputMakePlayerHostile( inputdata_t &inputdata )
{
	CBasePlayer *pPlayer = AI_GetSinglePlayer();
	if ( !pPlayer || !CAI_Suspicion::IsSystemActive() )
		return;

	CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
	for ( int i = 0; i < g_AI_Manager.NumAIs(); i++ )
	{
		CAI_Suspicion *pSuspicion = ppAIs[i]->IsAlive() ? ppAIs[i]->GetSuspicion() : NULL;
		if ( pSuspicion )
			pSuspicion->BecomeHostile( this, pPlayer->GetAbsOrigin(), false );
	}
}

void CAI_SuspicionManager::InputCalmAll( inputdata_t &inputdata )
{
	CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
	for ( int i = 0; i < g_AI_Manager.NumAIs(); i++ )
	{
		CAI_Suspicion *pSuspicion = ppAIs[i]->GetSuspicion();
		if ( pSuspicion )
			pSuspicion->Calm();
	}
}

//=============================================================================
// trigger_restricted_area
//=============================================================================

LINK_ENTITY_TO_CLASS( trigger_restricted_area, CTriggerRestrictedArea );

BEGIN_DATADESC( CTriggerRestrictedArea )
	DEFINE_KEYFIELD( m_flSuspicionRate, FIELD_FLOAT, "SuspicionRate" ),
	DEFINE_KEYFIELD( m_bHostile, FIELD_BOOLEAN, "Hostile" ),
	DEFINE_FIELD( m_flNextSpottedOutputTime, FIELD_TIME ),

	DEFINE_OUTPUT( m_OnPlayerSpotted, "OnPlayerSpotted" ),
END_DATADESC()

static CUtlVector<CTriggerRestrictedArea *> s_RestrictedAreas;

CTriggerRestrictedArea::CTriggerRestrictedArea()
{
	m_flSuspicionRate = 0.5f;
	m_bHostile = false;
	m_flNextSpottedOutputTime = 0.0f;
	s_RestrictedAreas.AddToTail( this );
}

CTriggerRestrictedArea::~CTriggerRestrictedArea()
{
	s_RestrictedAreas.FindAndRemove( this );
}

void CTriggerRestrictedArea::Spawn()
{
	BaseClass::Spawn();
	InitTrigger();
}

CTriggerRestrictedArea *CTriggerRestrictedArea::FindAreaContaining( CBasePlayer *pPlayer )
{
	for ( int i = 0; i < s_RestrictedAreas.Count(); i++ )
	{
		CTriggerRestrictedArea *pArea = s_RestrictedAreas[i];
		if ( !pArea->m_bDisabled && pArea->IsTouching( pPlayer ) )
			return pArea;
	}

	return NULL;
}

void CTriggerRestrictedArea::OnSpotted( CAI_BaseNPC *pNPC )
{
	// Once per sighting rather than every think
	if ( gpGlobals->curtime < m_flNextSpottedOutputTime )
		return;

	m_flNextSpottedOutputTime = gpGlobals->curtime + 3.0f;
	m_OnPlayerSpotted.FireOutput( pNPC, this );
}

//-----------------------------------------------------------------------------
CON_COMMAND( ai_suspicion_dump, "Print every NPC's suspicion of the player." )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	static const char *s_pszStates[] = { "calm", "suspicious", "HOSTILE" };

	CBasePlayer *pPlayer = AI_GetSinglePlayer();
	Msg( "ai_suspicion_dump at %.1f: system %s\n", gpGlobals->curtime, CAI_Suspicion::IsSystemActive() ? "active" : "inactive" );

	int nCount = 0;
	CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
	for ( int i = 0; i < g_AI_Manager.NumAIs(); i++ )
	{
		CAI_BaseNPC *pNPC = ppAIs[i];
		CAI_Suspicion *pSuspicion = pNPC->GetSuspicion();
		if ( !pSuspicion )
			continue;

		nCount++;
		Msg( "  %-28s %-10s %.2f  dist %5.0f  %s%s\n", pNPC->GetDebugName(), s_pszStates[pSuspicion->GetState()], pSuspicion->GetLevel(),
			pPlayer ? ( pNPC->GetAbsOrigin() - pPlayer->GetAbsOrigin() ).Length() : 0.0f,
			( pPlayer && pNPC->GetSenses() && pNPC->GetSenses()->DidSeeEntity( pPlayer ) ) ? "sees player" : "",
			pNPC->IsAlive() ? "" : " (dead)" );
	}

	Msg( "  %d participating NPCs\n", nCount );
}
