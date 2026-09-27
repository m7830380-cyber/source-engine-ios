//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: CS:GO's Scaleform scoreboard, reconstructed. See scoreboard_scaleform.h.
//
// The movie (resource/flash/scoreboard.swf) lays the board out itself; the game
// fills in the clips under ScoreBoard.InnerScoreBoard:
//   header:  MapName.MapName, GameType.GameType, GameTime.GameTimeLeft,
//            RoundsLeft.RoundsLeft, CT_TeamName.CT_TeamNameText, T_TeamName.T_TeamNameText,
//            CT_Score.CT_Score, T_Score.T_Score, CT_Alive.CT_Alive, T_Alive.T_Alive
//   rows:    CT_Scoretable.CT_ScoreRow_<0..11>, T_Scoretable.T_ScoreRow_<0..11>, each with
//            Ping, Money, Kills, Assists, Death, Mvp, Score text fields, the name clip
//            Player_Name (frames Normal/Dead/Local/DeadLocal, set through the movie's
//            SetPlayerNormal/Dead/Local/DeadLocal) and icon clips (Bomb, defuser, Skull, ...)
// and calls showPanel/hidePanel. Round history, Elo, XP, quests and item drops need
// GC data and stay hidden.
//
//=============================================================================//
#include "cbase.h"
#if defined( INCLUDE_SCALEFORM )
#include <game/client/iviewport.h>
#include "scoreboard_scaleform.h"
#include "c_cs_playerresource.h"
#include "c_cs_player.h"
#include "c_team.h"
#include "cs_gamerules.h"
#include "gametypes.h"
#include "vgui/ILocalize.h"
#include "gameui_util.h"
#include "ios_avatar_share.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#define SCOREBOARD_ROWS 12

SFUI_BEGIN_GAME_API_DEF
	SFUI_DECL_METHOD( IsQueuedMatchmaking ),
	SFUI_DECL_METHOD( IsLocalPlayerHLTV ),
	SFUI_DECL_METHOD( GetGamePhase ),
	SFUI_DECL_METHOD( GetGameType ),
	SFUI_DECL_METHOD( GetGameMode ),
	SFUI_DECL_METHOD( GetCurrentRound ),
	SFUI_DECL_METHOD( AreTeamsPlayingSwitchedSides ),
	SFUI_DECL_METHOD( GetMouseEnableBindingName ),
	SFUI_DECL_METHOD( SelectPlayerRow ),
	SFUI_DECL_METHOD( GetSelectedPlayerXuid ),
	SFUI_DECL_METHOD( GetSelectedPlayerIndex ),
	// no round history, map vote, coop, quests, skirmishes or item drops offline
	SFUI_DECL_METHOD_AS( ReturnFalse, "HasRoundDataToShow" ),
	SFUI_DECL_METHOD_AS( ReturnFalse, "IsCoopMissionCompleted" ),
	SFUI_DECL_METHOD_AS( ReturnFalse, "NextMapAlreadySelected" ),
	SFUI_DECL_METHOD_AS( ReturnZero, "GetNumMapsInMapgroup" ),
	SFUI_DECL_METHOD_AS( ReturnEmpty, "GetMapNameInMapgroup" ),
	SFUI_DECL_METHOD_AS( ReturnZero, "GetQuestID" ),
	SFUI_DECL_METHOD_AS( ReturnEmpty, "GetQuestProgressReason" ),
	SFUI_DECL_METHOD_AS( ReturnZero, "GetCurrentSkirmishID" ),
	SFUI_DECL_METHOD_AS( ReturnEmpty, "GetSkirmishTitle" ),
	SFUI_DECL_METHOD_AS( ReturnEmpty, "GetSkirmishDetais" ),
	SFUI_DECL_METHOD_AS( ReturnEmpty, "GetSkirmishDesc" ),
	SFUI_DECL_METHOD_AS( ReturnZero, "GetCoopMatchScoreboardDetailsHandle" ),
	SFUI_DECL_METHOD_AS( ReturnFalse, "GetCasterIsHeard" ),
	SFUI_DECL_METHOD_AS( ReturnFalse, "GetCasterIsCameraman" ),
	SFUI_DECL_METHOD_AS( ReturnFalse, "GetCasterControlsXray" ),
	SFUI_DECL_METHOD_AS( ReturnFalse, "GetCasterControlsUI" ),
	SFUI_DECL_METHOD_AS( DoNothing, "SetCasterIsHeard" ),
	SFUI_DECL_METHOD_AS( DoNothing, "SetCasterIsCameraman" ),
	SFUI_DECL_METHOD_AS( DoNothing, "SetCasterControlsXray" ),
	SFUI_DECL_METHOD_AS( DoNothing, "SetCasterControlsUI" ),
	SFUI_DECL_METHOD_AS( DoNothing, "PlayItemDropSound" ),
	SFUI_DECL_METHOD_AS( DoNothing, "PlayItemDropSoundLocal" ),
	SFUI_DECL_METHOD_AS( DoNothing, "OpenPlayerDetailsPanel" ),
