//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: the voice status HUD (hudvoicestatus.swf), reconstructed; its glue
// isn't in this source. The movie does two things:
//
//   - "Player talking" panels (up to 3): ShowVoiceNotice( slot, text, team,
//     alive, speakerState, xuid, name, isLocal ) / HideVoiceNotice( slot ),
//     with the player's picture from img://avatar_<xuid>
//   - the recent chat lines, shown for a while without opening the chat: the
//     movie makes a notice clip (AddPanel), fills it (SetPanelText, which
//     returns its height) and removes it (RemovePanel); positions and fading
//     are ours, with the movie's SetConfig( total height, scroll-in time,
//     fade-out time, lifetime ): 4 lines, 0.1 s, 0.5 s, 15 s
//
//=============================================================================//
#include "cbase.h"

#include "hud.h"
#include "hudelement.h"
#include "hud_element_helper.h"
#include "scaleformui/scaleformui.h"
#include "sfhudflashinterface.h"
#include "sfhud_chat.h"
#include "voice_status.h"
#include "c_playerresource.h"
#include "c_cs_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define VOICE_PANELS		3
#define MAX_NOTICES			8

class SFHudVoiceStatus : public SFHudFlashInterface
{
public:
	explicit SFHudVoiceStatus( const char *value );

	virtual void LevelInit( void );
	virtual void LevelShutdown( void );
	virtual bool ShouldDraw( void );
	virtual void ProcessInput( void );
	virtual void FlashReady( void );
	virtual bool PreUnloadFlash( void );

	void SetConfig( SCALEFORM_CALLBACK_ARGS_DECL );

	void AddNotice( const wchar_t *pwszHtml );

private:
	void UpdateSpeakers( void );
	void UpdateNotices( void );
	void RemoveNotice( int i );
	void ClearAll( void );

	// talking panels: which player is in each, and since when they're quiet
	int m_nSlotPlayer[VOICE_PANELS];
	float m_flSlotQuietSince[VOICE_PANELS];

	struct Notice_t
	{
		SFVALUE m_hClip;
		float m_flHeight;
		float m_flBorn;
	};
	Notice_t m_Notices[MAX_NOTICES];
	int m_nNotices;

	float m_flMaxHeight, m_flScrollInTime, m_flFadeOutTime, m_flLifetime;
	bool m_bNoticesHidden;
};

DECLARE_HUDELEMENT( SFHudVoiceStatus );

SFUI_BEGIN_GAME_API_DEF
	SFUI_DECL_METHOD( SetConfig ),
SFUI_END_GAME_API_DEF( SFHudVoiceStatus, HudVoiceStatus );

SFHudVoiceStatus::SFHudVoiceStatus( const char *value ) : SFHudFlashInterface( value ),
	m_nNotices( 0 ), m_flMaxHeight( 0 ), m_flScrollInTime( 0.1f ), m_flFadeOutTime( 0.5f ), m_flLifetime( 15.0f ),
	m_bNoticesHidden( false )
{
	SetHiddenBits( 0 );
	for ( int i = 0; i < VOICE_PANELS; i++ )
	{
		m_nSlotPlayer[i] = 0;
		m_flSlotQuietSince[i] = 0;
	}
}

void SFHudVoiceStatus::LevelInit( void )
{
	if ( !FlashAPIIsValid() )
		SFUI_REQUEST_ELEMENT( SF_SS_SLOT( GET_ACTIVE_SPLITSCREEN_SLOT() ), g_pScaleformUI, SFHudVoiceStatus, this, HudVoiceStatus );
}

void SFHudVoiceStatus::LevelShutdown( void )
{
	if ( FlashAPIIsValid() )
	{
		ClearAll();
		RemoveFlashElement();
	}
}

bool SFHudVoiceStatus::PreUnloadFlash( void )
{
	ClearAll();
	return true;
}

void SFHudVoiceStatus::FlashReady( void )
{
	for ( int i = 0; i < VOICE_PANELS; i++ )
		m_nSlotPlayer[i] = 0;
	m_nNotices = 0;
}

bool SFHudVoiceStatus::ShouldDraw( void )
{
	return cl_drawhud.GetBool() && CHudElement::ShouldDraw();
}

void SFHudVoiceStatus::SetConfig( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_flMaxHeight = (float)m_pScaleformUI->Params_GetArgAsNumber( obj, 0 );
	m_flScrollInTime = (float)m_pScaleformUI->Params_GetArgAsNumber( obj, 1 );
	m_flFadeOutTime = (float)m_pScaleformUI->Params_GetArgAsNumber( obj, 2 );
	m_flLifetime = (float)m_pScaleformUI->Params_GetArgAsNumber( obj, 3 );
	Msg( "[voicestatus] notices: %.0f px high, scroll in %.2f s, fade %.2f s, life %.0f s\n", m_flMaxHeight, m_flScrollInTime, m_flFadeOutTime, m_flLifetime );
}

