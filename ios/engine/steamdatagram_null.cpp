//===========================================================================//
//
// Purpose: Steam Datagram Relay client for builds without steamdatagramlib.
//
// The relay library is a prebuilt Valve binary that is not part of the source
// tree. The engine only uses it for Valve's relay network, so on iOS every
// entry point reports it as unavailable and the engine uses plain sockets.
//
//===========================================================================//

#include "tier1/strtools.h"
#include "steam/steamclientpublic.h"
#include "steamdatagram/isteamdatagramclient.h"
#include "steamdatagram/isteamdatagramserver.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void SteamDatagramClient_Init( const char *pszCacheDirectory, ESteamDatagramPartner ePartner, int iPartnerMask )
{
}

ISteamNetworkingUtils *SteamNetworkingUtils()
{
	return NULL;
}

ISteamDatagramTransportClient *SteamDatagramClient_Connect( CSteamID steamID )
{
	return NULL;
}

void SteamDatagramClient_Kill()
{
}

ISteamDatagramTransportGameserver *SteamDatagram_GameserverListen( EUniverse eUniverse, uint16 unBindPort, EResult *pOutResult, SteamDatagramErrMsg &errMsg )
{
	if ( pOutResult )
	{
		*pOutResult = k_EResultFail;
	}
	V_strncpy( errMsg, "Steam Datagram Relay is not available on this platform", sizeof( errMsg ) );
	return NULL;
}

int ISteamDatagramTransportClient::ConnectionStatus::Print( char *pszBuf, int cbBuf ) const
{
	return V_snprintf( pszBuf, cbBuf, "Steam Datagram Relay not available\n" );
}
