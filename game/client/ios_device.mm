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
   IOS_QRSetButton( kind )       the native QR button: 0 none, 1 "Join by QR", 2 "Show join QR"
   IOS_QRTakeButtonTap()         1 once after the button was tapped
   IOS_QRShow( text, caption )   a QR code over the game, tap to close
   IOS_QRScanStart()             the camera, looking for a QR code
   IOS_QRTakeScanned( buf, n )   1 and the text once a code was read
 */

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <CoreMotion/CoreMotion.h>
#import <CoreHaptics/CoreHaptics.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreImage/CoreImage.h>
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

// ---------------------------------------------------------------------------
// QR codes: joining a game by scanning the host's screen
// ---------------------------------------------------------------------------

static UIViewController *IOS_GameViewController( void )
{
	UIWindow *pWindow = nil;
	if ( @available( iOS 13.0, * ) )
	{
		for ( UIScene *scene in [UIApplication sharedApplication].connectedScenes )
		{
			if ( ![scene isKindOfClass:[UIWindowScene class]] )
				continue;
			for ( UIWindow *w in ((UIWindowScene *)scene).windows )
			{
				if ( w.isKeyWindow )
					pWindow = w;
			}
		}
	}
	if ( !pWindow )
		pWindow = [UIApplication sharedApplication].keyWindow;
	UIViewController *pVC = pWindow.rootViewController;
	while ( pVC.presentedViewController )
		pVC = pVC.presentedViewController;
	return pVC;
}

static pthread_mutex_t s_QRLock = PTHREAD_MUTEX_INITIALIZER;
static char s_szScanned[512];
static int s_bScanned = 0;
static int s_bButtonTapped = 0;

@interface IOSQRTarget : NSObject
@end
@implementation IOSQRTarget
- (void)buttonTapped:(id)sender { s_bButtonTapped = 1; }
- (void)overlayTapped:(UITapGestureRecognizer *)g { [g.view removeFromSuperview]; }
@end
static IOSQRTarget *s_pQRTarget = nil;
static UIButton *s_pQRButton = nil;
static int s_nQRButtonKind = 0;

