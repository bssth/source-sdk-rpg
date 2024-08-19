#ifndef BB_INVENTORY_H

#define BB_INVENTORY_H
#pragma once

#include "cbase.h"

// Not including player and other entities because inventory is a global standalone system

#define MAX_INVENTORY 50
#define MAX_INV_STRING 64

class CBBItemType
{
public:
	bool					isUsable = false;
	bool					oneTimeUse = false;
	char					szClassName[MAX_INV_STRING];
	char					szPrintName[MAX_INV_STRING];

	inline bool isHealthKit () { return !Q_strcmp(szClassName, "item_healthkit"); }
	inline bool isHealthVial() { return !Q_strcmp(szClassName, "item_healthvial"); }
};

class CBBInventory
{
public:
	CBBInventory();
	~CBBInventory();

	DECLARE_CLASS_NOBASE(CBBInventory);
	DECLARE_DATADESC();

	string_t m_iInventory[MAX_INVENTORY];

	static string_t GetItemByModel(string_t model);
	static CBBItemType *GetItemData(const char* name);

	bool AddItem(const char* name);
	bool RemoveItem(const char* name);
	bool RemoveItemPosition(int Position);
	bool HasItem(const char* name);
	bool UseItem(const char* name);
	bool CanUseItem(const char* name);
	void Think(void);

	inline int GetItemCount()
	{
		int item_count = 0;
		for (int i = 0; i < MAX_INVENTORY; i++)
		{
			if (m_iInventory[i] != NULL_STRING)
			{
				item_count++;
			}
		}
		return item_count;
	}
};

#endif