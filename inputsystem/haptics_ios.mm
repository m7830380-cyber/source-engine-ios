//========= Source Engine iOS modernization ==============================//
//
// Purpose: CoreHaptics implementation of the haptics backend.
//
//	Only compiled when the --haptics waf flag is set (SRC_HAPTICS).
//
//	Design notes:
//	 - CoreHaptics is iOS 13+. We check CHHapticEngine availability at
//	   runtime rather than assuming, because the engine also targets
//	   devices and simulators with no Taptic Engine, where
//	   supportsHaptics is NO and creating an engine throws.
//	 - Every ObjC call that can throw is wrapped. CoreHaptics reports
//	   failure through NSError, and an uncaught raise here would take
//	   down the game on a device that merely lacks hardware.
//	 - The engine stops itself when the app is backgrounded; we register
//	   the reset and stopped handlers so we transparently recover instead
//	   of silently losing haptics for the rest of the session.
//
//=======================================================================//

#include "haptics_ios.h"

#if defined( SRC_HAPTICS )

#import <Foundation/Foundation.h>
#import <CoreHaptics/CoreHaptics.h>

#include "tier0/dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CHHapticEngine        *g_pHapticEngine   = nil;
static id<CHHapticPatternPlayer> g_pRumblePlayer = nil;
static bool                   g_bHapticsActive  = false;
static float                  g_flLastIntensity = 0.0f;
static float                  g_flLastSharpness = 0.0f;
static bool                   g_bPlaying        = false;
// When the current continuous event is due to expire. CoreHaptics events
// have a finite duration, so a long rumble would otherwise fall silent.
static NSTimeInterval         g_flPlaybackEndsAt = 0.0;

// Below this level we treat the request as "off" and stop the player
// instead of playing an imperceptible buzz.
static const float kHapticEpsilon = 0.01f;

// CoreHaptics requires a finite duration for a continuous event. We use a
// long one and keep retuning it, restarting only if playback ever lapses;
// this is the documented pattern for an open-ended rumble.
static const NSTimeInterval kHapticContinuousDuration = 30.0;