SFUI_END_GAME_API_DEF( CCSScoreboardScaleform, Scoreboard );

CCSScoreboardScaleform::CCSScoreboardScaleform( CounterStrikeViewport *pViewPort ) :
	m_bVisible( false ),
	m_bLoading( false ),
	m_flNextUpdate( 0.0f ),
	m_nSelectedIndex( 0 ),
	m_SelectedXuid( 0 ),
	m_nAvatarVersion( 0 )
{
	m_iSplitScreenSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	V_memset( m_RowXuids, 0xFF, sizeof( m_RowXuids ) );
	ListenForGameEvent( "cs_game_disconnected" );
	ListenForGameEvent( "game_newmap" );
}

CCSScoreboardScaleform::~CCSScoreboardScaleform()
{
	StopListeningForAllEvents();
}

//-----------------------------------------------------------------------------
// gameAPI
//-----------------------------------------------------------------------------
void CCSScoreboardScaleform::IsQueuedMatchmaking( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, CSGameRules() && CSGameRules()->IsQueuedMatchmaking() );
}

void CCSScoreboardScaleform::IsLocalPlayerHLTV( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, engine->IsHLTV() );
}

void CCSScoreboardScaleform::GetGamePhase( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, CSGameRules() ? (int)CSGameRules()->GetGamePhase() : 0 );
}

void CCSScoreboardScaleform::GetGameType( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, g_pGameTypes ? g_pGameTypes->GetCurrentGameType() : 0 );
}

void CCSScoreboardScaleform::GetGameMode( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, g_pGameTypes ? g_pGameTypes->GetCurrentGameMode() : 0 );
}

void CCSScoreboardScaleform::GetCurrentRound( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, CSGameRules() ? CSGameRules()->GetTotalRoundsPlayed() + 1 : 0 );
}

void CCSScoreboardScaleform::AreTeamsPlayingSwitchedSides( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, CSGameRules() && CSGameRules()->AreTeamsPlayingSwitchedSides() );
}

void CCSScoreboardScaleform::GetMouseEnableBindingName( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, "" );
}

void CCSScoreboardScaleform::SelectPlayerRow( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_nSelectedIndex = (int)m_pScaleformUI->Params_GetArgAsNumber( obj, 0 );
	int nTeam = (int)m_pScaleformUI->Params_GetArgAsNumber( obj, 1 );
	int nTeamIdx = ( nTeam == TEAM_CT ) ? 1 : 0;
	m_SelectedXuid = ( m_nSelectedIndex >= 0 && m_nSelectedIndex < SCOREBOARD_ROWS ) ? m_RowXuids[nTeamIdx][m_nSelectedIndex] : 0;
}

