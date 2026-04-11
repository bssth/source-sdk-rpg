//========= [Mapbase port] =====================================================
//
// Purpose: Client-side proxy for env_instructor_hint. Exists solely so the
//          server entity has a corresponding entry in the client entity list,
//          which lets c_baselesson.cpp resolve C_BaseEntity::Instance(entindex)
//          when reading the "hint_target" field of an
//          instructor_server_hint_create event. Once we get a non-NULL
//          C_BaseEntity back, the locator code anchors the hint icon to the
//          entity's m_vecOrigin (already streamed by the base CBaseEntity
//          send table) instead of falling back to the local player.
//
//=============================================================================

#include "cbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class C_EnvInstructorHint : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_EnvInstructorHint, C_BaseEntity );
	DECLARE_CLIENTCLASS();
};

IMPLEMENT_CLIENTCLASS_DT( C_EnvInstructorHint, DT_EnvInstructorHint, CEnvInstructorHint )
END_RECV_TABLE()