//-----------------------------------------------------------------------------
// Build the continuous pattern we retune at runtime.
//-----------------------------------------------------------------------------
static id<CHHapticPatternPlayer> Haptics_CreateContinuousPlayer( void )
{
	if ( g_pHapticEngine == nil )
		return nil;

	NSError *pError = nil;

	CHHapticEventParameter *pIntensity =
		[[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticIntensity
													  value:0.0f];
	CHHapticEventParameter *pSharpness =
		[[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticSharpness
													  value:0.0f];

	// A very long continuous event that we keep alive and modulate. This is
	// the documented way to emulate a variable-strength rumble motor.
	CHHapticEvent *pEvent =
		[[CHHapticEvent alloc] initWithEventType:CHHapticEventTypeHapticContinuous
									  parameters:@[ pIntensity, pSharpness ]
									relativeTime:0.0
										duration:kHapticContinuousDuration];

	CHHapticPattern *pPattern =
		[[CHHapticPattern alloc] initWithEvents:@[ pEvent ] parameters:@[] error:&pError];
	if ( pPattern == nil || pError != nil )
	{
		Warning( "Haptics: failed to build pattern (%s)\n",
			pError ? [[pError localizedDescription] UTF8String] : "unknown" );
		return nil;
	}

	id<CHHapticPatternPlayer> pPlayer = [g_pHapticEngine createPlayerWithPattern:pPattern error:&pError];
	if ( pPlayer == nil || pError != nil )
	{
		Warning( "Haptics: failed to create player (%s)\n",
			pError ? [[pError localizedDescription] UTF8String] : "unknown" );
		return nil;
	}

	return pPlayer;
}

//-----------------------------------------------------------------------------
bool Haptics_Init( void )
{
	if ( g_bHapticsActive )
		return true;

	// Devices without a Taptic Engine (and the simulator) report NO here.
	if ( ![CHHapticEngine capabilitiesForHardware].supportsHaptics )
	{
		DevMsg( "Haptics: device does not support CoreHaptics, disabling.\n" );
		return false;
	}

	NSError *pError = nil;
	g_pHapticEngine = [[CHHapticEngine alloc] initAndReturnError:&pError];
	if ( g_pHapticEngine == nil || pError != nil )
	{
		Warning( "Haptics: engine init failed (%s)\n",
			pError ? [[pError localizedDescription] UTF8String] : "unknown" );
		g_pHapticEngine = nil;
		return false;
	}

	// Keep the engine warm; otherwise the first tap has audible latency.
	g_pHapticEngine.playsHapticsOnly = YES;
	g_pHapticEngine.autoShutdownEnabled = NO;

	// iOS stops the engine on interruption (backgrounding, a phone call).
	// Recreate the player so haptics survive the round trip.
	__block void (^restart)(void) = ^{
		NSError *pRestartError = nil;
		[g_pHapticEngine startAndReturnError:&pRestartError];
		if ( pRestartError == nil )
		{
			g_pRumblePlayer = Haptics_CreateContinuousPlayer();
			g_bPlaying = false;
		}
	};

	g_pHapticEngine.stoppedHandler = ^( CHHapticEngineStoppedReason reason ) {
		g_bPlaying = false;
	};
	g_pHapticEngine.resetHandler = ^{
		restart();
	};

	[g_pHapticEngine startAndReturnError:&pError];
	if ( pError != nil )
	{
		Warning( "Haptics: engine start failed (%s)\n", [[pError localizedDescription] UTF8String] );
		g_pHapticEngine = nil;
		return false;
	}

	g_pRumblePlayer = Haptics_CreateContinuousPlayer();
	if ( g_pRumblePlayer == nil )
	{
		[g_pHapticEngine stopWithCompletionHandler:nil];
		g_pHapticEngine = nil;
		return false;
	}

	g_bHapticsActive = true;
	DevMsg( "Haptics: CoreHaptics backend ready.\n" );
	return true;
}

//-----------------------------------------------------------------------------
void Haptics_Shutdown( void )
{
	if ( !g_bHapticsActive )
		return;

	Haptics_Stop();

	[g_pHapticEngine stopWithCompletionHandler:nil];
	g_pRumblePlayer   = nil;
	g_pHapticEngine   = nil;
	g_bHapticsActive  = false;
	g_flLastIntensity = 0.0f;
	g_flLastSharpness = 0.0f;
}

//-----------------------------------------------------------------------------
void Haptics_SetRumble( float flLeftMotor, float flRightMotor )
{
	if ( !g_bHapticsActive || g_pRumblePlayer == nil )
		return;

	// The engine hands us Xbox-style motor strengths. Clamp defensively:
	// game code has been known to push values outside 0..1.
	// NOTE: Clamp() (capital C) is the tier0 template from basetypes.h;
	// lowercase clamp() lives in mathlib, which inputsystem does not pull in.
	flLeftMotor  = Clamp( flLeftMotor,  0.0f, 1.0f );
	flRightMotor = Clamp( flRightMotor, 0.0f, 1.0f );

	// Low frequency motor -> how hard it buzzes.
	// High frequency motor -> how crisp it feels.
	const float flIntensity = MAX( flLeftMotor, flRightMotor );
	const float flSharpness = flRightMotor;

	// Fully off: stop rather than play silence, so the Taptic Engine can idle.
	if ( flIntensity < kHapticEpsilon )
	{
		Haptics_Stop();
		return;
	}

	NSError *pError = nil;

	// Restart shortly before the continuous event's duration elapses,
	// otherwise a sustained rumble would silently stop after
	// kHapticContinuousDuration seconds.
	const NSTimeInterval flNow = [NSDate timeIntervalSinceReferenceDate];
	if ( g_bPlaying && flNow >= g_flPlaybackEndsAt - 1.0 )
	{
		[g_pRumblePlayer stopAtTime:0 error:&pError];
		g_bPlaying = false;
		pError = nil;
	}

	if ( !g_bPlaying )
	{
		[g_pRumblePlayer startAtTime:0 error:&pError];
		if ( pError != nil )
		{
			// Non-fatal: most often the engine was stopped by the OS and the
			// reset handler has not run yet.
			return;
		}
		g_bPlaying = true;
		g_flPlaybackEndsAt = flNow + kHapticContinuousDuration;
		// Force the parameter send below on the first frame.
		g_flLastIntensity = -1.0f;
	}

	// Avoid flooding CoreHaptics with identical parameter updates every frame.
	if ( fabsf( flIntensity - g_flLastIntensity ) < 0.01f &&
		 fabsf( flSharpness - g_flLastSharpness ) < 0.01f )
	{
		return;
	}

	CHHapticDynamicParameter *pIntensityParam =
		[[CHHapticDynamicParameter alloc] initWithParameterID:CHHapticDynamicParameterIDHapticIntensityControl
														value:flIntensity
												 relativeTime:0];
	CHHapticDynamicParameter *pSharpnessParam =
		[[CHHapticDynamicParameter alloc] initWithParameterID:CHHapticDynamicParameterIDHapticSharpnessControl
														value:flSharpness
												 relativeTime:0];

	[g_pRumblePlayer sendParameters:@[ pIntensityParam, pSharpnessParam ]
					   atTime:CHHapticTimeImmediate
						error:&pError];

	if ( pError == nil )
	{
		g_flLastIntensity = flIntensity;
		g_flLastSharpness = flSharpness;
	}
}

//-----------------------------------------------------------------------------
void Haptics_Stop( void )
{
	if ( !g_bHapticsActive || g_pRumblePlayer == nil || !g_bPlaying )
		return;

	NSError *pError = nil;
	[g_pRumblePlayer stopAtTime:0 error:&pError];

	g_bPlaying        = false;
	g_flLastIntensity = 0.0f;
	g_flLastSharpness = 0.0f;
}

#endif // SRC_HAPTICS