void CCSScoreboardScaleform::GetSelectedPlayerXuid( SCALEFORM_CALLBACK_ARGS_DECL )
{
	char szXuid[32];
	V_snprintf( szXuid, sizeof( szXuid ), "%llu", m_SelectedXuid );
	m_pScaleformUI->Params_SetResult( obj, szXuid );
}

void CCSScoreboardScaleform::GetSelectedPlayerIndex( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, m_nSelectedIndex );
}

void CCSScoreboardScaleform::ReturnFalse( SCALEFORM_CALLBACK_ARGS_DECL ) { m_pScaleformUI->Params_SetResult( obj, false ); }
void CCSScoreboardScaleform::ReturnZero( SCALEFORM_CALLBACK_ARGS_DECL ) { m_pScaleformUI->Params_SetResult( obj, 0 ); }
void CCSScoreboardScaleform::ReturnEmpty( SCALEFORM_CALLBACK_ARGS_DECL ) { m_pScaleformUI->Params_SetResult( obj, "" ); }
void CCSScoreboardScaleform::DoNothing( SCALEFORM_CALLBACK_ARGS_DECL ) {}

//-----------------------------------------------------------------------------
// Flash lifecycle
//-----------------------------------------------------------------------------
void CCSScoreboardScaleform::FlashReady( void )
{
	m_bLoading = false;
	V_memset( m_RowXuids, 0xFF, sizeof( m_RowXuids ) );

	WITH_SLOT_LOCKED
	{
		// 12 rows per team; the TenPlayer/SixteenPlayer layouts are picked by the
		// movie from FriendsList.GetPlayerCount, which only exists in the menu slot
		m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "ShowStandardVersion", NULL, 0 );
		SetVisible( m_FlashAPI, "ScoreBoard.InnerScoreBoard.Spectator_Scoretable", false );
	}

	if ( m_bVisible )
		Show();
}

bool CCSScoreboardScaleform::PreUnloadFlash( void )
{
	m_bLoading = false;
	m_bVisible = false;
	return ScaleformFlashInterface::PreUnloadFlash();
}

void CCSScoreboardScaleform::ShowPanel( bool bShow )
{
	if ( !IsValidSplitScreenSlot( m_iSplitScreenSlot ) )
		return;

	if ( bShow )
		Show();
	else
		Hide();
}

void CCSScoreboardScaleform::Show( void )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	m_bVisible = true;

	if ( !FlashAPIIsValid() )
	{
		if ( !m_bLoading )
		{
			m_bLoading = true;
			SFUI_REQUEST_ELEMENT( SF_SS_SLOT( m_iSplitScreenSlot ), g_pScaleformUI, CCSScoreboardScaleform, this, Scoreboard );
		}
		return;		// FlashReady shows it
	}

	if ( m_bLoading )
		return;

	// fill in first: showPanel lays the header out from the text widths
	UpdateData();
	m_flNextUpdate = gpGlobals->curtime + 0.25f;

	WITH_SLOT_LOCKED
	{
		m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showPanel", NULL, 0 );
	}
}

void CCSScoreboardScaleform::Hide( void )
{
	m_bVisible = false;

	if ( FlashAPIIsValid() && !m_bLoading )
	{
		WITH_SLOT_LOCKED
		{
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanel", NULL, 0 );
		}
	}
}

void CCSScoreboardScaleform::ViewportThink( void )
{
	if ( !m_bVisible || m_bLoading || !FlashAPIIsValid() )
		return;

	if ( gpGlobals->curtime >= m_flNextUpdate || gpGlobals->curtime < m_flNextUpdate - 1.0f )
	{
		m_flNextUpdate = gpGlobals->curtime + 0.25f;
		UpdateData();
	}
}

void CCSScoreboardScaleform::FireGameEvent( IGameEvent *event )
{
	// the HUD slot goes away with the level; load again next time
	Hide();
	RemoveFlashElement();
}

