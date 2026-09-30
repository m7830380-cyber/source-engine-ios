//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Offline inventory (-allskinsunlocked / -unlockitemgivermenu).
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
#include "tier1/utlbuffer.h"

#ifdef CLIENT_DLL
#include "GameEventListener.h"
#include "c_playerresource.h"
#define OFFLINE_SIDE "client"
#else
#define OFFLINE_SIDE "server"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static const char *k_pszLoadoutFile = "cfg/offline_loadout.txt";
static const char *k_pszPathID = "MOD";

//-----------------------------------------------------------------------------
// Modes
//-----------------------------------------------------------------------------
bool OfflineInventory_IsGiverMode()
{
	static int s_nGiver = -1;
	if ( s_nGiver < 0 )
		s_nGiver = CommandLine()->FindParm( "-unlockitemgivermenu" ) ? 1 : 0;
	return s_nGiver != 0;
}

// -unlockitemgivermenu wins if both are given: the inventory starts empty
static bool IsUnlockAll()
{
	static int s_nAll = -1;
	if ( s_nAll < 0 )
		s_nAll = CommandLine()->FindParm( "-allskinsunlocked" ) ? 1 : 0;
	return s_nAll != 0 && !OfflineInventory_IsGiverMode();
}

bool OfflineInventory_IsEnabled()
{
	return IsUnlockAll() || OfflineInventory_IsGiverMode();
}

//-----------------------------------------------------------------------------
// IDs from before items carried their look: ( 1 << 40 ) | ( def << 16 ) | paint,
// plus 1 << 41 for StatTrak. Loadouts and StatTrak counts saved with them are
// moved over to the equivalent -allskinsunlocked item.
//-----------------------------------------------------------------------------
// Item IDs in files: written as decimal strings, but KeyValues' parser types a
// digits-only value with strtol, which on 64-bit iOS (64-bit long) takes the
// whole ID without overflow and then keeps it as a 32-bit int: every saved ID
// came back truncated (no equips in matches, received items lost). Files that
// hold IDs are loaded through LoadIDFile, which turns long decimal values into
// the "0x" + 16 hex digits form KeyValues reads as a uint64.
static bool LoadIDFile( KeyValues *pKV, const char *pszFile )
{
	CUtlBuffer bufIn;
	if ( !g_pFullFileSystem->ReadFile( pszFile, k_pszPathID, bufIn ) )
		return false;
	const char *pIn = (const char *)bufIn.Base();
	int nIn = bufIn.TellPut();
	CUtlVector< char > vecOut;
	for ( int i = 0; i < nIn; )
	{
		if ( pIn[i] == '"' )
		{
			int j = i + 1;
			while ( j < nIn && pIn[j] >= '0' && pIn[j] <= '9' )
				j++;
			int nDigits = j - i - 1;
			if ( j < nIn && pIn[j] == '"' && nDigits >= 10 && nDigits <= 20 )
			{
				char szDigits[24];
				V_strncpy( szDigits, pIn + i + 1, nDigits + 1 );
				char szHex[32];
				V_snprintf( szHex, sizeof( szHex ), "\"0x%016llX\"", V_atoui64( szDigits ) );
				vecOut.AddMultipleToTail( V_strlen( szHex ), szHex );
				i = j + 1;
				continue;
			}
		}
		vecOut.AddToTail( pIn[i++] );
	}
	vecOut.AddToTail( 0 );
	return pKV->LoadFromBuffer( pszFile, vecOut.Base() );
}

// an ID value: TYPE_UINT64 from LoadIDFile, or a decimal string (set in memory,
// e.g. a LAN player's loadout)
static uint64 ReadIDValue( KeyValues *pSub )
{
	if ( !pSub )
		return 0;
	if ( pSub->GetDataType() == KeyValues::TYPE_UINT64 )
		return pSub->GetUint64();
	if ( pSub->GetDataType() != KeyValues::TYPE_STRING )
		return 0;	// a truncated int: not an ID
	const char *psz = pSub->GetString();
	return ( psz && psz[0] ) ? V_atoui64( psz ) : 0;
}

static uint64 ReadID( KeyValues *pKV, const char *pszKey )
{
	return ReadIDValue( pKV->FindKey( pszKey ) );
}

static bool IsOldID( uint64 ullID )
{
	return ( ullID >> 42 ) == 0 && ( ullID & ( 1ull << 40 ) );
}

static const CCStrike15ItemDefinition *CSDef( int nDef )
{
	return dynamic_cast< const CCStrike15ItemDefinition * >( GetItemSchema()->GetItemDefinition( nDef ) );
}

// the look -allskinsunlocked gives a skin: its cleanest wear, pattern 0
static uint64 DefaultSkinID( int nDef, int nPaint, bool bStatTrak )
{
	const CPaintKit *pPaintKit = nPaint ? GetItemSchema()->GetPaintKitDefinition( nPaint ) : NULL;
	float flWear = pPaintKit ? MAX( pPaintKit->flWearRemapMin, 0.0001f ) : 0.0f;
	return OfflineID_MakeSkin( nDef, nPaint, 0, flWear, bStatTrak );
}

static uint64 MigrateID( uint64 ullID )
{
	if ( !IsOldID( ullID ) )
		return ullID;
	return DefaultSkinID( int( ( ullID >> 16 ) & 0xFFFF ), int( ullID & 0xFFFF ), ( ullID & ( 1ull << 41 ) ) != 0 );
}

