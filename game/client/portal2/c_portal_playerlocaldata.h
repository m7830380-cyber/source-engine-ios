//========= Copyright (c) Valve Corporation, All rights reserved. ============//
//
// Purpose: Portal 2 player data sent only to the local player (client).
//			(Reconstructed for the iOS port: the cstrike15 tree's Portal 2
//			code uses it but does not include it.)
//
//=============================================================================//

#ifndef C_PORTAL_PLAYERLOCALDATA_H
#define C_PORTAL_PLAYERLOCALDATA_H
#ifdef _WIN32
#pragma once
#endif

#include "dt_recv.h"
#include "simtimer.h"
#include "portal_player_shared.h"

class C_Trigger_TractorBeam;

EXTERN_RECV_TABLE( DT_PortalLocal );

class C_PortalPlayerLocalData
{
public:
	DECLARE_PREDICTABLE();
	DECLARE_CLASS_NOBASE( C_PortalPlayerLocalData );
	DECLARE_EMBEDDED_NETWORKVAR();

	C_PortalPlayerLocalData();

	PaintPowerType m_PaintedPowerType;
	Vector m_StickNormal;
	Vector m_OldStickNormal;
	Vector m_Up;
	Vector m_vLocalUp;
	Vector m_vStickRotationAxis;
	Vector m_vEyeOffset;
	QAngle m_qQuaternionPunch;
	Vector m_vPreUpdateVelocity;
	Vector m_StandHullMin;
	Vector m_StandHullMax;
	Vector m_DuckHullMin;
	Vector m_DuckHullMax;
	Vector m_CachedStandHullMinAttempt;
	Vector m_CachedStandHullMaxAttempt;
	Vector m_CachedDuckHullMinAttempt;
	Vector m_CachedDuckHullMaxAttempt;
	StickCameraState m_nStickCameraState;
	InAirState m_InAirState;
	bool m_bAttemptHullResize;
	bool m_bDoneStickInterp;
	bool m_bDoneCorrectPitch;
	bool m_bJumpedThisFrame;
	bool m_bBouncedThisFrame;
	bool m_bDuckedInAir;
	bool m_bPreventedCrouchJumpThisFrame;
	float m_fBouncedTime;
	float m_flAirInputScale;
	float m_flAirControlSupressionTime;
	int m_nTractorBeamCount;
	bool m_bZoomedIn;
	bool m_bSlowingTime;
	float m_flSlowTimeRemaining;
	float m_flSlowTimeMaximum;
	bool m_bShowingViewFinder;

	CHandle< C_Trigger_TractorBeam > m_hTractorBeam;

	CountdownTimer m_PaintedPowerTimer;
	CachedPaintPowerChoiceResult m_CachedPaintPowerChoiceResults[ PAINT_POWER_TYPE_COUNT ];
};

#endif // C_PORTAL_PLAYERLOCALDATA_H
