//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: CS:GO Scaleform pause menu (pausemenu.swf). Valve's glue was
//          removed from the leaked source; this one is written against this
//          tree's Scaleform integration, following the calls pausemenu.swf
//          makes (reference: github.com/Rileyboy1223/cstrike15-restoration).
//
//===========================================================================//
#include "cbase.h"

#if defined( INCLUDE_SCALEFORM )
#include "basepanel.h"
#include "pausemenuscreen_scaleform.h"
#include "c_cs_player.h"
#include "cs_gamerules.h"
#include "gameui_interface.h"

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

CPauseMenuScreenScaleform *CPauseMenuScreenScaleform::m_pInstance = NULL;

SFUI_BEGIN_GAME_API_DEF
	SFUI_DECL_METHOD( BasePanelRunCommand ),
	SFUI_DECL_METHOD( IsQueuedMatchmaking ),
	SFUI_DECL_METHOD( IsGotvSpectating ),
	SFUI_DECL_METHOD( IsMultiplayer ),
	SFUI_DECL_METHOD( NeedsInviteFriends ),
	SFUI_DECL_METHOD( IsTraining ),
	SFUI_DECL_METHOD( GetTeamNumber ),
	SFUI_DECL_METHOD( CanMakeSessionPublic ),
	SFUI_DECL_METHOD( SwitchTeams ),
	SFUI_DECL_METHOD( IsPlayingGunGameProgressive ),
	SFUI_DECL_METHOD( CallVote ),
	SFUI_DECL_METHOD( OpenPlayerDetailsPanel ),
	SFUI_DECL_METHOD( IsWorkshopMap ),
	SFUI_DECL_METHOD( ViewMapInWorkshop ),
	SFUI_DECL_METHOD( GetScaleformComponentEventParamString ),
SFUI_END_GAME_API_DEF( CPauseMenuScreenScaleform, PauseMenu );

CPauseMenuScreenScaleform::CPauseMenuScreenScaleform()
{
	m_bVisible = true;
	m_bReady = false;
}

void CPauseMenuScreenScaleform::LoadDialog()
{
	if ( !m_pInstance )
	{
		m_pInstance = new CPauseMenuScreenScaleform();
		SFUI_REQUEST_ELEMENT( SF_FULL_SCREEN_SLOT, g_pScaleformUI, CPauseMenuScreenScaleform, m_pInstance, PauseMenu );
		printf( "[pausemenu] load\n" );
		fflush( stdout );
	}
	else
	{
		ShowMenu( true );
	}
}

void CPauseMenuScreenScaleform::UnloadDialog()
{
	if ( m_pInstance )
	{
		// hidePanel runs the movie's onHide, which pops its navigation layout
		// (the one that shows the cursor and blocks game input); unloading
		// without it left the cursor unlocked after Resume
		if ( m_pInstance->m_bReady && m_pInstance->m_bVisible )
			m_pInstance->Hide();
		m_pInstance->m_bVisible = false;
		m_pInstance->RemoveFlashElement();
	}
}

void CPauseMenuScreenScaleform::RestorePanel()
{
	// back from a dialog opened from the menu (options, how to play...)
	if ( m_pInstance )
		m_pInstance->Show();
}

void CPauseMenuScreenScaleform::ShowMenu( bool bShow )
{
	if ( !m_pInstance )
	{
		if ( bShow )
			LoadDialog();
		return;
	}

	if ( bShow )
		m_pInstance->Show();
	else
		m_pInstance->Hide();
}

void CPauseMenuScreenScaleform::FlashLoaded()
{
}

void CPauseMenuScreenScaleform::FlashReady()
{
	m_bReady = true;
	if ( m_bVisible )
		Show();
	else
		Hide();
}

void CPauseMenuScreenScaleform::PostUnloadFlash()
{
	printf( "[pausemenu] unloaded\n" );
	fflush( stdout );
	m_pInstance = NULL;
	delete this;
}

