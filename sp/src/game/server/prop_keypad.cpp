#include "cbase.h"

#include "prop_keypad.h"

#include "baseentity.h"
#include "entityinput.h"
#include "entityoutput.h"
#include "recipientfilter.h"
#include "usermessages.h"

#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS(prop_keypad, CPropKeypad);

BEGIN_DATADESC(CPropKeypad)
DEFINE_FIELD(s_code, FIELD_STRING),
DEFINE_FIELD(s_frame_label, FIELD_STRING),
DEFINE_FIELD(b_hide_pass, FIELD_BOOLEAN),

DEFINE_INPUTFUNC(FIELD_VOID, "Enable", Enable),

DEFINE_OUTPUT(OnCorrectCode, "OnCorrectCode"),
DEFINE_OUTPUT(OnIncorrectCode, "OnIncorrectCode"),

DEFINE_USEFUNC(Use)
END_DATADESC()

#define	ENTITY_MODEL "models/props_lab/keypad.mdl"

void CPropKeypad::Precache(void)
{
	PrecacheModel(ENTITY_MODEL);
	BaseClass::Precache();
}

void CPropKeypad::Spawn(void)
{
	Precache();
	SetModel(ENTITY_MODEL);
	SetSolid(SOLID_BBOX);
	SetMoveType(MOVETYPE_NONE);
	m_nSkin = 0;
}

bool CPropKeypad::KeyValue(const char* szKeyName, const char* szValue)
{
	if (FStrEq(szKeyName, "code"))
	{
		s_code = AllocPooledString(szValue);
	}
	else if (FStrEq(szKeyName, "label"))
	{
		s_frame_label = AllocPooledString(szValue);
	}
	else if (FStrEq(szKeyName, "hide_pass"))
	{
		b_hide_pass = (atoi(szValue) != 0);
	}
	else
	{
		return BaseClass::KeyValue(szKeyName, szValue);
	}
	return true;
}

void CPropKeypad::Enable(inputdata_t& inputdata)
{
	CBasePlayer* pPlayer = NULL;
	if (inputdata.pActivator && inputdata.pActivator->IsPlayer())
	{
		pPlayer = ToBasePlayer(inputdata.pActivator);
	}
	else
	{
		pPlayer = UTIL_GetLocalPlayer();
	}

	ShowKeypad(pPlayer);
}

void CPropKeypad::FireTarget(void)
{
	m_nSkin = 1;
	OnCorrectCode.FireOutput(this, this);
}

void CPropKeypad::WrongCode(void)
{
	m_nSkin = 2;
	OnIncorrectCode.FireOutput(this, this);
}

void CPropKeypad::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (!pActivator || !pActivator->IsPlayer())
		return;

	ShowKeypad(ToBasePlayer(pActivator));
	BaseClass::Use(pActivator, pCaller, useType, value);
}

void CPropKeypad::ShowKeypad(CBasePlayer* pPlayer)
{
	m_nSkin = 0;

	CSingleUserRecipientFilter user(pPlayer);
	UserMessageBegin(user, "KeypadHandler");
	WRITE_STRING(STRING(s_code));
	WRITE_STRING(STRING(s_frame_label));
	WRITE_BOOL(b_hide_pass);
	MessageEnd();
}