void SFHudVoiceStatus::ClearAll( void )
{
	while ( m_nNotices > 0 )
		RemoveNotice( 0 );
	if ( FlashAPIIsValid() )
	{
		for ( int i = 0; i < VOICE_PANELS; i++ )
		{
			if ( m_nSlotPlayer[i] )
			{
				WITH_SFVALUEARRAY_SLOT_LOCKED( args, 1 )
				{
					m_pScaleformUI->ValueArray_SetElement( args, 0, i );
					m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "HideVoiceNotice", args, 1 );
				}
			}
			m_nSlotPlayer[i] = 0;
		}
	}
}

void SFHudVoiceStatus::ProcessInput( void )
{
	if ( !FlashAPIIsValid() )
		return;
	UpdateSpeakers();
	UpdateNotices();
}

// ---- talking panels -------------------------------------------------------

void SFHudVoiceStatus::UpdateSpeakers( void )
{
	CVoiceStatus *pVoiceMgr = GetClientVoiceMgr();
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( !pVoiceMgr || !pLocal || !g_PR )
		return;
	int nLocal = pLocal->entindex();
	float flNow = gpGlobals->curtime;

	// who's talking now (the local player while the mic is open)
	bool bTalking[MAX_PLAYERS + 1] = {};
	for ( int i = 1; i <= gpGlobals->maxClients && i <= MAX_PLAYERS; i++ )
	{
		if ( !g_PR->IsConnected( i ) )
			continue;
		if ( i == nLocal )
			bTalking[i] = pVoiceMgr->IsLocalPlayerSpeaking( GET_ACTIVE_SPLITSCREEN_SLOT() );
		else
			bTalking[i] = pVoiceMgr->IsPlayerSpeaking( i );
	}

	// free the panels of those who stopped (after a moment, so pauses between
	// words don't flicker)
	for ( int s = 0; s < VOICE_PANELS; s++ )
	{
		int nPlayer = m_nSlotPlayer[s];
		if ( !nPlayer )
			continue;
		if ( nPlayer <= MAX_PLAYERS && bTalking[nPlayer] )
		{
			m_flSlotQuietSince[s] = 0;
			bTalking[nPlayer] = false;	// has its panel
			continue;
		}
		if ( !m_flSlotQuietSince[s] )
			m_flSlotQuietSince[s] = flNow;
		if ( flNow - m_flSlotQuietSince[s] > 0.4f || !g_PR->IsConnected( nPlayer ) )
		{
			WITH_SFVALUEARRAY_SLOT_LOCKED( args, 1 )
			{
				m_pScaleformUI->ValueArray_SetElement( args, 0, s );
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "HideVoiceNotice", args, 1 );
			}
			m_nSlotPlayer[s] = 0;
		}
	}

	// panels for those who started
	for ( int i = 1; i <= gpGlobals->maxClients && i <= MAX_PLAYERS; i++ )
	{
		if ( !bTalking[i] )
			continue;
		int s = 0;
		while ( s < VOICE_PANELS && m_nSlotPlayer[s] )
			s++;
		if ( s == VOICE_PANELS )
			break;	// all three in use

		bool bLocal = ( i == nLocal );
		player_info_t info;
		XUID xuid = 0;
		if ( engine->GetPlayerInfo( i, &info ) )
			xuid = info.xuid;
		if ( !xuid && bLocal && steamapicontext && steamapicontext->SteamUser() )
			xuid = steamapicontext->SteamUser()->GetSteamID().ConvertToUint64();
		char szXuid[32];
		V_snprintf( szXuid, sizeof( szXuid ), "%llu", xuid );

		const char *pszName = g_PR->GetPlayerName( i );
		wchar_t wszName[MAX_PLAYER_NAME_LENGTH];
		g_pVGuiLocalize->ConvertANSIToUnicode( pszName ? pszName : "", wszName, sizeof( wszName ) );

		WITH_SFVALUEARRAY_SLOT_LOCKED( args, 8 )
		{
			m_pScaleformUI->ValueArray_SetElement( args, 0, s );
			m_pScaleformUI->ValueArray_SetElement( args, 1, wszName );
			m_pScaleformUI->ValueArray_SetElement( args, 2, g_PR->GetTeam( i ) );
			m_pScaleformUI->ValueArray_SetElement( args, 3, g_PR->IsAlive( i ) );
			m_pScaleformUI->ValueArray_SetElement( args, 4, 1 );	// speaking: the animated icon
			m_pScaleformUI->ValueArray_SetElement( args, 5, szXuid );
			m_pScaleformUI->ValueArray_SetElement( args, 6, wszName );
			m_pScaleformUI->ValueArray_SetElement( args, 7, bLocal );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "ShowVoiceNotice", args, 8 );
		}
		m_nSlotPlayer[s] = i;
		m_flSlotQuietSince[s] = 0;
	}
}

