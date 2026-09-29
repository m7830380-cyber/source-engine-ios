//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: match history (iOS/offline). CS:GO's Watch > Your Matches came from
// Valve's servers; here every finished match (offline with bots or LAN) is
// recorded on this phone, and the Watch panel (watchpanel.swf) reads it through
// the components it always used:
//
//   CScaleformComponent_MatchList: GetState / GetCount / GetMatchByIndex for the
//       "your matches" lister (whose ID is your XUID)
//   CScaleformComponent_MatchInfo: map, date, duration, team scores, up to five
//       players a team with their stats, and per-round stats (comma lists)
//
// Recording: kills / headshots / deaths / MVPs and round wins per round from the
// game events; totals and team scores from the player resource at the match
// end (cs_win_panel_match). Stored in cfg/offline_matches.txt, newest first,
// the last 30.
//
//=============================================================================//
#include "cbase.h"

#include <time.h>
#include "scaleformui/scaleformui.h"
#include "GameEventListener.h"
#include "c_cs_playerresource.h"
#include "c_team.h"
#include "cs_gamerules.h"
#include "filesystem.h"
#include "tier1/fmtstr.h"
#include "steam/steam_api.h"
#include "createmainmenuscreen_scaleform.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define MATCH_FILE		"cfg/offline_matches.txt"
#define MATCH_PATHID	"MOD"
#define MAX_MATCHES		30
#define MAX_ROUNDS		30		// the Watch panel shows 30 rounds

static KeyValues *s_pMatches = NULL;

static KeyValues *Matches()
{
	if ( !s_pMatches )
	{
		s_pMatches = new KeyValues( "OfflineMatches" );
		s_pMatches->LoadFromFile( g_pFullFileSystem, MATCH_FILE, MATCH_PATHID );
	}
	return s_pMatches;
}

static void SaveMatches()
{
	Matches()->SaveToFile( g_pFullFileSystem, MATCH_FILE, MATCH_PATHID );
}

static KeyValues *FindMatch( const char *pszID )
{
	for ( KeyValues *pMatch = Matches()->GetFirstTrueSubKey(); pMatch; pMatch = pMatch->GetNextTrueSubKey() )
	{
		if ( !V_strcmp( pMatch->GetString( "id" ), pszID ) )
			return pMatch;
	}
	return NULL;
}

static uint64 LocalXuid()
{
	return ( steamapicontext && steamapicontext->SteamUser() ) ? steamapicontext->SteamUser()->GetSteamID().ConvertToUint64() : 0;
}

//-----------------------------------------------------------------------------
// Recording
//-----------------------------------------------------------------------------
class CMatchHistoryRecorder : public CAutoGameSystem, public CGameEventListener
{
public:
	CMatchHistoryRecorder() : CAutoGameSystem( "CMatchHistoryRecorder" ) {}

	virtual void LevelInitPostEntity()
	{
		Reset();
		ListenForGameEvent( "player_death" );
		ListenForGameEvent( "round_mvp" );
		ListenForGameEvent( "round_end" );
		ListenForGameEvent( "round_start" );
		ListenForGameEvent( "cs_win_panel_match" );
	}

	virtual void LevelShutdownPreEntity()
	{
		StopListeningForAllEvents();
		Reset();
	}

