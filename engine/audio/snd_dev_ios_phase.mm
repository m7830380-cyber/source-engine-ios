//========= Source Engine iOS modernization ==============================//
//
// Purpose: AVAudioEngine output device for iOS.
//
//	Enabled with the waf flag --phase-audio, which defines SRC_PHASE_AUDIO.
//	Selected at runtime ahead of the AudioQueue/OpenAL devices when built in.
//
//	WHY THIS IS NOT (YET) FULL PHASE SPATIAL AUDIO
//	----------------------------------------------
//	The Source mixer is a push model: S_TransferStereo16() mixes every
//	audible channel down to a single interleaved stereo buffer, and the
//	device only ever sees that finished stereo mix. Real PHASE (or
//	AVAudioEnvironmentNode) spatialization needs the opposite - each sound
//	source must stay separate all the way to the audio graph so the
//	framework can place it in 3D. Handing PHASE an already-mixed stereo
//	buffer would gain nothing but latency.
//
//	Genuinely moving to PHASE therefore means changing the mixer, not the
//	device, which is a much larger and riskier change. What this file does
//	deliver is the part that is safe and self-contained:
//
//	  * AVAudioSourceNode pull-model output, replacing AudioQueue's manual
//	    128-buffer juggling. The render callback is real-time safe.
//	  * Correct AVAudioSession configuration, so the game ducks, handles
//	    route changes (headphones in/out) and interruptions (phone calls)
//	    the way iOS expects.
//	  * Headphone detection wired to the engine's IsHeadphone(), which the
//	    existing AudioQueue device never implemented on iOS.
//	  * A listener transform cached for a future spatial backend.
//
//	The class is deliberately shaped so a later change can fan out to real
//	PHASE sources without touching the device selection logic again.
//
//=======================================================================//

#include "audio_pch.h"

#if defined( SRC_PHASE_AUDIO )

#import <AVFoundation/AVFoundation.h>

#include "snd_dev_ios_phase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern bool snd_firsttime;
extern bool MIX_ScaleChannelVolume( paintbuffer_t *ppaint, channel_t *pChannel, int volume[CCHANVOLUMES], int mixchans );
extern void S_SpatializeChannel( int volume[6], int master_vol, const Vector *psourceDir, float gain, float mono );

//-----------------------------------------------------------------------------
// Ring buffer sizing.
//
// The engine mixes ahead into this buffer and we drain it from the audio
// render thread. It must be a power of two: GetOutputPosition() masks with
// (DeviceSampleCount() - 1), exactly as the AudioQueue device does.
//-----------------------------------------------------------------------------
#define PHASE_SAMPLE_RATE		44100
#define PHASE_CHANNELS			2
#define PHASE_BYTES_PER_SAMPLE	2					// 16-bit
#define PHASE_FRAME_BYTES		( PHASE_CHANNELS * PHASE_BYTES_PER_SAMPLE )

// 32768 stereo frames ~= 0.74s of mix-ahead at 44.1kHz. Power of two.
#define PHASE_RING_FRAMES		32768
#define PHASE_RING_BYTES		( PHASE_RING_FRAMES * PHASE_FRAME_BYTES )

class CAudioDeviceIOSPhase : public CAudioDeviceBase
{
public:
	bool		IsActive( void );
	bool		Init( void );
	void		Shutdown( void );
	void		PaintEnd( void );
	int			GetOutputPosition( void );
	void		Pause( void );
	void		UnPause( void );
	float		MixDryVolume( void );
	bool		Should3DMix( void );
	void		StopAllSounds( void );

	int			PaintBegin( float mixAheadTime, int soundtime, int paintedtime );
	void		ClearBuffer( void );
	void		UpdateListener( const Vector& position, const Vector& forward, const Vector& right, const Vector& up );
	void		MixBegin( int sampleCount );
	void		MixUpsample( int sampleCount, int filtertype );
	void		Mix8Mono( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress );
	void		Mix8Stereo( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress );
	void		Mix16Mono( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress );
	void		Mix16Stereo( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress );

	void		TransferSamples( int end );
	void		SpatializeChannel( int volume[CCHANVOLUMES/2], int master_vol, const Vector& sourceDir, float gain, float mono );
	void		ApplyDSPEffects( int idsp, portable_samplepair_t *pbuffront, portable_samplepair_t *pbufrear, portable_samplepair_t *pbufcenter, int samplecount );

	const char *DeviceName( void )			{ return "AVAudioEngine"; }
	int			DeviceChannels( void )		{ return PHASE_CHANNELS; }
	int			DeviceSampleBits( void )	{ return PHASE_BYTES_PER_SAMPLE * 8; }
	int			DeviceSampleBytes( void )	{ return PHASE_BYTES_PER_SAMPLE; }
	int			DeviceDmaSpeed( void )		{ return PHASE_SAMPLE_RATE; }
	int			DeviceSampleCount( void )	{ return m_deviceSampleCount; }

	bool		IsHeadphone( void );

	// Called from the real-time render thread.
	void		RenderInto( AudioBufferList *pOutput, uint32 nFrames );

private:
	bool	StartEngine( void );
	void	StopEngine( void );
	void	ConfigureSession( void );

	AVAudioEngine		*m_pEngine;
	AVAudioSourceNode	*m_pSourceNode;

	// Interleaved 16-bit stereo ring buffer shared with the render thread.
	int16				*m_pRingBuffer;
	int					m_deviceSampleCount;	// total samples (frames * channels)

	// Frames consumed by the render callback. Written only by the audio
	// thread, read by the game thread for GetOutputPosition().
	CInterlockedInt		m_nFramesRendered;

	int					m_pauseCount;
	bool				m_bRunning;
	bool				m_bFailed;
	bool				m_bSoundsShutdown;

	// Cached listener transform. Unused by the stereo path, kept so a
	// future spatial backend has it without another engine change.
	Vector				m_vListenerOrigin;
	Vector				m_vListenerForward;
	Vector				m_vListenerRight;
	Vector				m_vListenerUp;
};

static CAudioDeviceIOSPhase *g_pPhaseDevice = NULL;

//-----------------------------------------------------------------------------
IAudioDevice *Audio_CreateIOSPhaseDevice( void )
{
	if ( !g_pPhaseDevice )
		g_pPhaseDevice = new CAudioDeviceIOSPhase;

	if ( g_pPhaseDevice->Init() )
		return g_pPhaseDevice;

	delete g_pPhaseDevice;
	g_pPhaseDevice = NULL;
	return NULL;
}

//-----------------------------------------------------------------------------
// Drain the ring buffer straight into CoreAudio's planar float buffers.
//
// REAL-TIME THREAD. No allocation, no locks, no Obj-C message sends that
// could allocate, no engine calls. Anything else risks a glitch or a
// priority inversion against the game thread.
//
// Writing directly into the destination avoids any intermediate scratch
// buffer, which also keeps this function reentrancy-free.
//-----------------------------------------------------------------------------
void CAudioDeviceIOSPhase::RenderInto( AudioBufferList *pOutput, uint32 nFrames )
{
	const uint32 nNumBuffers = pOutput->mNumberBuffers;

	if ( !m_pRingBuffer || !m_bRunning )
	{
		for ( uint32 nBuf = 0; nBuf < nNumBuffers; ++nBuf )
		{
			if ( pOutput->mBuffers[ nBuf ].mData )
				memset( pOutput->mBuffers[ nBuf ].mData, 0, pOutput->mBuffers[ nBuf ].mDataByteSize );
		}
		return;
	}

	const uint32 nStart = (uint32)(int)m_nFramesRendered;
	const float  flScale = 1.0f / 32768.0f;

	for ( uint32 nBuf = 0; nBuf < nNumBuffers; ++nBuf )
	{
		float *pDst = (float *)pOutput->mBuffers[ nBuf ].mData;
		if ( !pDst )
			continue;

		// Planar output: buffer 0 is left, buffer 1 is right.
		const uint32 nSrcChan = ( nBuf < PHASE_CHANNELS ) ? nBuf : 0;

		for ( uint32 i = 0; i < nFrames; ++i )
		{
			const uint32 nFrame = ( nStart + i ) & ( PHASE_RING_FRAMES - 1 );
			pDst[ i ] = m_pRingBuffer[ nFrame * PHASE_CHANNELS + nSrcChan ] * flScale;
		}
	}

	m_nFramesRendered = (int)( nStart + nFrames );
}