// skins and knives have a StatTrak version (gloves don't, as in CS:GO)
static bool CanBeStatTrak( const CCStrike15ItemDefinition *pDef, int nPaintKit )
{
	int nSlot = pDef->GetDefaultLoadoutSlot();
	return nSlot != LOADOUT_POSITION_CLOTHING_HANDS && ( nPaintKit || nSlot == LOADOUT_POSITION_MELEE );
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

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// StatTrak counts: cfg/offline_stattrak.txt, by item ID. The server has no say:
// CS:GO kept the count on Valve's item server; here your own game counts from
// the kill event, which names the item that made the kill (weapon_itemid).
//-----------------------------------------------------------------------------
static const char *k_pszStatTrakFile = "cfg/offline_stattrak.txt";
static KeyValues *s_pStatTrakCounts = NULL;

static KeyValues *StatTrakCounts()
{
	if ( !s_pStatTrakCounts )
	{
		s_pStatTrakCounts = new KeyValues( "OfflineStatTrak" );
		s_pStatTrakCounts->LoadFromFile( g_pFullFileSystem, k_pszStatTrakFile, k_pszPathID );

		// counts saved under old IDs go to the -allskinsunlocked StatTrak version
		CUtlVector< KeyValues * > vecOld;
		for ( KeyValues *p = s_pStatTrakCounts->GetFirstValue(); p; p = p->GetNextValue() )
		{
			if ( IsOldID( V_atoui64( p->GetName() ) ) )
				vecOld.AddToTail( p );
		}
		FOR_EACH_VEC( vecOld, i )
		{
			uint64 ullOld = V_atoui64( vecOld[i]->GetName() );
			uint64 ullNew = DefaultSkinID( int( ( ullOld >> 16 ) & 0xFFFF ), int( ullOld & 0xFFFF ), true );
			CFmtStr strNew( "%llu", ullNew );
			if ( !s_pStatTrakCounts->FindKey( strNew ) )
				s_pStatTrakCounts->SetInt( strNew, vecOld[i]->GetInt() );
			s_pStatTrakCounts->RemoveSubKey( vecOld[i] );
			vecOld[i]->deleteThis();
		}
		if ( vecOld.Count() )
			s_pStatTrakCounts->SaveToFile( g_pFullFileSystem, k_pszStatTrakFile, k_pszPathID );
	}
	return s_pStatTrakCounts;
}
#endif

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Customizations of our items (name tags so far): cfg/offline_custom.txt, by
// item ID: "<id>" { "name" "..." }. Only this game shows them (your viewmodel,
// your inventory); the ID stays what the item looks like underneath.
//-----------------------------------------------------------------------------
static const char *k_pszCustomFile = "cfg/offline_custom.txt";
static KeyValues *s_pCustom = NULL;

static KeyValues *Customizations()
{
	if ( !s_pCustom )
	{
		s_pCustom = new KeyValues( "OfflineCustom" );
		s_pCustom->LoadFromFile( g_pFullFileSystem, k_pszCustomFile, k_pszPathID );
	}
	return s_pCustom;
}

static void SaveCustomizations()
{
	g_pFullFileSystem->CreateDirHierarchy( "cfg", k_pszPathID );
	Customizations()->SaveToFile( g_pFullFileSystem, k_pszCustomFile, k_pszPathID );
}

static void ApplyCustomizations( CEconItem *pItem )
{
	KeyValues *pKV = Customizations()->FindKey( CFmtStr( "%llu", pItem->GetItemID() ) );
	const char *pszName = pKV ? pKV->GetString( "name", "" ) : "";
	pItem->SetCustomName( pszName[0] ? pszName : NULL );
}
#endif

static int NameTagDef()
{
	static int s_nDef = -1;
	if ( s_nDef < 0 )
	{
		const CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinitionByName( "Name Tag" );
		s_nDef = pDef ? pDef->GetDefinitionIndex() : 0;
	}
	return s_nDef;
}

bool OfflineItem_IsNameTag( uint64 ullItemID )
{
	return OfflineID_IsOffline( ullItemID ) && OfflineID_Kind( ullItemID ) == OFFLINE_KIND_TOOL && NameTagDef() && OfflineID_Def( ullItemID ) == NameTagDef();
}

//-----------------------------------------------------------------------------
// Building an item from its ID
//-----------------------------------------------------------------------------
static int s_nNextToken = 1;

static CEconItem *CreateItem( uint64 ullID, uint32 unAccountID )
{
	if ( !OfflineID_IsOffline( ullID ) )
		return NULL;

	int nKind = OfflineID_Kind( ullID );
	int nDef = OfflineID_Def( ullID );
	const CCStrike15ItemDefinition *pDef = ( nKind == OFFLINE_KIND_SKIN || nKind == OFFLINE_KIND_TOOL ) ? CSDef( nDef ) : NULL;
	if ( !pDef )
		return NULL;	// stickers and music kits: not yet

	CEconItem *pItem = new CEconItem();
	pItem->SetItemID( ullID );
	pItem->SetAccountID( unAccountID );
	pItem->SetDefinitionIndex( nDef );
	pItem->SetItemLevel( 1 );
	pItem->SetFlags( 0 );
	// backpack position 1..N: an acknowledged item (0 or the unacked bit would show "new item" popups)
	pItem->SetInventoryToken( ( s_nNextToken++ ) & kBackendPositionMask_Position );

	if ( nKind == OFFLINE_KIND_TOOL )
	{
		pItem->SetQuality( AE_UNIQUE );
		pItem->SetRarity( pDef->GetRarity() );
		return pItem;
	}

	int nPaint = OfflineID_Paint( ullID );
	const CPaintKit *pPaintKit = nPaint ? GetItemSchema()->GetPaintKitDefinition( nPaint ) : NULL;
	if ( nPaint && !pPaintKit )
	{
		delete pItem;
		return NULL;
	}
	int nSlot = pDef->GetDefaultLoadoutSlot();
	bool bStar = ( nSlot == LOADOUT_POSITION_MELEE || nSlot == LOADOUT_POSITION_CLOTHING_HANDS );
	pItem->SetQuality( bStar ? AE_UNUSUAL : AE_UNIQUE );
	pItem->SetRarity( pPaintKit ? EconRarity_CombinedItemAndPaintRarity( pDef->GetRarity(), pPaintKit->nRarity ) : pDef->GetRarity() );

	if ( pPaintKit )
	{
		static CSchemaAttributeDefHandle pAttr_PaintKit( "set item texture prefab" );
		static CSchemaAttributeDefHandle pAttr_PaintKitSeed( "set item texture seed" );
		static CSchemaAttributeDefHandle pAttr_PaintKitWear( "set item texture wear" );
		if ( pAttr_PaintKit )
			pItem->AddOrSetCustomAttribute( pAttr_PaintKit->GetDefinitionIndex(), nPaint );
		if ( pAttr_PaintKitSeed )
			pItem->AddOrSetCustomAttribute( pAttr_PaintKitSeed->GetDefinitionIndex(), OfflineID_Seed( ullID ) );
		if ( pAttr_PaintKitWear )
			pItem->AddOrSetCustomAttribute( pAttr_PaintKitWear->GetDefinitionIndex(), MAX( OfflineID_Wear( ullID ), 0.0001f ) );
	}

	// StatTrak: the kill counter attributes (both stored as integers)
	if ( OfflineID_IsStatTrak( ullID ) )
	{
		static CSchemaAttributeDefHandle pAttr_KillEater( "kill eater" );
		static CSchemaAttributeDefHandle pAttr_KillEaterType( "kill eater score type" );
		if ( pAttr_KillEater && pAttr_KillEaterType )
		{
			pItem->SetQuality( AE_STRANGE );
			uint32 unKills = 0;
#ifdef CLIENT_DLL
			unKills = (uint32)StatTrakCounts()->GetInt( CFmtStr( "%llu", ullID ), 0 );
#endif
			pItem->SetDynamicAttributeValue( pAttr_KillEater, unKills );
			pItem->SetDynamicAttributeValue( pAttr_KillEaterType, (uint32)0 );	// kills
		}
	}
	return pItem;
}

//-----------------------------------------------------------------------------
// Items: ours (s_mapItems: the client's inventory contents), and ones rebuilt
// from IDs we were only told about (s_mapDecoded). The client keeps the latter,
// and the Item Giver's catalog, in inventories of their own so they have views.
//-----------------------------------------------------------------------------
static CUtlMap< uint64, CEconItem * > s_mapItems( DefLessFunc( uint64 ) );
static CUtlVector< CEconItem * > s_vecItems;
static CUtlMap< uint64, CEconItem * > s_mapDecoded( DefLessFunc( uint64 ) );

#ifdef CLIENT_DLL
static const uint32 k_unRemoteAccount = 0x7FFFFFF0;		// other players' items
static const uint32 k_unCatalogAccount = 0x7FFFFFF1;	// the Item Giver's catalog
static CCSPlayerInventory *s_pRemoteInventory = NULL;
static CCSPlayerInventory *s_pCatalogInventory = NULL;
static CUtlMap< uint64, CEconItem * > s_mapCatalog( DefLessFunc( uint64 ) );
static CUtlVector< uint64 > s_vecCatalog;

static CCSPlayerInventory *HiddenInventory( CCSPlayerInventory *&pInventory, uint32 unAccount )
{
	if ( !pInventory && InventoryManager() )
	{
		pInventory = new CCSPlayerInventory();
		InventoryManager()->SteamRequestInventory( pInventory, CSteamID( unAccount, k_EUniversePublic, k_EAccountTypeIndividual ) );
	}
	return pInventory;
}
#endif

CEconItem *OfflineInventory_FindItem( uint64 ullItemID )
{
	if ( !ullItemID )
		return NULL;
	ullItemID = MigrateID( ullItemID );

	unsigned short i = s_mapItems.Find( ullItemID );
	if ( s_mapItems.IsValidIndex( i ) )
		return s_mapItems[i];
#ifdef CLIENT_DLL
	i = s_mapCatalog.Find( ullItemID );
	if ( s_mapCatalog.IsValidIndex( i ) )
		return s_mapCatalog[i];
#endif
	i = s_mapDecoded.Find( ullItemID );
	if ( s_mapDecoded.IsValidIndex( i ) )
		return s_mapDecoded[i];

	// someone else's item: rebuild it from its ID
#ifdef CLIENT_DLL
	CEconItem *pItem = CreateItem( ullItemID, k_unRemoteAccount );
#else
	CEconItem *pItem = CreateItem( ullItemID, 0 );
#endif
	if ( !pItem )
		return NULL;
	s_mapDecoded.Insert( ullItemID, pItem );
#ifdef CLIENT_DLL
	if ( CCSPlayerInventory *pRemote = HiddenInventory( s_pRemoteInventory, k_unRemoteAccount ) )
		pRemote->AddEconItem( pItem, false, false, false );
#endif
	return pItem;
}

#ifdef CLIENT_DLL
CEconItemView *OfflineInventory_FindView( uint64 ullItemID )
{
	ullItemID = MigrateID( ullItemID );
	if ( !OfflineInventory_FindItem( ullItemID ) )
		return NULL;
	CCSPlayerInventory *pInventories[] = { CSInventoryManager() ? CSInventoryManager()->GetLocalCSInventory() : NULL, s_pCatalogInventory, s_pRemoteInventory };
	for ( int i = 0; i < ARRAYSIZE( pInventories ); i++ )
	{
		CEconItemView *pView = pInventories[i] ? pInventories[i]->GetInventoryItemByItemID( ullItemID ) : NULL;
		if ( pView )
			return pView;
	}
	return NULL;
}
#endif

//-----------------------------------------------------------------------------
// The client's items: the catalog (every skin, knife and glove + paint kit pair
// the game has an icon for, a normal and a StatTrak version; the cases), which
// is the inventory with -allskinsunlocked and the Item Giver's list with
// -unlockitemgivermenu; there the inventory is what was received
// (cfg/offline_items.txt).
//-----------------------------------------------------------------------------
#ifdef CLIENT_DLL
static const char *k_pszItemsFile = "cfg/offline_items.txt";
static uint64 s_ullNextSerial = 1;
static bool s_bBuilt = false;
static const int k_nUnlockAllNameTags = 10;
static uint32 s_unLocalAccount = 0;

extern void OfflineCase_Build();

static void AddLocal( CEconItem *pItem )
{
	ApplyCustomizations( pItem );
	s_mapItems.InsertOrReplace( pItem->GetItemID(), pItem );
	s_vecItems.AddToTail( pItem );
}

static void AddCatalog( uint64 ullID )
{
	if ( s_mapCatalog.Find( ullID ) != s_mapCatalog.InvalidIndex() )
		return;
	CEconItem *pItem = CreateItem( ullID, OfflineInventory_IsGiverMode() ? k_unCatalogAccount : s_unLocalAccount );
	if ( !pItem )
		return;
	s_mapCatalog.Insert( ullID, pItem );
	s_vecCatalog.AddToTail( ullID );
}

static void BuildCatalog()
{
	// ( def << 16 ) + ( paint << 2 ) + wear bucket keys; sticker keys have bit 32 set
	CEconItemSchema::AlternateIconsMap_t &mapIcons = GetItemSchema()->GetAlternateIconsMap();
	FOR_EACH_MAP_FAST( mapIcons, i )
	{
		uint64 ullKey = mapIcons.Key( i );
		if ( ( ullKey >> 32 ) || ( ullKey & 3 ) != 0 )
			continue;
		int nDef = int( ullKey >> 16 ), nPaint = int( ( ullKey & 0xFFFF ) >> 2 );
		const CCStrike15ItemDefinition *pDef = CSDef( nDef );
		if ( !pDef || !nPaint )
			continue;
		AddCatalog( DefaultSkinID( nDef, nPaint, false ) );
		if ( CanBeStatTrak( pDef, nPaint ) )
			AddCatalog( DefaultSkinID( nDef, nPaint, true ) );
	}

	// plain ("vanilla") versions of every knife
	const CEconItemSchema::ItemDefinitionMap_t &mapDefs = GetItemSchema()->GetItemDefinitionMap();
	FOR_EACH_MAP_FAST( mapDefs, i )
	{
		const CCStrike15ItemDefinition *pDef = dynamic_cast< const CCStrike15ItemDefinition * >( mapDefs[i] );
		if ( pDef && IsVanillaKnife( pDef ) )
		{
			AddCatalog( DefaultSkinID( pDef->GetDefinitionIndex(), 0, false ) );
			AddCatalog( DefaultSkinID( pDef->GetDefinitionIndex(), 0, true ) );
		}
	}

	// the weapon cases (with contents among the skins above)
	OfflineCase_Build();

	// name tags (Item Giver)
	if ( NameTagDef() && OfflineInventory_IsGiverMode() )
		AddCatalog( OfflineID_MakeOther( OFFLINE_KIND_TOOL, NameTagDef() ) );
}

static void LoadOwned()
{
	KeyValues *pKV = new KeyValues( "OfflineItems" );
	KeyValues::AutoDelete autodelete( pKV );
	LoadIDFile( pKV, k_pszItemsFile );
	s_ullNextSerial = MAX( pKV->GetUint64( "next_serial", 1 ), (uint64)1 );
	if ( KeyValues *pList = pKV->FindKey( "items" ) )
	{
		for ( KeyValues *p = pList->GetFirstValue(); p; p = p->GetNextValue() )
		{
			uint64 ullID = ReadIDValue( p );
			if ( !ullID || s_mapItems.Find( ullID ) != s_mapItems.InvalidIndex() )
				continue;
			if ( CEconItem *pItem = CreateItem( ullID, s_unLocalAccount ) )
				AddLocal( pItem );
		}
	}
}

static void SaveOwned()
{
	if ( !OfflineInventory_IsGiverMode() )
		return;		// -allskinsunlocked: nothing kept (name tags come back each launch)
	KeyValues *pKV = new KeyValues( "OfflineItems" );
	KeyValues::AutoDelete autodelete( pKV );
	pKV->SetUint64( "next_serial", s_ullNextSerial );
	KeyValues *pList = pKV->FindKey( "items", true );
	FOR_EACH_VEC( s_vecItems, i )
		pList->SetString( CFmtStr( "%d", i + 1 ), CFmtStr( "%llu", s_vecItems[i]->GetItemID() ) );
	g_pFullFileSystem->CreateDirHierarchy( "cfg", k_pszPathID );
	pKV->SaveToFile( g_pFullFileSystem, k_pszItemsFile, k_pszPathID );
}

static void BuildLocal( uint32 unAccountID )
{
	if ( s_bBuilt )
		return;
	s_bBuilt = true;
	s_unLocalAccount = unAccountID;

	BuildCatalog();

	if ( OfflineInventory_IsGiverMode() )
	{
		// the catalog lives in its own inventory (views for the Item Giver tab)
		if ( CCSPlayerInventory *pCatalog = HiddenInventory( s_pCatalogInventory, k_unCatalogAccount ) )
		{
			FOR_EACH_VEC( s_vecCatalog, i )
				pCatalog->AddEconItem( s_mapCatalog[ s_mapCatalog.Find( s_vecCatalog[i] ) ], false, false, false );
		}
		LoadOwned();
		Msg( "[offline inventory] item giver: %d in the catalog, %d received\n", s_vecCatalog.Count(), s_vecItems.Count() );
	}
	else
	{
		// -allskinsunlocked: the catalog is the inventory
		FOR_EACH_VEC( s_vecCatalog, i )
		{
			unsigned short iItem = s_mapCatalog.Find( s_vecCatalog[i] );
			CEconItem *pItem = s_mapCatalog[iItem];
			s_mapCatalog.RemoveAt( iItem );
			AddLocal( pItem );
		}
		s_vecCatalog.Purge();

		// name tags: used up like in CS:GO, a fresh set each launch
		if ( NameTagDef() )
		{
			for ( int i = 1; i <= k_nUnlockAllNameTags; i++ )
			{
				if ( CEconItem *pItem = CreateItem( OfflineID_MakeOther( OFFLINE_KIND_TOOL, NameTagDef(), i ), unAccountID ) )
					AddLocal( pItem );
			}
		}
		Msg( "[offline inventory] %d items unlocked\n", s_vecItems.Count() );
	}
}

const CUtlVector< uint64 > &OfflineGiver_GetCatalog()
{
	return s_vecCatalog;
}

static void AddToLocalInventory( CEconItem *pItem )
{
	CCSPlayerInventory *pInventory = CSInventoryManager() ? CSInventoryManager()->GetLocalCSInventory() : NULL;
	if ( pInventory )
		pInventory->AddEconItem( pItem, false, false, false );
}

// a received skin: the look rolled like a case drop (pattern 0-1000, wear
// within the paint kit's range); vanilla knives have neither
static uint64 RollSkin( uint64 ullCatalogID )
{
	int nDef = OfflineID_Def( ullCatalogID ), nPaint = OfflineID_Paint( ullCatalogID );
	bool bStatTrak = OfflineID_IsStatTrak( ullCatalogID );
	const CPaintKit *pPaintKit = nPaint ? GetItemSchema()->GetPaintKitDefinition( nPaint ) : NULL;
	for ( int nTry = 0; nTry < 16; nTry++ )
	{
		int nSeed = pPaintKit ? RandomInt( 0, 1000 ) : 0;
		float flWear = pPaintKit ? RandomFloat( pPaintKit->flWearRemapMin, pPaintKit->flWearRemapMax ) : 0.0f;
		uint64 ullID = OfflineID_MakeSkin( nDef, nPaint, nSeed, flWear, bStatTrak );
		if ( s_mapItems.Find( ullID ) == s_mapItems.InvalidIndex() )
			return ullID;
	}
	return 0;
}

uint64 OfflineGiver_Receive( uint64 ullCatalogID )
{
	if ( !OfflineInventory_IsGiverMode() || s_mapCatalog.Find( ullCatalogID ) == s_mapCatalog.InvalidIndex() )
		return 0;

	uint64 ullID = 0;
	if ( OfflineID_Kind( ullCatalogID ) == OFFLINE_KIND_SKIN )
		ullID = RollSkin( ullCatalogID );
	else
		ullID = OfflineID_MakeOther( OfflineID_Kind( ullCatalogID ), OfflineID_Def( ullCatalogID ), s_ullNextSerial++ );
	CEconItem *pItem = ullID ? CreateItem( ullID, s_unLocalAccount ) : NULL;
	if ( !pItem )
		return 0;

	AddLocal( pItem );
	AddToLocalInventory( pItem );
	SaveOwned();
	return ullID;
}

// a received item used up (a case opened, a name tag used) or deleted
static void RemoveOwned( uint64 ullID )
{
	unsigned short i = s_mapItems.Find( ullID );
	if ( !s_mapItems.IsValidIndex( i ) )
		return;
	CEconItem *pItem = s_mapItems[i];
	s_mapItems.RemoveAt( i );
	s_vecItems.FindAndRemove( pItem );
	SaveOwned();
	// rebuild the inventory without it (and with the loadout re-applied)
	CCSPlayerInventory *pInventory = CSInventoryManager() ? CSInventoryManager()->GetLocalCSInventory() : NULL;
	if ( pInventory && steamapicontext && steamapicontext->SteamUser() )
	{
		OfflineInventory_Fill( pInventory, steamapicontext->SteamUser()->GetSteamID() );
		OfflineInventory_SaveLoadout( pInventory );	// without it, if it was equipped
	}
	// the item object may still be referenced by a view this frame: keep it
}

//-----------------------------------------------------------------------------
// Name tags
//-----------------------------------------------------------------------------
static bool IsOwned( uint64 ullID )
{
	return s_mapItems.Find( ullID ) != s_mapItems.InvalidIndex();
}

// weapons and knives take a name; gloves don't (as in CS:GO)
bool OfflineItem_CanBeNamed( uint64 ullItemID )
{
	if ( !IsOwned( ullItemID ) || OfflineID_Kind( ullItemID ) != OFFLINE_KIND_SKIN )
		return false;
	const CCStrike15ItemDefinition *pDef = CSDef( OfflineID_Def( ullItemID ) );
	return pDef && pDef->GetDefaultLoadoutSlot() != LOADOUT_POSITION_CLOTHING_HANDS;
}

void OfflineNameTag_GetOwned( CUtlVector< uint64 > &vecTags )
{
	vecTags.RemoveAll();
	FOR_EACH_VEC( s_vecItems, i )
	{
		if ( OfflineItem_IsNameTag( s_vecItems[i]->GetItemID() ) )
			vecTags.AddToTail( s_vecItems[i]->GetItemID() );
	}
}

void OfflineItem_GetNameable( CUtlVector< uint64 > &vecItems )
{
	vecItems.RemoveAll();
	FOR_EACH_VEC( s_vecItems, i )
	{
		if ( OfflineItem_CanBeNamed( s_vecItems[i]->GetItemID() ) )
			vecItems.AddToTail( s_vecItems[i]->GetItemID() );
	}
}

// what the viewmodel's name plate can show: printable ASCII (its font has
// nothing else), up to 20 characters, not only spaces; no quotes or
// backslashes (the file keeps them as KeyValues strings)
bool OfflineNameTag_IsValidName( const char *pszName )
{
	if ( !pszName )
		return false;
	int nLen = V_strlen( pszName );
	if ( nLen < 1 || nLen > 20 )		// NUM_UID_CHARS
		return false;
	bool bVisible = false;
	for ( const char *p = pszName; *p; p++ )
	{
		unsigned char c = (unsigned char)*p;
		if ( c < 32 || c > 126 || c == '"' || c == '\\' )
			return false;
		bVisible |= ( c != ' ' );
	}
	return bVisible;
}

const char *OfflineItem_GetCustomName( uint64 ullItemID )
{
	unsigned short i = s_mapItems.Find( ullItemID );
	return s_mapItems.IsValidIndex( i ) ? s_mapItems[i]->GetCustomName() : NULL;
}

static void SetCustomName( uint64 ullItemID, const char *pszName )
{
	unsigned short i = s_mapItems.Find( ullItemID );
	if ( !s_mapItems.IsValidIndex( i ) )
		return;
	CEconItem *pItem = s_mapItems[i];
	pItem->SetCustomName( ( pszName && pszName[0] ) ? pszName : NULL );
	pItem->SetSOUpdateFrame( gpGlobals->framecount + 1 );	// views re-read it

	KeyValues *pAll = Customizations();
	CFmtStr strID( "%llu", ullItemID );
	KeyValues *pKV = pAll->FindKey( strID, true );
	if ( pszName && pszName[0] )
		pKV->SetString( "name", pszName );
	else
	{
		if ( KeyValues *pName = pKV->FindKey( "name" ) )
		{
			pKV->RemoveSubKey( pName );
			pName->deleteThis();
		}
		if ( !pKV->GetFirstSubKey() )
		{
			pAll->RemoveSubKey( pKV );
			pKV->deleteThis();
		}
	}
	SaveCustomizations();
}

bool OfflineNameTag_Apply( uint64 ullTag, uint64 ullItemID, const char *pszName )
{
	if ( !OfflineItem_IsNameTag( ullTag ) || !IsOwned( ullTag ) || !OfflineItem_CanBeNamed( ullItemID ) || !OfflineNameTag_IsValidName( pszName ) )
		return false;
	SetCustomName( ullItemID, pszName );
	RemoveOwned( ullTag );		// used up
	Msg( "[name tag] %llu is now \"%s\"\n", ullItemID, pszName );
	return true;
}

void OfflineItem_ClearCustomName( uint64 ullItemID )
{
	if ( OfflineItem_GetCustomName( ullItemID ) )
	{
		SetCustomName( ullItemID, NULL );
		Msg( "[name tag] %llu name removed\n", ullItemID );
	}
}

//-----------------------------------------------------------------------------
// Deleting: what the Item Giver gave can be thrown away again
//-----------------------------------------------------------------------------
bool OfflineItem_IsDeletable( uint64 ullItemID )
{
	return OfflineInventory_IsGiverMode() && IsOwned( ullItemID );
}

void OfflineItem_Delete( uint64 ullItemID )
{
	if ( !OfflineItem_IsDeletable( ullItemID ) )
		return;
	if ( OfflineItem_GetCustomName( ullItemID ) )
		SetCustomName( ullItemID, NULL );
	RemoveOwned( ullItemID );
	Msg( "[offline inventory] deleted %llu\n", ullItemID );
}
#endif

//-----------------------------------------------------------------------------
// Loadout file
//-----------------------------------------------------------------------------
static CUtlMap< CCSPlayerInventory *, long > s_mapFilledFileTime( DefLessFunc( CCSPlayerInventory * ) );

static long LoadoutFileTime()
{
	return g_pFullFileSystem->FileExists( k_pszLoadoutFile, k_pszPathID ) ? g_pFullFileSystem->GetFileTime( k_pszLoadoutFile, k_pszPathID ) : 0;
}

bool OfflineInventory_NeedsRefill( CCSPlayerInventory *pInventory )
{
	unsigned short i = s_mapFilledFileTime.Find( pInventory );
	// an empty inventory too: a new player (map change) can get the address of the
	// last one, whose entry would say it is already filled
	return !s_mapFilledFileTime.IsValidIndex( i ) || pInventory->GetItemCount() == 0 || s_mapFilledFileTime[i] != LoadoutFileTime();
}

#ifndef CLIENT_DLL
// the server's own copies of the items equipped in each inventory
static CUtlMap< CCSPlayerInventory *, CUtlVector< CEconItem * > * > s_mapServerCopies( DefLessFunc( CCSPlayerInventory * ) );
#endif

void OfflineInventory_Fill( CCSPlayerInventory *pInventory, const CSteamID &owner, KeyValues *pLoadout )
{
	if ( !pInventory )
		return;

#ifdef CLIENT_DLL
	BuildLocal( owner.GetAccountID() );
#endif

	// Sets the owner and registers the inventory so lookups by account ID find it
	InventoryManager()->SteamRequestInventory( pInventory, owner );

	pInventory->SOClear();
	pInventory->ResetLoadoutItemIDs();

	KeyValues *pFileKV = NULL;
	KeyValues *pKV = pLoadout;
	if ( !pKV )
	{
		pFileKV = new KeyValues( "OfflineLoadout" );
		LoadIDFile( pFileKV, k_pszLoadoutFile );
		pKV = pFileKV;
	}
	KeyValues::AutoDelete autodelete( pFileKV );

	int nEquipped = 0;
#ifdef CLIENT_DLL
	// Equipped state lives on the items; start clean, then apply the file
	FOR_EACH_VEC( s_vecItems, i )
	{
		for ( int iTeam = 0; iTeam < LOADOUT_COUNT; iTeam++ )
			s_vecItems[i]->UpdateEquippedState( iTeam, INVALID_EQUIPPED_SLOT );
	}
	for ( int iTeam = 0; iTeam < LOADOUT_COUNT; iTeam++ )
	{
		for ( int iSlot = 0; iSlot < LOADOUT_POSITION_COUNT; iSlot++ )
		{
			uint64 ullID = MigrateID( ReadID( pKV, CFmtStr( "item_%d_%d", iTeam, iSlot ) ) );
			unsigned short iItem = s_mapItems.Find( ullID );
			if ( s_mapItems.IsValidIndex( iItem ) )
			{
				s_mapItems[iItem]->UpdateEquippedState( iTeam, iSlot );
				nEquipped++;
			}
		}
	}
	// ItemHasBeenUpdated() puts each equipped item into the loadout slots
	FOR_EACH_VEC( s_vecItems, i )
		pInventory->AddEconItem( s_vecItems[i], false, false, false );
#else
	// the server: copies of just the equipped items, this inventory's own (each
	// player equips their own; the IDs say what the items look like)
	unsigned short iCopies = s_mapServerCopies.Find( pInventory );
	if ( !s_mapServerCopies.IsValidIndex( iCopies ) )
		iCopies = s_mapServerCopies.Insert( pInventory, new CUtlVector< CEconItem * > );
	CUtlVector< CEconItem * > &vecCopies = *s_mapServerCopies[iCopies];
	vecCopies.PurgeAndDeleteElements();

	for ( int iTeam = 0; iTeam < LOADOUT_COUNT; iTeam++ )
	{
		for ( int iSlot = 0; iSlot < LOADOUT_POSITION_COUNT; iSlot++ )
		{
			uint64 ullID = MigrateID( ReadID( pKV, CFmtStr( "item_%d_%d", iTeam, iSlot ) ) );
			if ( !ullID )
				continue;
			CEconItem *pItem = NULL;
			FOR_EACH_VEC( vecCopies, i )
			{
				if ( vecCopies[i]->GetItemID() == ullID )
					pItem = vecCopies[i];
			}
			if ( !pItem )
			{
				pItem = CreateItem( ullID, owner.GetAccountID() );
				if ( !pItem )
					continue;
				vecCopies.AddToTail( pItem );
			}
			pItem->UpdateEquippedState( iTeam, iSlot );
			nEquipped++;
		}
	}
	FOR_EACH_VEC( vecCopies, i )
		pInventory->AddEconItem( vecCopies[i], false, false, false );
#endif

	for ( int iTeam = 0; iTeam < LOADOUT_COUNT; iTeam++ )
	{
		for ( int iSlot = 0; iSlot < LOADOUT_POSITION_COUNT; iSlot++ )
		{
			int nDef = pKV->GetInt( CFmtStr( "def_%d_%d", iTeam, iSlot ), 0 );
			if ( nDef )
				pInventory->SetDefaultEquippedDefinitionItemBySlot( iTeam, iSlot, nDef );
		}
	}

	if ( !pLoadout )
		s_mapFilledFileTime.InsertOrReplace( pInventory, LoadoutFileTime() );

	VERBOSE_PRINTF( "[offline] " OFFLINE_SIDE " fill: owner %llu, %d items, %d equipped from %s (exists %d, time %ld)\n",
		owner.ConvertToUint64(), pInventory->GetItemCount(), nEquipped, pLoadout ? "their loadout" : k_pszLoadoutFile,
		(int)g_pFullFileSystem->FileExists( k_pszLoadoutFile, k_pszPathID ), LoadoutFileTime() );
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
			if ( ullID != LOADOUT_SLOT_USE_BASE_ITEM && OfflineID_IsOffline( ullID ) && OfflineInventory_FindItem( ullID ) )
			{
				pKV->SetString( CFmtStr( "item_%d_%d", iTeam, iSlot ), CFmtStr( "%llu", ullID ) );
				nSaved++;
			}

			CEconItemView *pDefault = pInventory->FindDefaultEquippedDefinitionItemBySlot( iTeam, iSlot );
			if ( pDefault && pDefault->IsValid() )
				pKV->SetInt( CFmtStr( "def_%d_%d", iTeam, iSlot ), pDefault->GetItemDefinition()->GetDefinitionIndex() );
		}
	}

	g_pFullFileSystem->CreateDirHierarchy( "cfg", k_pszPathID );
	bool bSaved = pKV->SaveToFile( g_pFullFileSystem, k_pszLoadoutFile, k_pszPathID );
	char szFullPath[MAX_PATH] = "";
	g_pFullFileSystem->RelativePathToFullPath( k_pszLoadoutFile, k_pszPathID, szFullPath, sizeof( szFullPath ) );
	VERBOSE_PRINTF( "[offline] " OFFLINE_SIDE " saved loadout: %d items, write %s, path '%s'\n", nSaved, bSaved ? "ok" : "FAILED", szFullPath );
	fflush( stdout );

	// this inventory already matches the file it just wrote
	s_mapFilledFileTime.InsertOrReplace( pInventory, LoadoutFileTime() );

