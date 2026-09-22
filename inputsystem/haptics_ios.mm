//========= Source Engine iOS modernization ==============================//
//
// Purpose: CoreHaptics implementation of the haptics backend.
//
//	Only compiled when the --haptics waf flag is set (SRC_HAPTICS).
//
//	IMPORTANT: this file includes Objective-C framework headers and must
//	therefore NOT include any Valve header. public/tier0/basetypes.h does
//	`typedef int BOOL` while ObjC's objc.h does `typedef bool BOOL`, and a
//	translation unit that sees both fails to compile. The repo hit this
//	before - see commit 14f6ad3d "Fix iOS CI: keep UIKit BOOL out of
//	glmrendererinfo". Diagnostics therefore go through printf rather than
//	Msg/Warning/DevMsg.
//
//	Design notes:
//	 - CoreHaptics is iOS 13+. We check availability at runtime rather
//	   than assuming, because the engine also runs on devices and
//	   simulators with no Taptic Engine, where supportsHaptics is NO and
//	   creating an engine fails.
//	 - The engine stops itself when the app is backgrounded; the reset and
//	   stopped handlers recover instead of silently losing haptics for the
//	   rest of the session.
//
//=======================================================================//

#include "haptics_ios.h"

#if defined( SRC_HAPTICS )

#import <Foundation/Foundation.h>
#import <CoreHaptics/CoreHaptics.h>

#include <stdio.h>
#include <math.h>

static CHHapticEngine            *g_pHapticEngine  = nil;
static id<CHHapticPatternPlayer>  g_pRumblePlayer  = nil;
static bool                       g_bHapticsActive = false;
static float                      g_flLastIntensity = 0.0f;
static float                      g_flLastSharpness = 0.0f;
static bool                       g_bPlaying        = false;

// When the current continuous event is due to expire. CoreHaptics events
// have a finite duration, so a long rumble would otherwise fall silent.
static NSTimeInterval             g_flPlaybackEndsAt = 0.0;

// Below this level we treat the request as "off" and stop the player
// instead of playing an imperceptible buzz.
static const float kHapticEpsilon = 0.01f;

// CoreHaptics requires a finite duration for a continuous event. We use a
// long one and restart before it lapses; this is the documented pattern
// for an open-ended rumble.
static const NSTimeInterval kHapticContinuousDuration = 30.0;

//-----------------------------------------------------------------------------
static float Haptics_Clamp01( float flValue )
{
	if ( flValue < 0.0f ) return 0.0f;
	if ( flValue > 1.0f ) return 1.0f;
	return flValue;
}

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

	CHHapticEvent *pEvent =
		[[CHHapticEvent alloc] initWithEventType:CHHapticEventTypeHapticContinuous
									  parameters:@[ pIntensity, pSharpness ]
									relativeTime:0.0
										duration:kHapticContinuousDuration];

	CHHapticPattern *pPattern =
		[[CHHapticPattern alloc] initWithEvents:@[ pEvent ] parameters:@[] error:&pError];
	if ( pPattern == nil || pError != nil )
	{
		printf( "Haptics: failed to build pattern (%s)\n",
			pError ? [[pError localizedDescription] UTF8String] : "unknown" );
		return nil;
	}

	id<CHHapticPatternPlayer> pPlayer = [g_pHapticEngine createPlayerWithPattern:pPattern error:&pError];
	if ( pPlayer == nil || pError != nil )
	{
		printf( "Haptics: failed to create player (%s)\n",
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
		printf( "Haptics: device does not support CoreHaptics, disabling.\n" );
		return false;
	}

	NSError *pError = nil;
	g_pHapticEngine = [[CHHapticEngine alloc] initAndReturnError:&pError];
	if ( g_pHapticEngine == nil || pError != nil )
	{
		printf( "Haptics: engine init failed (%s)\n",
			pError ? [[pError localizedDescription] UTF8String] : "unknown" );
		g_pHapticEngine = nil;
		return false;
	}

	// Keep the engine warm; otherwise the first tap has audible latency.
	g_pHapticEngine.playsHapticsOnly = YES;
	g_pHapticEngine.autoShutdownEnabled = NO;

	// iOS stops the engine on interruption (backgrounding, a phone call).
	// Recreate the player so haptics survive the round trip.
	void (^restart)(void) = ^{
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
		printf( "Haptics: engine start failed (%s)\n", [[pError localizedDescription] UTF8String] );
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
	printf( "Haptics: CoreHaptics backend ready.\n" );
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
	flLeftMotor  = Haptics_Clamp01( flLeftMotor );
	flRightMotor = Haptics_Clamp01( flRightMotor );

	// Low frequency motor -> how hard it buzzes.
	// High frequency motor -> how crisp it feels.
	const float flIntensity = ( flLeftMotor > flRightMotor ) ? flLeftMotor : flRightMotor;
	const float flSharpness = flRightMotor;

	// Fully off: stop rather than play silence, so the Taptic Engine idles.
	if ( flIntensity < kHapticEpsilon )
	{
		Haptics_Stop();
		return;
	}

	NSError *pError = nil;

	// Restart shortly before the continuous event's duration elapses,
	// otherwise a sustained rumble would silently stop.
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
			// Non-fatal: usually the engine was stopped by the OS and the
			// reset handler has not run yet.
			return;
		}
		g_bPlaying = true;
		g_flPlaybackEndsAt = flNow + kHapticContinuousDuration;
		// Force the parameter send below on the first frame.
		g_flLastIntensity = -1.0f;
	}

	// Avoid flooding CoreHaptics with identical updates every frame.
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
