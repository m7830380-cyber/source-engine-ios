//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Offline inventory for the -allskinsunlocked launch option.
//
//          There is no game coordinator on this build, so inventories are
//          always empty. With -allskinsunlocked, every valid weapon/knife/glove
//          and paint kit combination from the item schema is added as a local
//          CEconItem to the local player's inventory (client) and to human
//          players' inventories (server), and the equipped loadout is kept in
//          cfg/offline_loadout.txt so both sides agree and it survives restarts.
//
//===========================================================================//
#ifndef OFFLINE_INVENTORY_H
#define OFFLINE_INVENTORY_H
#ifdef _WIN32
#pragma once
#endif

class CCSPlayerInventory;
class CEconItem;
class CSteamID;
class KeyValues;

bool OfflineInventory_IsEnabled();

// (Re)fills an inventory with every unlocked item and applies a loadout: pLoadout
// (a LAN player's, sent by their client) or else the saved cfg/offline_loadout.txt.
void OfflineInventory_Fill( CCSPlayerInventory *pInventory, const CSteamID &owner, KeyValues *pLoadout = NULL );

// True if cfg/offline_loadout.txt changed since this inventory was last filled.
bool OfflineInventory_NeedsRefill( CCSPlayerInventory *pInventory );

// Writes the inventory's current loadout to cfg/offline_loadout.txt.
void OfflineInventory_SaveLoadout( CCSPlayerInventory *pInventory );

// Item lookup for inventories without a shared object cache.
CEconItem *OfflineInventory_FindItem( uint64 ullItemID );

#ifdef CLIENT_DLL
// Sends the saved loadout to the server we're connected to, so on LAN games the
// host gives us our own skins (see CCSPlayer::ClientCommand "ios_offline_loadout").
void OfflineInventory_SendLoadoutToServer();
#endif

#endif // OFFLINE_INVENTORY_H