#ifdef CLIENT_DLL
	OfflineInventory_SendLoadoutToServer();
#endif
}

#ifdef CLIENT_DLL
void OfflineInventory_SendLoadoutToServer()
{
	if ( !OfflineInventory_IsEnabled() || !engine->IsConnected() )
		return;

	KeyValues *pKV = new KeyValues( "OfflineLoadout" );
	KeyValues::AutoDelete autodelete( pKV );
	LoadIDFile( pKV, k_pszLoadoutFile );

	// begin / set <key> <value> ... (several per command, well under the command
	// length limit) / end: the server swaps in the whole loadout at "end"
	engine->ServerCmd( "ios_offline_loadout begin\n", true );
	CUtlString strCmd;
	int nSent = 0;
	for ( KeyValues *pSub = pKV->GetFirstValue(); pSub; pSub = pSub->GetNextValue() )
	{
		const char *pszKey = pSub->GetName();
		// item IDs are 64-bit: GetString() gives nothing for a TYPE_UINT64 value
		char szValue[64];
		switch ( pSub->GetDataType() )
		{
		case KeyValues::TYPE_UINT64:	V_snprintf( szValue, sizeof( szValue ), "%llu", MigrateID( pSub->GetUint64() ) ); break;
		case KeyValues::TYPE_INT:		V_snprintf( szValue, sizeof( szValue ), "%d", pSub->GetInt() ); break;
		case KeyValues::TYPE_STRING:
			if ( StringHasPrefix( pszKey, "item_" ) )
				V_snprintf( szValue, sizeof( szValue ), "%llu", MigrateID( V_atoui64( pSub->GetString() ) ) );
			else
				V_strncpy( szValue, pSub->GetString(), sizeof( szValue ) );
			break;
		default:						szValue[0] = 0; break;
		}
		const char *pszValue = szValue;
		if ( !pszKey[0] || !pszValue[0] || V_strlen( pszKey ) > 32 || V_strlen( pszValue ) > 32 )
			continue;
		if ( strCmd.IsEmpty() )
			strCmd = "ios_offline_loadout set";
		strCmd += CFmtStr( " %s %s", pszKey, pszValue ).Access();
		if ( strCmd.Length() > 180 )
		{
			engine->ServerCmd( CFmtStr( "%s\n", strCmd.Get() ), true );
			strCmd.Clear();
		}
		++nSent;
	}
	if ( !strCmd.IsEmpty() )
		engine->ServerCmd( CFmtStr( "%s\n", strCmd.Get() ), true );
	engine->ServerCmd( "ios_offline_loadout end\n", true );

	Msg( "[offline] sent loadout to the server: %d entries\n", nSent );
}

