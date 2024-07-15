#if !defined HUD_HULL_HS
#define HUD_HULL_HS

#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"
#include "hud_numericdisplay.h"

//-----------------------------------------------------------------------------
// Purpose: Shows the hull bar
//-----------------------------------------------------------------------------

class CHudSuitBar : public CHudElement, public vgui::Panel
{

	DECLARE_CLASS_SIMPLE(CHudSuitBar, vgui::Panel);

public:
	CHudSuitBar(const char* pElementName);

	virtual void	Init(void);
	virtual void	Reset(void);
	void			MsgFunc_Battery(bf_read& msg);

protected:
	virtual void Paint();

private:
	CPanelAnimationVar(Color, m_HullColor, "HullColor", "0 100 255 255");
	CPanelAnimationVar(int, m_iHullDisabledAlpha, "HullDisabledAlpha", "50");
	CPanelAnimationVarAliasType(float, m_flBarInsetX, "BarInsetX", "26", "proportional_float");
	CPanelAnimationVarAliasType(float, m_flBarInsetY, "BarInsetY", "3", "proportional_float");
	CPanelAnimationVarAliasType(float, m_flBarWidth, "BarWidth", "84", "proportional_float");
	CPanelAnimationVarAliasType(float, m_flBarHeight, "BarHeight", "4", "proportional_float");
	CPanelAnimationVarAliasType(float, m_flBarChunkWidth, "BarChunkWidth", "2", "proportional_float");
	CPanelAnimationVarAliasType(float, m_flBarChunkGap, "BarChunkGap", "1", "proportional_float");
	CPanelAnimationVar(vgui::HFont, m_hTextFont, "TextFont", "HUDBarText");
	CPanelAnimationVarAliasType(float, text_xpos, "text_xpos", "2", "proportional_float");
	CPanelAnimationVarAliasType(float, text_ypos, "text_ypos", "2", "proportional_float");
	CPanelAnimationVarAliasType(float, text2_xpos, "text2_xpos", "8", "proportional_float");
	CPanelAnimationVarAliasType(float, text2_ypos, "text2_ypos", "40", "proportional_float");
	CPanelAnimationVarAliasType(float, text2_gap, "text2_gap", "10", "proportional_float");
	float m_flHull;
	int m_nHullLow;

};

#endif