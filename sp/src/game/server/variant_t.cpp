//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "variant_t.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void variant_t::SetEntity( CBaseEntity *val )
{
	eVal = val;
	fieldType = FIELD_EHANDLE;
}

#ifdef MAPBASE_VSCRIPT
void variant_t::SetScriptVariant( ScriptVariant_t &var )
{
	switch (FieldType())
	{
		case FIELD_INTEGER:		var = Int(); break;
		case FIELD_FLOAT:		var = Float(); break;
		case FIELD_STRING:		var = String(); break;
		case FIELD_POSITION_VECTOR:
		case FIELD_VECTOR:		var = reinterpret_cast<Vector*>(&flVal); break; // HACKHACK
		case FIELD_BOOLEAN:		var = Bool(); break;
		case FIELD_EHANDLE:		var = ToHScript( Entity() ); break;
	}
}
#endif