IOS_EXPORT void IOS_QRSetButton( int nKind )
{
	if ( nKind == s_nQRButtonKind )
		return;
	s_nQRButtonKind = nKind;
	@autoreleasepool
	{
		if ( !s_pQRTarget )
			s_pQRTarget = [[IOSQRTarget alloc] init];
		if ( !nKind )
		{
			[s_pQRButton removeFromSuperview];
			return;
		}
		if ( !s_pQRButton )
		{
			s_pQRButton = [[UIButton buttonWithType:UIButtonTypeSystem] retain];
			s_pQRButton.backgroundColor = [UIColor colorWithWhite:0.0 alpha:0.6];
			s_pQRButton.layer.cornerRadius = 8;
			s_pQRButton.layer.borderWidth = 1;
			s_pQRButton.layer.borderColor = [UIColor colorWithWhite:1.0 alpha:0.35].CGColor;
			s_pQRButton.titleLabel.font = [UIFont boldSystemFontOfSize:15];
			[s_pQRButton setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
			s_pQRButton.contentEdgeInsets = UIEdgeInsetsMake( 8, 14, 8, 14 );
			[s_pQRButton addTarget:s_pQRTarget action:@selector(buttonTapped:) forControlEvents:UIControlEventTouchUpInside];
		}
		[s_pQRButton setTitle:( nKind == 1 ? @"Join by QR" : @"Show join QR" ) forState:UIControlStateNormal];
		[s_pQRButton sizeToFit];

		UIView *pView = IOS_GameViewController().view;
		if ( !pView )
			return;
		// bottom left, clear of the safe area; the menus keep that corner free
		UIEdgeInsets inset = UIEdgeInsetsZero;
		if ( @available( iOS 11.0, * ) )
			inset = pView.safeAreaInsets;
		CGRect r = s_pQRButton.frame;
		r.origin.x = inset.left + 12;
		r.origin.y = pView.bounds.size.height - inset.bottom - r.size.height - 12;
		s_pQRButton.frame = r;
		s_pQRButton.autoresizingMask = UIViewAutoresizingFlexibleTopMargin | UIViewAutoresizingFlexibleRightMargin;
		[pView addSubview:s_pQRButton];
	}
}

IOS_EXPORT int IOS_QRTakeButtonTap( void )
{
	int b = s_bButtonTapped;
	s_bButtonTapped = 0;
	return b;
}

IOS_EXPORT void IOS_QRShow( const char *pszText, const char *pszCaption )
{
	@autoreleasepool
	{
		UIView *pView = IOS_GameViewController().view;
		if ( !pView || !pszText )
			return;

		CIFilter *pFilter = [CIFilter filterWithName:@"CIQRCodeGenerator"];
		[pFilter setValue:[[NSString stringWithUTF8String:pszText] dataUsingEncoding:NSUTF8StringEncoding] forKey:@"inputMessage"];
		[pFilter setValue:@"M" forKey:@"inputCorrectionLevel"];
		CIImage *pCode = pFilter.outputImage;
		if ( !pCode )
			return;

		CGFloat flSide = MIN( pView.bounds.size.width, pView.bounds.size.height ) * 0.62;
		// crisp modules: scale by a whole factor, then render to a bitmap
		CGFloat flScale = floor( flSide / pCode.extent.size.width );
		CIImage *pScaled = [pCode imageByApplyingTransform:CGAffineTransformMakeScale( flScale, flScale )];
		CIContext *pContext = [CIContext contextWithOptions:nil];
		CGImageRef cg = [pContext createCGImage:pScaled fromRect:pScaled.extent];
		UIImage *pImage = [UIImage imageWithCGImage:cg];
		CGImageRelease( cg );

		UIView *pOverlay = [[[UIView alloc] initWithFrame:pView.bounds] autorelease];
		pOverlay.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
		pOverlay.backgroundColor = [UIColor colorWithWhite:0.0 alpha:0.8];

		CGFloat flCard = pImage.size.width + 32;
		UIView *pCard = [[[UIView alloc] initWithFrame:CGRectMake( 0, 0, flCard, flCard )] autorelease];
		pCard.backgroundColor = [UIColor whiteColor];
		pCard.layer.cornerRadius = 12;
		pCard.center = CGPointMake( pView.bounds.size.width * 0.5, pView.bounds.size.height * 0.45 );
		pCard.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleRightMargin | UIViewAutoresizingFlexibleTopMargin | UIViewAutoresizingFlexibleBottomMargin;
		UIImageView *pQR = [[[UIImageView alloc] initWithImage:pImage] autorelease];
		pQR.frame = CGRectMake( 16, 16, pImage.size.width, pImage.size.height );
		pQR.layer.magnificationFilter = kCAFilterNearest;
		[pCard addSubview:pQR];
		[pOverlay addSubview:pCard];

		UILabel *pLabel = [[[UILabel alloc] initWithFrame:CGRectMake( 20, CGRectGetMaxY( pCard.frame ) + 10, pView.bounds.size.width - 40, 60 )] autorelease];
		pLabel.text = [NSString stringWithFormat:@"%s\nTap anywhere to close", pszCaption ? pszCaption : ""];
		pLabel.textColor = [UIColor whiteColor];
		pLabel.font = [UIFont systemFontOfSize:14];
		pLabel.textAlignment = NSTextAlignmentCenter;
		pLabel.numberOfLines = 0;
		pLabel.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleTopMargin | UIViewAutoresizingFlexibleBottomMargin;
		[pOverlay addSubview:pLabel];

		if ( !s_pQRTarget )
			s_pQRTarget = [[IOSQRTarget alloc] init];
		UITapGestureRecognizer *pTap = [[[UITapGestureRecognizer alloc] initWithTarget:s_pQRTarget action:@selector(overlayTapped:)] autorelease];
		[pOverlay addGestureRecognizer:pTap];
		[pView addSubview:pOverlay];
		printf( "[qr] showing %s\n", pszText );
	}
}

// the camera, landscape like the game, until a QR code is read or Cancel
@interface IOSQRScanController : UIViewController <AVCaptureMetadataOutputObjectsDelegate>
@property (nonatomic, retain) AVCaptureSession *session;
@property (nonatomic, retain) AVCaptureVideoPreviewLayer *preview;
@end

@implementation IOSQRScanController
- (UIInterfaceOrientationMask)supportedInterfaceOrientations { return UIInterfaceOrientationMaskLandscape; }
- (BOOL)prefersStatusBarHidden { return YES; }

- (void)viewDidLoad
{
	[super viewDidLoad];
	self.view.backgroundColor = [UIColor blackColor];

	AVCaptureDevice *pCamera = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
	NSError *pErr = nil;
	AVCaptureDeviceInput *pInput = pCamera ? [AVCaptureDeviceInput deviceInputWithDevice:pCamera error:&pErr] : nil;
	if ( pInput )
	{
		self.session = [[[AVCaptureSession alloc] init] autorelease];
		[self.session addInput:pInput];
		AVCaptureMetadataOutput *pOutput = [[[AVCaptureMetadataOutput alloc] init] autorelease];
		[self.session addOutput:pOutput];
		[pOutput setMetadataObjectsDelegate:self queue:dispatch_get_main_queue()];
		if ( [pOutput.availableMetadataObjectTypes containsObject:AVMetadataObjectTypeQRCode] )
			pOutput.metadataObjectTypes = @[ AVMetadataObjectTypeQRCode ];
		self.preview = [AVCaptureVideoPreviewLayer layerWithSession:self.session];
		self.preview.videoGravity = AVLayerVideoGravityResizeAspectFill;
		[self.view.layer addSublayer:self.preview];
	}
	else
		printf( "[qr] no camera: %s\n", pErr ? pErr.localizedDescription.UTF8String : "?" );

	UILabel *pHint = [[[UILabel alloc] init] autorelease];
	pHint.text = pInput ? @"Point the camera at the host's QR code" : @"The camera isn't available (Settings > Privacy > Camera)";
	pHint.textColor = [UIColor whiteColor];
	pHint.backgroundColor = [UIColor colorWithWhite:0 alpha:0.5];
	pHint.textAlignment = NSTextAlignmentCenter;
	pHint.tag = 1;
	[self.view addSubview:pHint];

	UIButton *pCancel = [UIButton buttonWithType:UIButtonTypeSystem];
	[pCancel setTitle:@"Cancel" forState:UIControlStateNormal];
	pCancel.titleLabel.font = [UIFont boldSystemFontOfSize:17];
	[pCancel setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
	pCancel.backgroundColor = [UIColor colorWithWhite:0 alpha:0.5];
	pCancel.layer.cornerRadius = 8;
	pCancel.tag = 2;
	[pCancel addTarget:self action:@selector(cancel) forControlEvents:UIControlEventTouchUpInside];
	[self.view addSubview:pCancel];
}

- (void)viewDidLayoutSubviews
{
	[super viewDidLayoutSubviews];
	CGRect b = self.view.bounds;
	self.preview.frame = b;
	AVCaptureConnection *pConn = self.preview.connection;
	if ( pConn.isVideoOrientationSupported )
	{
		UIInterfaceOrientation o = UIInterfaceOrientationLandscapeRight;
		if ( @available( iOS 13.0, * ) )
			o = self.view.window.windowScene.interfaceOrientation;
		pConn.videoOrientation = ( o == UIInterfaceOrientationLandscapeLeft ) ? AVCaptureVideoOrientationLandscapeLeft : AVCaptureVideoOrientationLandscapeRight;
	}
	[self.view viewWithTag:1].frame = CGRectMake( 0, 20, b.size.width, 36 );
	[self.view viewWithTag:2].frame = CGRectMake( b.size.width - 130, b.size.height - 64, 110, 44 );
}

- (void)viewDidAppear:(BOOL)animated
{
	[super viewDidAppear:animated];
	AVCaptureSession *pSession = self.session;
	dispatch_async( dispatch_get_global_queue( QOS_CLASS_USER_INITIATED, 0 ), ^{ [pSession startRunning]; } );
}

- (void)finish
{
	[self.session stopRunning];
	[self dismissViewControllerAnimated:YES completion:nil];
}

- (void)cancel { [self finish]; }

- (void)captureOutput:(AVCaptureOutput *)output didOutputMetadataObjects:(NSArray *)objects fromConnection:(AVCaptureConnection *)connection
{
	for ( AVMetadataObject *pObj in objects )
	{
		if ( ![pObj isKindOfClass:[AVMetadataMachineReadableCodeObject class]] )
			continue;
		NSString *pText = ((AVMetadataMachineReadableCodeObject *)pObj).stringValue;
		if ( !pText )
			continue;
		pthread_mutex_lock( &s_QRLock );
		strlcpy( s_szScanned, pText.UTF8String, sizeof( s_szScanned ) );
		s_bScanned = 1;
		pthread_mutex_unlock( &s_QRLock );
		printf( "[qr] scanned %s\n", s_szScanned );
		[self finish];
		return;
	}
}

- (void)dealloc
{
	[_session release];
	[_preview release];
	[super dealloc];
}
@end

IOS_EXPORT void IOS_QRScanStart( void )
{
	@autoreleasepool
	{
		UIViewController *pVC = IOS_GameViewController();
		if ( !pVC )
			return;
		IOSQRScanController *pScan = [[[IOSQRScanController alloc] init] autorelease];
		pScan.modalPresentationStyle = UIModalPresentationFullScreen;
		[pVC presentViewController:pScan animated:YES completion:nil];
	}
}

IOS_EXPORT int IOS_QRTakeScanned( char *pszOut, int nOutSize )
{
	int b = 0;
	pthread_mutex_lock( &s_QRLock );
	if ( s_bScanned )
	{
		strlcpy( pszOut, s_szScanned, nOutSize );
		s_bScanned = 0;
		b = 1;
	}
	pthread_mutex_unlock( &s_QRLock );
	return b;
}

