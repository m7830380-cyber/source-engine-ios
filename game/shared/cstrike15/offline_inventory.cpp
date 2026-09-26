//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Offline inventory for the -allskinsunlocked launch option.
//          See offline_inventory.h.
//
//===========================================================================//
#include "cbase.h"
#include "offline_inventory.h"
#include "cstrike15_item_inventory.h"
#include "econ_item_schema.h"
#include "econ_item.h"
#include "filesystem.h"
#include "tier0/icommandline.h"
#include "tier1/fmtstr.h"
#include "tier1/utlmap.h"

#ifdef CLIENT_DLL
#define OFFLINE_SIDE "client"
#else
#define OFFLINE_SIDE "server"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static const char *k_pszLoadoutFile = "cfg/offline_loadout.txt";
static const char *k_pszLoadoutPathID = "MOD";

bool OfflineInventory_IsEnabled()
{
	static int s_nEnabled = -1;
	if ( s_nEnabled < 0 )
		s_nEnabled = CommandLine()->FindParm( "-allskinsunlocked" ) ? 1 : 0;
	return s_nEnabled != 0;
}

//-----------------------------------------------------------------------------
// The item list: one CEconItem per unlocked weapon/paint combination. Owned
// here for the life of the module; inventories only reference them.
//-----------------------------------------------------------------------------
static CUtlVector< CEconItem * > s_vecItems;
static CUtlMap< uint64, CEconItem * > s_mapItems( DefLessFunc( uint64 ) );

// Deterministic, so the client and server builds of this module agree on IDs
// (the client finds a weapon's paint kit by the item ID the server gives it).
// Well below the 0xF000... range used for "base item" pseudo IDs.
static uint64 MakeItemID( int nDefIndex, int nPaintKit )
{
	return ( 1ull << 40 ) | ( uint64( nDefIndex & 0xFFFF ) << 16 ) | uint64( nPaintKit & 0xFFFF );
}

CEconItem *OfflineInventory_FindItem( uint64 ullItemID )
{
	unsigned short i = s_mapItems.Find( ullItemID );
	return s_mapItems.IsValidIndex( i ) ? s_mapItems[i] : NULL;
}

static bool IsVanillaKnife( const CCStrike15ItemDefinition *pDef )
{
	if ( pDef->GetDefaultLoadoutSlot() != LOADOUT_POSITION_MELEE || pDef->IsBaseItem() )
		return false;
	const char *pszName = pDef->GetDefinitionName();
	if ( !pszName || V_stristr( pszName, "_gg" ) || V_stristr( pszName, "ghost" ) )
		return false;
	const char *pszModel = pDef->GetBasePlayerDisplayModel();
	return pszModel && pszModel[0];
}

static void AddItem( int nDefIndex, int nPaintKit, uint32 unAccountID )
{
	uint64 ullID = MakeItemID( nDefIndex, nPaintKit );
	if ( OfflineInventory_FindItem( ullID ) )
		return;

	const CCStrike15ItemDefinition *pDef = dynamic_cast< const CCStrike15ItemDefinition * >( GetItemSchema()->GetItemDefinition( nDefIndex ) );
	if ( !pDef )
		return;
	const CPaintKit *pPaintKit = nPaintKit ? GetItemSchema()->GetPaintKitDefinition( nPaintKit ) : NULL;
	if ( nPaintKit && !pPaintKit )
		return;

	int nSlot = pDef->GetDefaultLoadoutSlot();
	bool bStar = ( nSlot == LOADOUT_POSITION_MELEE || nSlot == LOADOUT_POSITION_CLOTHING_HANDS );

	CEconItem *pItem = new CEconItem();
	pItem->SetItemID( ullID );
	pItem->SetAccountID( unAccountID );
	pItem->SetDefinitionIndex( nDefIndex );
	pItem->SetItemLevel( 1 );
	pItem->SetQuality( bStar ? AE_UNUSUAL : AE_UNIQUE );
	pItem->SetRarity( pPaintKit ? EconRarity_CombinedItemAndPaintRarity( pDef->GetRarity(), pPaintKit->nRarity ) : pDef->GetRarity() );
	pItem->SetFlags( 0 );
	// backpack position 1..N: an acknowledged item (0 or the unacked bit would show "new item" popups)
	pItem->SetInventoryToken( ( s_vecItems.Count() + 1 ) & kBackendPositionMask_Position );

	if ( pPaintKit )
	{
		static CSchemaAttributeDefHandle pAttr_PaintKit( "set item texture prefab" );
		static CSchemaAttributeDefHandle pAttr_PaintKitSeed( "set item texture seed" );
		static CSchemaAttributeDefHandle pAttr_PaintKitWear( "set item texture wear" );
		if ( pAttr_PaintKit )
			pItem->AddOrSetCustomAttribute( pAttr_PaintKit->GetDefinitionIndex(), nPaintKit );
		if ( pAttr_PaintKitSeed )
			pItem->AddOrSetCustomAttribute( pAttr_PaintKitSeed->GetDefinitionIndex(), 0 );
		if ( pAttr_PaintKitWear )	// the cleanest wear this paint kit allows
			pItem->AddOrSetCustomAttribute( pAttr_PaintKitWear->GetDefinitionIndex(), MAX( pPaintKit->flWearRemapMin, 0.0001f ) );
	}

	s_vecItems.AddToTail( pItem );
	s_mapItems.Insert( ullID, pItem );
}

