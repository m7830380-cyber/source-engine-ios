/*
 ios_device.mm - device features for the client, as plain C functions
 (Objective-C++ kept apart from Valve's headers, whose BOOL clashes with ObjC's):

   IOS_GyroSetEnabled( on )      start / stop the gyroscope
   IOS_GyroTakeDelta( &yaw, &pitch )
                                 degrees turned since the last call, in the
                                 player's frame: yaw around the real vertical
                                 (from gravity, so it doesn't matter how the
                                 phone is tilted), pitch around the screen's
                                 horizontal axis (either landscape side)
   IOS_Haptic( kind )            0 light, 1 medium, 2 heavy, 3 success
   IOS_HapticPulse( i, s, t )    a hit then a fading rumble: strength, sharpness 0..1, seconds
   IOS_HapticCrackle( t, i )     getting tased: an electric crackle
   IOS_ThermalState()            0 nominal, 1 fair, 2 serious, 3 critical
 */

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <CoreMotion/CoreMotion.h>
#import <CoreHaptics/CoreHaptics.h>
#include <pthread.h>
#include <math.h>
#include <stdio.h>

#define IOS_EXPORT extern "C"

static CMMotionManager *s_pMotion = nil;
static NSOperationQueue *s_pMotionQueue = nil;
static pthread_mutex_t s_GyroLock = PTHREAD_MUTEX_INITIALIZER;
static double s_flYaw = 0.0, s_flPitch = 0.0;	// degrees, not yet taken
static int s_nPitchSign = 1;					// flips with the landscape side

// which landscape side is up decides the sign of pitch
static void IOS_UpdatePitchSign( void )
{
	dispatch_async( dispatch_get_main_queue(), ^{
		UIInterfaceOrientation o = UIInterfaceOrientationLandscapeRight;
		if ( @available( iOS 13.0, * ) )
		{
			for ( UIScene *scene in [UIApplication sharedApplication].connectedScenes )
			{
				if ( [scene isKindOfClass:[UIWindowScene class]] )
				{
					o = ((UIWindowScene *)scene).interfaceOrientation;
					break;
				}
			}
		}
		pthread_mutex_lock( &s_GyroLock );
		s_nPitchSign = ( o == UIInterfaceOrientationLandscapeLeft ) ? -1 : 1;
		pthread_mutex_unlock( &s_GyroLock );
	});
}

IOS_EXPORT void IOS_GyroSetEnabled( int bOn )
{
	dispatch_async( dispatch_get_main_queue(), ^{
		if ( bOn )
		{
			if ( !s_pMotion )
			{
				s_pMotion = [[CMMotionManager alloc] init];
				s_pMotionQueue = [[NSOperationQueue alloc] init];
				s_pMotionQueue.maxConcurrentOperationCount = 1;
			}
			if ( !s_pMotion.deviceMotionAvailable || s_pMotion.deviceMotionActive )
				return;
			IOS_UpdatePitchSign();
			s_pMotion.deviceMotionUpdateInterval = 1.0 / 120.0;
			__block NSTimeInterval s_flLast = 0.0;
			[s_pMotion startDeviceMotionUpdatesToQueue:s_pMotionQueue withHandler:^( CMDeviceMotion *motion, NSError *error ) {
				if ( !motion )
					return;
				double dt = s_flLast > 0.0 ? motion.timestamp - s_flLast : 0.0;
				s_flLast = motion.timestamp;
				if ( dt <= 0.0 || dt > 0.1 )
					return;
				CMRotationRate r = motion.rotationRate;		// rad/s, device axes
				CMAcceleration g = motion.gravity;			// unit-ish, device axes
				double gl = sqrt( g.x * g.x + g.y * g.y + g.z * g.z );
				if ( gl < 0.1 )
					return;
				// turning around the real vertical, whatever the phone's tilt
				double yawRate = -( r.x * g.x + r.y * g.y + r.z * g.z ) / gl;
				// tilting the top edge towards / away from you: the device's long axis
				double pitchRate = r.y;
				const double k = 180.0 / M_PI;
				pthread_mutex_lock( &s_GyroLock );
				s_flYaw += yawRate * dt * k;
				s_flPitch += -pitchRate * dt * k * s_nPitchSign;
				pthread_mutex_unlock( &s_GyroLock );
			}];
		}
		else if ( s_pMotion && s_pMotion.deviceMotionActive )
		{
			[s_pMotion stopDeviceMotionUpdates];
			pthread_mutex_lock( &s_GyroLock );
			s_flYaw = s_flPitch = 0.0;
			pthread_mutex_unlock( &s_GyroLock );
		}
	});
}