//-----------------------------------------------------------------------------
// Clip helpers
//-----------------------------------------------------------------------------
SFVALUE CCSScoreboardScaleform::GetPath( SFVALUE root, const char *pszPath )
{
	if ( !root )
		return NULL;

	char szPath[256];
	V_strncpy( szPath, pszPath, sizeof( szPath ) );

	SFVALUE cur = NULL;
	char *pszPart = szPath;
	while ( pszPart && *pszPart )
	{
		char *pszDot = strchr( pszPart, '.' );
		if ( pszDot )
			*pszDot = 0;

		SFVALUE next = m_pScaleformUI->Value_GetMember( cur ? cur : root, pszPart );
		if ( cur )
			m_pScaleformUI->ReleaseValue( cur );
		cur = next;
		if ( !cur || m_pScaleformUI->Value_GetType( cur ) == IScaleformUI::VT_Undefined )
		{
			if ( cur )
				m_pScaleformUI->ReleaseValue( cur );
			return NULL;
		}

		pszPart = pszDot ? pszDot + 1 : NULL;
	}
	return cur;
}

void CCSScoreboardScaleform::SetText( SFVALUE root, const char *pszPath, const wchar_t *pszText )
{
	SFVALUE value = GetPath( root, pszPath );
	if ( value )
	{
		m_pScaleformUI->Value_SetText( value, pszText );
		m_pScaleformUI->ReleaseValue( value );
	}
}

void CCSScoreboardScaleform::SetText( SFVALUE root, const char *pszPath, const char *pszText )
{
	wchar_t wszText[256];
	V_UTF8ToUnicode( pszText, wszText, sizeof( wszText ) );
	SetText( root, pszPath, wszText );
}

void CCSScoreboardScaleform::SetVisible( SFVALUE root, const char *pszPath, bool bVisible )
{
	SFVALUE value = GetPath( root, pszPath );
	if ( value )
	{
		m_pScaleformUI->Value_SetVisible( value, bVisible );
		m_pScaleformUI->ReleaseValue( value );
	}
}

static const wchar_t *LocalizeOr( const char *pszToken, const wchar_t *pszFallback )
{
	const wchar_t *pszText = g_pVGuiLocalize ? g_pVGuiLocalize->Find( pszToken ) : NULL;
	return pszText ? pszText : pszFallback;
}

//-----------------------------------------------------------------------------
// Data
//-----------------------------------------------------------------------------
void CCSScoreboardScaleform::UpdateData( void )
{
	if ( !FlashAPIIsValid() || !g_PR )
		return;

	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

#if defined( IOS )
	// a LAN player's profile picture arrived: show the avatars again
	if ( m_nAvatarVersion != IOSAvatar_GetVersion() )
	{
		m_nAvatarVersion = IOSAvatar_GetVersion();
		V_memset( m_RowXuids, 0xFF, sizeof( m_RowXuids ) );
	}
#endif

	WITH_SLOT_LOCKED
	{
		UpdateHeader();
		UpdateTeam( TEAM_CT );
		UpdateTeam( TEAM_TERRORIST );
	}
}

