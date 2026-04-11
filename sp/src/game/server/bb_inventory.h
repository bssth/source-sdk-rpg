#ifndef BB_INVENTORY_H
#define BB_INVENTORY_H
#pragma once

#include "string_t.h"
#include "datamap.h"
#include "networkvar.h" // DECLARE_CLASS_NOBASE

// Player inventory is a per-player system. See bb_inventory.cpp for the
// item-type database (loaded once from scripts/bb_items.txt).

#define MAX_INVENTORY  50
#define MAX_INV_STRING 64

enum BBItemCategory_t
{
	BB_ITEM_GENERIC = 0,
	BB_ITEM_HEALTHKIT,
	BB_ITEM_HEALTHVIAL,
	BB_ITEM_ZOOM,
};

class CBBItemType
{
public:
	CBBItemType()
		: isUsable( false )
		, oneTimeUse( false )
		, category( BB_ITEM_GENERIC )
	{
		szClassName[0]   = '\0';
		szPrintName[0]   = '\0';
		szPickupModel[0] = '\0';
	}

	bool            isUsable;
	bool            oneTimeUse;
	BBItemCategory_t category;
	char            szClassName  [MAX_INV_STRING];
	char            szPrintName  [MAX_INV_STRING];
	char            szPickupModel[128];
};

class CBBInventory
{
public:
	CBBInventory();
	~CBBInventory();

	DECLARE_CLASS_NOBASE(CBBInventory);
	DECLARE_DATADESC();

	string_t m_iInventory[MAX_INVENTORY];

	// Item-type DB (loaded lazily from scripts/bb_items.txt on first access).
	static const CBBItemType *GetItemData(const char *name);
	static string_t           GetItemByModel(string_t model);

	// Player inventory ops.
	bool AddItem    (const char *name);
	bool RemoveItem (const char *name);
	bool RemoveItemPosition(int position);
	bool HasItem    (const char *name) const;
	bool UseItem    (const char *name);
	bool CanUseItem (const char *name) const;

	int  GetItemCount() const
	{
		int n = 0;
		for ( int i = 0; i < MAX_INVENTORY; i++ )
			if ( m_iInventory[i] != NULL_STRING )
				n++;
		return n;
	}
};

#endif // BB_INVENTORY_H
