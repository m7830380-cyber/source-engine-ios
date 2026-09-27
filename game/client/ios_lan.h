//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: LAN game discovery (iOS). Hosts are offline matches; their engine
// answers the 'g' "IOSLAN1" query (CBaseServer::ProcessConnectionlessPacket).
// Implemented in cdll_client_int.cpp next to the lan_find/lan_join commands.
//
//=============================================================================//

#ifndef IOS_LAN_H
#define IOS_LAN_H

#if defined( IOS )

struct LanGame_t
{
	char m_szAddress[64];	// ip:port
	char m_szName[64];		// the host's server name (its player name)
	char m_szMap[64];
	int m_nHumans, m_nBots, m_nMax;
	uint64 m_ullXuid;		// stable made-up ID for this address, for the friends list
	double m_flLastSeen;
};

// Browsing in the background (main menu): queries every few seconds, reads the
// replies without blocking, forgets games that stop answering. Returns true
// when the list changed.
bool LanBrowser_Frame();
int LanBrowser_Count();
const LanGame_t *LanBrowser_Get( int i );
const LanGame_t *LanBrowser_FindByXuid( uint64 ullXuid );
void LanBrowser_Join( const LanGame_t *pGame );

#endif // IOS

#endif // IOS_LAN_H