//-----------------------------------------------------------------------------
// Console access:
//   offline_list [filter]                 lists the items
//   offline_equip <item id>               equips an item for every team that can use it
//-----------------------------------------------------------------------------
CON_COMMAND_F( offline_list, "List the offline inventory's items. Optional name filter.", FCVAR_RELEASE )
{
	const char *pszFilter = args.ArgC() > 1 ? args[1] : NULL;
	FOR_EACH_VEC( s_vecItems, i )
	{
		uint64 ullID = s_vecItems[i]->GetItemID();
		const CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinition( s_vecItems[i]->GetDefinitionIndex() );
		const CPaintKit *pPaintKit = OfflineID_Kind( ullID ) == OFFLINE_KIND_SKIN ? GetItemSchema()->GetPaintKitDefinition( OfflineID_Paint( ullID ) ) : NULL;
		const char *pszDef = pDef ? pDef->GetDefinitionName() : "?";
		const char *pszPaint = pPaintKit ? pPaintKit->sName.String() : "-";
		if ( pszFilter && !V_stristr( pszDef, pszFilter ) && !V_stristr( pszPaint, pszFilter ) )
			continue;
		Msg( "%llu  %s  %s  pattern %d  wear %.4f%s\n", ullID, pszDef, pszPaint, OfflineID_Seed( ullID ), OfflineID_Wear( ullID ), OfflineID_IsStatTrak( ullID ) ? "  StatTrak" : "" );
	}
}

