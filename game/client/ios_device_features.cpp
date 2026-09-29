//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: iOS device features, the game side (the device side is ios_device.mm):
//
//   ios_gyro / ios_gyro_sensitivity / ios_gyro_invert_pitch
//       gyro aiming, added to the view like the touch look area (off by default)
//   ios_haptics
//       shots (by how hard the gun hits), hits, damage taken (a headshot
//       harder, a Zeus as a crackle), kills, being flashed, and explosions
//       nearby (off by default); shots come from the gun's fire
//       code (IOS_HapticLocalShot), the rest from events
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
#include "c_plantedc4.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern "C" void IOS_GyroSetEnabled( int bOn );
extern "C" void IOS_GyroTakeDelta( float *pflYaw, float *pflPitch );
extern "C" void IOS_Haptic( int nKind );
extern "C" void IOS_HapticPulse( float flIntensity, float flSharpness, float flDuration );
extern "C" void IOS_HapticCrackle( float flDuration, float flIntensity );
extern "C" int IOS_ThermalState( void );
extern bool IOS_IsMenuActive();

ConVar ios_gyro( "ios_gyro", "0", FCVAR_ARCHIVE | FCVAR_RELEASE, "Aim by moving the phone (gyroscope)" );
ConVar ios_gyro_sensitivity( "ios_gyro_sensitivity", "1.0", FCVAR_ARCHIVE | FCVAR_RELEASE, "Gyro aiming sensitivity (1 = the view turns as much as the phone)", true, 0.2f, true, 4.0f );
ConVar ios_gyro_invert_pitch( "ios_gyro_invert_pitch", "0", FCVAR_ARCHIVE | FCVAR_RELEASE, "Invert gyro up/down" );
ConVar ios_haptics( "ios_haptics", "0", FCVAR_ARCHIVE | FCVAR_RELEASE, "Vibrate on shots, hits, damage taken and kills" );
ConVar ios_haptics_real( "ios_haptics_real", "0", FCVAR_ARCHIVE | FCVAR_RELEASE, "With ios_haptics: also landings, reload parts, knife inspects, bomb keys, grenade pin and throw, scope clicks" );
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
		UpdateTimedTaps( flNow );
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
			if ( nUser == nLocal && nAttacker != nLocal && !V_stricmp( event->GetString( "weapon" ), "taser" ) )
			{
				// zapped: an electric crackle
				if ( ios_haptics.GetBool() )
					IOS_HapticCrackle( 1.0f, 1.0f );
				nKind = 99;
			}
			else if ( nUser == nLocal && event->GetInt( "hitgroup" ) == HITGROUP_HEAD )
			{
				// taking a headshot: a hard, sharp jolt
				if ( ios_haptics.GetBool() )
					IOS_HapticPulse( 1.0f, 0.95f, 0.45f );
				nKind = 99;
			}
			else if ( ( nAttacker == nLocal && nUser != nLocal ) || nUser == nLocal )
				nKind = 1;		// medium: we hit someone, or got hit
		}
		else if ( !V_strcmp( pszName, "player_blind" ) )
		{
			if ( nUser == nLocal )
			{
				// flashed: a dull buzz fading out as the sight comes back
				float flBlind = event->GetFloat( "blind_duration" );
				if ( ios_haptics.GetBool() && flBlind > 0.1f )
					IOS_HapticPulse( 0.45f + 0.55f * clamp( flBlind / 3.0f, 0.0f, 1.0f ), 0.15f, MIN( flBlind, 5.0f ) );
				nKind = 99;
			}
		}

		else if ( !V_strcmp( pszName, "hegrenade_detonate" ) || !V_strcmp( pszName, "flashbang_detonate" ) ||
				  !V_strcmp( pszName, "inferno_startburn" ) )
		{
			Vector vecPos( event->GetFloat( "x" ), event->GetFloat( "y" ), event->GetFloat( "z" ) );
			float flRadius, flScale;
			if ( pszName[0] == 'h' )		{ flRadius = 1200.0f; flScale = 1.0f; }		// HE
			else if ( pszName[0] == 'f' )	{ flRadius = 900.0f; flScale = 0.55f; }		// flash: a pop
			else							{ flRadius = 600.0f; flScale = 0.45f; }		// molotov catching
			ExplosionPulse( pLocal, vecPos, flRadius, flScale, pszName );
			return;
		}
		else if ( !V_strcmp( pszName, "bomb_exploded" ) )
		{
			// no position in the event: the planted bomb is still there
			if ( g_PlantedC4s.Count() > 0 && g_PlantedC4s[0] )
				ExplosionPulse( pLocal, g_PlantedC4s[0]->GetAbsOrigin(), 3000.0f, 1.6f, pszName );
			return;
		}

		// the first few per map, so a log shows whether events arrive
		if ( m_nHapticLogs < 6 )
		{
			++m_nHapticLogs;
			Msg( "[haptics] %s userid %d attacker %d (local %d) -> %s\n", pszName, nUser, nAttacker, nLocal,
				nKind < 0 ? "not ours" : ios_haptics.GetBool() ? "vibrate" : "off (ios_haptics 0)" );
		}
		if ( nKind >= 0 && nKind < 99 && ios_haptics.GetBool() )
			IOS_Haptic( nKind );
	}