IOS_EXPORT void IOS_GyroTakeDelta( float *pflYaw, float *pflPitch )
{
	pthread_mutex_lock( &s_GyroLock );
	*pflYaw = (float)s_flYaw;
	*pflPitch = (float)s_flPitch;
	s_flYaw = s_flPitch = 0.0;
	pthread_mutex_unlock( &s_GyroLock );

	// the player may have turned the phone around: refresh now and then
	static int s_nCalls = 0;
	if ( ( ++s_nCalls % 120 ) == 0 )
		IOS_UpdatePitchSign();
}

// Core Haptics first: it plays straight from the engine, without UIKit's
// feedback generators (which stay silent in some states, e.g. without a view
// in the foreground scene). The generators are the fallback.
// this file builds without ARC
#if __has_feature( objc_arc )
#define IOS_AUTORELEASE( ... ) ( __VA_ARGS__ )
#else
#define IOS_AUTORELEASE( ... ) [( __VA_ARGS__ ) autorelease]
#endif

static CHHapticEngine *s_pHapticEngine = nil;
static bool s_bHapticEngineTried = false;
static int s_nHapticLogs = 0;

static bool IOS_HapticEngineReady( void )
{
	if ( s_pHapticEngine )
		return true;
	if ( s_bHapticEngineTried )
		return false;
	s_bHapticEngineTried = true;

	if ( ![CHHapticEngine capabilitiesForHardware].supportsHaptics )
	{
		printf( "[haptics] this device has no haptics engine (Core Haptics)\n" );
		return false;
	}
	NSError *err = nil;
	CHHapticEngine *engine = [[CHHapticEngine alloc] initAndReturnError:&err];
	if ( !engine )
	{
		printf( "[haptics] Core Haptics engine failed: %s\n", err ? err.localizedDescription.UTF8String : "?" );
		return false;
	}
	engine.playsHapticsOnly = YES;		// never touch the game's audio
	engine.autoShutdownEnabled = YES;	// sleeps when idle, starts again on play
	engine.resetHandler = ^{
		NSError *e = nil;
		[s_pHapticEngine startAndReturnError:&e];
	};
	if ( ![engine startAndReturnError:&err] )
	{
		printf( "[haptics] Core Haptics engine start failed: %s\n", err ? err.localizedDescription.UTF8String : "?" );
#if !__has_feature( objc_arc )
		[engine release];
#endif
		return false;
	}
	s_pHapticEngine = engine;
	s_bHapticEngineTried = false;
	printf( "[haptics] Core Haptics engine started\n" );
	return true;
}