	virtual void FireGameEvent( IGameEvent *event )
	{
		const char *pszName = event->GetName();
		bool bWarmup = CSGameRules() && CSGameRules()->IsWarmupPeriod();

		if ( !V_strcmp( pszName, "round_start" ) )
		{
			if ( !bWarmup && m_flMatchStart == 0.0f )
				m_flMatchStart = gpGlobals->curtime;
			return;
		}
		if ( bWarmup || m_nRound >= MAX_ROUNDS )
			return;

		if ( !V_strcmp( pszName, "player_death" ) )
		{
			int nVictim = engine->GetPlayerForUserID( event->GetInt( "userid" ) );
			int nAttacker = engine->GetPlayerForUserID( event->GetInt( "attacker" ) );
			if ( nVictim > 0 && nVictim <= MAX_PLAYERS )
				m_Deaths[nVictim][m_nRound] = 1;
			if ( nAttacker > 0 && nAttacker <= MAX_PLAYERS && nAttacker != nVictim && g_PR &&
				 g_PR->GetTeam( nAttacker ) != g_PR->GetTeam( nVictim ) )
			{
				m_Kills[nAttacker][m_nRound]++;
				if ( event->GetBool( "headshot" ) )
					m_Headshots[nAttacker][m_nRound]++;
			}
		}
		else if ( !V_strcmp( pszName, "round_mvp" ) )
		{
			int nPlayer = engine->GetPlayerForUserID( event->GetInt( "userid" ) );
			if ( nPlayer > 0 && nPlayer <= MAX_PLAYERS )
				m_Mvps[nPlayer][m_nRound] = 1;
		}
		else if ( !V_strcmp( pszName, "round_end" ) )
		{
			int nWinner = event->GetInt( "winner" );
			if ( nWinner != TEAM_CT && nWinner != TEAM_TERRORIST )
				return;		// a draw or a restart: not a played round
			for ( int i = 1; i <= gpGlobals->maxClients && i <= MAX_PLAYERS; i++ )
			{
				if ( g_PR && g_PR->IsConnected( i ) )
					m_Wins[i][m_nRound] = ( g_PR->GetTeam( i ) == nWinner ) ? 1 : 0;
			}
			m_nRound++;
		}
		else if ( !V_strcmp( pszName, "cs_win_panel_match" ) )
		{
			RecordMatch();
			Reset();
		}
	}

private:
	void Reset()
	{
		m_nRound = 0;
		m_flMatchStart = 0.0f;
		V_memset( m_Kills, 0, sizeof( m_Kills ) );
		V_memset( m_Headshots, 0, sizeof( m_Headshots ) );
		V_memset( m_Deaths, 0, sizeof( m_Deaths ) );
		V_memset( m_Mvps, 0, sizeof( m_Mvps ) );
		V_memset( m_Wins, 0, sizeof( m_Wins ) );
	}

	static void RoundList( char *pszOut, int nOutSize, const unsigned char *pValues, int nRounds )
	{
		pszOut[0] = 0;
		for ( int r = 0; r < nRounds; r++ )
			V_strncat( pszOut, CFmtStr( r ? ",%d" : "%d", pValues[r] ), nOutSize );
	}

