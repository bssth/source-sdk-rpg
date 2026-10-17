//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#ifndef C_BREAKABLEPROP_H
#define C_BREAKABLEPROP_H
#ifdef _WIN32
#pragma once
#endif

#ifdef GLOWS_ENABLE
#include "glow_outline_effect.h"
#endif

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class C_BreakableProp : public C_BaseAnimating
{
	typedef C_BaseAnimating BaseClass;
public:
	DECLARE_CLIENTCLASS();

	C_BreakableProp();
#ifdef GLOWS_ENABLE
	virtual ~C_BreakableProp();

	virtual void OnDataChanged( DataUpdateType_t type );
	virtual void ClientThink();

	// Props currently showing the inventory highlight, for the floating item
	// names (hud_inventorylabels.cpp).
	static int GetInventoryHighlightCount() { return s_InventoryHighlighted.Count(); }
	static C_BreakableProp *GetInventoryHighlight( int i ) { return s_InventoryHighlighted[i]; }

	// 0..1, fades with the outline.
	float GetInventoryHighlight() const { return m_flInventoryHighlight; }
	const wchar_t *GetInventoryLabel() const { return m_wszInventoryLabel; }
#endif

	virtual void SetFadeMinMax( float fademin, float fademax );

	// Copy fade from another breakable prop
	void CopyFadeFrom( C_BreakableProp *pSource );

#ifdef GLOWS_ENABLE
private:
	void UpdateGlowEffect();
	void DestroyGlowEffect();

	bool m_bGlowEnabled;
	Vector m_vecGlowColor;

	// Name of the item +use would put into the inventory; the server sets it
	// while the player is near and clears it otherwise. m_flInventoryHighlight
	// follows it in ClientThink so the outline and label fade in/out, and
	// m_wszInventoryLabel keeps the last name while fading out.
	char m_szInventoryLabel[64];
	wchar_t m_wszInventoryLabel[64];
	float m_flInventoryHighlight;

	CGlowObject *m_pGlowEffect;

	static CUtlVector<C_BreakableProp *> s_InventoryHighlighted;
#endif
};

#endif // C_BREAKABLEPROP_H