void CPauseMenuScreenScaleform::Show()
{
	m_bVisible = true;
	if ( !m_bReady )
		return;
	WITH_SLOT_LOCKED
	{
		ScaleformUI()->Value_InvokeWithoutReturn( m_FlashAPI, "showPanel", NULL, 0 );
		// SWF may skip ShowCursor/AddInputConsumer on the second open; force the cursor visible from C++
		ScaleformUI()->ShowCursor();
	}
}

void CPauseMenuScreenScaleform::Hide()
{
	m_bVisible = false;
	if ( !m_bReady )
		return;
	WITH_SLOT_LOCKED
	{
		ScaleformUI()->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanel", NULL, 0 );
		ScaleformUI()->HideCursor();
	}
}

// BasePanelRunCommand( command [, "bCloseMenu" | "bHideMenu" | "bReturnToMPGameDialog"] ):
// the buttons' commands go to the base panel like the other menus' do
void CPauseMenuScreenScaleform::BasePanelRunCommand( SCALEFORM_CALLBACK_ARGS_DECL )
{
	char szCommand[1024];
	V_strncpy( szCommand, pui->Params_GetArgAsString( obj, 0 ), sizeof( szCommand ) );

	printf( "[pausemenu] command %s\n", szCommand );
	fflush( stdout );

	if ( pui->Params_GetNumArgs( obj ) >= 2 )
	{
		const char *pszFlag = pui->Params_GetArgAsString( obj, 1 );
		if ( !V_stricmp( pszFlag, "bCloseMenu" ) )
			UnloadDialog();
		else if ( !V_stricmp( pszFlag, "bHideMenu" ) )
			Hide();
	}

	char szSlot[2] = { (char)( '0' + GET_ACTIVE_SPLITSCREEN_SLOT() ), 0 };
	BasePanel()->PostMessage( BasePanel(), new KeyValues( "RunSlottedMenuCommand", "slot", szSlot, "command", szCommand ) );
}

void CPauseMenuScreenScaleform::IsQueuedMatchmaking( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, CSGameRules() && CSGameRules()->IsQueuedMatchmaking() );
}

void CPauseMenuScreenScaleform::IsGotvSpectating( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, engine->IsHLTV() || engine->IsPlayingDemo() );
}

void CPauseMenuScreenScaleform::IsMultiplayer( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, gpGlobals->maxClients > 1 );
}

void CPauseMenuScreenScaleform::NeedsInviteFriends( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, false );
}

void CPauseMenuScreenScaleform::IsTraining( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, CSGameRules() && CSGameRules()->IsPlayingTraining() );
}

void CPauseMenuScreenScaleform::GetTeamNumber( SCALEFORM_CALLBACK_ARGS_DECL )
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	m_pScaleformUI->Params_SetResult( obj, pPlayer ? pPlayer->GetTeamNumber() : 0 );
}

void CPauseMenuScreenScaleform::CanMakeSessionPublic( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, false );
}

void CPauseMenuScreenScaleform::SwitchTeams( SCALEFORM_CALLBACK_ARGS_DECL )
{
	UnloadDialog();
	engine->ClientCmd_Unrestricted( "gameui_hide" );
	engine->ClientCmd_Unrestricted( "teammenu" );
}

void CPauseMenuScreenScaleform::IsPlayingGunGameProgressive( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, CSGameRules() && CSGameRules()->IsPlayingGunGameProgressive() );
}

void CPauseMenuScreenScaleform::CallVote( SCALEFORM_CALLBACK_ARGS_DECL )
{
}

void CPauseMenuScreenScaleform::OpenPlayerDetailsPanel( SCALEFORM_CALLBACK_ARGS_DECL )
{
}

void CPauseMenuScreenScaleform::IsWorkshopMap( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, false );
}

void CPauseMenuScreenScaleform::ViewMapInWorkshop( SCALEFORM_CALLBACK_ARGS_DECL )
{
}

void CPauseMenuScreenScaleform::GetScaleformComponentEventParamString( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, "" );
}

#endif // INCLUDE_SCALEFORM
