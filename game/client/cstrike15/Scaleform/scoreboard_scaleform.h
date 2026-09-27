//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: CS:GO's Scaleform scoreboard (scoreboard.swf). The movie ships with
// the game but its C++ glue is not in the source; this is a reconstruction
// from the SWF: the gameAPI methods it calls, and the row/header clips it
// expects the game to fill in.
//
//=============================================================================//

#if defined( INCLUDE_SCALEFORM )

#if !defined( __SCOREBOARD_SCALEFORM_H__ )
#define __SCOREBOARD_SCALEFORM_H__

#include "../VGUI/counterstrikeviewport.h"
#include "GameEventListener.h"

class CCSScoreboardScaleform : public ScaleformFlashInterface, public IViewPortPanel, public CGameEventListener
{
public:
	explicit CCSScoreboardScaleform( CounterStrikeViewport *pViewPort );
	virtual ~CCSScoreboardScaleform();

	/************************************
	 * gameAPI methods the movie calls
	 */

	void IsQueuedMatchmaking( SCALEFORM_CALLBACK_ARGS_DECL );
	void IsLocalPlayerHLTV( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetGamePhase( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetGameType( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetGameMode( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetCurrentRound( SCALEFORM_CALLBACK_ARGS_DECL );
	void AreTeamsPlayingSwitchedSides( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetMouseEnableBindingName( SCALEFORM_CALLBACK_ARGS_DECL );
	void SelectPlayerRow( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetSelectedPlayerXuid( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetSelectedPlayerIndex( SCALEFORM_CALLBACK_ARGS_DECL );
	void ReturnFalse( SCALEFORM_CALLBACK_ARGS_DECL );
	void ReturnZero( SCALEFORM_CALLBACK_ARGS_DECL );
	void ReturnEmpty( SCALEFORM_CALLBACK_ARGS_DECL );
	void DoNothing( SCALEFORM_CALLBACK_ARGS_DECL );

	/************************************************************
	 *  Flash Interface methods
	 */

	virtual void FlashReady( void );
	virtual bool PreUnloadFlash( void );

	/*************************************************************
	 * IViewPortPanel interface
	 */

	virtual const char *GetName( void ) { return PANEL_SCOREBOARD; }
	virtual void SetData( KeyValues *data ) {}
	virtual void Reset( void ) {}
	virtual void Update( void ) {}
	virtual bool NeedsUpdate( void ) { return false; }
	// no input: it is shown while the scoreboard key/button is held, over the game
	virtual bool HasInputElements( void ) { return false; }
	virtual void ReloadScheme( void ) {}
	virtual bool CanReplace( const char *panelName ) const { return true; }
	virtual bool CanBeReopened( void ) const { return true; }
	virtual void ViewportThink( void );
	virtual void ShowPanel( bool bShow );
	virtual vgui::VPANEL GetVPanel( void ) { return 0; }
	virtual bool IsVisible( void ) { return m_bVisible; }
	virtual void SetParent( vgui::VPANEL parent ) {}
	virtual bool WantsBackgroundBlurred( void ) { return false; }

	/********************************************
	 * CGameEventListener methods
	 */

	virtual void FireGameEvent( IGameEvent *event );

private:
	void Show( void );
	void Hide( void );
	void UpdateData( void );
	void UpdateHeader( void );
	void UpdateTeam( int nTeam );

	// member lookups by dotted path ("ScoreBoard.InnerScoreBoard.MapName"); caller releases
	SFVALUE GetPath( SFVALUE root, const char *pszPath );
	void SetText( SFVALUE root, const char *pszPath, const wchar_t *pszText );
	void SetText( SFVALUE root, const char *pszPath, const char *pszText );
	void SetVisible( SFVALUE root, const char *pszPath, bool bVisible );

	int m_iSplitScreenSlot;
	bool m_bVisible;
	bool m_bLoading;
	float m_flNextUpdate;
	int m_nSelectedIndex;
	XUID m_SelectedXuid;
	XUID m_RowXuids[2][12];		// per team (T, CT) and row, what the row's avatar shows
};

#endif // __SCOREBOARD_SCALEFORM_H__

#endif // INCLUDE_SCALEFORM
