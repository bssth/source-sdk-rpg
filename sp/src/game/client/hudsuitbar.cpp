#include "cbase.h" 
#include "hud.h" 
#include "hud_macros.h" 
#include "c_baseplayer.h"
#include "hudsuitbar.h" 
#include "iclientmode.h" 
#include "vgui/ISurface.h"

using namespace vgui;

#include "tier0/memdbgon.h" 

DECLARE_HUDELEMENT(CHudSuitBar);
DECLARE_HUD_MESSAGE(CHudSuitBar, Battery);

#define HULL_INIT 0 

// TODO: maybe create parent class for health and suit bars

//------------------------------------------------------------------------
// Purpose: Constructor
//------------------------------------------------------------------------

CHudSuitBar::CHudSuitBar(const char* pElementName) :
	CHudElement(pElementName), BaseClass(NULL, "HudSuitbar")
{
	vgui::Panel* pParent = g_pClientMode->GetViewport();
	SetParent(pParent);

	SetHiddenBits(HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT);
}

//------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------

void CHudSuitBar::Init()
{
	HOOK_HUD_MESSAGE(CHudSuitBar, Battery);
	Reset();
}

//------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------

void CHudSuitBar::Reset(void)
{
	m_flHull = HULL_INIT;
	m_nHullLow = -1;
	SetBgColor(Color(0, 0, 0, 128));
}

//------------------------------------------------------------------------
// Purpose: draws the power bar
//------------------------------------------------------------------------

void CHudSuitBar::Paint()
{
	// Get bar chunks

	int chunkCount = m_flBarWidth / (m_flBarChunkWidth + m_flBarChunkGap);
	int enabledChunks = (int)((float)chunkCount * (m_flHull / 100.0f) + 0.5f);

	// Draw the suit power bar
	surface()->DrawSetColor(m_HullColor);

	int xpos = m_flBarInsetX, ypos = m_flBarInsetY;

	for (int i = 0; i < enabledChunks; i++)
	{
		surface()->DrawFilledRect(xpos, ypos, xpos + m_flBarChunkWidth, ypos + m_flBarHeight);
		xpos += (m_flBarChunkWidth + m_flBarChunkGap);
	}

	// Draw the exhausted portion of the bar.
	surface()->DrawSetColor(Color(m_HullColor[0], m_HullColor[1], m_HullColor[2], m_iHullDisabledAlpha));

	for (int i = enabledChunks; i < chunkCount; i++)
	{
		surface()->DrawFilledRect(xpos, ypos, xpos + m_flBarChunkWidth, ypos + m_flBarHeight);
		xpos += (m_flBarChunkWidth + m_flBarChunkGap);
	}

	// Draw our name

	surface()->DrawSetTextFont(m_hTextFont);
	surface()->DrawSetTextColor(m_HullColor);
	surface()->DrawSetTextPos(text_xpos, text_ypos);

	surface()->DrawPrintText(L"HULL", wcslen(L"HULL"));
}

void CHudSuitBar::MsgFunc_Battery(bf_read& msg)
{
	float newHull = msg.ReadShort();

	// Never below zero
	newHull = max(newHull, 0);

	if (newHull == m_flHull)
		return;

	m_flHull = newHull;
}