//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: iOS touch input for inputsystem. SDL finger events become
//          IE_FingerDown/Up/Motion input events, which the engine forwards
//          to the client's touch controls (game/client/touch.cpp).
//          Based on the source-engine port's inputsystem/touch_sdl.cpp.
//
//===========================================================================//

#include "../../inputsystem/inputsystem.h"
#include "tier0/icommandline.h"
#include "SDL.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

#define TOUCH_FINGER_MAX_COUNT 10

struct FingerSlot_t
{
	SDL_FingerID id;
	bool active;
};

static FingerSlot_t s_Fingers[TOUCH_FINGER_MAX_COUNT];

static int TouchSDLWatcher( void *userInfo, SDL_Event *event )
{
	CInputSystem *pInputSystem = (CInputSystem *)userInfo;
	if ( !event || !pInputSystem )
		return 1;

	switch ( event->type )
	{
	case SDL_FINGERDOWN:
		for ( int i = 0; i < TOUCH_FINGER_MAX_COUNT; ++i )
		{
			if ( !s_Fingers[i].active )
			{
				s_Fingers[i].id = event->tfinger.fingerId;
				s_Fingers[i].active = true;
				pInputSystem->FingerEvent( IE_FingerDown, i, event->tfinger.x, event->tfinger.y );
				break;
			}
		}
		break;

	case SDL_FINGERUP:
		for ( int i = 0; i < TOUCH_FINGER_MAX_COUNT; ++i )
		{
			if ( s_Fingers[i].active && s_Fingers[i].id == event->tfinger.fingerId )
			{
				s_Fingers[i].active = false;
				pInputSystem->FingerEvent( IE_FingerUp, i, event->tfinger.x, event->tfinger.y );
				break;
			}
		}
		break;

	case SDL_FINGERMOTION:
		for ( int i = 0; i < TOUCH_FINGER_MAX_COUNT; ++i )
		{
			if ( s_Fingers[i].active && s_Fingers[i].id == event->tfinger.fingerId )
			{
				pInputSystem->FingerEvent( IE_FingerMotion, i, event->tfinger.x, event->tfinger.y );
				break;
			}
		}
		break;
	}

	return 1;
}

void CInputSystem::InitializeTouch( void )
{
	if ( m_bTouchInitialized )
		ShutdownTouch();

	if ( CommandLine()->FindParm( "-notouch" ) )
		return;

	// Touches are handled as touches; SDL would otherwise also turn every
	// finger into mouse clicks and mouse-look.
	SDL_SetHint( SDL_HINT_TOUCH_MOUSE_EVENTS, "0" );

	memset( s_Fingers, 0, sizeof( s_Fingers ) );
	SDL_AddEventWatch( TouchSDLWatcher, this );
	m_bTouchInitialized = true;
}

void CInputSystem::ShutdownTouch( void )
{
	if ( !m_bTouchInitialized )
		return;

	SDL_DelEventWatch( TouchSDLWatcher, this );
	m_bTouchInitialized = false;
}

void CInputSystem::FingerEvent( int eventType, int fingerId, float x, float y )
{
	if ( fingerId < 0 || fingerId >= TOUCH_FINGER_MAX_COUNT )
		return;

	int nX, nY;
	memcpy( &nX, &x, sizeof( float ) );
	memcpy( &nY, &y, sizeof( float ) );
	PostEvent( eventType, m_nLastSampleTick, fingerId, nX, nY );
}