void CCSScoreboardScaleform::UpdateHeader( void )
{
	SFVALUE inner = GetPath( m_FlashAPI, "ScoreBoard.InnerScoreBoard" );
	if ( !inner )
		return;

	// map: "Mirage" (SFUI_Map_de_mirage) or the file name
	char szMap[MAX_PATH];
	V_FileBase( engine->GetLevelName(), szMap, sizeof( szMap ) );
	char szToken[MAX_PATH + 16];
	V_snprintf( szToken, sizeof( szToken ), "#SFUI_Map_%s", szMap );
	wchar_t wszMap[MAX_PATH];
	V_UTF8ToUnicode( szMap, wszMap, sizeof( wszMap ) );
	SetText( inner, "MapName.MapName", LocalizeOr( szToken, wszMap ) );

	// game mode: "Casual", "Competitive", ...
	const char *pszMode = g_pGameTypes ? g_pGameTypes->GetCurrentGameModeNameID() : NULL;
	SetText( inner, "GameType.GameType", pszMode && pszMode[0] ? LocalizeOr( pszMode, L"" ) : L"" );
	SetText( inner, "ServerName.ServerName", L"" );

	// round time left
	wchar_t wszTime[32] = L"";
	if ( CSGameRules() )
	{
		int nSecs = MAX( 0, (int)ceilf( CSGameRules()->GetRoundRemainingTime() ) );
		V_snwprintf( wszTime, ARRAYSIZE( wszTime ), L"%d:%02d", nSecs / 60, nSecs % 60 );
	}
	SetText( inner, "GameTime.GameTimeLeft", wszTime );
	SetText( inner, "GameTime.TimerAnim.GameTimeLeft", wszTime );

	// rounds left
	static ConVarRef mp_maxrounds( "mp_maxrounds" );
	wchar_t wszRounds[64] = L"";
	if ( CSGameRules() && mp_maxrounds.IsValid() && mp_maxrounds.GetInt() > 0 )
	{
		wchar_t wszCount[16];
		V_snwprintf( wszCount, ARRAYSIZE( wszCount ), L"%d", MAX( 0, mp_maxrounds.GetInt() - CSGameRules()->GetTotalRoundsPlayed() ) );
		g_pVGuiLocalize->ConstructString( wszRounds, sizeof( wszRounds ), LocalizeOr( "#SFUI_Scoreboard_RoundsLeft", L"Rounds Left: %s1" ), 1, wszCount );
	}
	SetText( inner, "RoundsLeft.RoundsLeft", wszRounds );

	// teams
	SetText( inner, "CT_TeamName.CT_TeamNameText", LocalizeOr( "#SFUI_CT_Label", L"COUNTER-TERRORISTS" ) );
	SetText( inner, "T_TeamName.T_TeamNameText", LocalizeOr( "#SFUI_T_Label", L"TERRORISTS" ) );

	for ( int iTeam = TEAM_TERRORIST; iTeam <= TEAM_CT; iTeam++ )
	{
		bool bCT = ( iTeam == TEAM_CT );
		C_Team *pTeam = GetGlobalTeam( iTeam );
		wchar_t wszScore[16];
		V_snwprintf( wszScore, ARRAYSIZE( wszScore ), L"%d", pTeam ? pTeam->Get_Score() : 0 );
		SetText( inner, bCT ? "CT_Score.CT_Score" : "T_Score.T_Score", wszScore );
		SetText( inner, bCT ? "TeamOneScoreTotal.TeamOneScoreText" : "TeamTwoScoreTotal.TeamTwoScoreText", wszScore );

		int nAlive = 0;
		for ( int i = 1; i <= MAX_PLAYERS; i++ )
		{
			if ( g_PR->IsConnected( i ) && g_PR->GetTeam( i ) == iTeam && g_PR->IsAlive( i ) )
				nAlive++;
		}
		wchar_t wszCount[16], wszAlive[64];
		V_snwprintf( wszCount, ARRAYSIZE( wszCount ), L"%d", nAlive );
		g_pVGuiLocalize->ConstructString( wszAlive, sizeof( wszAlive ), LocalizeOr( "#SFUI_Scoreboard_Players", L"Players Alive: %s1" ), 1, wszCount );
		SetText( inner, bCT ? "CT_Alive.CT_Alive" : "T_Alive.T_Alive", wszAlive );
	}

	m_pScaleformUI->ReleaseValue( inner );
}

struct ScoreboardEntry_t
{
	int m_nIndex;
	int m_nScore;
	int m_nKills;
};

static int SortEntries( const ScoreboardEntry_t *a, const ScoreboardEntry_t *b )
{
	if ( a->m_nScore != b->m_nScore )
		return b->m_nScore - a->m_nScore;
	if ( a->m_nKills != b->m_nKills )
		return b->m_nKills - a->m_nKills;
	return a->m_nIndex - b->m_nIndex;
}

