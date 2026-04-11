// Inventory system for the player.
// Item-type DB is loaded once from scripts/bb_items.txt.

#include "cbase.h"
#include "bb_inventory.h"
#include "hl2/hl2_player.h"
#include "filesystem.h"
#include "KeyValues.h"
#include "tier1/utldict.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CHL2_Player *GetLocalHL2Player()
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if ( !pPlayer )
		return NULL;
	return dynamic_cast<CHL2_Player *>( pPlayer );
}

//-----------------------------------------------------------------------------
// Static item-type database. Owned by this TU, freed on shutdown by the
// CItemTypeDB destructor.
//-----------------------------------------------------------------------------
typedef unsigned short INV_ITEM_HANDLE;

class CItemTypeDB
{
public:
	CItemTypeDB() : m_bLoaded( false ) {}
	~CItemTypeDB() { Clear(); }

	void EnsureLoaded()
	{
		if ( m_bLoaded )
			return;
		m_bLoaded = true; // set first so a failed load still counts as "tried"

		const char *szFile = "scripts/bb_items.txt";
		KeyValues *pkv = new KeyValues( "items" );
		if ( !pkv->LoadFromFile( filesystem, szFile, "MOD" ) )
		{
			Warning( "bb_inventory: failed to load %s, inventory will be empty\n", szFile );
			pkv->deleteThis();
			return;
		}

		for ( KeyValues *pNode = pkv->GetFirstSubKey(); pNode; pNode = pNode->GetNextKey() )
		{
			const char *pszClass = pNode->GetName();
			if ( !pszClass || !pszClass[0] )
				continue;

			CBBItemType *pItem = new CBBItemType();
			Q_strncpy( pItem->szClassName,   pszClass,                              sizeof(pItem->szClassName)   );
			Q_strncpy( pItem->szPrintName,   pNode->GetString("print_name", pszClass), sizeof(pItem->szPrintName) );
			Q_strncpy( pItem->szPickupModel, pNode->GetString("pickup_model", ""),   sizeof(pItem->szPickupModel) );
			pItem->isUsable   = pNode->GetBool( "usable",       false );
			pItem->oneTimeUse = pNode->GetBool( "one_time_use", false );

			const char *pszCat = pNode->GetString( "category", "generic" );
			if      ( !Q_stricmp( pszCat, "healthkit"  ) ) pItem->category = BB_ITEM_HEALTHKIT;
			else if ( !Q_stricmp( pszCat, "healthvial" ) ) pItem->category = BB_ITEM_HEALTHVIAL;
			else if ( !Q_stricmp( pszCat, "zoom"       ) ) pItem->category = BB_ITEM_ZOOM;
			else                                           pItem->category = BB_ITEM_GENERIC;

			// Pool the classname so HasItem() can compare by string_t identity.
			AllocPooledString( pItem->szClassName );

			m_dict.Insert( pItem->szClassName, pItem );
		}

		pkv->deleteThis();
		DevMsg( "bb_inventory: loaded %i item types from %s\n", m_dict.Count(), szFile );
	}

	void Clear()
	{
		for ( unsigned short i = m_dict.First(); i != m_dict.InvalidIndex(); i = m_dict.Next(i) )
			delete m_dict[i];
		m_dict.RemoveAll();
		m_bLoaded = false;
	}

	const CBBItemType *Find( const char *name ) const
	{
		INV_ITEM_HANDLE h = m_dict.Find( name );
		if ( h == m_dict.InvalidIndex() )
			return NULL;
		return m_dict[h];
	}

	// Iterate all items to resolve a pickup model.
	const CBBItemType *FindByPickupModel( const char *modelName ) const
	{
		if ( !modelName || !modelName[0] )
			return NULL;
		for ( unsigned short i = m_dict.First(); i != m_dict.InvalidIndex(); i = m_dict.Next(i) )
		{
			const CBBItemType *p = m_dict[i];
			if ( p->szPickupModel[0] && !Q_stricmp( p->szPickupModel, modelName ) )
				return p;
		}
		return NULL;
	}

private:
	CUtlDict< CBBItemType *, unsigned short > m_dict;
	bool m_bLoaded;
};

static CItemTypeDB g_ItemTypeDB;

//-----------------------------------------------------------------------------
// CBBInventory save/restore
//-----------------------------------------------------------------------------
BEGIN_DATADESC_NO_BASE( CBBInventory )
	DEFINE_AUTO_ARRAY( m_iInventory, FIELD_STRING )
END_DATADESC()

CBBInventory::CBBInventory()
{
	for ( int i = 0; i < MAX_INVENTORY; i++ )
		m_iInventory[i] = NULL_STRING;
}

CBBInventory::~CBBInventory()
{
}

//-----------------------------------------------------------------------------
// Item-type DB accessors
//-----------------------------------------------------------------------------
const CBBItemType *CBBInventory::GetItemData( const char *name )
{
	if ( !name || !name[0] )
		return NULL;
	g_ItemTypeDB.EnsureLoaded();
	return g_ItemTypeDB.Find( name );
}