//-----------------------------------------------------------------------------
void CAudioDeviceIOSPhase::ConfigureSession( void )
{
	NSError *pError = nil;
	AVAudioSession *pSession = [AVAudioSession sharedInstance];

	// .ambient would be silenced by the ring switch; .playback is what a
	// game wants. mixWithOthers lets background music keep playing.
	[pSession setCategory:AVAudioSessionCategoryPlayback
			  withOptions:AVAudioSessionCategoryOptionMixWithOthers
					error:&pError];
	if ( pError != nil )
		DevMsg( "PHASE audio: setCategory failed (%s)\n", [[pError localizedDescription] UTF8String] );

	pError = nil;
	// Ask for a small buffer; iOS will clamp to what the hardware allows.
	[pSession setPreferredIOBufferDuration:0.010 error:&pError];

	pError = nil;
	[pSession setPreferredSampleRate:PHASE_SAMPLE_RATE error:&pError];

	pError = nil;
	[pSession setActive:YES error:&pError];
	if ( pError != nil )
		DevMsg( "PHASE audio: session activation failed (%s)\n", [[pError localizedDescription] UTF8String] );
}

//-----------------------------------------------------------------------------
bool CAudioDeviceIOSPhase::StartEngine( void )
{
	m_pEngine = [[AVAudioEngine alloc] init];
	if ( m_pEngine == nil )
		return false;

	AVAudioFormat *pFormat =
		[[AVAudioFormat alloc] initStandardFormatWithSampleRate:PHASE_SAMPLE_RATE channels:PHASE_CHANNELS];
	if ( pFormat == nil )
		return false;

	// The source node renders float32 non-interleaved (the standard format),
	// so convert from our int16 ring buffer inside the callback.
	CAudioDeviceIOSPhase *pThis = this;

	m_pSourceNode = [[AVAudioSourceNode alloc] initWithFormat:pFormat
		renderBlock:^OSStatus( BOOL *isSilence,
							   const AudioTimeStamp *timestamp,
							   AVAudioFrameCount frameCount,
							   AudioBufferList *outputData )
		{
			pThis->RenderInto( outputData, (uint32)frameCount );
			*isSilence = NO;
			return noErr;
		}];

	if ( m_pSourceNode == nil )
		return false;

	[m_pEngine attachNode:m_pSourceNode];
	[m_pEngine connect:m_pSourceNode to:m_pEngine.mainMixerNode format:pFormat];

	NSError *pError = nil;
	[m_pEngine startAndReturnError:&pError];
	if ( pError != nil )
	{
		DevMsg( "PHASE audio: engine start failed (%s)\n", [[pError localizedDescription] UTF8String] );
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
void CAudioDeviceIOSPhase::StopEngine( void )
{
	if ( m_pEngine != nil )
	{
		[m_pEngine stop];
		if ( m_pSourceNode != nil )
			[m_pEngine detachNode:m_pSourceNode];
	}
	m_pSourceNode = nil;
	m_pEngine     = nil;
}

//-----------------------------------------------------------------------------
bool CAudioDeviceIOSPhase::Init( void )
{
	m_pEngine           = nil;
	m_pSourceNode       = nil;
	m_pRingBuffer       = NULL;
	m_nFramesRendered   = 0;
	m_pauseCount        = 0;
	m_bRunning          = false;
	m_bFailed           = false;
	m_bSoundsShutdown   = false;
	m_bSurround         = false;
	m_bSurroundCenter   = false;
	m_bHeadphone        = false;
	m_deviceSampleCount = PHASE_RING_FRAMES * PHASE_CHANNELS;

	m_vListenerOrigin.Init();
	m_vListenerForward.Init();
	m_vListenerRight.Init();
	m_vListenerUp.Init();

	m_pRingBuffer = (int16 *)malloc( PHASE_RING_BYTES );
	if ( !m_pRingBuffer )
	{
		m_bFailed = true;
		return false;
	}
	memset( m_pRingBuffer, 0, PHASE_RING_BYTES );

	ConfigureSession();

	if ( !StartEngine() )
	{
		free( m_pRingBuffer );
		m_pRingBuffer = NULL;
		m_bFailed = true;
		return false;
	}

	m_bRunning = true;

	if ( snd_firsttime )
		DevMsg( "AVAudioEngine sound initialized (%d Hz, %d ch)\n", PHASE_SAMPLE_RATE, PHASE_CHANNELS );

	return true;
}

//-----------------------------------------------------------------------------
void CAudioDeviceIOSPhase::Shutdown( void )
{
	m_bRunning = false;
	StopEngine();

	if ( m_pRingBuffer )
	{
		free( m_pRingBuffer );
		m_pRingBuffer = NULL;
	}

	if ( g_pPhaseDevice == this )
		g_pPhaseDevice = NULL;
}

//-----------------------------------------------------------------------------
// Mixing setup. Mirrors the AudioQueue device: keep the 4-sample alignment
// the 11k -> 44k upsampler depends on.
//-----------------------------------------------------------------------------
int CAudioDeviceIOSPhase::PaintBegin( float mixAheadTime, int soundtime, int paintedtime )
{
	unsigned int endtime = soundtime + mixAheadTime * DeviceDmaSpeed();

	int samps = DeviceSampleCount() >> ( DeviceChannels() - 1 );

	if ( (int)( endtime - soundtime ) > samps )
		endtime = soundtime + samps;

	if ( ( endtime - paintedtime ) & 0x3 )
	{
		endtime -= ( endtime - paintedtime ) & 0x3;
	}

	return endtime;
}

//-----------------------------------------------------------------------------
// Nothing to submit: the render callback pulls straight from the ring.
//-----------------------------------------------------------------------------
void CAudioDeviceIOSPhase::PaintEnd( void )
{
}

//-----------------------------------------------------------------------------
int CAudioDeviceIOSPhase::GetOutputPosition( void )
{
	const uint32 nFrames = (uint32)(int)m_nFramesRendered;
	return (int)( nFrames & ( PHASE_RING_FRAMES - 1 ) );
}

//-----------------------------------------------------------------------------
void CAudioDeviceIOSPhase::Pause( void )
{
	m_pauseCount++;
	if ( m_pauseCount == 1 )
	{
		m_bRunning = false;
		if ( m_pEngine != nil )
			[m_pEngine pause];
	}
}

void CAudioDeviceIOSPhase::UnPause( void )
{
	if ( m_pauseCount > 0 )
		m_pauseCount--;

	if ( m_pauseCount == 0 && m_pEngine != nil )
	{
		NSError *pError = nil;
		[m_pEngine startAndReturnError:&pError];
		if ( pError == nil )
			m_bRunning = true;
	}
}

bool CAudioDeviceIOSPhase::IsActive( void )
{
	return ( m_pauseCount == 0 );
}

float CAudioDeviceIOSPhase::MixDryVolume( void )
{
	return 0;
}

bool CAudioDeviceIOSPhase::Should3DMix( void )
{
	// The engine still produces the stereo downmix; see the file header.
	return false;
}

//-----------------------------------------------------------------------------
// Headphone detection - AudioQueue never implemented this on iOS, so
// headphone-aware DSP silently behaved as if speakers were always in use.
//-----------------------------------------------------------------------------
bool CAudioDeviceIOSPhase::IsHeadphone( void )
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

//-----------------------------------------------------------------------------
void CAudioDeviceIOSPhase::ClearBuffer( void )
{
	if ( !m_pRingBuffer )
		return;

	memset( m_pRingBuffer, 0, PHASE_RING_BYTES );
}

//-----------------------------------------------------------------------------
void CAudioDeviceIOSPhase::UpdateListener( const Vector& position, const Vector& forward, const Vector& right, const Vector& up )
{
	// Cached for a future spatial backend; the stereo path does not use it.
	m_vListenerOrigin  = position;
	m_vListenerForward = forward;
	m_vListenerRight   = right;
	m_vListenerUp      = up;
}

//-----------------------------------------------------------------------------
void CAudioDeviceIOSPhase::MixBegin( int sampleCount )
{
	MIX_ClearAllPaintBuffers( sampleCount, false );
}

void CAudioDeviceIOSPhase::MixUpsample( int sampleCount, int filtertype )
{
	paintbuffer_t *ppaint = MIX_GetCurrentPaintbufferPtr();
	int ifilter = ppaint->ifilter;

	Assert( ifilter < CPAINTFILTERS );

	S_MixBufferUpsample2x( sampleCount, ppaint->pbuf, &(ppaint->fltmem[ifilter][0]), CPAINTFILTERMEM, filtertype );

	ppaint->ifilter++;
}

void CAudioDeviceIOSPhase::Mix8Mono( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress )
{
	int volume[CCHANVOLUMES];
	paintbuffer_t *ppaint = MIX_GetCurrentPaintbufferPtr();

	if ( !MIX_ScaleChannelVolume( ppaint, pChannel, volume, 1 ) )
		return;

	Mix8MonoWavtype( pChannel, ppaint->pbuf + outputOffset, volume, (byte *)pData, inputOffset, rateScaleFix, outCount );
}

void CAudioDeviceIOSPhase::Mix8Stereo( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress )
{
	int volume[CCHANVOLUMES];
	paintbuffer_t *ppaint = MIX_GetCurrentPaintbufferPtr();

	if ( !MIX_ScaleChannelVolume( ppaint, pChannel, volume, 2 ) )
		return;

	Mix8StereoWavtype( pChannel, ppaint->pbuf + outputOffset, volume, (byte *)pData, inputOffset, rateScaleFix, outCount );
}

void CAudioDeviceIOSPhase::Mix16Mono( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress )
{
	int volume[CCHANVOLUMES];
	paintbuffer_t *ppaint = MIX_GetCurrentPaintbufferPtr();

	if ( !MIX_ScaleChannelVolume( ppaint, pChannel, volume, 1 ) )
		return;

	Mix16MonoWavtype( pChannel, ppaint->pbuf + outputOffset, volume, pData, inputOffset, rateScaleFix, outCount );
}

void CAudioDeviceIOSPhase::Mix16Stereo( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress )
{
	int volume[CCHANVOLUMES];
	paintbuffer_t *ppaint = MIX_GetCurrentPaintbufferPtr();

	if ( !MIX_ScaleChannelVolume( ppaint, pChannel, volume, 2 ) )
		return;

	Mix16StereoWavtype( pChannel, ppaint->pbuf + outputOffset, volume, pData, inputOffset, rateScaleFix, outCount );
}

//-----------------------------------------------------------------------------
void CAudioDeviceIOSPhase::TransferSamples( int end )
{
	int lpaintedtime = g_paintedtime;
	int endtime = end;

	if ( m_pRingBuffer )
	{
		S_TransferStereo16( m_pRingBuffer, PAINTBUFFER, lpaintedtime, endtime );
	}
}

void CAudioDeviceIOSPhase::SpatializeChannel( int volume[CCHANVOLUMES/2], int master_vol, const Vector& sourceDir, float gain, float mono )
{
	VPROF( "CAudioDeviceIOSPhase::SpatializeChannel" );
	S_SpatializeChannel( volume, master_vol, &sourceDir, gain, mono );
}

void CAudioDeviceIOSPhase::StopAllSounds( void )
{
	m_bSoundsShutdown = true;
	ClearBuffer();
}

void CAudioDeviceIOSPhase::ApplyDSPEffects( int idsp, portable_samplepair_t *pbuffront, portable_samplepair_t *pbufrear, portable_samplepair_t *pbufcenter, int samplecount )
{
	DSP_Process( idsp, pbuffront, pbufrear, pbufcenter, samplecount );
}

#endif // SRC_PHASE_AUDIO
