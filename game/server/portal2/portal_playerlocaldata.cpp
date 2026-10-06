//========= Copyright (c) Valve Corporation, All rights reserved. ============//
//
// Purpose: Portal 2 player data sent only to the local player.
//			(Reconstructed for the iOS port.)
//
//=============================================================================//

#include "cbase.h"
#include "portal_playerlocaldata.h"
#include "portal_player.h"
#include "trigger_tractorbeam_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BEGIN_SEND_TABLE_NOBASE( CPortalPlayerLocalData, DT_PortalLocal )
	SendPropInt( SENDINFO( m_PaintedPowerType ), 3, SPROP_UNSIGNED ),
	SendPropVector( SENDINFO( m_StickNormal ), -1, SPROP_NORMAL ),
	SendPropVector( SENDINFO( m_OldStickNormal ), -1, SPROP_NORMAL ),
	SendPropVector( SENDINFO( m_Up ), -1, SPROP_NORMAL ),
	SendPropVector( SENDINFO( m_vLocalUp ), -1, SPROP_NORMAL ),
	SendPropVector( SENDINFO( m_vStickRotationAxis ), -1, SPROP_NORMAL ),
	SendPropVector( SENDINFO( m_vEyeOffset ), -1, SPROP_NOSCALE ),
	SendPropQAngles( SENDINFO( m_qQuaternionPunch ), -1, SPROP_NOSCALE ),
	SendPropVector( SENDINFO( m_vPreUpdateVelocity ), -1, SPROP_NOSCALE ),
	SendPropVector( SENDINFO( m_StandHullMin ), -1, SPROP_NOSCALE ),
	SendPropVector( SENDINFO( m_StandHullMax ), -1, SPROP_NOSCALE ),
	SendPropVector( SENDINFO( m_DuckHullMin ), -1, SPROP_NOSCALE ),
	SendPropVector( SENDINFO( m_DuckHullMax ), -1, SPROP_NOSCALE ),
	SendPropVector( SENDINFO( m_CachedStandHullMinAttempt ), -1, SPROP_NOSCALE ),
	SendPropVector( SENDINFO( m_CachedStandHullMaxAttempt ), -1, SPROP_NOSCALE ),
	SendPropVector( SENDINFO( m_CachedDuckHullMinAttempt ), -1, SPROP_NOSCALE ),
	SendPropVector( SENDINFO( m_CachedDuckHullMaxAttempt ), -1, SPROP_NOSCALE ),
	SendPropInt( SENDINFO( m_nStickCameraState ), 4, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_InAirState ), 3, SPROP_UNSIGNED ),
	SendPropBool( SENDINFO( m_bAttemptHullResize ) ),
	SendPropBool( SENDINFO( m_bDoneStickInterp ) ),
	SendPropBool( SENDINFO( m_bDoneCorrectPitch ) ),
	SendPropBool( SENDINFO( m_bJumpedThisFrame ) ),
	SendPropBool( SENDINFO( m_bBouncedThisFrame ) ),
	SendPropBool( SENDINFO( m_bDuckedInAir ) ),
	SendPropBool( SENDINFO( m_bPreventedCrouchJumpThisFrame ) ),
	SendPropFloat( SENDINFO( m_fBouncedTime ), -1, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flAirInputScale ), -1, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flAirControlSupressionTime ), -1, SPROP_NOSCALE ),
	SendPropEHandle( SENDINFO( m_hTractorBeam ) ),
	SendPropInt( SENDINFO( m_nTractorBeamCount ), 8, SPROP_UNSIGNED ),
	SendPropBool( SENDINFO( m_bZoomedIn ) ),
	SendPropBool( SENDINFO( m_bSlowingTime ) ),
	SendPropFloat( SENDINFO( m_flSlowTimeRemaining ), -1, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flSlowTimeMaximum ), -1, SPROP_NOSCALE ),
	SendPropBool( SENDINFO( m_bShowingViewFinder ) ),
END_SEND_TABLE()