private:
	// (shots don't come from here: IOS_HapticLocalShot, from the gun's fire code)
	void ListenForEvents()
	{
		ListenForGameEvent( "player_hurt" );
		ListenForGameEvent( "player_death" );
		ListenForGameEvent( "player_blind" );
		ListenForGameEvent( "hegrenade_detonate" );
		ListenForGameEvent( "flashbang_detonate" );
		ListenForGameEvent( "inferno_startburn" );
		ListenForGameEvent( "bomb_exploded" );
	}

	// stronger, longer and duller the closer it is; nothing past flRadius.
	// flScale > 1 (the bomb) lengthens it past what 1 gives
	void ExplosionPulse( C_BasePlayer *pLocal, const Vector &vecPos, float flRadius, float flScale, const char *pszName )
	{
		if ( !pLocal->IsAlive() )
			return;
		float flDist = ( pLocal->EyePosition() - vecPos ).Length();
		float n = 1.0f - flDist / flRadius;
		bool bFelt = n > 0.0f && ios_haptics.GetBool();
		if ( m_nHapticLogs < 6 )
		{
			++m_nHapticLogs;
			Msg( "[haptics] %s %.0f units away -> %s\n", pszName, flDist, !ios_haptics.GetBool() ? "off (ios_haptics 0)" : bFelt ? "rumble" : "too far" );
		}
		if ( !bFelt )
			return;
		n = n * n;	// falls off quickly: close ones are the big ones
		float flStrength = MIN( 1.0f, n * flScale );
		IOS_HapticPulse( 0.3f + 0.7f * flStrength, 0.25f - 0.15f * flStrength, ( 0.25f + 0.75f * n ) * MAX( flScale, 0.6f ) );
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

public:
	// taps timed into a sound that has several hits in one recording (the
	// butterfly knife's handle clacks); dropped if the weapon changes
	void QueueTap( double flWhen, float flIntensity, float flSharpness, float flDuration = 0.0f )
	{
		C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
		m_hTapWeapon = pLocal ? pLocal->GetActiveWeapon() : NULL;
		TimedTap_t tap = { flWhen, flIntensity, flSharpness, flDuration };
		m_TimedTaps.AddToTail( tap );
	}

private:
	struct TimedTap_t { double m_flWhen; float m_flIntensity, m_flSharpness, m_flDuration; };
	CUtlVector< TimedTap_t > m_TimedTaps;
	CHandle< C_BaseCombatWeapon > m_hTapWeapon;

	void UpdateTimedTaps( double flNow )
	{
		if ( !m_TimedTaps.Count() )
			return;
		C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
		if ( !pLocal || !pLocal->IsAlive() || pLocal->GetActiveWeapon() != m_hTapWeapon.Get() || !ios_haptics.GetBool() )
		{
			m_TimedTaps.RemoveAll();
			return;
		}
		for ( int i = m_TimedTaps.Count() - 1; i >= 0; --i )
		{
			if ( flNow >= m_TimedTaps[i].m_flWhen )
			{
				IOS_HapticPulse( m_TimedTaps[i].m_flIntensity, m_TimedTaps[i].m_flSharpness, m_TimedTaps[i].m_flDuration );
				m_TimedTaps.Remove( i );
			}
		}
	}
};