// ---- recent chat lines ----------------------------------------------------

void SFHudVoiceStatus::AddNotice( const wchar_t *pwszHtml )
{
	if ( !FlashAPIIsValid() || !pwszHtml || !pwszHtml[0] )
		return;

	if ( m_nNotices == MAX_NOTICES )
		RemoveNotice( 0 );

	WITH_SLOT_LOCKED
	{
		SFVALUE hClip = m_pScaleformUI->Value_Invoke( m_FlashAPI, "AddPanel", NULL, 0 );
		if ( !hClip )
			return;

		float flHeight = 0;
		WITH_SFVALUEARRAY( args, 2 )
		{
			m_pScaleformUI->ValueArray_SetElement( args, 0, hClip );
			m_pScaleformUI->ValueArray_SetElement( args, 1, pwszHtml );
			SFVALUE hHeight = m_pScaleformUI->Value_Invoke( m_FlashAPI, "SetPanelText", args, 2 );
			if ( hHeight )
			{
				flHeight = (float)m_pScaleformUI->Value_GetNumber( hHeight );
				m_pScaleformUI->ReleaseValue( hHeight );
			}
		}

		Notice_t &n = m_Notices[m_nNotices++];
		n.m_hClip = hClip;
		n.m_flHeight = flHeight > 0 ? flHeight : 20.0f;
		n.m_flBorn = gpGlobals->curtime;
	}
	UpdateNotices();
}

void SFHudVoiceStatus::RemoveNotice( int i )
{
	if ( i < 0 || i >= m_nNotices )
		return;
	if ( m_Notices[i].m_hClip )
	{
		if ( FlashAPIIsValid() )
		{
			WITH_SFVALUEARRAY_SLOT_LOCKED( args, 1 )
			{
				m_pScaleformUI->ValueArray_SetElement( args, 0, m_Notices[i].m_hClip );
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "RemovePanel", args, 1 );
			}
		}
		m_pScaleformUI->ReleaseValue( m_Notices[i].m_hClip );
	}
	for ( int j = i; j < m_nNotices - 1; j++ )
		m_Notices[j] = m_Notices[j + 1];
	m_nNotices--;
}

void SFHudVoiceStatus::UpdateNotices( void )
{
	float flNow = gpGlobals->curtime;

	// expired ones go
	for ( int i = m_nNotices - 1; i >= 0; i-- )
	{
		if ( flNow - m_Notices[i].m_flBorn > m_flLifetime + m_flFadeOutTime || flNow < m_Notices[i].m_flBorn )
			RemoveNotice( i );
	}
	// no more than fit: the oldest go
	float flTotal = 0;
	for ( int i = 0; i < m_nNotices; i++ )
		flTotal += m_Notices[i].m_flHeight;
	while ( m_nNotices > 1 && m_flMaxHeight > 0 && flTotal > m_flMaxHeight )
	{
		flTotal -= m_Notices[0].m_flHeight;
		RemoveNotice( 0 );
	}
	if ( !m_nNotices )
		return;

	// the full history is up while the chat is open
	SFHudChat *pChat = GET_HUDELEMENT( SFHudChat );
	bool bHide = pChat && pChat->ChatRaised();

	// newest at the bottom (the notice template's place), older ones above;
	// a new line slides in from below
	WITH_SLOT_LOCKED
	{
		float flY = 0;
		for ( int i = m_nNotices - 1; i >= 0; i-- )
		{
			Notice_t &n = m_Notices[i];
			float flAge = flNow - n.m_flBorn;
			float flSlide = ( m_flScrollInTime > 0 && flAge < m_flScrollInTime ) ? ( 1.0f - flAge / m_flScrollInTime ) * n.m_flHeight : 0;
			float flAlpha = 100.0f;
			if ( flAge > m_flLifetime )
				flAlpha = 100.0f * ( 1.0f - ( flAge - m_flLifetime ) / MAX( m_flFadeOutTime, 0.01f ) );

			ScaleformDisplayInfo dinfo;
			dinfo.SetY( -flY + flSlide );
			dinfo.SetAlpha( clamp( flAlpha, 0.0f, 100.0f ) );
			dinfo.SetVisibility( !bHide );
			m_pScaleformUI->Value_SetDisplayInfo( n.m_hClip, &dinfo );

			flY += n.m_flHeight;
		}
	}
}

// chat lines (hud_basechat.cpp, next to the chat history)
void IOS_VoiceStatusAddNotice( const wchar_t *pwszHtml )
{
	SFHudVoiceStatus *pStatus = GET_HUDELEMENT( SFHudVoiceStatus );
	if ( pStatus )
		pStatus->AddNotice( pwszHtml );
}