	void RecordMatch()
	{
		C_CS_PlayerResource *pCSPR = dynamic_cast< C_CS_PlayerResource * >( g_PR );
		C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
		if ( !pCSPR || !pLocal )
			return;
		int nLocal = pLocal->entindex();
		if ( pCSPR->GetTeam( nLocal ) != TEAM_CT && pCSPR->GetTeam( nLocal ) != TEAM_TERRORIST )
			return;		// watched it as a spectator

		char szMap[64];
		V_FileBase( engine->GetLevelName(), szMap, sizeof( szMap ) );
		C_Team *pCT = GetGlobalTeam( TEAM_CT ), *pT = GetGlobalTeam( TEAM_TERRORIST );

		KeyValues *pMatch = new KeyValues( "match" );
		time_t now = time( NULL );
		pMatch->SetString( "id", CFmtStr( "%lld", (long long)now ) );
		pMatch->SetString( "map", szMap );
		pMatch->SetUint64( "time", (uint64)now );
		pMatch->SetInt( "duration", m_flMatchStart > 0.0f ? (int)( gpGlobals->curtime - m_flMatchStart ) : 0 );
		pMatch->SetInt( "score_ct", pCT ? pCT->Get_Score() : 0 );
		pMatch->SetInt( "score_t", pT ? pT->Get_Score() : 0 );

		KeyValues *pPlayers = pMatch->FindKey( "players", true );
		char szList[MAX_ROUNDS * 4];
		for ( int i = 1; i <= gpGlobals->maxClients && i <= MAX_PLAYERS; i++ )
		{
			if ( !pCSPR->IsConnected( i ) )
				continue;
			int nTeam = pCSPR->GetTeam( i );
			if ( nTeam != TEAM_CT && nTeam != TEAM_TERRORIST )
				continue;

			// the local player by the XUID the menus know it by; bots get a made-up one
			uint64 ullXuid = 0;
			player_info_t info;
			if ( i == nLocal )
				ullXuid = LocalXuid();
			else if ( engine->GetPlayerInfo( i, &info ) && !info.fakeplayer )
				ullXuid = info.xuid;
			if ( !ullXuid )
				ullXuid = 900000000000000000ull + (uint64)now % 1000000 * 100 + i;

			KeyValues *pPlayer = pPlayers->CreateNewKey();
			pPlayer->SetString( "xuid", CFmtStr( "%llu", ullXuid ) );
			pPlayer->SetString( "name", pCSPR->GetPlayerName( i ) );
			pPlayer->SetInt( "team", nTeam );
			pPlayer->SetInt( "kills", pCSPR->GetKills( i ) );
			pPlayer->SetInt( "assists", pCSPR->GetAssists( i ) );
			pPlayer->SetInt( "deaths", pCSPR->GetDeaths( i ) );
			pPlayer->SetInt( "mvps", pCSPR->GetNumMVPs( i ) );
			pPlayer->SetInt( "score", pCSPR->GetScore( i ) );
			RoundList( szList, sizeof( szList ), m_Kills[i], m_nRound );		pPlayer->SetString( "r_kills", szList );
			RoundList( szList, sizeof( szList ), m_Headshots[i], m_nRound );	pPlayer->SetString( "r_headshots", szList );
			RoundList( szList, sizeof( szList ), m_Mvps[i], m_nRound );		pPlayer->SetString( "r_mvps", szList );
			RoundList( szList, sizeof( szList ), m_Deaths[i], m_nRound );		pPlayer->SetString( "r_deaths", szList );
			RoundList( szList, sizeof( szList ), m_Wins[i], m_nRound );		pPlayer->SetString( "r_wins", szList );
		}

		// newest first, keep the last MAX_MATCHES
		KeyValues *pList = Matches();
		KeyValues *pOld = new KeyValues( "OfflineMatches" );
		for ( KeyValues *p = pList->GetFirstTrueSubKey(); p; p = p->GetNextTrueSubKey() )
			pOld->AddSubKey( p->MakeCopy() );
		pList->Clear();
		pList->AddSubKey( pMatch );
		int n = 1;
		for ( KeyValues *p = pOld->GetFirstTrueSubKey(); p && n < MAX_MATCHES; p = p->GetNextTrueSubKey(), n++ )
			pList->AddSubKey( p->MakeCopy() );
		pOld->deleteThis();
		SaveMatches();

		Msg( "[matches] recorded %s: CT %d - T %d, %d rounds, %d s\n", szMap, pMatch->GetInt( "score_ct" ), pMatch->GetInt( "score_t" ), m_nRound, pMatch->GetInt( "duration" ) );
	}

	int m_nRound;
	float m_flMatchStart;
	unsigned char m_Kills[MAX_PLAYERS + 1][MAX_ROUNDS];
	unsigned char m_Headshots[MAX_PLAYERS + 1][MAX_ROUNDS];
	unsigned char m_Deaths[MAX_PLAYERS + 1][MAX_ROUNDS];
	unsigned char m_Mvps[MAX_PLAYERS + 1][MAX_ROUNDS];
	unsigned char m_Wins[MAX_PLAYERS + 1][MAX_ROUNDS];
};
static CMatchHistoryRecorder s_MatchHistoryRecorder;

//-----------------------------------------------------------------------------
// The Watch panel's components
//-----------------------------------------------------------------------------
static const char *ArgStr( IUIMarshalHelper *pui, SFPARAMS obj, int i )
{
	if ( (int)pui->Params_GetNumArgs( obj ) <= i )
		return "";
	IUIMarshalHelper::_ValueType t = pui->Params_GetArgType( obj, i );
	if ( t == IUIMarshalHelper::VT_String || t == IUIMarshalHelper::VT_StringW )
	{
		const char *psz = pui->Params_GetArgAsString( obj, i );
		return psz ? psz : "";
	}
	if ( t == IUIMarshalHelper::VT_Number )
	{
		static char s_szNum[32];
		V_snprintf( s_szNum, sizeof( s_szNum ), "%.0f", pui->Params_GetArgAsNumber( obj, i ) );
		return s_szNum;
	}
	return "";
}

