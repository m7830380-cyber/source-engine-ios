#include "cbase.h"
#include "gameinterface.h"
#include "tier1/KeyValues.h"
#include "steam/steamtypes.h"
#include "tier0/icommandline.h"

#if defined( PORTAL2 ) && defined( GAME_DLL )

void CServerGameDLL::UpdateGCInformation()
{
}

void CServerGameDLL::ReportGCQueuedMatchStart( int32, uint32 *, int )
{
}

void CServerGameDLL::GetMatchmakingGameData( char *buf, size_t bufSize )
{
	if ( buf && bufSize > 0 )
	{
		buf[0] = '\0';
	}
}

bool CServerGameDLL::ValidateAndAddActiveCaster( const CSteamID & )
{
	return false;
}

EncryptedMessageKeyType_t CServerGameDLL::GetMessageEncryptionKey( INetMessage * )
{
	return kEncryptedMessageKeyType_None;
}

void CServerGameDLL::OnEngineClientNetworkEvent( edict_t *, uint64, int, void * )
{
}

void CServerGameDLL::EngineGotvSyncPacket( const CEngineGotvSyncPacket * )
{
}

bool CServerGameDLL::OnEngineClientProxiedRedirect( uint64, const char *, const char * )
{
	return false;
}

bool CServerGameDLL::LogForHTTPListeners( const char * )
{
	return false;
}

extern ConVar sv_portal_players;
extern ConVar cm_is_current_community_map_coop;

// The engine asks before every map start ("::ExecGameTypeCfg" with the map);
// like Portal 2 (P2ASW's base_gameinterface.cpp), co-op maps get 2 player
// slots and everything else 1, and the engine sets maxclients from that, which
// picks CPortalMPGameRules or CPortalGameRules (InstallGameRules).
void CServerGameDLL::ApplyGameSettings( KeyValues *pKV )
{
	if ( !pKV || V_stricmp( pKV->GetName(), "::ExecGameTypeCfg" ) )
		return;

	const char *pszMap = pKV->GetString( "map/mapname", "" );
	if ( !pszMap[0] )
		return;

	// (only a new game asks: a changelevel keeps the server and its slots)
	int numSlots = 1;
	if ( V_stristr( pszMap, "mp_coop_" ) || cm_is_current_community_map_coop.GetBool() )
		numSlots = CommandLine()->FindParm( "-allowspectators" ) ? 3 : 2;

	pKV->SetInt( "members/numSlots", numSlots );
	sv_portal_players.SetValue( numSlots );
	ConVarRef coop( "coop" );
	if ( coop.IsValid() )
		coop.SetValue( numSlots >= 2 ? 1 : 0 );
	Msg( "[coop] %s: %d player slot%s\n", pszMap, numSlots, numSlots > 1 ? "s" : "" );
}

bool CServerGameDLL::ShouldHoldGameServerReservation( float )
{
	return false;
}

void CServerGameDLL::OnPureServerFileValidationFailure( edict_t *, const char *, const char *, uint32, int32, int32, int, int )
{
}

char const *CServerGameDLL::ClientConnectionValidatePreNetChan( bool, char const *, int, uint64 )
{
	return NULL;
}

void CServerGameClients::NetworkIDValidated( const char *pszUserName, const char *pszNetworkID, CSteamID steamID )
{
	(void)pszUserName;
	(void)pszNetworkID;
	(void)steamID;
}

const char *CServerGameClients::ClientNameHandler( uint64 xuid, const char *pchName )
{
	(void)xuid;
	return pchName;
}

void CServerGameClients::ClientSvcUserMessage( edict_t *pEntity, int nType, int nPassthrough, uint32 cbSize, const void *pvBuffer )
{
	(void)pEntity;
	(void)nType;
	(void)nPassthrough;
	(void)cbSize;
	(void)pvBuffer;
}

#endif