static void BuildItems( uint32 unAccountID )
{
	if ( s_vecItems.Count() )
		return;

	// Every weapon/knife/glove + paint kit pair the game has an icon for is a
	// real combination (alternate_icons2/weapon_icons in items_game.txt), keyed
	// ( def << 16 ) + ( paint << 2 ) + wear bucket. Sticker keys have bit 32 set.
	CEconItemSchema::AlternateIconsMap_t &mapIcons = GetItemSchema()->GetAlternateIconsMap();
	FOR_EACH_MAP_FAST( mapIcons, i )
	{
		uint64 ullKey = mapIcons.Key( i );
		if ( ullKey >> 32 )
			continue;
		if ( ( ullKey & 3 ) != 0 )	// one entry per wear bucket; keep the first
			continue;
		int nDefIndex = int( ullKey >> 16 );
		int nPaintKit = int( ( ullKey & 0xFFFF ) >> 2 );
		if ( nPaintKit )
			AddItem( nDefIndex, nPaintKit, unAccountID );
	}

	// Plain ("vanilla") versions of every knife
	const CEconItemSchema::ItemDefinitionMap_t &mapDefs = GetItemSchema()->GetItemDefinitionMap();
	FOR_EACH_MAP_FAST( mapDefs, i )
	{
		const CCStrike15ItemDefinition *pDef = dynamic_cast< const CCStrike15ItemDefinition * >( mapDefs[i] );
		if ( pDef && IsVanillaKnife( pDef ) )
			AddItem( pDef->GetDefinitionIndex(), 0, unAccountID );
	}

	Msg( "[offline inventory] %d items unlocked\n", s_vecItems.Count() );
}

//-----------------------------------------------------------------------------
// Loadout file
//-----------------------------------------------------------------------------
static CUtlMap< CCSPlayerInventory *, long > s_mapFilledFileTime( DefLessFunc( CCSPlayerInventory * ) );

static long LoadoutFileTime()
{
	return g_pFullFileSystem->FileExists( k_pszLoadoutFile, k_pszLoadoutPathID ) ? g_pFullFileSystem->GetFileTime( k_pszLoadoutFile, k_pszLoadoutPathID ) : 0;
}

bool OfflineInventory_NeedsRefill( CCSPlayerInventory *pInventory )
{
	unsigned short i = s_mapFilledFileTime.Find( pInventory );
	return !s_mapFilledFileTime.IsValidIndex( i ) || pInventory->GetItemCount() == 0 || s_mapFilledFileTime[i] != LoadoutFileTime();
}

void OfflineInventory_Fill( CCSPlayerInventory *pInventory, const CSteamID &owner )
{
	if ( !pInventory )
		return;

	BuildItems( owner.GetAccountID() );

	// Sets the owner and registers the inventory so lookups by account ID find it
	InventoryManager()->SteamRequestInventory( pInventory, owner );

	pInventory->SOClear();
	pInventory->ResetLoadoutItemIDs();

	KeyValues *pKV = new KeyValues( "OfflineLoadout" );
	KeyValues::AutoDelete autodelete( pKV );
	pKV->LoadFromFile( g_pFullFileSystem, k_pszLoadoutFile, k_pszLoadoutPathID );

	// Equipped state lives on the items; start clean, then apply the file
	FOR_EACH_VEC( s_vecItems, i )
	{
		for ( int iTeam = 0; iTeam < LOADOUT_COUNT; iTeam++ )
			s_vecItems[i]->UpdateEquippedState( iTeam, INVALID_EQUIPPED_SLOT );
	}
	int nEquipped = 0;
	for ( int iTeam = 0; iTeam < LOADOUT_COUNT; iTeam++ )
	{
		for ( int iSlot = 0; iSlot < LOADOUT_POSITION_COUNT; iSlot++ )
		{
			CEconItem *pItem = OfflineInventory_FindItem( pKV->GetUint64( CFmtStr( "item_%d_%d", iTeam, iSlot ), 0 ) );
			if ( pItem )
			{
				pItem->UpdateEquippedState( iTeam, iSlot );
				nEquipped++;
			}
		}
	}

	// ItemHasBeenUpdated() puts each equipped item into the loadout slots
	FOR_EACH_VEC( s_vecItems, i )
	{
		pInventory->AddEconItem( s_vecItems[i], false, false, false );
	}

	for ( int iTeam = 0; iTeam < LOADOUT_COUNT; iTeam++ )
	{
		for ( int iSlot = 0; iSlot < LOADOUT_POSITION_COUNT; iSlot++ )
		{
			int nDef = pKV->GetInt( CFmtStr( "def_%d_%d", iTeam, iSlot ), 0 );
			if ( nDef )
				pInventory->SetDefaultEquippedDefinitionItemBySlot( iTeam, iSlot, nDef );
		}
	}

	s_mapFilledFileTime.InsertOrReplace( pInventory, LoadoutFileTime() );

	printf( "[offline] " OFFLINE_SIDE " fill: owner %llu, %d items, %d equipped from %s (exists %d, time %ld)\n",
		owner.ConvertToUint64(), pInventory->GetItemCount(), nEquipped, k_pszLoadoutFile,
		(int)g_pFullFileSystem->FileExists( k_pszLoadoutFile, k_pszLoadoutPathID ), LoadoutFileTime() );
	fflush( stdout );
}