// "your matches" is listed under your XUID; other listers (live, downloaded,
// tournaments) have nothing offline
static bool IsYourLister( const char *pszLister )
{
	uint64 ullLocal = LocalXuid();
	uint64 ull = V_atoui64( pszLister );
	return ull && ullLocal && ( ull & 0xFFFFFFFFull ) == ( ullLocal & 0xFFFFFFFFull );
}

// the Watch panel's team 0 is CT, team 1 T
static int TeamFor( int nWatchTeam )
{
	return nWatchTeam == 0 ? TEAM_CT : TEAM_TERRORIST;
}

// a team's players, best score first
static int SortedTeam( KeyValues *pMatch, int nTeam, KeyValues **ppOut, int nMax )
{
	int n = 0;
	KeyValues *pPlayers = pMatch->FindKey( "players" );
	for ( KeyValues *p = pPlayers ? pPlayers->GetFirstTrueSubKey() : NULL; p && n < nMax; p = p->GetNextTrueSubKey() )
	{
		if ( p->GetInt( "team" ) == nTeam )
			ppOut[n++] = p;
	}
	for ( int i = 1; i < n; i++ )
	{
		for ( int j = i; j > 0 && ppOut[j]->GetInt( "score" ) > ppOut[j - 1]->GetInt( "score" ); j-- )
		{
			KeyValues *pTmp = ppOut[j]; ppOut[j] = ppOut[j - 1]; ppOut[j - 1] = pTmp;
		}
	}
	return n;
}

static KeyValues *FindPlayer( KeyValues *pMatch, const char *pszXuid )
{
	KeyValues *pPlayers = pMatch ? pMatch->FindKey( "players" ) : NULL;
	for ( KeyValues *p = pPlayers ? pPlayers->GetFirstTrueSubKey() : NULL; p; p = p->GetNextTrueSubKey() )
	{
		if ( !V_strcmp( p->GetString( "xuid" ), pszXuid ) )
			return p;
	}
	return NULL;
}

static void FireMatchListChanged()
{
	if ( !CCreateMainMenuScreenScaleform::IsActive() )
		return;
	KeyValues *pEvent = new KeyValues( "ScaleformComponent_MatchList_StateChange" );
	pEvent->SetString( "matchlist", CFmtStr( "%llu", LocalXuid() ) );
	CCreateMainMenuScreenScaleform::GetInstance()->OnEvent( pEvent );
	pEvent->deleteThis();
}

class CScaleformComponentMatchList : public ScaleformUIFunctionHandlerObject
{
public:
	void GetState( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, "ready" ); }
	void Refresh( SCALEFORM_CALLBACK_ARGS_DECL ) {}
	void HowManyMinutesAgoCached( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, 0 ); }
	void StartGOTVTheater( SCALEFORM_CALLBACK_ARGS_DECL ) {}

	void GetCount( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		int n = 0;
		if ( IsYourLister( ArgStr( pui, obj, 0 ) ) )
		{
			for ( KeyValues *p = Matches()->GetFirstTrueSubKey(); p; p = p->GetNextTrueSubKey() )
				n++;
		}
		pui->Params_SetResult( obj, n );
	}

	void GetMatchByIndex( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		if ( !IsYourLister( ArgStr( pui, obj, 0 ) ) )
			return;
		int nIndex = (int)pui->Params_GetArgAsNumber( obj, 1 );
		for ( KeyValues *p = Matches()->GetFirstTrueSubKey(); p; p = p->GetNextTrueSubKey(), nIndex-- )
		{
			if ( nIndex == 0 )
			{
				pui->Params_SetResult( obj, p->GetString( "id" ) );
				return;
			}
		}
	}
};

