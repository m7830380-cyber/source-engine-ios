//===========================================================================//
//
// Purpose: libsteam_api for platforms without Steam.
//
// The client side is an offline Steam client (steam_offline.cpp): SteamAPI
// init succeeds and CSteamAPIContext gets working interfaces for a single
// logged-in user. The game server side reports Steam as unavailable, which
// makes the engine run its server in LAN mode. Callbacks are accepted but
// never fire, since nothing asynchronous ever completes.
//
//===========================================================================//

#define STEAM_API_EXPORTS

#include <string.h>

#include "steam/steam_api.h"
#include "steam/steam_gameserver.h"

#include "steam_offline.h"

//-----------------------------------------------------------------------------
// Client
//-----------------------------------------------------------------------------

S_API bool S_CALLTYPE SteamAPI_Init()
{
	return true;
}

S_API bool S_CALLTYPE SteamAPI_InitSafe()
{
	return true;
}

S_API bool S_CALLTYPE SteamInternal_Init()
{
	return true;
}

S_API void S_CALLTYPE SteamAPI_Shutdown()
{
}

S_API bool S_CALLTYPE SteamAPI_RestartAppIfNecessary( uint32 unOwnAppID )
{
	return false;
}

S_API bool S_CALLTYPE SteamAPI_IsSteamRunning()
{
	return true;
}

S_API void S_CALLTYPE SteamAPI_ReleaseCurrentThreadMemory()
{
}

S_API void S_CALLTYPE SteamAPI_WriteMiniDump( uint32 uStructuredExceptionCode, void *pvExceptionInfo, uint32 uBuildID )
{
}

S_API void S_CALLTYPE SteamAPI_SetMiniDumpComment( const char *pchMsg )
{
}

S_API void S_CALLTYPE SteamAPI_UseBreakpadCrashHandler( char const *pchVersion, char const *pchDate, char const *pchTime, bool bFullMemoryDumps, void *pvContext, PFNPreMinidumpCallback m_pfnPreMinidumpCallback )
{
}

S_API void S_CALLTYPE SteamAPI_SetBreakpadAppID( uint32 unAppID )
{
}

S_API void S_CALLTYPE SteamAPI_RunCallbacks()
{
}

S_API void S_CALLTYPE SteamAPI_RegisterCallback( class CCallbackBase *pCallback, int iCallback )
{
}

S_API void S_CALLTYPE SteamAPI_UnregisterCallback( class CCallbackBase *pCallback )
{
}

S_API void S_CALLTYPE SteamAPI_RegisterCallResult( class CCallbackBase *pCallback, SteamAPICall_t hAPICall )
{
}

S_API void S_CALLTYPE SteamAPI_UnregisterCallResult( class CCallbackBase *pCallback, SteamAPICall_t hAPICall )
{
}

S_API void SteamAPI_SetTryCatchCallbacks( bool bTryCatchCallbacks )
{
}

S_API void Steam_RunCallbacks( HSteamPipe hSteamPipe, bool bGameServerCallbacks )
{
}

S_API void Steam_RegisterInterfaceFuncs( void *hModule )
{
}

S_API const char *SteamAPI_GetSteamInstallPath()
{
	return "";
}

S_API HSteamPipe SteamAPI_GetHSteamPipe()
{
	return k_hOfflineSteamPipe;
}

S_API HSteamUser SteamAPI_GetHSteamUser()
{
	return k_hOfflineSteamUser;
}

S_API HSteamPipe GetHSteamPipe()
{
	return k_hOfflineSteamPipe;
}

S_API HSteamUser GetHSteamUser()
{
	return k_hOfflineSteamUser;
}

S_API HSteamUser Steam_GetHSteamUserCurrent()
{
	return k_hOfflineSteamUser;
}

S_API void * S_CALLTYPE SteamInternal_CreateInterface( const char *ver )
{
	// CSteamAPIContext::Init asks for the client, everything else comes from it
	if ( ver && !strncmp( ver, "SteamClient", 11 ) )
		return OfflineSteamClient();
	return NULL;
}

//-----------------------------------------------------------------------------
// Game server: unavailable, the engine falls back to LAN mode
//-----------------------------------------------------------------------------

S_API bool S_CALLTYPE SteamInternal_GameServer_Init( uint32 unIP, uint16 usPort, uint16 usGamePort, uint16 usQueryPort, EServerMode eServerMode, const char *pchVersionString )
{
	return false;
}

S_API void * S_CALLTYPE SteamGameServerInternal_CreateInterface( const char *ver )
{
	return NULL;
}

S_API void SteamGameServer_Shutdown()
{
}

S_API void SteamGameServer_RunCallbacks()
{
}

S_API bool SteamGameServer_BSecure()
{
	return false;
}

S_API uint64 SteamGameServer_GetSteamID()
{
	return 0;
}

S_API HSteamPipe S_CALLTYPE SteamGameServer_GetHSteamPipe()
{
	return 0;
}

S_API HSteamUser S_CALLTYPE SteamGameServer_GetHSteamUser()
{
	return 0;
}

// The headers call accessors on the returned context, so it has to be real
// (zeroed) storage: CSteamGameServerAPIContext then hands out NULL interfaces.
S_API CSteamGameServerAPIContext * S_CALLTYPE SteamInternal_GlobalContextGameServerPtr( uint32 size )
{
	static unsigned long long s_context[256];
	if ( size > sizeof( s_context ) )
		return NULL;
	return (CSteamGameServerAPIContext *)s_context;
}