static CIOSDeviceFeatures s_IOSDeviceFeatures;

// a shot of the local player's gun (CWeaponCSBaseGun::CSBaseGunFire), felt by
// how hard the gun hits: damage x pellets, less with a silencer. A USP-S is a
// short light tap, a Deagle a solid kick with a fading rumble, an AWP or a
// shotgun the most. Full-auto guns are cut to their fire rate so a spray
// stays separate shots. Only the first prediction of a shot: re-simulated
// commands run it again.
void IOS_HapticLocalShot( C_BasePlayer *pPlayer, float flDamage, int nBullets, bool bSilenced, float flCycleTime, bool bFullAuto )
{
	if ( !ios_haptics.GetBool() || !pPlayer || !pPlayer->IsLocalPlayer() )
		return;
	if ( prediction->InPrediction() && !prediction->IsFirstTimePredicted() )
		return;

	float flPower = flDamage * MAX( nBullets, 1 ) * ( bSilenced ? 0.55f : 1.0f );
	float n = clamp( flPower / 110.0f, 0.12f, 1.0f );		// 110: an AWP
	float flIntensity = 0.35f + 0.65f * n;
	float flSharpness = 0.75f - 0.45f * n;					// big guns: deeper
	float flDuration = 0.06f + 0.6f * n;
	if ( bFullAuto && flCycleTime > 0.0f )
		flDuration = MIN( flDuration, flCycleTime * 1.3f );
	IOS_HapticPulse( flIntensity, flSharpness, flDuration );
}

// "too real hapteekz!!!" (ios_haptics_real): landing, by fall speed (CCSPlayer::OnLand).
// A plain jump lands at ~300 u/s: a small bump; the safe limit (580) is solid;
// 1100 (fatal) the most. Only the first prediction.
void IOS_HapticLanding( C_BasePlayer *pPlayer, float flFallVelocity )
{
	if ( !ios_haptics.GetBool() || !ios_haptics_real.GetBool() || !pPlayer || !pPlayer->IsLocalPlayer() )
		return;
	if ( prediction->InPrediction() && !prediction->IsFirstTimePredicted() )
		return;
	if ( flFallVelocity < 200.0f )
		return;
	float n = clamp( ( flFallVelocity - 200.0f ) / 900.0f, 0.0f, 1.0f );
	IOS_HapticPulse( 0.3f + 0.7f * n, 0.5f - 0.35f * n, 0.04f + 0.5f * n );
}

// "too real hapteekz!!!": reload parts, from the sounds the viewmodel's
// animation plays at those frames (C_BaseViewModel::FireEvent), e.g.
// "Weapon_AK47.Clipin" when the mag goes in
// "too real hapteekz!!!": a grenade leaving the hand (CBaseCSGrenade), and the
// scope clicking in or out (CWeaponCSBaseGun zoom). Only the first prediction.
static bool IOS_TooRealFor( C_BasePlayer *pPlayer )
{
	if ( !ios_haptics.GetBool() || !ios_haptics_real.GetBool() || !pPlayer || !pPlayer->IsLocalPlayer() )
		return false;
	return !prediction->InPrediction() || prediction->IsFirstTimePredicted();
}

void IOS_HapticGrenadeThrow( C_BasePlayer *pPlayer )
{
	if ( IOS_TooRealFor( pPlayer ) )
		IOS_HapticPulse( 0.4f, 0.3f, 0.08f );		// a soft push
}

void IOS_HapticZoom( C_BasePlayer *pPlayer )
{
	if ( IOS_TooRealFor( pPlayer ) )
		IOS_HapticPulse( 0.25f, 0.9f, 0.0f );		// a small click
}

// the knife hitting a wall or anything that isn't a player (CKnife::SwingOrStab;
// hitting a player comes from player_hurt): a solid thunk, harder for a stab
void IOS_HapticKnifeWall( C_BasePlayer *pPlayer, bool bStab )
{
	if ( !ios_haptics.GetBool() || !pPlayer || !pPlayer->IsLocalPlayer() )
		return;
	if ( prediction->InPrediction() && !prediction->IsFirstTimePredicted() )
		return;
	IOS_HapticPulse( bStab ? 0.85f : 0.65f, 0.55f, bStab ? 0.12f : 0.08f );
}

