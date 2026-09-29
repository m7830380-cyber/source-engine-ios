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

#ifdef CLIENT_DLL
// StatTrak: the unlocked skins and knives come as StatTrak versions, counting
// your kills (bots too: offline there's hardly anyone else) into
// cfg/offline_stattrak.txt, keyed by item ID. The server has no say in it: CS:GO
// kept the count on Valve's item server; here your own game counts from the
// kill event, which names the item that made the kill (weapon_itemid).
ConVar ios_stattrak( "ios_stattrak", "1", FCVAR_ARCHIVE | FCVAR_RELEASE, "Skins and knives are StatTrak versions counting your kills (after a restart)" );
static const char *k_pszStatTrakFile = "cfg/offline_stattrak.txt";
static KeyValues *s_pStatTrakCounts = NULL;

static KeyValues *StatTrakCounts()
{
	if ( !s_pStatTrakCounts )
	{
		s_pStatTrakCounts = new KeyValues( "OfflineStatTrak" );
		s_pStatTrakCounts->LoadFromFile( g_pFullFileSystem, k_pszStatTrakFile, k_pszLoadoutPathID );
	}
	return s_pStatTrakCounts;
}
#endif

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

#ifdef CLIENT_DLL
	// StatTrak versions of the skins and knives: the kill counter attributes
	// (both stored as integers) with the saved count
	if ( ( pPaintKit || bStar ) && nSlot != LOADOUT_POSITION_CLOTHING_HANDS && ios_stattrak.GetBool() )
	{
		static CSchemaAttributeDefHandle pAttr_KillEater( "kill eater" );
		static CSchemaAttributeDefHandle pAttr_KillEaterType( "kill eater score type" );
		if ( pAttr_KillEater && pAttr_KillEaterType )
		{
			pItem->SetQuality( AE_STRANGE );
			uint32 unKills = (uint32)StatTrakCounts()->GetInt( CFmtStr( "%llu", ullID ), 0 );
			pItem->SetDynamicAttributeValue( pAttr_KillEater, unKills );
			pItem->SetDynamicAttributeValue( pAttr_KillEaterType, (uint32)0 );	// kills
		}
	}
#endif

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

#ifdef CLIENT_DLL
	// the weapon cases whose contents are among the unlocked items
	extern void OfflineCase_Build( uint32 unAccountID );
	OfflineCase_Build( unAccountID );
#endif

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

void OfflineInventory_Fill( CCSPlayerInventory *pInventory, const CSteamID &owner, KeyValues *pLoadout )
{
	if ( !pInventory )
		return;

	BuildItems( owner.GetAccountID() );

	// Sets the owner and registers the inventory so lookups by account ID find it
	InventoryManager()->SteamRequestInventory( pInventory, owner );

	pInventory->SOClear();
	pInventory->ResetLoadoutItemIDs();

	KeyValues *pFileKV = NULL;
	KeyValues *pKV = pLoadout;
	if ( !pKV )
	{
		pFileKV = new KeyValues( "OfflineLoadout" );
		pFileKV->LoadFromFile( g_pFullFileSystem, k_pszLoadoutFile, k_pszLoadoutPathID );
		pKV = pFileKV;
	}
	KeyValues::AutoDelete autodelete( pFileKV );

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

	if ( !pLoadout )
		s_mapFilledFileTime.InsertOrReplace( pInventory, LoadoutFileTime() );

	VERBOSE_PRINTF( "[offline] " OFFLINE_SIDE " fill: owner %llu, %d items, %d equipped from %s (exists %d, time %ld)\n",
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
	pKV->LoadFromFile( g_pFullFileSystem, k_pszLoadoutFile, k_pszLoadoutPathID );

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
		case KeyValues::TYPE_UINT64:	V_snprintf( szValue, sizeof( szValue ), "%llu", pSub->GetUint64() ); break;
		case KeyValues::TYPE_INT:		V_snprintf( szValue, sizeof( szValue ), "%d", pSub->GetInt() ); break;
		case KeyValues::TYPE_STRING:	V_strncpy( szValue, pSub->GetString(), sizeof( szValue ) ); break;
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
#endif

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

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// StatTrak counting: a kill by the local player on an enemy, with a StatTrak
// item (player_death's weapon_itemid), adds one and saves the file
//-----------------------------------------------------------------------------
#include "GameEventListener.h"
#include "c_playerresource.h"

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
		CEconItem *pItem = ullItemID ? OfflineInventory_FindItem( ullItemID ) : NULL;
		static CSchemaAttributeDefHandle pAttr_KillEater( "kill eater" );
		if ( !pItem || pItem->GetQuality() != AE_STRANGE || !pAttr_KillEater )
			return;

		uint32 unKills = 0;
		pItem->FindAttribute( pAttr_KillEater, &unKills );
		unKills++;
		pItem->SetDynamicAttributeValue( pAttr_KillEater, unKills );
		pItem->SetSOUpdateFrame( gpGlobals->framecount + 1 );	// views re-read it (their value is cached per update)

		KeyValues *pCounts = StatTrakCounts();
		pCounts->SetInt( CFmtStr( "%llu", ullItemID ), (int)unKills );
		pCounts->SaveToFile( g_pFullFileSystem, k_pszStatTrakFile, k_pszLoadoutPathID );
		VERBOSE_PRINTF( "[stattrak] %s: %u kills\n", event->GetString( "weapon" ), unKills );
	}
};
static COfflineStatTrak s_OfflineStatTrak;
#endif

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Cases: a case's contents come from its loot list (items_game client_loot_lists,
// found through its "set supply crate series" and revolving_loot_lists): one
// nested list per rarity, plus an "unusual" entry for the star items. Opening
// rolls CS:GO's odds: Mil-Spec 79.92%, Restricted 15.98%, Classified 3.2%,
// Covert 0.64%, star 0.26%; the star item is a random knife (gloves for the
// glove cases) from the unlocked items.
//-----------------------------------------------------------------------------
struct OfflineCase_t
{
	uint64 m_ullID;
	CUtlVector< uint64 > m_vecItems;
	bool m_bRareSpecial;
};
static CUtlVector< OfflineCase_t * > s_vecCases;

