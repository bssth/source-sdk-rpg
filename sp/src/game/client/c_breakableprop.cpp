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
#include <vgui/ILocalize.h>
#include "tier3/tier3.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef GLOWS_ENABLE
// Warm amber. The glow pass darkens it towards orange, hence the extra green.
static const Vector s_vecInventoryGlowColor( 1.0f, 0.9f, 0.4f );
#define INVENTORY_HIGHLIGHT_FADE_TIME	0.25f

CUtlVector<C_BreakableProp *> C_BreakableProp::s_InventoryHighlighted;
#endif

IMPLEMENT_CLIENTCLASS_DT(C_BreakableProp, DT_BreakableProp, CBreakableProp)
#ifdef GLOWS_ENABLE
	RecvPropBool( RECVINFO( m_bGlowEnabled ) ),
	RecvPropVector( RECVINFO( m_vecGlowColor ) ),
	RecvPropString( RECVINFO_NAME( m_szInventoryLabel, m_iszInventoryLabel ) ),
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
	m_szInventoryLabel[0] = '\0';
	m_wszInventoryLabel[0] = L'\0';
	m_flInventoryHighlight = 0.0f;
	m_pGlowEffect = NULL;
#endif
}

#ifdef GLOWS_ENABLE
//-----------------------------------------------------------------------------
C_BreakableProp::~C_BreakableProp()
{
	s_InventoryHighlighted.FindAndRemove( this );
	DestroyGlowEffect();
}

//-----------------------------------------------------------------------------
void C_BreakableProp::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	// print_name may be a localization token.
	const wchar_t *pszLocalized = m_szInventoryLabel[0] == '#' ? g_pVGuiLocalize->Find( m_szInventoryLabel ) : NULL;
	if ( pszLocalized )
	{
		V_wcsncpy( m_wszInventoryLabel, pszLocalized, sizeof( m_wszInventoryLabel ) );
	}
	else if ( m_szInventoryLabel[0] )
	{
		g_pVGuiLocalize->ConvertANSIToUnicode( m_szInventoryLabel, m_wszInventoryLabel, sizeof( m_wszInventoryLabel ) );
	}

	// Fade the inventory outline and label in/out instead of popping them.
	const float flTarget = m_szInventoryLabel[0] ? 1.0f : 0.0f;
	if ( m_flInventoryHighlight != flTarget )
		SetNextClientThink( CLIENT_THINK_ALWAYS );

	UpdateGlowEffect();
}

//-----------------------------------------------------------------------------
void C_BreakableProp::ClientThink()
{
	BaseClass::ClientThink();

	const float flTarget = m_szInventoryLabel[0] ? 1.0f : 0.0f;
	m_flInventoryHighlight = Approach( flTarget, m_flInventoryHighlight, gpGlobals->frametime / INVENTORY_HIGHLIGHT_FADE_TIME );
	if ( m_flInventoryHighlight == flTarget )
		SetNextClientThink( CLIENT_THINK_NEVER );

	UpdateGlowEffect();
}

//-----------------------------------------------------------------------------
void C_BreakableProp::UpdateGlowEffect()
{
	// The HUD draws the floating item name for everything in this list.
	if ( m_flInventoryHighlight > 0.0f )
	{
		if ( !s_InventoryHighlighted.HasElement( this ) )
			s_InventoryHighlighted.AddToTail( this );
	}
	else
	{
		s_InventoryHighlighted.FindAndRemove( this );
	}

	// A mapper-set glow (glow_enabled / EnableGlow) wins over the inventory outline.
	Vector vecColor;
	float flAlpha;
	if ( m_bGlowEnabled )
	{
		vecColor = m_vecGlowColor;
		flAlpha = 1.0f;
	}
	else if ( m_flInventoryHighlight > 0.0f )
	{
		vecColor = s_vecInventoryGlowColor;
		flAlpha = m_flInventoryHighlight;
	}
	else
	{
		DestroyGlowEffect();
		return;
	}

	if ( !m_pGlowEffect )
	{
		m_pGlowEffect = new CGlowObject( this, vecColor, flAlpha, false, true );
		return;
	}

	m_pGlowEffect->SetColor( vecColor );
	m_pGlowEffect->SetAlpha( flAlpha );
}

//-----------------------------------------------------------------------------
void C_BreakableProp::DestroyGlowEffect()
{
	if ( m_pGlowEffect )
	{
		delete m_pGlowEffect;
		m_pGlowEffect = NULL;
	}
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
