//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: "Offline with bots" dialog (single-player.swf). Valve's glue was
//          removed from the leaked source; this one is written against this
//          tree's Scaleform integration following the calls single-player.swf
//          makes, with the session launch from
//          github.com/Rileyboy1223/cstrike15-restoration.
//
//          Flow: the movie calls SetMatchmakingQuery( settings ) and OnOk;
//          OnOk hides the dialog, the hide animation removes the element, and
//          PostUnloadFlash starts an offline match session with the settings.
//          Cancelling unloads without a query and restores the main menu.
//
//===========================================================================//
#include "cbase.h"

#if defined( INCLUDE_SCALEFORM )
#include "basepanel.h"
#include "singleplayergamedialog_scaleform.h"

#include "matchmaking/imatchframework.h"
#include "gametypes/igametypes.h"
#include "gameui_interface.h"

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

extern IGameTypes *g_pGameTypes;

CCreateSinglePlayerGameDialogScaleform *CCreateSinglePlayerGameDialogScaleform::m_pInstance = NULL;

SFUI_BEGIN_GAME_API_DEF
	SFUI_DECL_METHOD( OnOk ),
	SFUI_DECL_METHOD( CheckGameSettingsRequirements ),
	SFUI_DECL_METHOD( SetMatchmakingQuery ),
	SFUI_DECL_METHOD( SetCustomBotDifficulty ),
	SFUI_DECL_METHOD( UpdatedSelections ),
	SFUI_DECL_METHOD( UpdatePendingInvites ),
	SFUI_DECL_METHOD( ShowInviteOverlay ),
	SFUI_DECL_METHOD( GetQueuedMatchmakingTime ),
	SFUI_DECL_METHOD( GetQueuedMatchmakingPlayers ),
	SFUI_DECL_METHOD( GetQueuedMatchmakingPreferredMaplist ),
	SFUI_DECL_METHOD( FilterWorkshopMapsByTags ),
	SFUI_DECL_METHOD( ViewMapInWorkshop ),
	SFUI_DECL_METHOD( ViewAllMapsInWorkshop ),
	SFUI_DECL_METHOD( GetWorkshopMapPath ),
	SFUI_DECL_METHOD( GetWorkshopMapID ),
	SFUI_DECL_METHOD( GetWorkshopMapDownloadProgress ),
	SFUI_DECL_METHOD( EnumerateWorkshopMapsFailed ),
	SFUI_DECL_METHOD( RefreshFileInfo ),
	SFUI_DECL_METHOD( DownloadCurrentGamesCount ),
	SFUI_DECL_METHOD( QueryWorkshopMapSubscriptionsFromScript ),
	SFUI_DECL_METHOD( ScriptGetTotalMapSubscriptions ),
SFUI_END_GAME_API_DEF( CCreateSinglePlayerGameDialogScaleform, StartSinglePlayer );

void CCreateSinglePlayerGameDialogScaleform::LoadDialog( bool bUsingMatchmaking, bool bTeamLobbyMode, bool bTrainingMode )
{
	if ( !m_pInstance )
	{
		m_pInstance = new CCreateSinglePlayerGameDialogScaleform( bUsingMatchmaking, bTeamLobbyMode, bTrainingMode );
		SFUI_REQUEST_ELEMENT( SF_FULL_SCREEN_SLOT, g_pScaleformUI, CCreateSinglePlayerGameDialogScaleform, m_pInstance, StartSinglePlayer );
		printf( "[offline] dialog load (matchmaking %d, team lobby %d, training %d)\n", bUsingMatchmaking, bTeamLobbyMode, bTrainingMode );
		fflush( stdout );
	}
}

void CCreateSinglePlayerGameDialogScaleform::UnloadDialog()
{
	if ( m_pInstance )
		m_pInstance->RemoveFlashElement();
}

CCreateSinglePlayerGameDialogScaleform::CCreateSinglePlayerGameDialogScaleform( bool bUsingMatchmaking, bool bTeamLobbyMode, bool bTrainingMode ) :
	m_bUsingMatchmaking( bUsingMatchmaking ),
	m_bTeamLobbyMode( bTeamLobbyMode ),
	m_bTrainingMode( bTrainingMode )
{
	m_szMatchmakingQuery[0] = 0;
}

void CCreateSinglePlayerGameDialogScaleform::FlashLoaded()
{
	WITH_SFVALUEARRAY( data, 3 )
	{
		m_pScaleformUI->ValueArray_SetElement( data, 0, m_bUsingMatchmaking );
		m_pScaleformUI->ValueArray_SetElement( data, 1, m_bTeamLobbyMode );
		m_pScaleformUI->ValueArray_SetElement( data, 2, m_bTrainingMode );
		m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "InitDialogData", data, 3 );
	}
}

void CCreateSinglePlayerGameDialogScaleform::FlashReady()
{
	Show();
}

void CCreateSinglePlayerGameDialogScaleform::Show()
{
	WITH_SLOT_LOCKED
	{
		ScaleformUI()->Value_InvokeWithoutReturn( m_FlashAPI, "showPanel", NULL, 0 );
	}
}

