//========= Copyright (c) Valve Corporation, All rights reserved. ============//
//
// Purpose: Portal 2 player data sent only to the local player.
//			(Reconstructed for the iOS port: the cstrike15 tree's Portal 2
//			code uses it but does not include it.)
//
//=============================================================================//

#ifndef PORTAL_PLAYERLOCALDATA_H
#define PORTAL_PLAYERLOCALDATA_H
#ifdef _WIN32
#pragma once
#endif

#include "networkvar.h"
#include "simtimer.h"
#include "portal_player_shared.h"
#include "portal2/portal2_paint_defs.h"
#include "portal2/trigger_tractorbeam.h"

class CTrigger_TractorBeam;

class CPortalPlayerLocalData
{
public:
	// Save/restore
	DECLARE_SIMPLE_DATADESC();
	DECLARE_CLASS_NOBASE( CPortalPlayerLocalData );
	DECLARE_EMBEDDED_NETWORKVAR();

	CPortalPlayerLocalData();

	// paint powers / wall stick movement (predicted)
	CNetworkVar( PaintPowerType, m_PaintedPowerType );
	CNetworkVar( Vector, m_StickNormal );
	CNetworkVar( Vector, m_OldStickNormal );
	CNetworkVar( Vector, m_Up );
	CNetworkVar( Vector, m_vLocalUp );
	CNetworkVar( Vector, m_vStickRotationAxis );
	CNetworkVar( Vector, m_vEyeOffset );
	CNetworkVar( QAngle, m_qQuaternionPunch );
	CNetworkVar( Vector, m_vPreUpdateVelocity );
	CNetworkVar( Vector, m_StandHullMin );
	CNetworkVar( Vector, m_StandHullMax );
	CNetworkVar( Vector, m_DuckHullMin );
	CNetworkVar( Vector, m_DuckHullMax );
	CNetworkVar( Vector, m_CachedStandHullMinAttempt );
	CNetworkVar( Vector, m_CachedStandHullMaxAttempt );
	CNetworkVar( Vector, m_CachedDuckHullMinAttempt );
	CNetworkVar( Vector, m_CachedDuckHullMaxAttempt );
	CNetworkVar( StickCameraState, m_nStickCameraState );
	CNetworkVar( InAirState, m_InAirState );
	CNetworkVar( bool, m_bAttemptHullResize );
	CNetworkVar( bool, m_bDoneStickInterp );
	CNetworkVar( bool, m_bDoneCorrectPitch );
	CNetworkVar( bool, m_bJumpedThisFrame );
	CNetworkVar( bool, m_bBouncedThisFrame );
	CNetworkVar( bool, m_bDuckedInAir );
	CNetworkVar( bool, m_bPreventedCrouchJumpThisFrame );
	CNetworkVar( float, m_fBouncedTime );
	CNetworkVar( float, m_flAirInputScale );
	CNetworkVar( float, m_flAirControlSupressionTime );

	// excursion funnels
	CNetworkHandle( CTriggerTractorBeam, m_hTractorBeam );
	CNetworkVar( int, m_nTractorBeamCount );

	// zoom, slow time, view finder
	CNetworkVar( bool, m_bZoomedIn );
	CNetworkVar( bool, m_bSlowingTime );
	CNetworkVar( float, m_flSlowTimeRemaining );
	CNetworkVar( float, m_flSlowTimeMaximum );
	CNetworkVar( bool, m_bShowingViewFinder );

	CountdownTimer m_PaintedPowerTimer;
	CachedPaintPowerChoiceResult m_CachedPaintPowerChoiceResults[ PAINT_POWER_TYPE_COUNT ];
};

EXTERN_SEND_TABLE( DT_PortalLocal );

#endif // PORTAL_PLAYERLOCALDATA_H
