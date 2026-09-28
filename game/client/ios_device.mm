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
   IOS_ThermalState()            0 nominal, 1 fair, 2 serious, 3 critical
 */

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <CoreMotion/CoreMotion.h>
#include <pthread.h>
#include <math.h>

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

IOS_EXPORT void IOS_Haptic( int nKind )
{
	dispatch_async( dispatch_get_main_queue(), ^{
		static UIImpactFeedbackGenerator *s_pLight = nil, *s_pMedium = nil, *s_pHeavy = nil;
		static UINotificationFeedbackGenerator *s_pNotify = nil;
		if ( !s_pLight )
		{
			s_pLight = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleLight];
			s_pMedium = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleMedium];
			s_pHeavy = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleHeavy];
			s_pNotify = [[UINotificationFeedbackGenerator alloc] init];
		}
		switch ( nKind )
		{
		case 0: [s_pLight impactOccurred]; [s_pLight prepare]; break;
		case 1: [s_pMedium impactOccurred]; [s_pMedium prepare]; break;
		case 2: [s_pHeavy impactOccurred]; [s_pHeavy prepare]; break;
		default: [s_pNotify notificationOccurred:UINotificationFeedbackTypeSuccess]; break;
		}
	});
}

IOS_EXPORT int IOS_ThermalState( void )
{
	return (int)[NSProcessInfo processInfo].thermalState;
}