// One pulse: a sharp hit at the start, then a rumble that fades out over the
// duration (shots, explosions). flIntensity/flSharpness 0..1, duration in s.
static bool IOS_HapticPlayCore( float flIntensity, float flSharpness, float flDuration )
{
	if ( !IOS_HapticEngineReady() )
		return false;

	CHHapticEventParameter *pHitIntensity = IOS_AUTORELEASE( [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticIntensity value:flIntensity] );
	CHHapticEventParameter *pHitSharpness = IOS_AUTORELEASE( [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticSharpness value:flSharpness] );
	NSMutableArray *events = [NSMutableArray arrayWithObject:
		IOS_AUTORELEASE( [[CHHapticEvent alloc] initWithEventType:CHHapticEventTypeHapticTransient parameters:@[ pHitIntensity, pHitSharpness ] relativeTime:0] )];
	NSMutableArray *curves = [NSMutableArray array];

	if ( flDuration > 0.05f )
	{
		// the rumble: a little duller than the hit, fading to nothing
		CHHapticEventParameter *pIntensity = IOS_AUTORELEASE( [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticIntensity value:flIntensity] );
		CHHapticEventParameter *pSharpness = IOS_AUTORELEASE( [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticSharpness value:flSharpness * 0.6f] );
		[events addObject:IOS_AUTORELEASE( [[CHHapticEvent alloc] initWithEventType:CHHapticEventTypeHapticContinuous parameters:@[ pIntensity, pSharpness ] relativeTime:0 duration:flDuration] )];

		NSArray *points = @[
			IOS_AUTORELEASE( [[CHHapticParameterCurveControlPoint alloc] initWithRelativeTime:0 value:1.0f] ),
			IOS_AUTORELEASE( [[CHHapticParameterCurveControlPoint alloc] initWithRelativeTime:flDuration * 0.25f value:0.55f] ),
			IOS_AUTORELEASE( [[CHHapticParameterCurveControlPoint alloc] initWithRelativeTime:flDuration * 0.6f value:0.2f] ),
			IOS_AUTORELEASE( [[CHHapticParameterCurveControlPoint alloc] initWithRelativeTime:flDuration value:0.0f] ) ];
		[curves addObject:IOS_AUTORELEASE( [[CHHapticParameterCurve alloc] initWithParameterID:CHHapticDynamicParameterIDHapticIntensityControl controlPoints:points relativeTime:0] )];
	}

	NSError *err = nil;
	CHHapticPattern *pattern = IOS_AUTORELEASE( [[CHHapticPattern alloc] initWithEvents:events parameterCurves:curves error:&err] );
	id<CHHapticPatternPlayer> player = pattern ? [s_pHapticEngine createPlayerWithPattern:pattern error:&err] : nil;
	if ( !player || ![player startAtTime:CHHapticTimeImmediate error:&err] )
	{
		// the engine may have been stopped (backgrounded): start it and retry once
		NSError *e = nil;
		if ( ![s_pHapticEngine startAndReturnError:&e] || !player || ![player startAtTime:CHHapticTimeImmediate error:&err] )
		{
			if ( s_nHapticLogs < 10 )
			{
				++s_nHapticLogs;
				printf( "[haptics] Core Haptics play failed: %s\n", err ? err.localizedDescription.UTF8String : "?" );
			}
			return false;
		}
	}
	return true;
}

static void IOS_HapticPlayUIKit( float flIntensity, float flDuration )
{
	static UIImpactFeedbackGenerator *s_pLight = nil, *s_pHeavy = nil;
	if ( !s_pLight )
	{
		s_pLight = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleLight];
		s_pHeavy = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleHeavy];
	}
	UIImpactFeedbackGenerator *pGen = ( flIntensity > 0.6f || flDuration > 0.3f ) ? s_pHeavy : s_pLight;
	if ( @available( iOS 13.0, * ) )
		[pGen impactOccurredWithIntensity:flIntensity];
	else
		[pGen impactOccurred];
	[pGen prepare];
}

IOS_EXPORT void IOS_HapticPulse( float flIntensity, float flSharpness, float flDuration )
{
	flIntensity = fminf( fmaxf( flIntensity, 0.0f ), 1.0f );
	flSharpness = fminf( fmaxf( flSharpness, 0.0f ), 1.0f );
	flDuration = fminf( fmaxf( flDuration, 0.0f ), 6.0f );
	dispatch_block_t work = ^{
		@autoreleasepool {
			bool bCore = IOS_HapticPlayCore( flIntensity, flSharpness, flDuration );
			if ( !bCore )
				IOS_HapticPlayUIKit( flIntensity, flDuration );
			if ( s_nHapticLogs < 10 )
			{
				++s_nHapticLogs;
				printf( "[haptics] played %.2f strength, %.2f sharpness, %.2f s via %s\n", flIntensity, flSharpness, flDuration, bCore ? "Core Haptics" : "UIKit" );
				fflush( stdout );
			}
		}
	};
	// the game runs on the main thread: play right away, don't wait for the run loop
	if ( [NSThread isMainThread] )
		work();
	else
		dispatch_async( dispatch_get_main_queue(), work );
}

