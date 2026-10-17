// Floating item names above props +use would put into the inventory. The
// props come from C_BreakableProp's inventory highlight list.

#include "cbase.h"
#include "hudelement.h"
#include "iclientmode.h"
#include "c_breakableprop.h"
#include "vgui_controls/Panel.h"
#include "vgui/ISurface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef GLOWS_ENABLE

using namespace vgui;

// How far above the top of the prop the label floats; it rises over the second
// half of this while fading in.
#define INVENTORY_LABEL_HEIGHT	10.0f

class CHudInventoryLabels : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudInventoryLabels, vgui::Panel );

public:
	CHudInventoryLabels( const char *pElementName );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual bool ShouldDraw();
	virtual void Paint();

private:
	CPanelAnimationVar( vgui::HFont, m_hTextFont, "TextFont", "InstructorTitle" );
	CPanelAnimationVar( Color, m_TextColor, "TextColor", "255 220 0 255" );
};

DECLARE_HUDELEMENT( CHudInventoryLabels );

CHudInventoryLabels::CHudInventoryLabels( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudInventoryLabels" )
{
	SetParent( g_pClientMode->GetViewport() );
	SetHiddenBits( HIDEHUD_PLAYERDEAD );
}

void CHudInventoryLabels::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// Labels can land anywhere on screen.
	SetBounds( 0, 0, ScreenWidth(), ScreenHeight() );
	SetPaintBackgroundEnabled( false );
}

bool CHudInventoryLabels::ShouldDraw()
{
	return C_BreakableProp::GetInventoryHighlightCount() > 0 && CHudElement::ShouldDraw();
}

void CHudInventoryLabels::Paint()
{
	surface()->DrawSetTextFont( m_hTextFont );

	for ( int i = 0; i < C_BreakableProp::GetInventoryHighlightCount(); i++ )
	{
		C_BreakableProp *pProp = C_BreakableProp::GetInventoryHighlight( i );
		const wchar_t *pszLabel = pProp->GetInventoryLabel();
		if ( !pszLabel[0] || pProp->IsDormant() )
			continue;

		const float flFade = pProp->GetInventoryHighlight();

		Vector vecMins, vecMaxs;
		pProp->CollisionProp()->WorldSpaceAABB( &vecMins, &vecMaxs );
		Vector vecPos = ( vecMins + vecMaxs ) * 0.5f;
		vecPos.z = vecMaxs.z + INVENTORY_LABEL_HEIGHT * ( 0.5f + 0.5f * flFade );

		int x, y;
		if ( !GetVectorInScreenSpace( vecPos, x, y ) )
			continue;

		int wide, tall;
		surface()->GetTextSize( m_hTextFont, pszLabel, wide, tall );

		surface()->DrawSetTextColor( m_TextColor.r(), m_TextColor.g(), m_TextColor.b(), (int)( m_TextColor.a() * flFade ) );
		surface()->DrawSetTextPos( x - wide / 2, y - tall );
		surface()->DrawPrintText( pszLabel, wcslen( pszLabel ) );
	}
}

#endif // GLOWS_ENABLE