void CCreateSinglePlayerGameDialogScaleform::Hide()
{
	if ( m_szMatchmakingQuery[0] && !m_bUsingMatchmaking )
		GameUI().StartBackgroundMusicFade();

	WITH_SLOT_LOCKED
	{
		ScaleformUI()->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanel", NULL, 0 );
	}
}

static void StartSessionFromSettings( KeyValues *pSettings )
{
	g_pMatchFramework->CreateSession( pSettings );
	IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
	if ( pMatchSession )
	{
		pMatchSession->Command( KeyValues::AutoDeleteInline( new KeyValues( "Start" ) ) );
	}
	else
	{
		Warning( "CCreateSinglePlayerGameDialogScaleform: unable to create the offline session.\n" );
		BasePanel()->RestoreMainMenuScreen();
	}
}

void CCreateSinglePlayerGameDialogScaleform::PostUnloadFlash()
{
	g_pScaleformUI->RefreshKeyBindings();

	if ( m_szMatchmakingQuery[0] )
	{
		printf( "[offline] starting: %s\n", m_szMatchmakingQuery );
		fflush( stdout );

		KeyValues *pSettings = KeyValues::FromString( "Settings", m_szMatchmakingQuery );
		KeyValues::AutoDelete autodelete( pSettings );

		BasePanel()->SetSinglePlayer( !m_bUsingMatchmaking );

		if ( m_bUsingMatchmaking )
		{
			// no online matchmaking here: play the same settings offline with bots
			pSettings->SetString( "system/network", "offline" );
			pSettings->SetString( "system/access", "private" );
		}
		StartSessionFromSettings( pSettings );
	}
	else
	{
		printf( "[offline] dialog closed without starting a game\n" );
		fflush( stdout );
		BasePanel()->RestoreMainMenuScreen();
	}

	m_pInstance = NULL;
	delete this;
}

void CCreateSinglePlayerGameDialogScaleform::OnOk( SCALEFORM_CALLBACK_ARGS_DECL )
{
	engine->ClientCmd_Unrestricted( VarArgs( "host_writeconfig_ss %d", XBX_GetActiveUserId() ) );
	if ( !BasePanel()->ShowLockInput() )
		Hide();
}

void CCreateSinglePlayerGameDialogScaleform::CheckGameSettingsRequirements( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, true );
}

void CCreateSinglePlayerGameDialogScaleform::SetMatchmakingQuery( SCALEFORM_CALLBACK_ARGS_DECL )
{
	V_strncpy( m_szMatchmakingQuery, pui->Params_GetArgAsString( obj, 0 ), sizeof( m_szMatchmakingQuery ) );
}

void CCreateSinglePlayerGameDialogScaleform::SetCustomBotDifficulty( SCALEFORM_CALLBACK_ARGS_DECL )
{
	if ( g_pGameTypes )
		g_pGameTypes->SetCustomBotDifficulty( (int)pui->Params_GetArgAsNumber( obj, 0 ) );
}

void CCreateSinglePlayerGameDialogScaleform::UpdatedSelections( SCALEFORM_CALLBACK_ARGS_DECL ) {}
void CCreateSinglePlayerGameDialogScaleform::UpdatePendingInvites( SCALEFORM_CALLBACK_ARGS_DECL ) {}
void CCreateSinglePlayerGameDialogScaleform::ShowInviteOverlay( SCALEFORM_CALLBACK_ARGS_DECL ) {}

// queued matchmaking and the Workshop need Steam: report nothing
void CCreateSinglePlayerGameDialogScaleform::GetQueuedMatchmakingTime( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, "" );
}

void CCreateSinglePlayerGameDialogScaleform::GetQueuedMatchmakingPlayers( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, 0 );
}

void CCreateSinglePlayerGameDialogScaleform::GetQueuedMatchmakingPreferredMaplist( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, "" );
}

void CCreateSinglePlayerGameDialogScaleform::FilterWorkshopMapsByTags( SCALEFORM_CALLBACK_ARGS_DECL ) {}
void CCreateSinglePlayerGameDialogScaleform::ViewMapInWorkshop( SCALEFORM_CALLBACK_ARGS_DECL ) {}
void CCreateSinglePlayerGameDialogScaleform::ViewAllMapsInWorkshop( SCALEFORM_CALLBACK_ARGS_DECL ) {}

void CCreateSinglePlayerGameDialogScaleform::GetWorkshopMapPath( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, "" );
}

void CCreateSinglePlayerGameDialogScaleform::GetWorkshopMapID( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, "0" );
}

void CCreateSinglePlayerGameDialogScaleform::GetWorkshopMapDownloadProgress( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, 100 );
}

void CCreateSinglePlayerGameDialogScaleform::EnumerateWorkshopMapsFailed( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, false );
}

void CCreateSinglePlayerGameDialogScaleform::RefreshFileInfo( SCALEFORM_CALLBACK_ARGS_DECL ) {}

void CCreateSinglePlayerGameDialogScaleform::DownloadCurrentGamesCount( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, 0 );
}

void CCreateSinglePlayerGameDialogScaleform::QueryWorkshopMapSubscriptionsFromScript( SCALEFORM_CALLBACK_ARGS_DECL ) {}

void CCreateSinglePlayerGameDialogScaleform::ScriptGetTotalMapSubscriptions( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, 0 );
}

#endif // INCLUDE_SCALEFORM
