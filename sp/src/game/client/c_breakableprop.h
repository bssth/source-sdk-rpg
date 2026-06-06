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

	bool m_bClientGlowActive;
	Vector m_vecClientGlowColor;
	CGlowObject *m_pGlowEffect;
#endif
};

#endif // C_BREAKABLEPROP_H
