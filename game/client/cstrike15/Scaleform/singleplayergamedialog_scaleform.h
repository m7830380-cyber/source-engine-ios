#if defined( INCLUDE_SCALEFORM )
//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: "Offline with bots" dialog (single-player.swf). Valve's glue was
//          removed from the leaked source; this one is written against this
//          tree's Scaleform integration following the calls single-player.swf
//          makes, with the session launch from
//          github.com/Rileyboy1223/cstrike15-restoration.
//
//===========================================================================//

#ifndef SINGLEPLAYERGAMEDIALOG_SCALEFORM_H
#define SINGLEPLAYERGAMEDIALOG_SCALEFORM_H
#ifdef _WIN32
#pragma once
#endif

#include "scaleformui/scaleformui.h"

class CCreateSinglePlayerGameDialogScaleform : public ScaleformFlashInterface
{
protected:
	static CCreateSinglePlayerGameDialogScaleform *m_pInstance;

	CCreateSinglePlayerGameDialogScaleform( bool bUsingMatchmaking, bool bTeamLobbyMode, bool bTrainingMode );

public:
	static void LoadDialog( bool bUsingMatchmaking, bool bTeamLobbyMode, bool bTrainingMode );
	static void UnloadDialog();
	static bool IsActive() { return m_pInstance != NULL; }

	void Show();
	void Hide();

	// called by single-player.swf (as _global.SinglePlayerAPI)
	void OnOk( SCALEFORM_CALLBACK_ARGS_DECL );
	void CheckGameSettingsRequirements( SCALEFORM_CALLBACK_ARGS_DECL );
	void SetMatchmakingQuery( SCALEFORM_CALLBACK_ARGS_DECL );
	void SetCustomBotDifficulty( SCALEFORM_CALLBACK_ARGS_DECL );
	void UpdatedSelections( SCALEFORM_CALLBACK_ARGS_DECL );
	void UpdatePendingInvites( SCALEFORM_CALLBACK_ARGS_DECL );
	void ShowInviteOverlay( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetQueuedMatchmakingTime( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetQueuedMatchmakingPlayers( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetQueuedMatchmakingPreferredMaplist( SCALEFORM_CALLBACK_ARGS_DECL );
	void FilterWorkshopMapsByTags( SCALEFORM_CALLBACK_ARGS_DECL );
	void ViewMapInWorkshop( SCALEFORM_CALLBACK_ARGS_DECL );
	void ViewAllMapsInWorkshop( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetWorkshopMapPath( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetWorkshopMapID( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetWorkshopMapDownloadProgress( SCALEFORM_CALLBACK_ARGS_DECL );
	void EnumerateWorkshopMapsFailed( SCALEFORM_CALLBACK_ARGS_DECL );
	void RefreshFileInfo( SCALEFORM_CALLBACK_ARGS_DECL );
	void DownloadCurrentGamesCount( SCALEFORM_CALLBACK_ARGS_DECL );
	void QueryWorkshopMapSubscriptionsFromScript( SCALEFORM_CALLBACK_ARGS_DECL );
	void ScriptGetTotalMapSubscriptions( SCALEFORM_CALLBACK_ARGS_DECL );

protected:
	virtual void FlashLoaded();
	virtual void FlashReady();
	virtual void PostUnloadFlash();

	char m_szMatchmakingQuery[8192];
	bool m_bUsingMatchmaking;
	bool m_bTeamLobbyMode;
	bool m_bTrainingMode;
};

#endif // SINGLEPLAYERGAMEDIALOG_SCALEFORM_H
#endif // INCLUDE_SCALEFORM
