//========= Copyright (c) Valve Corporation, All rights reserved. ============//
//
// Purpose: Portal 2 player data sent only to the local player (client).
//			(Reconstructed for the iOS port.)
//
//=============================================================================//

#include "cbase.h"
#include "c_portal_playerlocaldata.h"
#include "c_portal_player.h"
#include "c_trigger_tractorbeam.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

BEGIN_RECV_TABLE_NOBASE( C_PortalPlayerLocalData, DT_PortalLocal )
	RecvPropInt( RECVINFO( m_PaintedPowerType ) ),
	RecvPropVector( RECVINFO( m_StickNormal ) ),
	RecvPropVector( RECVINFO( m_OldStickNormal ) ),
	RecvPropVector( RECVINFO( m_Up ) ),
	RecvPropVector( RECVINFO( m_vLocalUp ) ),
	RecvPropVector( RECVINFO( m_vStickRotationAxis ) ),
	RecvPropVector( RECVINFO( m_vEyeOffset ) ),
	RecvPropQAngles( RECVINFO( m_qQuaternionPunch ) ),
	RecvPropVector( RECVINFO( m_vPreUpdateVelocity ) ),
	RecvPropVector( RECVINFO( m_StandHullMin ) ),
	RecvPropVector( RECVINFO( m_StandHullMax ) ),
	RecvPropVector( RECVINFO( m_DuckHullMin ) ),
	RecvPropVector( RECVINFO( m_DuckHullMax ) ),
	RecvPropVector( RECVINFO( m_CachedStandHullMinAttempt ) ),
	RecvPropVector( RECVINFO( m_CachedStandHullMaxAttempt ) ),
	RecvPropVector( RECVINFO( m_CachedDuckHullMinAttempt ) ),
	RecvPropVector( RECVINFO( m_CachedDuckHullMaxAttempt ) ),
	RecvPropInt( RECVINFO( m_nStickCameraState ) ),
	RecvPropInt( RECVINFO( m_InAirState ) ),
	RecvPropBool( RECVINFO( m_bAttemptHullResize ) ),
	RecvPropBool( RECVINFO( m_bDoneStickInterp ) ),
	RecvPropBool( RECVINFO( m_bDoneCorrectPitch ) ),
	RecvPropBool( RECVINFO( m_bJumpedThisFrame ) ),
	RecvPropBool( RECVINFO( m_bBouncedThisFrame ) ),
	RecvPropBool( RECVINFO( m_bDuckedInAir ) ),
	RecvPropBool( RECVINFO( m_bPreventedCrouchJumpThisFrame ) ),
	RecvPropFloat( RECVINFO( m_fBouncedTime ) ),
	RecvPropFloat( RECVINFO( m_flAirInputScale ) ),
	RecvPropFloat( RECVINFO( m_flAirControlSupressionTime ) ),
	RecvPropEHandle( RECVINFO( m_hTractorBeam ) ),
	RecvPropInt( RECVINFO( m_nTractorBeamCount ) ),
	RecvPropBool( RECVINFO( m_bZoomedIn ) ),
	RecvPropBool( RECVINFO( m_bSlowingTime ) ),
	RecvPropFloat( RECVINFO( m_flSlowTimeRemaining ) ),
	RecvPropFloat( RECVINFO( m_flSlowTimeMaximum ) ),
	RecvPropBool( RECVINFO( m_bShowingViewFinder ) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA_NO_BASE( C_PortalPlayerLocalData )
	DEFINE_PRED_FIELD( m_PaintedPowerType, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_StickNormal, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_OldStickNormal, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_Up, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_vLocalUp, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_vStickRotationAxis, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_vEyeOffset, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_qQuaternionPunch, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_vPreUpdateVelocity, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_StandHullMin, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_StandHullMax, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_DuckHullMin, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_DuckHullMax, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_CachedStandHullMinAttempt, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_CachedStandHullMaxAttempt, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_CachedDuckHullMinAttempt, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_CachedDuckHullMaxAttempt, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nStickCameraState, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_InAirState, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bAttemptHullResize, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bDoneStickInterp, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bDoneCorrectPitch, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bJumpedThisFrame, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bBouncedThisFrame, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bDuckedInAir, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bPreventedCrouchJumpThisFrame, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_fBouncedTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flAirInputScale, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flAirControlSupressionTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_hTractorBeam, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nTractorBeamCount, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()

C_PortalPlayerLocalData::C_PortalPlayerLocalData()
{
	m_PaintedPowerType = NO_POWER;
	m_StickNormal.Init( 0, 0, 1 );
	m_OldStickNormal.Init( 0, 0, 1 );
	m_Up.Init( 0, 0, 1 );
	m_vLocalUp.Init( 0, 0, 1 );
	m_vStickRotationAxis.Init( 0, 0, 1 );
	m_vEyeOffset.Init();
	m_qQuaternionPunch.Init();
	m_vPreUpdateVelocity.Init();
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
