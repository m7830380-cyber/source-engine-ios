//========= Source Engine iOS modernization ==============================//
//
// Purpose: AVAudioEngine output backend, isolated from Valve headers.
//
//	WHY THIS FILE IS SEPARATE
//	-------------------------
//	public/tier0/basetypes.h does `typedef int BOOL`, while Objective-C's
//	objc.h does `typedef bool BOOL`. Any translation unit that pulls in
//	both fails with "typedef redefinition with different types". The repo
//	already hit this once and solved it the same way - see commit
//	14f6ad3d "Fix iOS CI: keep UIKit BOOL out of glmrendererinfo", which
//	moved UIKit access into ios_metal_layer.mm behind extern "C".
//
//	So: this file includes AVFoundation and NO Valve headers. The device
//	class in snd_dev_ios_phase.mm includes Valve headers and NO ObjC
//	framework headers. They talk over the plain C interface declared in
//	snd_dev_ios_phase_backend.h.
//
//=======================================================================//

#import <AVFoundation/AVFoundation.h>

#include "snd_dev_ios_phase_backend.h"

#include <string.h>
#include <stdio.h>

static AVAudioEngine        *g_pEngine     = nil;
static AVAudioSourceNode    *g_pSourceNode = nil;

// Supplied by the device class; called on the real-time render thread.
static IOSPhaseRenderFunc    g_pfnRender   = NULL;
static void                 *g_pRenderCtx  = NULL;

static bool                  g_bRunning    = false;

//-----------------------------------------------------------------------------
void IOSPhase_ConfigureSession( void )
{
	NSError *pError = nil;
	AVAudioSession *pSession = [AVAudioSession sharedInstance];

	// .ambient would be silenced by the ring switch; .playback is what a
	// game wants. mixWithOthers lets background music keep playing.
	[pSession setCategory:AVAudioSessionCategoryPlayback
			  withOptions:AVAudioSessionCategoryOptionMixWithOthers
					error:&pError];
	if ( pError != nil )
		printf( "PHASE audio: setCategory failed (%s)\n", [[pError localizedDescription] UTF8String] );

	pError = nil;
	[pSession setPreferredIOBufferDuration:0.010 error:&pError];

	pError = nil;
	[pSession setPreferredSampleRate:IOSPHASE_SAMPLE_RATE error:&pError];

	pError = nil;
	[pSession setActive:YES error:&pError];
	if ( pError != nil )
		printf( "PHASE audio: session activation failed (%s)\n", [[pError localizedDescription] UTF8String] );
}

//-----------------------------------------------------------------------------
bool IOSPhase_Start( IOSPhaseRenderFunc pfnRender, void *pContext )
{
	if ( g_bRunning )
		return true;

	g_pfnRender  = pfnRender;
	g_pRenderCtx = pContext;

	g_pEngine = [[AVAudioEngine alloc] init];
	if ( g_pEngine == nil )
		return false;

	AVAudioFormat *pFormat =
		[[AVAudioFormat alloc] initStandardFormatWithSampleRate:IOSPHASE_SAMPLE_RATE
													   channels:IOSPHASE_CHANNELS];
	if ( pFormat == nil )
	{
		g_pEngine = nil;
		return false;
	}

	g_pSourceNode = [[AVAudioSourceNode alloc] initWithFormat:pFormat
		renderBlock:^OSStatus( BOOL *isSilence,
							   const AudioTimeStamp *timestamp,
							   AVAudioFrameCount frameCount,
							   AudioBufferList *outputData )
		{
			if ( g_pfnRender )
			{
				// Hand the planar float buffers straight to the device.
				float *pLeft  = ( outputData->mNumberBuffers > 0 )
								? (float *)outputData->mBuffers[ 0 ].mData : NULL;
				float *pRight = ( outputData->mNumberBuffers > 1 )
								? (float *)outputData->mBuffers[ 1 ].mData : pLeft;

				g_pfnRender( g_pRenderCtx, pLeft, pRight, (unsigned int)frameCount );
			}
			else
			{
				for ( unsigned int i = 0; i < outputData->mNumberBuffers; ++i )
				{
					if ( outputData->mBuffers[ i ].mData )
						memset( outputData->mBuffers[ i ].mData, 0,
								outputData->mBuffers[ i ].mDataByteSize );
				}
			}

			*isSilence = NO;
			return noErr;
		}];

	if ( g_pSourceNode == nil )
	{
		g_pEngine = nil;
		return false;
	}

	[g_pEngine attachNode:g_pSourceNode];
	[g_pEngine connect:g_pSourceNode to:g_pEngine.mainMixerNode format:pFormat];

	NSError *pError = nil;
	[g_pEngine startAndReturnError:&pError];
	if ( pError != nil )
	{
		printf( "PHASE audio: engine start failed (%s)\n", [[pError localizedDescription] UTF8String] );
		[g_pEngine detachNode:g_pSourceNode];
		g_pSourceNode = nil;
		g_pEngine     = nil;
		return false;
	}

	g_bRunning = true;
	return true;
}

//-----------------------------------------------------------------------------
void IOSPhase_Stop( void )
{
	if ( g_pEngine != nil )
	{
		[g_pEngine stop];
		if ( g_pSourceNode != nil )
			[g_pEngine detachNode:g_pSourceNode];
	}
	g_pSourceNode = nil;
	g_pEngine     = nil;
	g_pfnRender   = NULL;
	g_pRenderCtx  = NULL;
	g_bRunning    = false;
}

//-----------------------------------------------------------------------------
void IOSPhase_Pause( void )
{
	if ( g_pEngine != nil )
		[g_pEngine pause];
	g_bRunning = false;
}

//-----------------------------------------------------------------------------
bool IOSPhase_Resume( void )
{
	if ( g_pEngine == nil )
		return false;

	NSError *pError = nil;
	[g_pEngine startAndReturnError:&pError];
	if ( pError != nil )
		return false;

	g_bRunning = true;
	return true;
}

//-----------------------------------------------------------------------------
// Headphone detection. The AudioQueue device never implemented this on
// iOS, so headphone-aware DSP always behaved as if speakers were in use.
//-----------------------------------------------------------------------------
bool IOSPhase_IsHeadphone( void )
{
	AVAudioSessionRouteDescription *pRoute = [[AVAudioSession sharedInstance] currentRoute];
	for ( AVAudioSessionPortDescription *pPort in pRoute.outputs )
	{
		if ( [pPort.portType isEqualToString:AVAudioSessionPortHeadphones] ||
			 [pPort.portType isEqualToString:AVAudioSessionPortBluetoothA2DP] ||
			 [pPort.portType isEqualToString:AVAudioSessionPortBluetoothHFP] )
		{
			return true;
		}
	}
	return false;
}