class CScaleformComponentMatchList_Table : public IScaleformUIFunctionHandlerDefinitionTable
{
public:
	virtual const ScaleformUIFunctionHandlerDefinition *GetTable( void ) const
	{
		typedef CScaleformComponentMatchList T;
		static const ScaleformUIFunctionHandlerDefinition table[] = {
			SFUI_DECL_METHOD( GetState ),
			SFUI_DECL_METHOD( Refresh ),
			SFUI_DECL_METHOD( HowManyMinutesAgoCached ),
			SFUI_DECL_METHOD( StartGOTVTheater ),
			SFUI_DECL_METHOD( GetCount ),
			SFUI_DECL_METHOD( GetMatchByIndex ),
			{ NULL, NULL }
		};
		return table;
	}
};

class CScaleformComponentMatchInfo : public ScaleformUIFunctionHandlerObject
{
public:
	void GetMatchMap( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		if ( KeyValues *p = FindMatch( ArgStr( pui, obj, 0 ) ) )
			pui->Params_SetResult( obj, p->GetString( "map" ) );
	}

	void GetMatchTimestamp( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		KeyValues *p = FindMatch( ArgStr( pui, obj, 0 ) );
		if ( !p )
			return;
		time_t t = (time_t)p->GetUint64( "time" );
		struct tm tmLocal;
		localtime_r( &t, &tmLocal );
		char szDate[64];
		strftime( szDate, sizeof( szDate ), "%Y-%m-%d %H:%M", &tmLocal );
		pui->Params_SetResult( obj, szDate );
	}

	void GetMatchDuration( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		KeyValues *p = FindMatch( ArgStr( pui, obj, 0 ) );
		pui->Params_SetResult( obj, p ? p->GetInt( "duration" ) : 0 );
	}

	void GetMatchRoundScoreForTeam( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		KeyValues *p = FindMatch( ArgStr( pui, obj, 0 ) );
		int nTeam = (int)pui->Params_GetArgAsNumber( obj, 1 );
		pui->Params_SetResult( obj, p ? p->GetInt( nTeam == 0 ? "score_ct" : "score_t" ) : 0 );
	}

	void GetMatchPlayerXuidByIndexForTeam( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		KeyValues *pMatch = FindMatch( ArgStr( pui, obj, 0 ) );
		if ( !pMatch )
			return;
		int nTeam = TeamFor( (int)pui->Params_GetArgAsNumber( obj, 1 ) );
		int nIndex = (int)pui->Params_GetArgAsNumber( obj, 2 );
		KeyValues *pSorted[MAX_PLAYERS];
		int n = SortedTeam( pMatch, nTeam, pSorted, ARRAYSIZE( pSorted ) );
		if ( nIndex >= 0 && nIndex < n )
			pui->Params_SetResult( obj, pSorted[nIndex]->GetString( "xuid" ) );
	}

	void GetMatchPlayerStat( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		KeyValues *pPlayer = FindPlayer( FindMatch( ArgStr( pui, obj, 0 ) ), ArgStr( pui, obj, 1 ) );
		if ( !pPlayer )
			return;
		const char *pszStat = ArgStr( pui, obj, 2 );
		if ( !V_strcmp( pszStat, "name" ) )
			pui->Params_SetResult( obj, pPlayer->GetString( "name" ) );
		else
			pui->Params_SetResult( obj, pPlayer->GetInt( pszStat ) );
	}

	void GetMatchPlayerRoundStats( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		KeyValues *pPlayer = FindPlayer( FindMatch( ArgStr( pui, obj, 0 ) ), ArgStr( pui, obj, 1 ) );
		if ( !pPlayer )
			return;
		const char *pszStat = ArgStr( pui, obj, 2 );
		const char *pszKey = !V_strcmp( pszStat, "enemy_kills" ) ? "r_kills" :
							 !V_strcmp( pszStat, "enemy_headshots" ) ? "r_headshots" :
							 !V_strcmp( pszStat, "mvps" ) ? "r_mvps" :
							 !V_strcmp( pszStat, "deaths" ) ? "r_deaths" :
							 !V_strcmp( pszStat, "round_wins" ) ? "r_wins" : NULL;
		if ( pszKey )
			pui->Params_SetResult( obj, pPlayer->GetString( pszKey ) );
	}

