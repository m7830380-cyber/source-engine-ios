//========= Source Engine iOS modernization ==============================//
//
// Purpose: AVAudioEngine-backed IAudioDevice for iOS.
//
//	Enabled with the waf flag --phase-audio, which defines SRC_PHASE_AUDIO.
//	Selected at runtime ahead of the SDL/AudioQueue devices when built in.
//
//	This file is plain C++ and deliberately includes NO Objective-C
//	framework headers - all AVFoundation work lives in
//	snd_dev_ios_phase_backend.mm behind a C interface, because Valve's
//	`typedef int BOOL` and ObjC's `typedef bool BOOL` cannot coexist in one
//	translation unit. (Same problem and same fix as commit 14f6ad3d.)
//
//	WHY THIS IS NOT (YET) FULL PHASE SPATIAL AUDIO
//	----------------------------------------------
//	The Source mixer is a push model: S_TransferStereo16() mixes every
//	audible channel down to one interleaved stereo buffer, and the device
//	only ever sees that finished mix. Real PHASE (or AVAudioEnvironmentNode)
//	spatialization needs each source kept separate all the way to the audio
//	graph. Feeding PHASE an already-mixed stereo buffer would add latency
//	and gain nothing.
//
//	Moving to real PHASE therefore means changing the mixer, not the
//	device. What this delivers is the part that is safe and self-contained:
//
//	  * AVAudioSourceNode pull-model output, replacing AudioQueue's manual
//	    128-buffer juggling. The render callback is real-time safe.
//	  * Correct AVAudioSession setup, so the game handles route changes
//	    and interruptions the way iOS expects.
//	  * Real headphone detection wired to IsHeadphone(), which the
//	    AudioQueue device never implemented on iOS.
//	  * A cached listener transform for a future spatial backend.
//
//=======================================================================//

#include "audio_pch.h"

#if defined( SRC_PHASE_AUDIO )

#include "snd_dev_ios_phase.h"
#include "snd_dev_ios_phase_backend.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern bool snd_firsttime;
extern bool MIX_ScaleChannelVolume( paintbuffer_t *ppaint, channel_t *pChannel, int volume[CCHANVOLUMES], int mixchans );
extern void S_SpatializeChannel( int volume[6], int master_vol, const Vector *psourceDir, float gain, float mono );

//-----------------------------------------------------------------------------
// Ring buffer sizing.
//
// The engine mixes ahead into this buffer and the audio thread drains it.
// Must be a power of two: GetOutputPosition() masks with
// (PHASE_RING_FRAMES - 1), exactly as the AudioQueue device does.
//-----------------------------------------------------------------------------
#define PHASE_BYTES_PER_SAMPLE	2					// 16-bit
#define PHASE_FRAME_BYTES		( IOSPHASE_CHANNELS * PHASE_BYTES_PER_SAMPLE )

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
	int			DeviceChannels( void )		{ return IOSPHASE_CHANNELS; }
	int			DeviceSampleBits( void )	{ return PHASE_BYTES_PER_SAMPLE * 8; }
	int			DeviceSampleBytes( void )	{ return PHASE_BYTES_PER_SAMPLE; }
	int			DeviceDmaSpeed( void )		{ return IOSPHASE_SAMPLE_RATE; }
	int			DeviceSampleCount( void )	{ return m_deviceSampleCount; }

	bool		IsHeadphone( void )			{ return IOSPhase_IsHeadphone(); }

	// Real-time render thread entry point.
	void		RenderInto( float *pLeft, float *pRight, unsigned int nFrames );

private:
	int16			*m_pRingBuffer;		// interleaved 16-bit stereo
	int				m_deviceSampleCount;

	// Frames consumed by the audio thread. Written there, read by the game
	// thread for GetOutputPosition().
	CInterlockedInt	m_nFramesRendered;

	int				m_pauseCount;
	bool			m_bRunning;
	bool			m_bSoundsShutdown;

	// Cached for a future spatial backend; unused by the stereo path.
	Vector			m_vListenerOrigin;
	Vector			m_vListenerForward;
	Vector			m_vListenerRight;
	Vector			m_vListenerUp;
};

static CAudioDeviceIOSPhase *g_pPhaseDevice = NULL;

