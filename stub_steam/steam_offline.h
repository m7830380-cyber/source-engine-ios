//===========================================================================//
//
// Purpose: The offline Steam client used by stub_steam (steam_offline.cpp).
//
//===========================================================================//

#ifndef STEAM_OFFLINE_H
#define STEAM_OFFLINE_H

class ISteamClient;

// the one pipe and user of the offline client
const HSteamPipe k_hOfflineSteamPipe = 1;
const HSteamUser k_hOfflineSteamUser = 1;

ISteamClient *OfflineSteamClient();

#endif // STEAM_OFFLINE_H