void CCSScoreboardScaleform::UpdateTeam( int nTeam )
{
	C_CS_PlayerResource *pCSPR = GetCSResources();
	if ( !pCSPR )
		return;

	bool bCT = ( nTeam == TEAM_CT );
	int nTeamIdx = bCT ? 1 : 0;

	SFVALUE table = GetPath( m_FlashAPI, bCT ? "ScoreBoard.InnerScoreBoard.CT_Scoretable" : "ScoreBoard.InnerScoreBoard.T_Scoretable" );
	if ( !table )
		return;

	CUtlVector< ScoreboardEntry_t > entries;
	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		if ( !g_PR->IsConnected( i ) || g_PR->GetTeam( i ) != nTeam )
			continue;
		ScoreboardEntry_t entry = { i, pCSPR->GetScore( i ), g_PR->GetKills( i ) };
		entries.AddToTail( entry );
	}
	entries.Sort( SortEntries );

	int nLocal = GetLocalPlayerIndex();
	int nLocalTeam = ( nLocal > 0 && g_PR->IsConnected( nLocal ) ) ? g_PR->GetTeam( nLocal ) : TEAM_UNASSIGNED;
	// teammates (and spectators) see money and the bomb carrier
	bool bFriendly = ( nLocalTeam == nTeam ) || ( nLocalTeam == TEAM_SPECTATOR ) || engine->IsHLTV();

	for ( int iRow = 0; iRow < SCOREBOARD_ROWS; iRow++ )
	{
		char szRow[32];
		V_snprintf( szRow, sizeof( szRow ), bCT ? "CT_ScoreRow_%d" : "T_ScoreRow_%d", iRow );
		SFVALUE row = m_pScaleformUI->Value_GetMember( table, szRow );
		if ( !row )
			continue;

		if ( iRow >= entries.Count() )
		{
			m_pScaleformUI->Value_SetVisible( row, false );
			if ( m_RowXuids[nTeamIdx][iRow] != (XUID)-1 )
			{
				WITH_SFVALUEARRAY( args, 1 )
				{
					m_pScaleformUI->ValueArray_SetElement( args, 0, row );
					m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "HideAvatar", args, 1 );
				}
				m_RowXuids[nTeamIdx][iRow] = (XUID)-1;
			}
			m_pScaleformUI->ReleaseValue( row );
			continue;
		}

		int i = entries[iRow].m_nIndex;
		bool bAlive = g_PR->IsAlive( i );
		bool bLocal = ( i == nLocal );
		bool bBot = g_PR->IsFakePlayer( i );

		m_pScaleformUI->Value_SetVisible( row, true );

		// name clip frame first: it recreates the text field inside
		SFVALUE nameMovie = m_pScaleformUI->Value_GetMember( row, "Player_Name" );
		if ( nameMovie )
		{
			const char *pszState = bLocal ? ( bAlive ? "SetPlayerLocal" : "SetPlayerDeadLocal" ) : ( bAlive ? "SetPlayerNormal" : "SetPlayerDead" );
			WITH_SFVALUEARRAY( args, 1 )
			{
				m_pScaleformUI->ValueArray_SetElement( args, 0, nameMovie );
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, pszState, args, 1 );
			}
			m_pScaleformUI->ReleaseValue( nameMovie );
		}

		const char *pszName = g_PR->GetPlayerName( i );
		SetText( row, "Player_Name.Player_Name", pszName ? pszName : "" );

		wchar_t wsz[32];
		if ( bBot )
			V_wcsncpy( wsz, L"BOT", sizeof( wsz ) );
		else
			V_snwprintf( wsz, ARRAYSIZE( wsz ), L"%d", g_PR->GetPing( i ) );
		SetText( row, "Ping", wsz );

		V_snwprintf( wsz, ARRAYSIZE( wsz ), L"%d", g_PR->GetKills( i ) );
		SetText( row, "Kills", wsz );
		V_snwprintf( wsz, ARRAYSIZE( wsz ), L"%d", g_PR->GetAssists( i ) );
		SetText( row, "Assists", wsz );
		V_snwprintf( wsz, ARRAYSIZE( wsz ), L"%d", g_PR->GetDeaths( i ) );
		SetText( row, "Death", wsz );
		V_snwprintf( wsz, ARRAYSIZE( wsz ), L"%d", pCSPR->GetScore( i ) );
		SetText( row, "Score", wsz );
		int nMVPs = pCSPR->GetNumMVPs( i );
		if ( nMVPs > 0 )
			V_snwprintf( wsz, ARRAYSIZE( wsz ), L"%d", nMVPs );
		else
			wsz[0] = 0;
		SetText( row, "Mvp", wsz );

		wsz[0] = 0;
		C_CSPlayer *pPlayer = bFriendly ? ToCSPlayer( UTIL_PlayerByIndex( i ) ) : NULL;
		if ( pPlayer )
			V_snwprintf( wsz, ARRAYSIZE( wsz ), L"$%d", pPlayer->GetAccount() );
		SetText( row, "Money", wsz );

		// icons: only the ones this data can drive, the rest hidden
		SetVisible( row, "Bomb", bFriendly && bAlive && pCSPR->HasC4( i ) );
		SetVisible( row, "defuser", bFriendly && bAlive && pCSPR->HasDefuser( i ) );
		SetVisible( row, "Skull", !bAlive );
		SetVisible( row, "DeadBG", !bAlive );
		SetVisible( row, "PlayerOutline", bLocal );
		static const char *s_pszHidden[] = { "Dominated", "DominatedDead", "Nemesis", "NemesisDead", "switch_teams", "Mute",
			"MusicIcon", "RankIcon_00", "XpIcon_00", "CoinIcon", "fIcon", "lIcon", "tIcon", "Sheen" };
		for ( int k = 0; k < ARRAYSIZE( s_pszHidden ); k++ )
			SetVisible( row, s_pszHidden[k], false );

		// avatar: only when the player in the row changes. From player info: the
		// player resource keeps a XUID only once Steam's avatar image for it
		// exists, which never happens offline (it gave 0, the team icon)
		XUID xuid = 0;
		player_info_t info;
		if ( !bBot && engine->GetPlayerInfo( i, &info ) )
			xuid = info.xuid;
		// our own row: the avatar loader knows our Steam ID (profile_avatar)
		if ( !xuid && bLocal && steamapicontext && steamapicontext->SteamUser() )
			xuid = steamapicontext->SteamUser()->GetSteamID().ConvertToUint64();
		if ( m_RowXuids[nTeamIdx][iRow] != xuid )
		{
			m_RowXuids[nTeamIdx][iRow] = xuid;
			char szXuid[32];
			V_snprintf( szXuid, sizeof( szXuid ), "%llu", xuid );
			WITH_SFVALUEARRAY( args, 6 )
			{
				m_pScaleformUI->ValueArray_SetElement( args, 0, row );
				m_pScaleformUI->ValueArray_SetElement( args, 1, nTeam );
				m_pScaleformUI->ValueArray_SetElement( args, 2, szXuid );
				m_pScaleformUI->ValueArray_SetElement( args, 3, (int)( xuid & 0xFFFFFFFF ) );
				m_pScaleformUI->ValueArray_SetElement( args, 4, pszName ? pszName : "" );
				m_pScaleformUI->ValueArray_SetElement( args, 5, true );
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "ShowAvatar", args, 6 );
			}
		}

		m_pScaleformUI->ReleaseValue( row );
	}

	m_pScaleformUI->ReleaseValue( table );
}

#endif // INCLUDE_SCALEFORM