//-----------------------------------------------------------------------------
// Trampoline from the C backend into the device.
//-----------------------------------------------------------------------------
static void PhaseRenderTrampoline( void *pContext, float *pLeft, float *pRight, unsigned int nFrames )
{
	CAudioDeviceIOSPhase *pDevice = (CAudioDeviceIOSPhase *)pContext;
	if ( pDevice )
		pDevice->RenderInto( pLeft, pRight, nFrames );
}

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
// Drain the ring buffer into CoreAudio's planar float buffers.
//
// REAL-TIME THREAD. No allocation, no locks, no engine calls - anything
// else risks a glitch or a priority inversion against the game thread.
//-----------------------------------------------------------------------------
void CAudioDeviceIOSPhase::RenderInto( float *pLeft, float *pRight, unsigned int nFrames )
{
	if ( !m_pRingBuffer || !m_bRunning )
	{
		if ( pLeft )  memset( pLeft,  0, nFrames * sizeof( float ) );
		if ( pRight && pRight != pLeft ) memset( pRight, 0, nFrames * sizeof( float ) );
		return;
	}

	const uint32 nStart  = (uint32)(int)m_nFramesRendered;
	const float  flScale = 1.0f / 32768.0f;

	for ( unsigned int i = 0; i < nFrames; ++i )
	{
		const uint32 nFrame = ( nStart + i ) & ( PHASE_RING_FRAMES - 1 );
		const float flL = m_pRingBuffer[ nFrame * IOSPHASE_CHANNELS + 0 ] * flScale;
		const float flR = m_pRingBuffer[ nFrame * IOSPHASE_CHANNELS + 1 ] * flScale;

		if ( pLeft )
			pLeft[ i ] = flL;
		if ( pRight && pRight != pLeft )
			pRight[ i ] = flR;
	}

	m_nFramesRendered = (int)( nStart + nFrames );
}

//-----------------------------------------------------------------------------
bool CAudioDeviceIOSPhase::Init( void )
{
	m_pRingBuffer       = NULL;
	m_nFramesRendered   = 0;
	m_pauseCount        = 0;
	m_bRunning          = false;
	m_bSoundsShutdown   = false;
	m_bSurround         = false;
	m_bSurroundCenter   = false;
	m_bHeadphone        = false;
	m_deviceSampleCount = PHASE_RING_FRAMES * IOSPHASE_CHANNELS;

	m_vListenerOrigin.Init();
	m_vListenerForward.Init();
	m_vListenerRight.Init();
	m_vListenerUp.Init();

	m_pRingBuffer = (int16 *)malloc( PHASE_RING_BYTES );
	if ( !m_pRingBuffer )
		return false;
	memset( m_pRingBuffer, 0, PHASE_RING_BYTES );

	IOSPhase_ConfigureSession();

	if ( !IOSPhase_Start( PhaseRenderTrampoline, this ) )
	{
		free( m_pRingBuffer );
		m_pRingBuffer = NULL;
		return false;
	}

	m_bRunning = true;

	if ( snd_firsttime )
		DevMsg( "AVAudioEngine sound initialized (%d Hz, %d ch)\n", IOSPHASE_SAMPLE_RATE, IOSPHASE_CHANNELS );

	return true;
}

//-----------------------------------------------------------------------------
void CAudioDeviceIOSPhase::Shutdown( void )
{
	m_bRunning = false;
	IOSPhase_Stop();

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
		endtime -= ( endtime - paintedtime ) & 0x3;

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
		IOSPhase_Pause();
	}
}

void CAudioDeviceIOSPhase::UnPause( void )
{
	if ( m_pauseCount > 0 )
		m_pauseCount--;

	if ( m_pauseCount == 0 )
	{
		if ( IOSPhase_Resume() )
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
void CAudioDeviceIOSPhase::ClearBuffer( void )
{
	if ( !m_pRingBuffer )
		return;

	memset( m_pRingBuffer, 0, PHASE_RING_BYTES );
}

//-----------------------------------------------------------------------------
void CAudioDeviceIOSPhase::UpdateListener( const Vector& position, const Vector& forward, const Vector& right, const Vector& up )
{
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