// The butterfly knife's inspects and draws: each sound is one recording of
// several flips. Times (s) and relative loudness of the handle clacks, found
// as sharp high-frequency onsets in sound/weapons/bknife/*.wav.
struct KnifeClacks_t { const char *m_pszSound; int m_nCount; float m_flTime[8]; float m_flLoud[8]; float m_flSharpness; float m_flScale; };
static const KnifeClacks_t s_KnifeClacks[] =
{
	{ "ButterflyKnife.look01_a", 5, { 0.045f, 0.315f, 0.645f, 0.705f, 0.915f }, { 0.72f, 0.39f, 1.00f, 0.45f, 0.49f }, 0.95f, 1.0f },
	{ "ButterflyKnife.look01_b", 5, { 0.035f, 0.240f, 0.520f, 0.575f, 0.755f }, { 0.56f, 0.88f, 0.94f, 0.89f, 1.00f }, 0.95f, 1.0f },
	{ "ButterflyKnife.look02_a", 6, { 0.035f, 0.290f, 0.550f, 0.830f, 0.885f, 1.150f }, { 0.94f, 0.96f, 1.00f, 0.82f, 0.99f, 0.63f }, 0.95f, 1.0f },
	{ "ButterflyKnife.look02_b", 4, { 0.035f, 0.325f, 0.400f, 0.615f }, { 0.48f, 0.73f, 1.00f, 0.71f }, 0.95f, 1.0f },
	{ "ButterflyKnife.look03_a", 8, { 0.030f, 0.210f, 0.370f, 0.675f, 0.950f, 1.175f, 1.255f, 1.555f }, { 0.68f, 0.59f, 0.52f, 0.28f, 0.51f, 1.00f, 0.81f, 0.39f }, 0.95f, 1.0f },
	{ "ButterflyKnife.look03_b", 4, { 0.025f, 0.155f, 0.645f, 0.710f }, { 0.44f, 0.68f, 1.00f, 0.55f }, 0.95f, 1.0f },
	{ "ButterflyKnife.draw01", 4, { 0.245f, 0.390f, 0.575f, 0.640f }, { 0.62f, 0.43f, 1.00f, 0.50f }, 0.95f, 1.0f },
	{ "ButterflyKnife.draw02", 1, { 0.355f }, { 1.00f }, 0.95f, 1.0f },
	// the other knives with inspect sounds of their own: the blade / handle
	// knocking in the hand as it's twirled, and the Falchion's catch
	{ "KnifeFalchion.inspect", 7, { 0.300f, 0.345f, 0.455f, 0.510f, 0.640f, 0.745f, 0.925f }, { 0.45f, 0.56f, 1.00f, 0.30f, 0.74f, 0.44f, 0.72f }, 0.8f, 1.0f },
	{ "KnifeFalchion.Catch", 2, { 0.185f, 0.370f }, { 0.40f, 1.00f }, 0.5f, 1.0f },
	{ "KnifePush.LookAtStart", 3, { 0.275f, 0.350f, 0.475f }, { 0.54f, 0.45f, 1.00f }, 0.8f, 1.0f },
	{ "KnifePush.LookAtEnd", 5, { 0.205f, 0.250f, 0.335f, 0.480f, 0.605f }, { 0.36f, 0.48f, 0.40f, 1.00f, 0.70f }, 0.8f, 1.0f },
	{ "KnifeBowie.LookAtStart", 3, { 0.060f, 0.195f, 0.260f }, { 0.49f, 1.00f, 0.43f }, 0.75f, 1.0f },
	{ "KnifeBowie.LookAtEnd", 2, { 0.395f, 0.665f }, { 0.29f, 1.00f }, 0.75f, 1.0f },
	// pulling a knife out: just a little. Most knives share Weapon_Knife.Deploy
	// (the blade out at 0.075 s); the Bowie's spin is a soft whirr (0.06-0.26 s,
	// added below) between the pull and the catch
	{ "Weapon_Knife.Deploy", 1, { 0.075f }, { 1.00f }, 0.8f, 0.55f },
	{ "KnifeFalchion.draw", 1, { 0.495f }, { 1.00f }, 0.8f, 0.55f },
	{ "KnifePush.Draw", 3, { 0.390f, 0.520f, 0.595f }, { 0.60f, 1.00f, 0.95f }, 0.8f, 0.55f },
	{ "KnifeBowie.draw", 2, { 0.000f, 0.255f }, { 0.60f, 1.00f }, 0.75f, 0.55f },
};

