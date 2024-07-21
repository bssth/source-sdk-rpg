class CPropKeypad : public CBaseAnimating
{
public:
	DECLARE_CLASS(CPropKeypad, CBaseAnimating);

	void Spawn(void);
	void Precache(void);
	void FireTarget(void);
	void WrongCode(void);
	void ShowKeypad(CBasePlayer* pPlayer);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	bool KeyValue(const char* szKeyName, const char* szValue);
	virtual int ObjectCaps(void) { return BaseClass::ObjectCaps() | FCAP_IMPULSE_USE; };

	string_t	s_code;
	string_t	s_frame_label;

	bool		b_hide_pass;

private:
	void Enable(inputdata_t& inputdata);

	COutputEvent OnCorrectCode;
	COutputEvent OnIncorrectCode;

	bool	m_bActive;

	DECLARE_DATADESC();
};