void OfflineInventory_SaveLoadout( CCSPlayerInventory *pInventory )
{
	if ( !pInventory )
		return;

	KeyValues *pKV = new KeyValues( "OfflineLoadout" );
	KeyValues::AutoDelete autodelete( pKV );
	int nSaved = 0;

	for ( int iTeam = 0; iTeam < LOADOUT_COUNT; iTeam++ )
	{
		for ( int iSlot = 0; iSlot < LOADOUT_POSITION_COUNT; iSlot++ )
		{
			itemid_t ullID = pInventory->GetLoadoutItemID( iTeam, iSlot );
			if ( ullID != LOADOUT_SLOT_USE_BASE_ITEM && OfflineInventory_FindItem( ullID ) )
			{
				pKV->SetUint64( CFmtStr( "item_%d_%d", iTeam, iSlot ), ullID );
				nSaved++;
			}

			CEconItemView *pDefault = pInventory->FindDefaultEquippedDefinitionItemBySlot( iTeam, iSlot );
			if ( pDefault && pDefault->IsValid() )
				pKV->SetInt( CFmtStr( "def_%d_%d", iTeam, iSlot ), pDefault->GetItemDefinition()->GetDefinitionIndex() );
		}
	}

	g_pFullFileSystem->CreateDirHierarchy( "cfg", k_pszLoadoutPathID );
	bool bSaved = pKV->SaveToFile( g_pFullFileSystem, k_pszLoadoutFile, k_pszLoadoutPathID );
	char szFullPath[MAX_PATH] = "";
	g_pFullFileSystem->RelativePathToFullPath( k_pszLoadoutFile, k_pszLoadoutPathID, szFullPath, sizeof( szFullPath ) );
	printf( "[offline] " OFFLINE_SIDE " saved loadout: %d items, write %s, path '%s'\n", nSaved, bSaved ? "ok" : "FAILED", szFullPath );
	fflush( stdout );

	// this inventory already matches the file it just wrote
	s_mapFilledFileTime.InsertOrReplace( pInventory, LoadoutFileTime() );
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Console access, handy until the inventory screens work:
//   offline_list [filter]                 lists unlocked items
//   offline_equip <item id>               equips an item for every team that can use it
//-----------------------------------------------------------------------------
CON_COMMAND_F( offline_list, "List unlocked items (with -allskinsunlocked). Optional name filter.", FCVAR_RELEASE )
{
	const char *pszFilter = args.ArgC() > 1 ? args[1] : NULL;
	FOR_EACH_VEC( s_vecItems, i )
	{
		CEconItem *pItem = s_vecItems[i];
		const CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinition( pItem->GetDefinitionIndex() );
		const CPaintKit *pPaintKit = GetItemSchema()->GetPaintKitDefinition( int( pItem->GetItemID() & 0xFFFF ) );
		const char *pszDef = pDef ? pDef->GetDefinitionName() : "?";
		const char *pszPaint = pPaintKit ? pPaintKit->sName.String() : "vanilla";
		if ( pszFilter && !V_stristr( pszDef, pszFilter ) && !V_stristr( pszPaint, pszFilter ) )
			continue;
		Msg( "%llu  %s  %s\n", pItem->GetItemID(), pszDef, pszPaint );
	}
}

CON_COMMAND_F( offline_equip, "Equip an unlocked item by id (see offline_list) for every team that can use it", FCVAR_RELEASE )
{
	if ( args.ArgC() < 2 )
		return;
	uint64 ullID = V_atoui64( args[1] );
	CEconItem *pItem = OfflineInventory_FindItem( ullID );
	if ( !pItem )
	{
		Msg( "offline_equip: no item %s\n", args[1] );
		return;
	}
	const CCStrike15ItemDefinition *pDef = dynamic_cast< const CCStrike15ItemDefinition * >( GetItemSchema()->GetItemDefinition( pItem->GetDefinitionIndex() ) );
	for ( int iTeam = TEAM_TERRORIST; iTeam <= TEAM_CT; iTeam++ )
	{
		if ( !pDef || !pDef->CanBeUsedByTeam( iTeam ) )
			continue;
		int nSlot = pDef->GetLoadoutSlot( iTeam );
		if ( nSlot >= 0 )
		{
			bool bOK = CSInventoryManager()->EquipItemInLoadout( iTeam, nSlot, ullID );
			Msg( "offline_equip: %s for team %d slot %d: %s\n", pDef->GetDefinitionName(), iTeam, nSlot, bOK ? "ok" : "failed" );
		}
	}
}
#endif