// Getting tased: an electric crackle, a sharp buzz under irregular jolts,
// fading at the end
IOS_EXPORT void IOS_HapticCrackle( float flDuration, float flIntensity )
{
	flDuration = fminf( fmaxf( flDuration, 0.1f ), 3.0f );
	flIntensity = fminf( fmaxf( flIntensity, 0.0f ), 1.0f );
	dispatch_block_t work = ^{
		@autoreleasepool {
			if ( !IOS_HapticEngineReady() )
			{
				IOS_HapticPlayUIKit( flIntensity, flDuration );
				return;
			}
			NSMutableArray *events = [NSMutableArray array];
			CHHapticEventParameter *pBuzzI = IOS_AUTORELEASE( [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticIntensity value:flIntensity * 0.55f] );
			CHHapticEventParameter *pBuzzS = IOS_AUTORELEASE( [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticSharpness value:1.0f] );
			[events addObject:IOS_AUTORELEASE( [[CHHapticEvent alloc] initWithEventType:CHHapticEventTypeHapticContinuous parameters:@[ pBuzzI, pBuzzS ] relativeTime:0 duration:flDuration] )];

			// the jolts: every 25-60 ms, random strength, weaker toward the end
			unsigned int nSeed = 12345;
			for ( float t = 0.0f; t < flDuration; )
			{
				nSeed = nSeed * 1103515245u + 12345u;
				float r = (float)( ( nSeed >> 16 ) & 0x7fff ) / 32767.0f;
				float flFade = 1.0f - 0.6f * ( t / flDuration );
				CHHapticEventParameter *pI = IOS_AUTORELEASE( [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticIntensity value:flIntensity * flFade * ( 0.5f + 0.5f * r )] );
				CHHapticEventParameter *pS = IOS_AUTORELEASE( [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticSharpness value:0.8f + 0.2f * r] );
				[events addObject:IOS_AUTORELEASE( [[CHHapticEvent alloc] initWithEventType:CHHapticEventTypeHapticTransient parameters:@[ pI, pS ] relativeTime:t] )];
				t += 0.025f + 0.035f * r;
			}

			NSArray *points = @[
				IOS_AUTORELEASE( [[CHHapticParameterCurveControlPoint alloc] initWithRelativeTime:0 value:1.0f] ),
				IOS_AUTORELEASE( [[CHHapticParameterCurveControlPoint alloc] initWithRelativeTime:flDuration * 0.7f value:0.8f] ),
				IOS_AUTORELEASE( [[CHHapticParameterCurveControlPoint alloc] initWithRelativeTime:flDuration value:0.0f] ) ];
			NSArray *curves = @[ IOS_AUTORELEASE( [[CHHapticParameterCurve alloc] initWithParameterID:CHHapticDynamicParameterIDHapticIntensityControl controlPoints:points relativeTime:0] ) ];

			NSError *err = nil;
			CHHapticPattern *pattern = IOS_AUTORELEASE( [[CHHapticPattern alloc] initWithEvents:events parameterCurves:curves error:&err] );
			id<CHHapticPatternPlayer> player = pattern ? [s_pHapticEngine createPlayerWithPattern:pattern error:&err] : nil;
			if ( !player || ![player startAtTime:CHHapticTimeImmediate error:&err] )
			{
				NSError *e = nil;
				if ( ![s_pHapticEngine startAndReturnError:&e] || !player || ![player startAtTime:CHHapticTimeImmediate error:&err] )
					printf( "[haptics] crackle failed: %s\n", err ? err.localizedDescription.UTF8String : "?" );
			}
		}
	};
	if ( [NSThread isMainThread] )
		work();
	else
		dispatch_async( dispatch_get_main_queue(), work );
}

IOS_EXPORT void IOS_Haptic( int nKind )
{
	switch ( nKind )
	{
	case 0: IOS_HapticPulse( 0.45f, 0.6f, 0.0f ); break;	// light tap
	case 1: IOS_HapticPulse( 0.75f, 0.5f, 0.12f ); break;	// medium
	case 2: IOS_HapticPulse( 1.0f, 0.35f, 0.3f ); break;	// heavy
	default: IOS_HapticPulse( 0.9f, 0.7f, 0.15f ); break;
	}
}

IOS_EXPORT int IOS_ThermalState( void )
{
	return (int)[NSProcessInfo processInfo].thermalState;
}