CON_COMMAND_F( offline_equip, "Equip an offline item by id (see offline_list) for every team that can use it", FCVAR_RELEASE )
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
	const CCStrike15ItemDefinition *pDef = CSDef( pItem->GetDefinitionIndex() );
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

//-----------------------------------------------------------------------------
// StatTrak counting: a kill by the local player on an enemy, with a StatTrak
// item (player_death's weapon_itemid), adds one and saves the file
//-----------------------------------------------------------------------------
class COfflineStatTrak : public CAutoGameSystem, public CGameEventListener
{
public:
	COfflineStatTrak() : CAutoGameSystem( "COfflineStatTrak" ) {}

	// the event list isn't loaded at client init: listen once a map is up
	virtual void LevelInitPostEntity()
	{
		if ( OfflineInventory_IsEnabled() )
			ListenForGameEvent( "player_death" );
	}

	virtual void FireGameEvent( IGameEvent *event )
	{
		C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
		if ( !pLocal || event->GetInt( "attacker" ) != pLocal->GetUserID() || event->GetInt( "userid" ) == pLocal->GetUserID() )
			return;

		// enemies only (team kills don't count)
		int nVictim = engine->GetPlayerForUserID( event->GetInt( "userid" ) );
		if ( g_PR && nVictim > 0 && g_PR->GetTeam( nVictim ) == g_PR->GetTeam( pLocal->entindex() ) )
			return;

		uint64 ullItemID = V_atoui64( event->GetString( "weapon_itemid" ) );
		unsigned short i = s_mapItems.Find( ullItemID );		// ours only
		CEconItem *pItem = s_mapItems.IsValidIndex( i ) ? s_mapItems[i] : NULL;
		static CSchemaAttributeDefHandle pAttr_KillEater( "kill eater" );
		if ( !pItem || !OfflineID_IsStatTrak( ullItemID ) || !pAttr_KillEater )
			return;

		uint32 unKills = 0;
		pItem->FindAttribute( pAttr_KillEater, &unKills );
		unKills++;
		pItem->SetDynamicAttributeValue( pAttr_KillEater, unKills );
		pItem->SetSOUpdateFrame( gpGlobals->framecount + 1 );	// views re-read it (their value is cached per update)

		KeyValues *pCounts = StatTrakCounts();
		pCounts->SetInt( CFmtStr( "%llu", ullItemID ), (int)unKills );
		pCounts->SaveToFile( g_pFullFileSystem, k_pszStatTrakFile, k_pszPathID );
		VERBOSE_PRINTF( "[stattrak] %s: %u kills\n", event->GetString( "weapon" ), unKills );
	}
};
static COfflineStatTrak s_OfflineStatTrak;

