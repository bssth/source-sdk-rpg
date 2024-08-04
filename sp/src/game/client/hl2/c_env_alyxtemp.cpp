//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "particles_simple.h"
#include "citadel_effects_shared.h"
#include "particles_attractor.h"

class C_AlyxEmpEffect : public C_BaseEntity
{
	DECLARE_CLASS( C_AlyxEmpEffect, C_BaseEntity );
	DECLARE_CLIENTCLASS();

public:
	void			OnDataChanged( DataUpdateType_t updateType );
	RenderGroup_t	GetRenderGroup( void );

	void			ClientThink( void );
	void			NotifyShouldTransmit( ShouldTransmitState_t state );

	void			UpdateCharging( float percentage );
	void			UpdateDischarging( void );

private:	

	bool			SetupEmitters( void );
	inline float	GetStateDurationPercentage( void );

	int				m_nState;
	float			m_flDuration;
	float			m_flStartTime;
	TimedEvent		m_tParticleSpawn;
	
	CSmartPtr<CSimpleEmitter>		m_pSimpleEmitter;
	CSmartPtr<CParticleAttractor>	m_pAttractorEmitter;
};

IMPLEMENT_CLIENTCLASS_DT( C_AlyxEmpEffect, DT_AlyxEmpEffect, CAlyxEmpEffect )
	RecvPropInt( RECVINFO(m_nState) ),
	RecvPropFloat( RECVINFO(m_flDuration) ),
	RecvPropFloat( RECVINFO(m_flStartTime) ),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