	void GetMatchMetadataFullState( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		pui->Params_SetResult( obj, FindMatch( ArgStr( pui, obj, 0 ) ) != NULL );
	}

	void CanDelete( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		pui->Params_SetResult( obj, FindMatch( ArgStr( pui, obj, 0 ) ) != NULL );
	}

	void Delete( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		KeyValues *p = FindMatch( ArgStr( pui, obj, 0 ) );
		if ( !p )
			return;
		Matches()->RemoveSubKey( p );
		p->deleteThis();
		SaveMatches();
		FireMatchListChanged();
	}

	void ReturnEmpty( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, "" ); }
	void ReturnZero( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, 0 ); }
	void ReturnFalse( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, false ); }
	void DoNothing( SCALEFORM_CALLBACK_ARGS_DECL ) {}
};

class CScaleformComponentMatchInfo_Table : public IScaleformUIFunctionHandlerDefinitionTable
{
public:
	virtual const ScaleformUIFunctionHandlerDefinition *GetTable( void ) const
	{
		typedef CScaleformComponentMatchInfo T;
		static const ScaleformUIFunctionHandlerDefinition table[] = {
			SFUI_DECL_METHOD( GetMatchMap ),
			SFUI_DECL_METHOD( GetMatchTimestamp ),
			SFUI_DECL_METHOD( GetMatchDuration ),
			SFUI_DECL_METHOD( GetMatchRoundScoreForTeam ),
			SFUI_DECL_METHOD( GetMatchPlayerXuidByIndexForTeam ),
			SFUI_DECL_METHOD( GetMatchPlayerStat ),
			SFUI_DECL_METHOD( GetMatchPlayerRoundStats ),
			SFUI_DECL_METHOD( GetMatchMetadataFullState ),
			SFUI_DECL_METHOD( CanDelete ),
			SFUI_DECL_METHOD( Delete ),
			// no demos or Valve servers offline
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetMatchState" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetMatchTournamentName" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetMatchTournamentStageName" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetMatchTournamentTeamName" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetMatchTournamentTeamTag" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetMatchTournamentTeamFlag" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetMatchTournamentTeamID" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetMatchShareToken" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetMatchSpectators" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetMatchSkillGroup" ),
			SFUI_DECL_METHOD_AS( ReturnFalse, "GetTeamsAreSwitchedFromStart" ),
			SFUI_DECL_METHOD_AS( ReturnFalse, "CanWatch" ),
			SFUI_DECL_METHOD_AS( ReturnFalse, "CanDownload" ),
			SFUI_DECL_METHOD_AS( ReturnFalse, "CanWatchHighlights" ),
			SFUI_DECL_METHOD_AS( DoNothing, "Watch" ),
			SFUI_DECL_METHOD_AS( DoNothing, "WatchHighlights" ),
			SFUI_DECL_METHOD_AS( DoNothing, "WatchLowlights" ),
			SFUI_DECL_METHOD_AS( DoNothing, "Download" ),
			SFUI_DECL_METHOD_AS( DoNothing, "DownloadWithShareToken" ),
			{ NULL, NULL }
		};
		return table;
	}
};

static CScaleformComponentMatchList s_ComponentMatchList;
static CScaleformComponentMatchList_Table s_ComponentMatchListTable;
static CScaleformComponentMatchInfo s_ComponentMatchInfo;
static CScaleformComponentMatchInfo_Table s_ComponentMatchInfoTable;

// installed with the other menu components (inventory_components_scaleform.cpp)
void MatchHistory_GetComponents( ScaleformUIFunctionHandlerObject **ppMatchList, const IScaleformUIFunctionHandlerDefinitionTable **ppMatchListTable,
	ScaleformUIFunctionHandlerObject **ppMatchInfo, const IScaleformUIFunctionHandlerDefinitionTable **ppMatchInfoTable )
{
	*ppMatchList = &s_ComponentMatchList;
	*ppMatchListTable = &s_ComponentMatchListTable;
	*ppMatchInfo = &s_ComponentMatchInfo;
	*ppMatchInfoTable = &s_ComponentMatchInfoTable;
}
