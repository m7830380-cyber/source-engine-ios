//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: iOS device features, the game side (the device side is ios_device.mm):
//
//   ios_gyro / ios_gyro_sensitivity / ios_gyro_invert_pitch
//       gyro aiming, added to the view like the touch look area (off by default)
//   ios_haptics
//       taps on shots, hits, damage taken and kills (off by default); shots
//       come from the gun's fire code (IOS_HapticLocalShot), the rest from events
//   ios_thermal_scale
//       as the phone heats up, render the 3D view at a lower resolution
//       (mat_viewportscale; the HUD stays sharp) instead of losing frame rate
//   the shader list (mat_save_glshaders) is saved every couple of minutes in game
//
// All settings are in Settings > Game (options_scaleform.cpp adds them).
//
//=============================================================================//
#include "cbase.h"

#if defined( IOS )

#include "igamesystem.h"
#include "GameEventListener.h"
#include "c_baseplayer.h"
#include "prediction.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern "C" void IOS_GyroSetEnabled( int bOn );
extern "C" void IOS_GyroTakeDelta( float *pflYaw, float *pflPitch );
extern "C" void IOS_Haptic( int nKind );
extern "C" int IOS_ThermalState( void );
extern bool IOS_IsMenuActive();

ConVar ios_gyro( "ios_gyro", "0", FCVAR_ARCHIVE | FCVAR_RELEASE, "Aim by moving the phone (gyroscope)" );
ConVar ios_gyro_sensitivity( "ios_gyro_sensitivity", "1.0", FCVAR_ARCHIVE | FCVAR_RELEASE, "Gyro aiming sensitivity (1 = the view turns as much as the phone)", true, 0.2f, true, 4.0f );
ConVar ios_gyro_invert_pitch( "ios_gyro_invert_pitch", "0", FCVAR_ARCHIVE | FCVAR_RELEASE, "Invert gyro up/down" );
ConVar ios_haptics( "ios_haptics", "0", FCVAR_ARCHIVE | FCVAR_RELEASE, "Vibrate on shots, hits, damage taken and kills" );
ConVar ios_thermal_scale( "ios_thermal_scale", "1", FCVAR_ARCHIVE | FCVAR_RELEASE, "Lower the 3D resolution as the phone heats up, instead of the frame rate" );

class CIOSDeviceFeatures : public CAutoGameSystemPerFrame, public CGameEventListener
{
public:
	CIOSDeviceFeatures() : CAutoGameSystemPerFrame( "CIOSDeviceFeatures" ),
		m_bGyroOn( false ), m_flNextThermal( 0.0 ), m_nThermalLevel( 0 ), m_nPendingLevel( 0 ),
		m_flPendingSince( 0.0 ), m_flAppliedScale( 1.0f ), m_flNextShaderSave( 0.0 ), m_nHapticLogs( 0 ) {}

	virtual bool Init()
	{
		ListenForEvents();
		return true;
	}

	// again on every map: registering at client init can come before the
	// event list is loaded (AddListener then fails with "unknown")
	virtual void LevelInitPostEntity()
	{
		ListenForEvents();
		m_nHapticLogs = 0;
	}

	virtual void Update( float frametime )
	{
		double flNow = Plat_FloatTime();
		UpdateGyro();
		if ( flNow >= m_flNextThermal )
		{
			m_flNextThermal = flNow + 1.0;
			UpdateThermalScale( flNow );
		}
		UpdateShaderSave( flNow );
	}

	virtual void FireGameEvent( IGameEvent *event )
	{
		C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
		if ( !pLocal )
			return;
		int nLocal = pLocal->GetUserID();
		const char *pszName = event->GetName();
		int nUser = event->GetInt( "userid" ), nAttacker = event->GetInt( "attacker" );

		int nKind = -1;
		if ( !V_strcmp( pszName, "player_death" ) )
		{
			if ( nAttacker == nLocal && nUser != nLocal )
				nKind = 2;		// heavy: a kill
		}
		else if ( !V_strcmp( pszName, "player_hurt" ) )
		{
			if ( ( nAttacker == nLocal && nUser != nLocal ) || nUser == nLocal )
				nKind = 1;		// medium: we hit someone, or got hit
		}

		// the first few per map, so a log shows whether events arrive
		if ( m_nHapticLogs < 6 )
		{
			++m_nHapticLogs;
			Msg( "[haptics] %s userid %d attacker %d (local %d) -> %s\n", pszName, nUser, nAttacker, nLocal,
				nKind < 0 ? "not ours" : ios_haptics.GetBool() ? "vibrate" : "off (ios_haptics 0)" );
		}
		if ( nKind >= 0 && ios_haptics.GetBool() )
			IOS_Haptic( nKind );
	}

private:
	// (shots don't come from here: IOS_HapticLocalShot, from the gun's fire code)
	void ListenForEvents()
	{
		ListenForGameEvent( "player_hurt" );
		ListenForGameEvent( "player_death" );
	}

