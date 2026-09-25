#if defined( INCLUDE_SCALEFORM )
//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: CS:GO Scaleform pause menu (pausemenu.swf). Valve's glue was
//          removed from the leaked source; this one is written against this
//          tree's Scaleform integration, following the calls pausemenu.swf
//          makes (reference: github.com/Rileyboy1223/cstrike15-restoration).
//
//===========================================================================//

#ifndef PAUSEMENUSCREEN_SCALEFORM_H
#define PAUSEMENUSCREEN_SCALEFORM_H
#ifdef _WIN32
#pragma once
#endif

#include "scaleformui/scaleformui.h"

class CPauseMenuScreenScaleform : public ScaleformFlashInterface
{
protected:
	static CPauseMenuScreenScaleform *m_pInstance;

	CPauseMenuScreenScaleform();

public:
	static void LoadDialog();
	static void UnloadDialog();
	static void RestorePanel();
	static void ShowMenu( bool bShow );
	static bool IsActive() { return m_pInstance != NULL; }
	static bool IsVisible() { return m_pInstance != NULL && m_pInstance->m_bVisible; }

	void Show();
	void Hide();

	// called by pausemenu.swf (as _global.PauseMenuAPI)
	void BasePanelRunCommand( SCALEFORM_CALLBACK_ARGS_DECL );
	void IsQueuedMatchmaking( SCALEFORM_CALLBACK_ARGS_DECL );
	void IsGotvSpectating( SCALEFORM_CALLBACK_ARGS_DECL );
	void IsMultiplayer( SCALEFORM_CALLBACK_ARGS_DECL );
	void NeedsInviteFriends( SCALEFORM_CALLBACK_ARGS_DECL );
	void IsTraining( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetTeamNumber( SCALEFORM_CALLBACK_ARGS_DECL );
	void CanMakeSessionPublic( SCALEFORM_CALLBACK_ARGS_DECL );
	void SwitchTeams( SCALEFORM_CALLBACK_ARGS_DECL );
	void IsPlayingGunGameProgressive( SCALEFORM_CALLBACK_ARGS_DECL );
	void CallVote( SCALEFORM_CALLBACK_ARGS_DECL );
	void OpenPlayerDetailsPanel( SCALEFORM_CALLBACK_ARGS_DECL );
	void IsWorkshopMap( SCALEFORM_CALLBACK_ARGS_DECL );
	void ViewMapInWorkshop( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetScaleformComponentEventParamString( SCALEFORM_CALLBACK_ARGS_DECL );

protected:
	virtual void FlashReady();
	virtual void FlashLoaded();
	virtual void PostUnloadFlash();

	bool m_bVisible;
	bool m_bReady;
};

#endif // PAUSEMENUSCREEN_SCALEFORM_H
#endif // INCLUDE_SCALEFORM