void IOS_HapticViewModelSound( C_BasePlayer *pOwner, const char *pszSound )
{
	if ( !ios_haptics.GetBool() || !ios_haptics_real.GetBool() || !pOwner || !pOwner->IsLocalPlayer() || !pszSound )
		return;

	for ( int i = 0; i < ARRAYSIZE( s_KnifeClacks ); ++i )
	{
		if ( V_stricmp( pszSound, s_KnifeClacks[i].m_pszSound ) )
			continue;
		// short taps by how loud each hit is (metal on metal: sharp)
		double flNow = Plat_FloatTime();
		for ( int j = 0; j < s_KnifeClacks[i].m_nCount; ++j )
			s_IOSDeviceFeatures.QueueTap( flNow + s_KnifeClacks[i].m_flTime[j], ( 0.25f + 0.45f * s_KnifeClacks[i].m_flLoud[j] ) * s_KnifeClacks[i].m_flScale, s_KnifeClacks[i].m_flSharpness );
		if ( !V_stricmp( pszSound, "KnifeBowie.draw" ) )
			s_IOSDeviceFeatures.QueueTap( flNow + 0.06, 0.22f, 0.35f, 0.2f );	// the spin: a soft whirr
		return;
	}
	const char *pszPart = V_strrchr( pszSound, '.' );
	pszPart = pszPart ? pszPart + 1 : pszSound;

	if ( !V_stricmp( pszSound, "c4.keypressquiet" ) )
	{
		IOS_HapticPulse( 0.18f, 0.8f, 0.0f );		// planting: a little tick per key
		return;
	}
	if ( !V_stricmp( pszPart, "PullPin_Grenade" ) )
	{
		IOS_HapticPulse( 0.4f, 0.85f, 0.0f );		// the pin pops out
		return;
	}

	// names as the viewmodel animations play them (every v_ model checked)
	static const char *s_pszSeated[] = { "Clipin", "Lclipin", "Rclipin", "Boxin", "Coverdown" };
	static const char *s_pszChamber[] = { "Boltforward", "Slideforward", "Sliderelease", "Siderelease", "Boltrelease", "Boltpull", "Pump", "PumpForward" };
	static const char *s_pszLight[] = { "Clipout", "Cliprelease", "Boxout", "Coverup", "Chain", "Boltback", "Slideback", "Sideback", "PumpBack", "BarrelRoll" };
	for ( int i = 0; i < ARRAYSIZE( s_pszSeated ); ++i )
		if ( !V_stricmp( pszPart, s_pszSeated[i] ) ) { IOS_HapticPulse( 0.7f, 0.7f, 0.07f ); return; }		// mag / box seated: a solid click
	if ( !V_stricmp( pszPart, "Cliphit" ) ) { IOS_HapticPulse( 0.18f, 0.5f, 0.0f ); return; }				// palm slap on the mag: just a little one
	if ( !V_stricmp( pszPart, "Insertshell" ) ) { IOS_HapticPulse( 0.45f, 0.6f, 0.0f ); return; }			// a shell
	for ( int i = 0; i < ARRAYSIZE( s_pszChamber ); ++i )
		if ( !V_stricmp( pszPart, s_pszChamber[i] ) ) { IOS_HapticPulse( 0.55f, 0.85f, 0.04f ); return; }	// chambering: sharp
	for ( int i = 0; i < ARRAYSIZE( s_pszLight ); ++i )
		if ( !V_stricmp( pszPart, s_pszLight[i] ) ) { IOS_HapticPulse( 0.3f, 0.6f, 0.0f ); return; }		// mag out, bolt back, cover up: light
}

#endif // IOS