BEGIN_SIMPLE_DATADESC( CPortalPlayerLocalData )
	DEFINE_FIELD( m_PaintedPowerType, FIELD_INTEGER ),
	DEFINE_FIELD( m_StickNormal, FIELD_VECTOR ),
	DEFINE_FIELD( m_OldStickNormal, FIELD_VECTOR ),
	DEFINE_FIELD( m_Up, FIELD_VECTOR ),
	DEFINE_FIELD( m_vLocalUp, FIELD_VECTOR ),
	DEFINE_FIELD( m_vStickRotationAxis, FIELD_VECTOR ),
	DEFINE_FIELD( m_vEyeOffset, FIELD_VECTOR ),
	DEFINE_FIELD( m_qQuaternionPunch, FIELD_VECTOR ),
	DEFINE_FIELD( m_StandHullMin, FIELD_VECTOR ),
	DEFINE_FIELD( m_StandHullMax, FIELD_VECTOR ),
	DEFINE_FIELD( m_DuckHullMin, FIELD_VECTOR ),
	DEFINE_FIELD( m_DuckHullMax, FIELD_VECTOR ),
	DEFINE_FIELD( m_nStickCameraState, FIELD_INTEGER ),
	DEFINE_FIELD( m_InAirState, FIELD_INTEGER ),
	DEFINE_FIELD( m_flAirInputScale, FIELD_FLOAT ),
	DEFINE_FIELD( m_hTractorBeam, FIELD_EHANDLE ),
	DEFINE_FIELD( m_nTractorBeamCount, FIELD_INTEGER ),
	DEFINE_FIELD( m_bZoomedIn, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bSlowingTime, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flSlowTimeRemaining, FIELD_FLOAT ),
	DEFINE_FIELD( m_flSlowTimeMaximum, FIELD_FLOAT ),
	DEFINE_FIELD( m_bShowingViewFinder, FIELD_BOOLEAN ),
END_DATADESC()

CPortalPlayerLocalData::CPortalPlayerLocalData()
{
	m_PaintedPowerType = NO_POWER;
	m_StickNormal = Vector( 0, 0, 1 );
	m_OldStickNormal = Vector( 0, 0, 1 );
	m_Up = Vector( 0, 0, 1 );
	m_vLocalUp = Vector( 0, 0, 1 );
	m_vStickRotationAxis = Vector( 0, 0, 1 );
	m_vEyeOffset = vec3_origin;
	m_qQuaternionPunch = vec3_angle;
	m_vPreUpdateVelocity = vec3_origin;
	m_StandHullMin = VEC_HULL_MIN;
	m_StandHullMax = VEC_HULL_MAX;
	m_DuckHullMin = VEC_DUCK_HULL_MIN;
	m_DuckHullMax = VEC_DUCK_HULL_MAX;
	m_CachedStandHullMinAttempt = VEC_HULL_MIN;
	m_CachedStandHullMaxAttempt = VEC_HULL_MAX;
	m_CachedDuckHullMinAttempt = VEC_DUCK_HULL_MIN;
	m_CachedDuckHullMaxAttempt = VEC_DUCK_HULL_MAX;
	m_nStickCameraState = STICK_CAMERA_UPRIGHT;
	m_InAirState = ON_GROUND;
	m_bAttemptHullResize = false;
	m_bDoneStickInterp = true;
	m_bDoneCorrectPitch = true;
	m_bJumpedThisFrame = false;
	m_bBouncedThisFrame = false;
	m_bDuckedInAir = false;
	m_bPreventedCrouchJumpThisFrame = false;
	m_fBouncedTime = 0.0f;
	m_flAirInputScale = 1.0f;
	m_flAirControlSupressionTime = 0.0f;
	m_hTractorBeam = NULL;
	m_nTractorBeamCount = 0;
	m_bZoomedIn = false;
	m_bSlowingTime = false;
	m_flSlowTimeRemaining = 0.0f;
	m_flSlowTimeMaximum = 0.0f;
	m_bShowingViewFinder = false;
	for ( int i = 0; i < PAINT_POWER_TYPE_COUNT; ++i )
	{
		m_CachedPaintPowerChoiceResults[ i ].Initialize();
	}
}
