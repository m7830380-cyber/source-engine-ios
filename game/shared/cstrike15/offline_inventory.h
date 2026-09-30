//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Offline inventory (no game coordinator on this build). Two modes:
//
//   -allskinsunlocked     every skin, knife and glove (a normal and a StatTrak
//                         version of each), the weapon cases (open freely)
//   -unlockitemgivermenu  an empty inventory; items come from the Item Giver
//                         tab (Receive), each one rolled like a case drop, and
//                         are kept in cfg/offline_items.txt
//
//          Name tags rename weapons and knives (cfg/offline_custom.txt).
//
//          Item IDs carry what an item looks like (weapon, paint kit, pattern,
//          wear, StatTrak), so any game that sees one - the LAN host, other
//          players - can rebuild the item from the ID alone. The equipped
//          loadout is kept in cfg/offline_loadout.txt.
//
//===========================================================================//
#ifndef OFFLINE_INVENTORY_H
#define OFFLINE_INVENTORY_H
#ifdef _WIN32
#pragma once
#endif

class CCSPlayerInventory;
class CPlayerInventory;
class CEconItem;
class CEconItemView;
class CSteamID;
class KeyValues;

bool OfflineInventory_IsEnabled();		// either mode
bool OfflineInventory_IsGiverMode();	// -unlockitemgivermenu

//-----------------------------------------------------------------------------
// Item IDs stay below 2^53: Flash (the menus) keeps numbers as doubles, exact
// only up to there. Bit 52 = offline item, 50-51 = kind, then
//   skin:  def 15 | paint kit 14 | pattern 10 | wear 10 | StatTrak 1
//          (wear on a squared scale: fine steps near Factory New)
//   other: value 16 (case / name tag def, sticker kit, music kit) | serial 34
//-----------------------------------------------------------------------------
enum EOfflineKind
{
	OFFLINE_KIND_SKIN = 0,
	OFFLINE_KIND_STICKER = 1,
	OFFLINE_KIND_MUSIC = 2,
	OFFLINE_KIND_TOOL = 3,		// cases, name tags (the value is the item definition)
};

inline bool OfflineID_IsOffline( uint64 ullID )		{ return ( ullID >> 52 ) == 1; }
inline int OfflineID_Kind( uint64 ullID )			{ return int( ( ullID >> 50 ) & 3 ); }
inline int OfflineID_Def( uint64 ullID )			// skin def, or the value of the other kinds
{
	return OfflineID_Kind( ullID ) == OFFLINE_KIND_SKIN ? int( ( ullID >> 35 ) & 0x7FFF ) : int( ( ullID >> 34 ) & 0xFFFF );
}
inline int OfflineID_Paint( uint64 ullID )			{ return int( ( ullID >> 21 ) & 0x3FFF ); }
inline int OfflineID_Seed( uint64 ullID )			{ return int( ( ullID >> 11 ) & 0x3FF ); }
inline float OfflineID_Wear( uint64 ullID )			{ float q = float( ( ullID >> 1 ) & 0x3FF ) / 1023.0f; return q * q; }
inline bool OfflineID_IsStatTrak( uint64 ullID )	{ return OfflineID_Kind( ullID ) == OFFLINE_KIND_SKIN && ( ullID & 1 ); }

inline uint64 OfflineID_MakeSkin( int nDef, int nPaint, int nSeed, float flWear, bool bStatTrak )
{
	float q = sqrtf( MIN( MAX( flWear, 0.0f ), 1.0f ) );
	uint64 ullWear = uint64( q * 1023.0f + 0.5f );
	return ( 1ull << 52 ) | ( uint64( OFFLINE_KIND_SKIN ) << 50 ) | ( uint64( nDef & 0x7FFF ) << 35 ) | ( uint64( nPaint & 0x3FFF ) << 21 ) |
		( uint64( nSeed & 0x3FF ) << 11 ) | ( ullWear << 1 ) | uint64( bStatTrak ? 1 : 0 );
}
inline uint64 OfflineID_MakeOther( int nKind, int nValue, uint64 ullSerial = 0 )
{
	return ( 1ull << 52 ) | ( uint64( nKind & 3 ) << 50 ) | ( uint64( nValue & 0xFFFF ) << 34 ) | ( ullSerial & ( ( 1ull << 34 ) - 1 ) );
}

// (Re)fills an inventory and applies a loadout: pLoadout (a LAN player's, sent
// by their client) or else the saved cfg/offline_loadout.txt. The client's own
// inventory gets its items (all of them, or the received ones); on the server an
// inventory gets its own copies of the equipped items only.
void OfflineInventory_Fill( CCSPlayerInventory *pInventory, const CSteamID &owner, KeyValues *pLoadout = NULL );

// True if cfg/offline_loadout.txt changed since this inventory was last filled.
bool OfflineInventory_NeedsRefill( CCSPlayerInventory *pInventory );

// Writes the inventory's current loadout to cfg/offline_loadout.txt.
void OfflineInventory_SaveLoadout( CCSPlayerInventory *pInventory );

// The item with this ID: one of ours, or one rebuilt from the ID (another
// player's, a LAN joiner's). NULL for IDs that aren't offline items.
CEconItem *OfflineInventory_FindItem( uint64 ullItemID );

// The same, but an inventory's own item first (the server keeps a copy of each
// player's equipped items per inventory).
CEconItem *OfflineInventory_FindItemIn( const CPlayerInventory *pInventory, uint64 ullItemID );

#ifdef CLIENT_DLL
// Sends the saved loadout to the server we're connected to, so on LAN games the
// host gives us our own skins (see CCSPlayer::ClientCommand "ios_offline_loadout").
void OfflineInventory_SendLoadoutToServer();

// A view of an item that isn't in the local inventory: the Item Giver's catalog,
// or an item rebuilt from its ID (in inventories of their own).
CEconItemView *OfflineInventory_FindView( uint64 ullItemID );

// Item Giver (-unlockitemgivermenu): what it offers, and receiving one (rolled
// like a case drop: pattern and wear; the StatTrak version is its own entry).
// Returns the new item's ID, 0 if nothing was received.
const CUtlVector< uint64 > &OfflineGiver_GetCatalog();
uint64 OfflineGiver_Receive( uint64 ullCatalogID );

// Cases: contents (best first; bRareSpecial: the "exceedingly rare" star slot)
// and opening one with CS:GO's odds. With -allskinsunlocked they open as often
// as you like; received cases are used up.
bool OfflineCase_IsCase( uint64 ullItemID );
void OfflineCase_GetContents( uint64 ullCaseID, CUtlVector< uint64 > &vecItems, bool &bRareSpecial );
uint64 OfflineCase_Open( uint64 ullCaseID );

// Name tags: used up when applied (-allskinsunlocked gives a fresh set each
// launch). Names are kept in cfg/offline_custom.txt by item ID.
bool OfflineItem_CanBeNamed( uint64 ullItemID );
void OfflineNameTag_GetOwned( CUtlVector< uint64 > &vecTags );
void OfflineItem_GetNameable( CUtlVector< uint64 > &vecItems );
bool OfflineNameTag_IsValidName( const char *pszName );
bool OfflineNameTag_Apply( uint64 ullTag, uint64 ullItemID, const char *pszName );
const char *OfflineItem_GetCustomName( uint64 ullItemID );
void OfflineItem_ClearCustomName( uint64 ullItemID );

// Deleting received items (item giver mode)
bool OfflineItem_IsDeletable( uint64 ullItemID );
void OfflineItem_Delete( uint64 ullItemID );
#endif

bool OfflineItem_IsNameTag( uint64 ullItemID );

#endif // OFFLINE_INVENTORY_H