//-----------------------------------------------------------------------------
// Cases: a case's contents come from its loot list (items_game client_loot_lists,
// found through its "set supply crate series" and revolving_loot_lists): one
// nested list per rarity, plus an "unusual" entry for the star items. Opening
// rolls CS:GO's odds: Mil-Spec 79.92%, Restricted 15.98%, Classified 3.2%,
// Covert 0.64%, star 0.26%; the star item is a random knife (gloves for the
// glove cases); one in ten comes out StatTrak. Contents are listed as the
// catalog's items (cleanest wear); a case opened in item giver mode gives a
// rolled item, like Receive, and is used up.
//-----------------------------------------------------------------------------
struct OfflineCase_t
{
	int m_nDef;
	CUtlVector< uint64 > m_vecItems;
	bool m_bRareSpecial;
};
static CUtlVector< OfflineCase_t * > s_vecCases;

static const OfflineCase_t *FindCase( uint64 ullID )
{
	if ( !OfflineID_IsOffline( ullID ) || OfflineID_Kind( ullID ) != OFFLINE_KIND_TOOL )
		return NULL;
	int nDef = OfflineID_Def( ullID );
	FOR_EACH_VEC( s_vecCases, i )
	{
		if ( s_vecCases[i]->m_nDef == nDef )
			return s_vecCases[i];
	}
	return NULL;
}

