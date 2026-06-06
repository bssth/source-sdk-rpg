//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//
#include "cbase.h"
#include "model_types.h"
#include "vcollide.h"
#include "vcollide_parse.h"
#include "solidsetdefaults.h"
#include "bone_setup.h"
#include "engine/ivmodelinfo.h"
#include "physics.h"
#include "c_breakableprop.h"
#include "view.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT(C_BreakableProp, DT_BreakableProp, CBreakableProp)
#ifdef GLOWS_ENABLE
	RecvPropBool( RECVINFO( m_bGlowEnabled ) ),
	RecvPropVector( RECVINFO( m_vecGlowColor ) ),
#endif
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
C_BreakableProp::C_BreakableProp( void )
{
	m_takedamage = DAMAGE_YES;

#ifdef GLOWS_ENABLE
	m_bGlowEnabled = false;
	m_vecGlowColor.Init( 1.0f, 1.0f, 1.0f );
	m_bClientGlowActive = false;
	m_vecClientGlowColor.Init( 1.0f, 1.0f, 1.0f );
	m_pGlowEffect = NULL;
#endif
}

#ifdef GLOWS_ENABLE
//-----------------------------------------------------------------------------
C_BreakableProp::~C_BreakableProp()
{
	DestroyGlowEffect();
}

//-----------------------------------------------------------------------------
void C_BreakableProp::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );
	UpdateGlowEffect();
}

//-----------------------------------------------------------------------------
void C_BreakableProp::ClientThink()
{
	BaseClass::ClientThink();
}

//-----------------------------------------------------------------------------
void C_BreakableProp::UpdateGlowEffect()
{
	const bool bWantGlow = m_bGlowEnabled;

	if ( bWantGlow && !m_pGlowEffect )
	{
		m_pGlowEffect = new CGlowObject( this, m_vecGlowColor, 1.0f, false, true );
		m_bClientGlowActive = true;
		m_vecClientGlowColor = m_vecGlowColor;
		return;
	}

	if ( !bWantGlow && m_pGlowEffect )
	{
		DestroyGlowEffect();
		return;
	}

	if ( m_pGlowEffect && m_vecGlowColor != m_vecClientGlowColor )
	{
		m_pGlowEffect->SetColor( m_vecGlowColor );
		m_vecClientGlowColor = m_vecGlowColor;
	}
}

//-----------------------------------------------------------------------------
void C_BreakableProp::DestroyGlowEffect()
{
	if ( m_pGlowEffect )
	{
		delete m_pGlowEffect;
		m_pGlowEffect = NULL;
	}
	m_bClientGlowActive = false;
}
#endif // GLOWS_ENABLE

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_BreakableProp::SetFadeMinMax( float fademin, float fademax )
{
	m_fadeMinDist = fademin;
	m_fadeMaxDist = fademax;
}

//-----------------------------------------------------------------------------
// Copy fade from another breakable prop
//-----------------------------------------------------------------------------
void C_BreakableProp::CopyFadeFrom( C_BreakableProp *pSource )
{
	m_flFadeScale = pSource->m_flFadeScale;
}