// Output : RenderGroup_t
//-----------------------------------------------------------------------------
RenderGroup_t C_AlyxEmpEffect::GetRenderGroup( void )
{
	return RENDER_GROUP_TRANSLUCENT_ENTITY;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : updateType - 
//-----------------------------------------------------------------------------
void C_AlyxEmpEffect::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		m_tParticleSpawn.Init( 32 );
		SetNextClientThink( CLIENT_THINK_ALWAYS );
		SetupEmitters();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_AlyxEmpEffect::SetupEmitters( void )
{
	// Setup the basic core emitter
	if ( m_pSimpleEmitter.IsValid() == false )
	{
		m_pSimpleEmitter = CSimpleEmitter::Create( "energycore" );

		if ( m_pSimpleEmitter.IsValid() == false )
			return false;
	}

	// Setup the attractor emitter
	if ( m_pAttractorEmitter.IsValid() == false )
	{
		m_pAttractorEmitter = CParticleAttractor::Create( GetAbsOrigin(), "energyattractor" );

		if ( m_pAttractorEmitter.IsValid() == false )
			return false;
	}

	return true;
}

#define EMP_SCALE	0.5f

#define	EMP_PARTICLES "effects/ar2_altfire1b"

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : percentage - 
//-----------------------------------------------------------------------------
void C_AlyxEmpEffect::UpdateCharging( float percentage )
{
	// Emitters must be valid
	if ( SetupEmitters() == false )
		return;

	if ( percentage <= 0.0f )
		return;

	// Reset our sort origin
	m_pSimpleEmitter->SetSortOrigin( GetAbsOrigin() );

	float flScale = 4.0f * EMP_SCALE * percentage;

	SimpleParticle *sParticle;

	float dTime = gpGlobals->frametime;
	
	while ( m_tParticleSpawn.NextEvent( dTime ) )
	{
		// Do the core effects
		sParticle = (SimpleParticle *) m_pSimpleEmitter->AddParticle( sizeof(SimpleParticle), m_pSimpleEmitter->GetPMaterial( EMP_PARTICLES ), GetAbsOrigin() );

		if ( sParticle == NULL )
			return;
		
		sParticle->m_vecVelocity	= vec3_origin;
		sParticle->m_flDieTime		= 0.1f;
		sParticle->m_flLifetime		= 0.0f;

		sParticle->m_flRoll			= Helper_RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= 0.0f;

		float alpha = 255 * percentage;

		sParticle->m_uchColor[0]	= alpha;
		sParticle->m_uchColor[1]	= alpha;
		sParticle->m_uchColor[2]	= alpha;
		sParticle->m_uchStartAlpha	= alpha;
		sParticle->m_uchEndAlpha	= 0;

		sParticle->m_uchStartSize	= flScale;
		sParticle->m_uchEndSize		= sParticle->m_uchStartSize * 2.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : percentage - 
//-----------------------------------------------------------------------------
void C_AlyxEmpEffect::UpdateDischarging( void )
{
	// Emitters must be valid
	if ( SetupEmitters() == false )
		return;

	// Reset our sort origin
	m_pSimpleEmitter->SetSortOrigin( GetAbsOrigin() );

	float flScale = EMP_SCALE * 8.0f;

	Vector forward, right, up;
	AngleVectors( GetAbsAngles(), &forward, &right, &up );

	SimpleParticle *sParticle;

	float dTime = gpGlobals->frametime;
	
	while ( m_tParticleSpawn.NextEvent( dTime ) )
	{
		// Base of the core effect
		sParticle = (SimpleParticle *) m_pSimpleEmitter->AddParticle( sizeof(SimpleParticle), m_pSimpleEmitter->GetPMaterial( EMP_PARTICLES ), GetAbsOrigin() );

		if ( sParticle == NULL )
			return;
		
		sParticle->m_vecVelocity	= vec3_origin;
		sParticle->m_flDieTime		= 0.25f;
		sParticle->m_flLifetime		= 0.0f;

		sParticle->m_flRoll			= Helper_RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= 0.0f;

		float alpha = 64;

		sParticle->m_uchColor[0]	= alpha;
		sParticle->m_uchColor[1]	= alpha;
		sParticle->m_uchColor[2]	= alpha;
		sParticle->m_uchStartAlpha	= alpha;
		sParticle->m_uchEndAlpha	= 0;

		sParticle->m_uchStartSize	= flScale * 4.0f;
		sParticle->m_uchEndSize		= 0.0f;

		// Base of the core effect
		sParticle = (SimpleParticle *) m_pSimpleEmitter->AddParticle( sizeof(SimpleParticle), m_pSimpleEmitter->GetPMaterial( EMP_PARTICLES ), GetAbsOrigin() );

		if ( sParticle == NULL )
			return;
		
		sParticle->m_vecVelocity	= vec3_origin;
		sParticle->m_flDieTime		= 0.1f;
		sParticle->m_flLifetime		= 0.0f;

		sParticle->m_flRoll			= Helper_RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= 0.0f;

		alpha = 128;

		sParticle->m_uchColor[0]	= alpha;
		sParticle->m_uchColor[1]	= alpha;
		sParticle->m_uchColor[2]	= alpha;
		sParticle->m_uchStartAlpha	= alpha;
		sParticle->m_uchEndAlpha	= 0;

		sParticle->m_uchStartSize	= 0.0f;
		sParticle->m_uchEndSize		= flScale * 2.0f;

		// Make sure we encompass the complete particle here!
		m_pSimpleEmitter->SetParticleCullRadius( sParticle->m_uchEndSize );

		// Do the core effects
		sParticle = (SimpleParticle *) m_pSimpleEmitter->AddParticle( sizeof(SimpleParticle), m_pSimpleEmitter->GetPMaterial( EMP_PARTICLES ), GetAbsOrigin() );

		if ( sParticle == NULL )
			return;
		
		sParticle->m_vecVelocity	= RandomVector( -32.0f, 32.0f );
		sParticle->m_flDieTime		= 0.2f;
		sParticle->m_flLifetime		= 0.0f;

		sParticle->m_flRoll			= Helper_RandomInt( 0, 360 );
		sParticle->m_flRollDelta	= 0.0f;

		alpha = 255;

		sParticle->m_uchColor[0]	= alpha;
		sParticle->m_uchColor[1]	= alpha;
		sParticle->m_uchColor[2]	= alpha;
		sParticle->m_uchStartAlpha	= alpha;
		sParticle->m_uchEndAlpha	= 0;

		sParticle->m_uchStartSize	= flScale;
		sParticle->m_uchEndSize		= 0.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : inline float
//-----------------------------------------------------------------------------
inline float C_AlyxEmpEffect::GetStateDurationPercentage( void )
{
	if ( m_flDuration == 0 )
		return 0.0f;

	return RemapValClamped( ( gpGlobals->curtime - m_flStartTime ), 0, m_flDuration, 0, 1.0f );;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_AlyxEmpEffect::NotifyShouldTransmit( ShouldTransmitState_t state )
{
	BaseClass::NotifyShouldTransmit( state );

	// Turn off
	if ( state == SHOULDTRANSMIT_END )
	{
		SetNextClientThink( CLIENT_THINK_NEVER );
	}

	// Turn on
	if ( state == SHOULDTRANSMIT_START )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_AlyxEmpEffect::ClientThink( void )
{
	if ( gpGlobals->frametime <= 0.0f )
		return;

	float flDuration = GetStateDurationPercentage();

	switch( m_nState )
	{
	case ENERGYCORE_STATE_OFF:
		break;

	case ENERGYCORE_STATE_CHARGING:
		UpdateCharging( flDuration );
		break;

	case ENERGYCORE_STATE_DISCHARGING:
		UpdateDischarging( );
		break;
	}
}