	void UpdateGyro()
	{
		bool bWant = ios_gyro.GetBool() && engine->IsInGame() && !IOS_IsMenuActive();
		if ( bWant != m_bGyroOn )
		{
			IOS_GyroSetEnabled( bWant ? 1 : 0 );
			m_bGyroOn = bWant;
		}

		// always take the delta, so nothing piles up while it isn't applied
		float flYaw = 0.0f, flPitch = 0.0f;
		IOS_GyroTakeDelta( &flYaw, &flPitch );
		if ( !bWant || ( flYaw == 0.0f && flPitch == 0.0f ) )
			return;

		C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
		if ( !pLocal )
			return;

		float flSens = ios_gyro_sensitivity.GetFloat();
		// zoomed in (scopes): turn less, like mouse sensitivity does
		float flFOV = pLocal->GetFOV(), flDefault = (float)pLocal->GetDefaultFOV();
		if ( flFOV > 0.0f && flDefault > 0.0f && flFOV < flDefault )
			flSens *= flFOV / flDefault;

		QAngle va;
		engine->GetViewAngles( va );
		va.y += flYaw * flSens;
		va.x += flPitch * flSens * ( ios_gyro_invert_pitch.GetBool() ? -1.0f : 1.0f );
		va.x = clamp( va.x, -89.0f, 89.0f );
		engine->SetViewAngles( va );
	}

	// thermal state -> 3D scale: nominal/fair full, serious 80%, critical 65%.
	// Scale down once the state has lasted 3 s, back up after 30 s cooler.
	void UpdateThermalScale( double flNow )
	{
		int nLevel = ios_thermal_scale.GetBool() ? IOS_ThermalState() : 0;
		if ( nLevel != m_nPendingLevel )
		{
			m_nPendingLevel = nLevel;
			m_flPendingSince = flNow;
		}
		double flHold = ( m_nPendingLevel > m_nThermalLevel ) ? 3.0 : 30.0;
		if ( !ios_thermal_scale.GetBool() )
			flHold = 0.0;
		if ( m_nPendingLevel != m_nThermalLevel && flNow - m_flPendingSince >= flHold )
		{
			m_nThermalLevel = m_nPendingLevel;
			static const char *s_pszStates[] = { "nominal", "fair", "serious", "critical" };
			Msg( "[thermal] %s: 3D resolution %d%%\n", s_pszStates[ clamp( m_nThermalLevel, 0, 3 ) ], (int)( ScaleForLevel( m_nThermalLevel ) * 100.0f ) );
		}

		// (re)apply: cheat convars get reset on connect / map load
		static ConVarRef mat_viewportscale( "mat_viewportscale" );
		float flScale = ScaleForLevel( m_nThermalLevel );
		if ( mat_viewportscale.IsValid() && ( mat_viewportscale.GetFloat() != flScale || m_flAppliedScale != flScale ) )
		{
			// only touch it when we set it before or need it lowered, so a manual
			// mat_viewportscale isn't overridden while ios_thermal_scale keeps it at 1
			if ( flScale < 1.0f || m_flAppliedScale < 1.0f )
				mat_viewportscale.SetValue( flScale );
			m_flAppliedScale = flScale;
		}
	}

	static float ScaleForLevel( int nLevel )
	{
		return nLevel >= 3 ? 0.65f : nLevel == 2 ? 0.8f : 1.0f;
	}

	void UpdateShaderSave( double flNow )
	{
		if ( !engine->IsInGame() )
		{
			m_flNextShaderSave = 0.0;
			return;
		}
		// the first save after a minute of play, then every two minutes
		if ( m_flNextShaderSave == 0.0 )
			m_flNextShaderSave = flNow + 60.0;
		if ( flNow >= m_flNextShaderSave )
		{
			m_flNextShaderSave = flNow + 120.0;
			engine->ClientCmd_Unrestricted( "mat_save_glshaders\n" );
		}
	}

	bool m_bGyroOn;
	double m_flNextThermal;
	int m_nThermalLevel, m_nPendingLevel;
	double m_flPendingSince;
	float m_flAppliedScale;
	double m_flNextShaderSave;
	int m_nHapticLogs;
};

static CIOSDeviceFeatures s_IOSDeviceFeatures;

// light tap per shot of the local player's gun (CWeaponCSBaseGun::CSBaseGunFire).
// Only the first prediction of a shot: re-simulated commands run it again.
void IOS_HapticLocalShot( C_BasePlayer *pPlayer )
{
	if ( !ios_haptics.GetBool() || !pPlayer || !pPlayer->IsLocalPlayer() )
		return;
	if ( prediction->InPrediction() && !prediction->IsFirstTimePredicted() )
		return;
	IOS_Haptic( 0 );
}

#endif // IOS
