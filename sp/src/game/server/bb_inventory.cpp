// Inventory system for the player

#include "cbase.h"
#include "bb_inventory.h"

static CUtlDict< CBBItemType*, unsigned short > m_invItemsDB;
typedef unsigned short INV_ITEM_HANDLE;

BEGIN_DATADESC_NO_BASE(CBBInventory)
DEFINE_AUTO_ARRAY(m_iInventory, FIELD_STRING)
END_DATADESC()

CBBInventory::CBBInventory()
{
	for (int i = 0; i < MAX_INVENTORY; i++)
	{
		m_iInventory[i] = NULL_STRING;
	}
}

CBBInventory::~CBBInventory()
{
	
}

string_t CBBInventory::GetItemByModel(string_t model)
{
	string_t ModelToItemTable[][2] = {
		{ FindPooledString("models/props_junk/popcan01a.mdl"), AllocPooledString("item_popcan") },
		{ FindPooledString("models/props_c17/tools_wrench01a.mdl"), AllocPooledString("item_wrench") },
		// { FindPooledString("models/props_combine/combine_binocular01.mdl"), AllocPooledString("item_binocular") },
	};

	for (int i = 0; i < ARRAYSIZE(ModelToItemTable); i++)
	{
		if (ModelToItemTable[i][0] == model)
			return ModelToItemTable[i][1];
	}

	return NULL_STRING;
}

CBBItemType *CBBInventory::GetItemData(const char* name)
{
	// TODO: it's temporary, we need to load items from a file
	if (!m_invItemsDB.Count()) {
		CBBItemType* pItem = new CBBItemType();
		Q_strncpy(pItem->szClassName, "item_healthkit", sizeof(pItem->szClassName));
		Q_strncpy(pItem->szPrintName, "Healthkit", sizeof(pItem->szPrintName));
		pItem->isUsable = true;
		pItem->oneTimeUse = true;
		m_invItemsDB.Insert("item_healthkit", pItem);
		AllocPooledString(pItem->szClassName);

		pItem = new CBBItemType();
		Q_strncpy(pItem->szClassName, "item_healthkit", sizeof(pItem->szClassName));
		Q_strncpy(pItem->szPrintName, "Healthvial", sizeof(pItem->szPrintName));
		pItem->isUsable = true;
		pItem->oneTimeUse = true;
		m_invItemsDB.Insert("item_healthvial", pItem);
		AllocPooledString(pItem->szClassName);

		pItem = new CBBItemType();
		Q_strncpy(pItem->szClassName, "item_popcan", sizeof(pItem->szClassName));
		Q_strncpy(pItem->szPrintName, "Water Can", sizeof(pItem->szPrintName));
		m_invItemsDB.Insert("item_popcan", pItem);
		AllocPooledString(pItem->szClassName);

		pItem = new CBBItemType();
		Q_strncpy(pItem->szClassName, "item_wrench", sizeof(pItem->szClassName));
		Q_strncpy(pItem->szPrintName, "Wrench Tool", sizeof(pItem->szPrintName));
		m_invItemsDB.Insert("item_wrench", pItem);
		AllocPooledString(pItem->szClassName);

		pItem = new CBBItemType();
		Q_strncpy(pItem->szClassName, "item_binocular", sizeof(pItem->szClassName));
		Q_strncpy(pItem->szPrintName, "Binocular (Zoom)", sizeof(pItem->szPrintName));
		m_invItemsDB.Insert("item_binocular", pItem);
		AllocPooledString(pItem->szClassName);
	}

	INV_ITEM_HANDLE hItem = m_invItemsDB.Find(name);
	if (hItem == m_invItemsDB.InvalidIndex()) {
		Assert(0);
		return nullptr;
	}

	Assert(m_invItemsDB[hItem]);
	return m_invItemsDB[hItem];
}

bool CBBInventory::AddItem(const char* name)
{
	for (int i = 0; i < MAX_INVENTORY; i++)
	{
		if (!m_iInventory[i])
		{
			// Preload item data if it's not loaded yet, check if it's valid
			if (!GetItemData(name)) {
				Assert(0);
				return false;
			}

			m_iInventory[i] = FindPooledString(name);
			UTIL_GetLocalPlayer()->EmitSound("Weapon_PhysCannon.Pickup");
			return true;
		}
	}

	return false;
}

bool CBBInventory::RemoveItem(const char* name)
{
	if (!GetItemData(name)) {
		Assert(0);
		return false;
	}

	for (int i = 0; i < MAX_INVENTORY; i++)
	{
		if (m_iInventory[i] == FindPooledString(name))
		{
			m_iInventory[i] = NULL_STRING;
			return true;
		}
	}

	return false;
}

bool CBBInventory::RemoveItemPosition(int Position)
{
	m_iInventory[Position] = NULL_STRING;
	return true;
}

bool CBBInventory::HasItem(const char* name)
{
	if (FindPooledString(name) == NULL_STRING)
		AllocPooledString(name);

	for (int i = 0; i < MAX_INVENTORY; i++)
	{
		if (m_iInventory[i] == FindPooledString(name))
			return true;
	}
	return false;
}

// Item usafe effect is implemented in player itself.
// This function is just a check and remove item from inventory if it's usable one-time
bool CBBInventory::UseItem(const char* name)
{
	if (!CanUseItem(name))
		return false;
	
	CBBItemType* pItem = GetItemData(name);
	if (!pItem)
		return false;
	if (pItem->oneTimeUse)
		return RemoveItem(name);

	return true;
}

bool CBBInventory::CanUseItem(const char* name)
{
	if (!HasItem(name))
		return false;

	CBBItemType* pItem = GetItemData(name);
	if (!pItem)
		return false;
	
	return pItem->isUsable;
}

void CBBInventory::Think(void)
{
}

ConCommand inventory("inventory", [](const CCommand &args)
{
	CBBInventory *pInventory = UTIL_GetLocalPlayer()->GetInventory();
	if (!pInventory)
		return;

	Msg("\nInventory: %i\n", pInventory->GetItemCount());
	for (int i = 0; i < MAX_INVENTORY; i++)
	{
		if (!pInventory->m_iInventory[i])
			continue;

		CBBItemType *pItem = CBBInventory::GetItemData(STRING(pInventory->m_iInventory[i]));
		Msg("Slot %i: %s\n", i, pItem->szPrintName);
	}

	Msg("\n");
});

// Commands for testing and binding

ConCommand additem("additem", [](const CCommand &args)
{
	if (sv_cheats->GetBool() == false)
		return;

	CBBInventory *pInventory = UTIL_GetLocalPlayer()->GetInventory();
	if (!pInventory)
		return;

	pInventory->AddItem(args.Arg(1));
});

ConCommand removeitem("removeitem", [](const CCommand &args)
{
	if (sv_cheats->GetBool() == false)
		return;

	CBBInventory *pInventory = UTIL_GetLocalPlayer()->GetInventory();
	if (!pInventory)
		return;

	pInventory->RemoveItem(args.Arg(1));
});

ConCommand useitem("useitem", [](const CCommand &args)
{
	if (UTIL_GetLocalPlayer()->UseItem(args.Arg(1))) {
		DevMsg("Item is used!\n");
	}
	else {
		DevMsg("Item use failed\n");
	}

});