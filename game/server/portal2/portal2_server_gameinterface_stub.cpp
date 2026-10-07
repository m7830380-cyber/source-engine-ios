#include "cbase.h"
#include "gameinterface.h"
#include "tier1/KeyValues.h"

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

void CServerGameDLL::ApplyGameSettings( KeyValues * )
{
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

#endif
