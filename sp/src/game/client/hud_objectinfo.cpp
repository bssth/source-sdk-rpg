//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "usermessages.h"
#include "iclientmode.h"
#include "c_basehlplayer.h"
#include "vgui_controls/Panel.h"
#include <vgui_controls/RichText.h>
#include "vgui_controls/AnimationController.h"
#include "vgui/ISurface.h"
#include <vgui/ILocalize.h>
#include "util_shared.h"

#include "tier0/memdbgon.h"

using namespace vgui;

class CHudObjectInfo : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE(CHudObjectInfo, vgui::Panel);

public:
	CHudObjectInfo(const char* pElementName);
	virtual void Init(void);
	virtual void Reset(void);
	virtual void MsgFunc_ShowObjectInfo(bf_read& msg);
	virtual void Paint();
	virtual bool ShouldDraw();

protected:
	wchar_t pOut[255];
	bool m_isMedic;
	char m_glyph;
	bool m_isEnemy;
	bool m_isDamaged;
	bool m_isMoving;

private:
	CPanelAnimationVar(vgui::HFont, m_hTextFont, "TextFont", "Default");
	CPanelAnimationVarAliasType(float, text_xpos, "text_xpos", "8", "proportional_float");
	CPanelAnimationVarAliasType(float, text_ypos, "text_ypos", "20", "proportional_float");
	CPanelAnimationVar(Color, m_objTextColor, "ObjTextColor", "255 220 0 160");
	CPanelAnimationVar(Color, m_objEnemyColor, "ObjEnemyColor", "200 0 0 160");
	CPanelAnimationVar(Color, m_objIconColor, "ObjIconColor", "255 220 0 160");
	CPanelAnimationVarAliasType(float, m_flIconInsetX, "IconInsetX", "8", "proportional_float");
	CPanelAnimationVarAliasType(float, m_flIconInsetY, "IconInsetY", "0", "proportional_float");
	CPanelAnimationVarAliasType(float, m_flIconGap, "IconGap", "24", "proportional_float");
	CPanelAnimationVar(vgui::HFont, m_hIconFont, "IconFont", "HudNumbers");
};


DECLARE_HUDELEMENT(CHudObjectInfo);
DECLARE_HUD_MESSAGE(CHudObjectInfo, ShowObjectInfo);

using namespace vgui;

CHudObjectInfo::CHudObjectInfo(const char* pElementName) : CHudElement(pElementName), BaseClass(NULL, "HudObjectInfo")
{
	vgui::Panel* pParent = g_pClientMode->GetViewport();
	SetParent(pParent);

	SetHiddenBits(HIDEHUD_PLAYERDEAD);
}

void CHudObjectInfo::Init(void)
{
	HOOK_HUD_MESSAGE(CHudObjectInfo, ShowObjectInfo);
	SetVisible(false);
}

void CHudObjectInfo::Reset(void)
{
	Init();
}

bool CHudObjectInfo::ShouldDraw(void)
{
	return IsVisible() && CHudElement::ShouldDraw();
}

void CHudObjectInfo::Paint()
{
	C_BaseHLPlayer* pPlayer = (C_BaseHLPlayer*)C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;
	if (!IsVisible())
		return;

	surface()->DrawSetTextColor(m_isEnemy ? m_objEnemyColor : m_objIconColor);
	surface()->DrawSetTextFont(m_hIconFont);
	int xpos = m_flIconInsetX, ypos = m_flIconInsetY;

	surface()->DrawSetTextPos(xpos, ypos);
	if (m_isMoving)
	{
		surface()->DrawUnicodeChar(m_isMedic ? 'N' : 'D');
	}
	else
	{
		surface()->DrawUnicodeChar(m_isMedic ? 'M' : 'C');
	}
	xpos += m_flIconGap;

	if (m_isDamaged)
	{
		surface()->DrawSetTextPos(xpos, ypos);
		surface()->DrawUnicodeChar('+');
		xpos += m_flIconGap;
	}

	if (m_glyph != '\0')
	{
		surface()->DrawSetTextPos(xpos, ypos);
		surface()->DrawUnicodeChar(m_glyph);
		xpos += m_flIconGap;
	}

	surface()->DrawSetTextFont(m_hTextFont);
	surface()->DrawSetTextColor(m_isEnemy ? m_objEnemyColor : m_objTextColor);
	surface()->DrawSetTextPos(text_xpos, text_ypos);
	surface()->DrawPrintText(pOut, wcslen(pOut));
}

void CHudObjectInfo::MsgFunc_ShowObjectInfo(bf_read& msg)
{
	int mode = msg.ReadOneBit();
	if (mode <= 0) {
		SetVisible(false);
		return;
	}

	char szString[255];
	szString[0] = 0;
	msg.ReadString(szString, sizeof(szString));
	g_pVGuiLocalize->ConvertANSIToUnicode(szString, pOut, sizeof(pOut));
	m_isMedic = msg.ReadOneBit() > 0;
	m_glyph = msg.ReadChar();
	m_isEnemy = msg.ReadOneBit() > 0;
	m_isDamaged = msg.ReadOneBit() > 0;
	m_isMoving = msg.ReadOneBit() > 0;
	SetVisible(true);
}