static bool InCatalog( uint64 ullID )
{
	return s_mapCatalog.Find( ullID ) != s_mapCatalog.InvalidIndex();
}

static void CollectLoot( const CEconLootListDefinition *pList, OfflineCase_t *pCase, int nDepth )
{
	if ( !pList || nDepth > 4 )
		return;
	const CUtlVector< item_list_entry_t > &vecEntries = pList->GetLootListContents();
	FOR_EACH_VEC( vecEntries, i )
	{
		const item_list_entry_t &entry = vecEntries[i];
		if ( entry.m_bIsUnusualList )
		{
			pCase->m_bRareSpecial = true;
			continue;
		}
		if ( entry.m_bIsNestedList )
		{
			CollectLoot( GetItemSchema()->GetLootListByIndex( entry.m_nItemDef ), pCase, nDepth + 1 );
			continue;
		}
		if ( entry.m_nItemDef > 0 && entry.m_nPaintKit > 0 )
		{
			uint64 ullItem = DefaultSkinID( entry.m_nItemDef, entry.m_nPaintKit, false );
			if ( InCatalog( ullItem ) && pCase->m_vecItems.Find( ullItem ) == pCase->m_vecItems.InvalidIndex() )
				pCase->m_vecItems.AddToTail( ullItem );
		}
	}
}