string_t CBBInventory::GetItemByModel( string_t model )
{
	g_ItemTypeDB.EnsureLoaded();
	const CBBItemType *p = g_ItemTypeDB.FindByPickupModel( STRING(model) );
	if ( !p )
		return NULL_STRING;
	// szClassName is already pooled during DB load.
	return FindPooledString( p->szClassName );
}

//-----------------------------------------------------------------------------
// Slot ops
//-----------------------------------------------------------------------------
bool CBBInventory::AddItem( const char *name )
{
	// Validate against the item DB.
	if ( !GetItemData( name ) )
	{
		DevWarning( "CBBInventory::AddItem: unknown item '%s'\n", name ? name : "(null)" );
		return false;
	}

	// Normalize the classname into the string pool once at insertion time so
	// every slot we compare later refers to the same string_t identity.
	string_t pooled = AllocPooledString( name );

	for ( int i = 0; i < MAX_INVENTORY; i++ )
	{
		if ( m_iInventory[i] == NULL_STRING )
		{
			m_iInventory[i] = pooled;
			return true;
		}
	}
	return false;
}

bool CBBInventory::RemoveItem( const char *name )
{
	string_t pooled = FindPooledString( name );
	if ( pooled == NULL_STRING )
		return false;

	for ( int i = 0; i < MAX_INVENTORY; i++ )
	{
		if ( m_iInventory[i] == pooled )
		{
			m_iInventory[i] = NULL_STRING;
			return true;
		}
	}
	return false;
}

bool CBBInventory::RemoveItemPosition( int position )
{
	if ( position < 0 || position >= MAX_INVENTORY )
		return false;
	m_iInventory[position] = NULL_STRING;
	return true;
}

bool CBBInventory::HasItem( const char *name ) const
{
	string_t pooled = FindPooledString( name );
	if ( pooled == NULL_STRING )
		return false; // never pooled => never added

	for ( int i = 0; i < MAX_INVENTORY; i++ )
	{
		if ( m_iInventory[i] == pooled )
			return true;
	}
	return false;
}

// "Use" is a state change: confirm the item is present and usable, and if
// it's one-time-use, remove it. The actual gameplay effect lives on the
// owning player (CHL2_Player::UseItem).
bool CBBInventory::UseItem( const char *name )
{
	if ( !CanUseItem( name ) )
		return false;

	const CBBItemType *pItem = GetItemData( name );
	if ( !pItem )
		return false;
	if ( pItem->oneTimeUse )
		return RemoveItem( name );

	return true;
}

bool CBBInventory::CanUseItem( const char *name ) const
{
	if ( !HasItem( name ) )
		return false;

	const CBBItemType *pItem = GetItemData( name );
	return pItem && pItem->isUsable;
}

//-----------------------------------------------------------------------------
// ConCommands
//-----------------------------------------------------------------------------
CON_COMMAND( inventory, "Print the local player's inventory." )
{
	CHL2_Player *pPlayer = GetLocalHL2Player();
	if ( !pPlayer )
		return;
	CBBInventory *pInventory = pPlayer->GetInventory();
	if ( !pInventory )
		return;

	Msg( "\nInventory: %i\n", pInventory->GetItemCount() );
	for ( int i = 0; i < MAX_INVENTORY; i++ )
	{
		if ( pInventory->m_iInventory[i] == NULL_STRING )
			continue;

		const char *szName = STRING( pInventory->m_iInventory[i] );
		const CBBItemType *pItem = CBBInventory::GetItemData( szName );
		Msg( "Slot %i: %s\n", i, pItem ? pItem->szPrintName : szName );
	}
	Msg( "\n" );
}

CON_COMMAND_F( additem, "Add an item to the player inventory by classname.", FCVAR_CHEAT )
{
	if ( args.ArgC() < 2 )
	{
		Msg( "usage: additem <item_classname>\n" );
		return;
	}
	CHL2_Player *pPlayer = GetLocalHL2Player();
	if ( !pPlayer )
		return;
	CBBInventory *pInventory = pPlayer->GetInventory();
	if ( !pInventory )
		return;

	if ( !pInventory->AddItem( args.Arg(1) ) )
		Msg( "additem: failed to add '%s'\n", args.Arg(1) );
}

CON_COMMAND_F( removeitem, "Remove an item from the player inventory by classname.", FCVAR_CHEAT )
{
	if ( args.ArgC() < 2 )
	{
		Msg( "usage: removeitem <item_classname>\n" );
		return;
	}
	CHL2_Player *pPlayer = GetLocalHL2Player();
	if ( !pPlayer )
		return;
	CBBInventory *pInventory = pPlayer->GetInventory();
	if ( !pInventory )
		return;

	pInventory->RemoveItem( args.Arg(1) );
}

CON_COMMAND_F( useitem, "Use an item from the player inventory by classname.", FCVAR_DONTRECORD )
{
	if ( args.ArgC() < 2 )
	{
		Msg( "usage: useitem <item_classname>\n" );
		return;
	}
	CHL2_Player *pPlayer = GetLocalHL2Player();
	if ( !pPlayer )
		return;

	if ( pPlayer->UseItem( args.Arg(1) ) )
		DevMsg( "Item is used!\n" );
	else
		DevMsg( "Item use failed\n" );
}