static const OfflineCase_t *FindCase( uint64 ullID )
{
	FOR_EACH_VEC( s_vecCases, i )
	{
		if ( s_vecCases[i]->m_ullID == ullID )
			return s_vecCases[i];
	}
	return NULL;
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
			uint64 ullItem = MakeItemID( entry.m_nItemDef, entry.m_nPaintKit );
			if ( OfflineInventory_FindItem( ullItem ) && pCase->m_vecItems.Find( ullItem ) == pCase->m_vecItems.InvalidIndex() )
				pCase->m_vecItems.AddToTail( ullItem );
		}
	}
}

static int ItemRarity( uint64 ullID )
{
	CEconItem *pItem = OfflineInventory_FindItem( ullID );
	return pItem ? pItem->GetRarity() : 0;
}

void OfflineCase_Build( uint32 unAccountID )
{
	if ( s_vecCases.Count() )
		return;
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
		pCase->m_ullID = MakeItemID( pDef->GetDefinitionIndex(), 0 );
		pCase->m_bRareSpecial = false;
		CollectLoot( GetItemSchema()->GetLootListByName( mapSeries[iSeries] ), pCase, 0 );
		if ( !pCase->m_vecItems.Count() )
		{
			delete pCase;		// souvenir packages, sticker capsules: nothing we unlock
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

		// the case itself, in the inventory
		CEconItem *pItem = new CEconItem();
		pItem->SetItemID( pCase->m_ullID );
		pItem->SetAccountID( unAccountID );
		pItem->SetDefinitionIndex( pDef->GetDefinitionIndex() );
		pItem->SetItemLevel( 1 );
		pItem->SetQuality( AE_UNIQUE );
		pItem->SetRarity( pDef->GetRarity() );
		pItem->SetFlags( 0 );
		pItem->SetInventoryToken( ( s_vecItems.Count() + 1 ) & kBackendPositionMask_Position );
		s_vecItems.AddToTail( pItem );
		s_mapItems.Insert( pCase->m_ullID, pItem );
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

uint64 OfflineCase_Open( uint64 ullCaseID )
{
	const OfflineCase_t *pCase = FindCase( ullCaseID );
	if ( !pCase )
		return 0;

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
		const CEconItemDefinition *pCaseDef = GetItemSchema()->GetItemDefinition( int( ( ullCaseID >> 16 ) & 0xFFFF ) );
		bool bGloves = pCaseDef && V_stristr( pCaseDef->GetDefinitionName(), "glove" );
		CUtlVector< uint64 > vecStar;
		FOR_EACH_VEC( s_vecItems, i )
		{
			const CCStrike15ItemDefinition *pDef = dynamic_cast< const CCStrike15ItemDefinition * >( s_vecItems[i]->GetItemDefinition() );
			if ( pDef && pDef->GetDefaultLoadoutSlot() == ( bGloves ? LOADOUT_POSITION_CLOTHING_HANDS : LOADOUT_POSITION_MELEE ) )
				vecStar.AddToTail( s_vecItems[i]->GetItemID() );
		}
		if ( vecStar.Count() )
			return vecStar[ RandomInt( 0, vecStar.Count() - 1 ) ];
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
			return vecTier[ RandomInt( 0, vecTier.Count() - 1 ) ];
	}
	return pCase->m_vecItems[ RandomInt( 0, pCase->m_vecItems.Count() - 1 ) ];
}
#endif