static int ItemRarity( uint64 ullID )
{
	CEconItem *pItem = OfflineInventory_FindItem( ullID );
	return pItem ? pItem->GetRarity() : 0;
}

void OfflineCase_Build()
{
	static CSchemaAttributeDefHandle pAttr_Series( "set supply crate series" );
	if ( !pAttr_Series )
		return;

	const CEconItemSchema::ItemDefinitionMap_t &mapDefs = GetItemSchema()->GetItemDefinitionMap();
	FOR_EACH_MAP_FAST( mapDefs, i )
	{
		// item_class supply_crate (GetEconTool() is always NULL here: the schema's
		// tool factory, CreateEconToolImpl, is a stub in this source)
		const CCStrike15ItemDefinition *pDef = dynamic_cast< const CCStrike15ItemDefinition * >( mapDefs[i] );
		if ( !pDef || !pDef->IsSupplyCrate() )
			continue;
		attrib_value_t unSeries = 0;
		if ( !FindAttribute_UnsafeBitwiseCast< attrib_value_t >( pDef, pAttr_Series, &unSeries ) )
			continue;
		const CEconItemSchema::RevolvingLootListDefinitionMap_t &mapSeries = GetItemSchema()->GetRevolvingLootLists();
		int iSeries = mapSeries.Find( (int)unSeries );
		if ( !mapSeries.IsValidIndex( iSeries ) )
			continue;

		OfflineCase_t *pCase = new OfflineCase_t;
		pCase->m_nDef = pDef->GetDefinitionIndex();
		pCase->m_bRareSpecial = false;
		CollectLoot( GetItemSchema()->GetLootListByName( mapSeries[iSeries] ), pCase, 0 );
		if ( !pCase->m_vecItems.Count() )
		{
			delete pCase;		// souvenir packages, sticker capsules: nothing we have
			continue;
		}
		// best first, like CS:GO's case contents
		for ( int a = 1; a < pCase->m_vecItems.Count(); a++ )
		{
			for ( int b = a; b > 0 && ItemRarity( pCase->m_vecItems[b] ) > ItemRarity( pCase->m_vecItems[b - 1] ); b-- )
			{
				uint64 t = pCase->m_vecItems[b]; pCase->m_vecItems[b] = pCase->m_vecItems[b - 1]; pCase->m_vecItems[b - 1] = t;
			}
		}
		s_vecCases.AddToTail( pCase );
		AddCatalog( OfflineID_MakeOther( OFFLINE_KIND_TOOL, pCase->m_nDef ) );
	}
	Msg( "[offline inventory] %d cases\n", s_vecCases.Count() );
}

bool OfflineCase_IsCase( uint64 ullItemID )
{
	return FindCase( ullItemID ) != NULL;
}

void OfflineCase_GetContents( uint64 ullCaseID, CUtlVector< uint64 > &vecItems, bool &bRareSpecial )
{
	vecItems.RemoveAll();
	bRareSpecial = false;
	if ( const OfflineCase_t *pCase = FindCase( ullCaseID ) )
	{
		vecItems.AddVectorToTail( pCase->m_vecItems );
		bRareSpecial = pCase->m_bRareSpecial;
	}
}

// the drop, as a catalog item: one in ten comes out StatTrak, as in CS:GO
static uint64 MaybeStatTrak( uint64 ullID )
{
	uint64 ullStatTrak = DefaultSkinID( OfflineID_Def( ullID ), OfflineID_Paint( ullID ), true );
	return ( RandomInt( 0, 9 ) == 0 && OfflineInventory_FindItem( ullStatTrak ) ) ? ullStatTrak : ullID;
}

static uint64 RollCaseDrop( const OfflineCase_t *pCase, uint64 ullCaseID )
{
	// the tier, by CS:GO's published odds (percent)
	float flRoll = RandomFloat( 0.0f, 100.0f );
	int nRarity;
	if ( pCase->m_bRareSpecial && flRoll < 0.26f )
		nRarity = 99;
	else if ( flRoll < 0.26f + 0.64f )
		nRarity = 6;
	else if ( flRoll < 0.26f + 0.64f + 3.2f )
		nRarity = 5;
	else if ( flRoll < 0.26f + 0.64f + 3.2f + 15.98f )
		nRarity = 4;
	else
		nRarity = 3;

	if ( nRarity == 99 )
	{
		// a star item: gloves from the glove cases, knives from the rest
		const CEconItemDefinition *pCaseDef = GetItemSchema()->GetItemDefinition( pCase->m_nDef );
		bool bGloves = pCaseDef && V_stristr( pCaseDef->GetDefinitionName(), "glove" );
		// the skins to pick from: the catalog (item giver) or the inventory
		CUtlVector< uint64 > vecAll, vecStar;
		if ( OfflineInventory_IsGiverMode() )
			vecAll.AddVectorToTail( s_vecCatalog );
		else
		{
			FOR_EACH_VEC( s_vecItems, i )
				vecAll.AddToTail( s_vecItems[i]->GetItemID() );
		}
		FOR_EACH_VEC( vecAll, i )
		{
			const CCStrike15ItemDefinition *pDef = CSDef( OfflineID_Def( vecAll[i] ) );
			if ( OfflineID_Kind( vecAll[i] ) == OFFLINE_KIND_SKIN && !OfflineID_IsStatTrak( vecAll[i] ) && pDef &&
				 pDef->GetDefaultLoadoutSlot() == ( bGloves ? LOADOUT_POSITION_CLOTHING_HANDS : LOADOUT_POSITION_MELEE ) )
				vecStar.AddToTail( vecAll[i] );
		}
		if ( vecStar.Count() )
			return MaybeStatTrak( vecStar[ RandomInt( 0, vecStar.Count() - 1 ) ] );
		nRarity = 6;
	}

	// that tier's items (or the nearest tier below that the case has)
	for ( ; nRarity >= 0; nRarity-- )
	{
		CUtlVector< uint64 > vecTier;
		FOR_EACH_VEC( pCase->m_vecItems, i )
		{
			if ( ItemRarity( pCase->m_vecItems[i] ) == nRarity )
				vecTier.AddToTail( pCase->m_vecItems[i] );
		}
		if ( vecTier.Count() )
			return MaybeStatTrak( vecTier[ RandomInt( 0, vecTier.Count() - 1 ) ] );
	}
	return MaybeStatTrak( pCase->m_vecItems[ RandomInt( 0, pCase->m_vecItems.Count() - 1 ) ] );
}

uint64 OfflineCase_Open( uint64 ullCaseID )
{
	const OfflineCase_t *pCase = FindCase( ullCaseID );
	if ( !pCase )
		return 0;
	uint64 ullDrop = RollCaseDrop( pCase, ullCaseID );
	if ( !OfflineInventory_IsGiverMode() )
		return ullDrop;		// -allskinsunlocked: already in the inventory

	// item giver mode: the drop is received (rolled look), the case used up
	uint64 ullID = OfflineGiver_Receive( ullDrop );
	if ( ullID )
		RemoveOwned( ullCaseID );
	return ullID;
}
#endif
