//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: voice recording on iOS. The engine's OpenAL capture path needs an
// audio session set up for recording, which nothing did, and Apple's OpenAL
// capture is unreliable on iOS; the Mac AudioQueue recorder is built on the
// desktop HAL. This records with AVAudioEngine: the mic's input node is tapped
// and converted to mono 16-bit at the codec's rate (11025 Hz for Speex), into a
// ring buffer the voice code reads from (GetRecordedData).
//
// The audio session becomes PlayAndRecord (to the speaker, mixing, haptics
// allowed) the first time you talk. The mic only runs while +voicerecord is
// held, so the orange dot is only on then.
//
//=============================================================================//

#import <AVFoundation/AVFoundation.h>
#include <pthread.h>
#include <stdio.h>
#include "tier0/platform.h"
#include "ivoicerecord.h"

// this file builds without ARC
#if __has_feature( objc_arc )
#define VR_RELEASE( x )
#else
#define VR_RELEASE( x ) [( x ) release]
#endif

class VoiceRecord_iOS : public IVoiceRecord
{
public:
	VoiceRecord_iOS( int nSampleRate ) : m_nSampleRate( nSampleRate ), m_nHead( 0 ), m_nCount( 0 ),
		m_pEngine( nil ), m_pConverter( nil ), m_pFormat( nil ), m_bRecording( false )
	{
		pthread_mutex_init( &m_Lock, NULL );
	}

	virtual void Release()
	{
		RecordStop();
		pthread_mutex_destroy( &m_Lock );
		delete this;
	}

	virtual bool RecordStart();
	virtual void RecordStop();
	virtual void Idle() {}

	virtual int GetRecordedData( short *pOut, int nSamplesWanted )
	{
		pthread_mutex_lock( &m_Lock );
		int n = nSamplesWanted < m_nCount ? nSamplesWanted : m_nCount;
		int nTail = ( m_nHead - m_nCount + RING ) % RING;
		for ( int i = 0; i < n; i++ )
			pOut[i] = m_Ring[( nTail + i ) % RING];
		m_nCount -= n;
		pthread_mutex_unlock( &m_Lock );
		return n;
	}

	// from the audio thread
	void Push( const short *pSamples, int n )
	{
		pthread_mutex_lock( &m_Lock );
		for ( int i = 0; i < n; i++ )
		{
			m_Ring[m_nHead] = pSamples[i];
			m_nHead = ( m_nHead + 1 ) % RING;
		}
		m_nCount += n;
		if ( m_nCount > RING )
			m_nCount = RING;	// the reader fell behind: keep the newest
		pthread_mutex_unlock( &m_Lock );
	}

private:
	virtual ~VoiceRecord_iOS() {}

	bool SetUpSession();

	enum { RING = 11025 * 4 };	// a few seconds at Speex's rate
	int m_nSampleRate;
	short m_Ring[RING];
	int m_nHead, m_nCount;
	pthread_mutex_t m_Lock;

	AVAudioEngine *m_pEngine;
	AVAudioConverter *m_pConverter;
	AVAudioFormat *m_pFormat;
	bool m_bRecording;
};

bool VoiceRecord_iOS::SetUpSession()
{
	AVAudioSession *pSession = [AVAudioSession sharedInstance];

	switch ( [pSession recordPermission] )
	{
	case AVAudioSessionRecordPermissionDenied:
		printf( "[voice] microphone access is off for this app (Settings > Privacy > Microphone)\n" );
		return false;
	case AVAudioSessionRecordPermissionUndetermined:
		// iOS asks once; hold the button again after answering
		printf( "[voice] asking for microphone access\n" );
		[pSession requestRecordPermission:^( BOOL bGranted ) {
			printf( "[voice] microphone access %s\n", bGranted ? "granted" : "denied" );
		}];
		return false;
	default:
		break;
	}

	if ( ![[pSession category] isEqualToString:AVAudioSessionCategoryPlayAndRecord] )
	{
		NSError *pErr = nil;
		AVAudioSessionCategoryOptions nOptions = AVAudioSessionCategoryOptionDefaultToSpeaker |
			AVAudioSessionCategoryOptionMixWithOthers | AVAudioSessionCategoryOptionAllowBluetoothA2DP;
		if ( ![pSession setCategory:AVAudioSessionCategoryPlayAndRecord mode:AVAudioSessionModeDefault options:nOptions error:&pErr] )
		{
			printf( "[voice] audio session: %s\n", pErr ? pErr.localizedDescription.UTF8String : "?" );
			return false;
		}
		// recording normally silences haptics
		if ( @available( iOS 13.0, * ) )
			[pSession setAllowHapticsAndSystemSoundsDuringRecording:YES error:nil];
		[pSession setActive:YES error:nil];
		printf( "[voice] audio session: play and record\n" );
	}
	return true;
}

bool VoiceRecord_iOS::RecordStart()
{
	if ( m_bRecording )
		return true;

	@autoreleasepool
	{
		if ( !SetUpSession() )
			return false;

		m_pEngine = [[AVAudioEngine alloc] init];
		AVAudioInputNode *pInput = [m_pEngine inputNode];
		AVAudioFormat *pHwFormat = [pInput outputFormatForBus:0];
		if ( !pHwFormat || pHwFormat.sampleRate <= 0 || pHwFormat.channelCount == 0 )
		{
			printf( "[voice] no microphone input\n" );
			VR_RELEASE( m_pEngine );
			m_pEngine = nil;
			return false;
		}

		m_pFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16 sampleRate:m_nSampleRate channels:1 interleaved:YES];
		m_pConverter = [[AVAudioConverter alloc] initFromFormat:pHwFormat toFormat:m_pFormat];
		if ( !m_pConverter )
		{
			printf( "[voice] can't convert %.0f Hz / %u ch to %d Hz\n", pHwFormat.sampleRate, (unsigned)pHwFormat.channelCount, m_nSampleRate );
			RecordStop();
			return false;
		}

		VoiceRecord_iOS *pThis = this;
		AVAudioConverter *pConverter = m_pConverter;
		AVAudioFormat *pFormat = m_pFormat;
		double flRatio = (double)m_nSampleRate / pHwFormat.sampleRate;
		[pInput installTapOnBus:0 bufferSize:1024 format:pHwFormat block:^( AVAudioPCMBuffer *pBuffer, AVAudioTime *pWhen ) {
			@autoreleasepool
			{
				AVAudioFrameCount nCapacity = (AVAudioFrameCount)( pBuffer.frameLength * flRatio ) + 32;
				AVAudioPCMBuffer *pOut = [[AVAudioPCMBuffer alloc] initWithPCMFormat:pFormat frameCapacity:nCapacity];
				__block BOOL bFed = NO;
				NSError *pErr = nil;
				[pConverter convertToBuffer:pOut error:&pErr withInputFromBlock:^AVAudioBuffer *( AVAudioPacketCount nPackets, AVAudioConverterInputStatus *pStatus ) {
					if ( bFed )
					{
						*pStatus = AVAudioConverterInputStatus_NoDataNow;
						return nil;
					}
					bFed = YES;
					*pStatus = AVAudioConverterInputStatus_HaveData;
					return pBuffer;
				}];
				if ( pOut.frameLength > 0 && pOut.int16ChannelData )
					pThis->Push( pOut.int16ChannelData[0], (int)pOut.frameLength );
				VR_RELEASE( pOut );
			}
		}];

		NSError *pErr = nil;
		[m_pEngine prepare];
		if ( ![m_pEngine startAndReturnError:&pErr] )
		{
			printf( "[voice] microphone start failed: %s\n", pErr ? pErr.localizedDescription.UTF8String : "?" );
			RecordStop();
			return false;
		}

		static int s_nLogs = 0;
		if ( s_nLogs++ < 3 )
			printf( "[voice] recording: mic %.0f Hz / %u ch -> %d Hz\n", pHwFormat.sampleRate, (unsigned)pHwFormat.channelCount, m_nSampleRate );
		m_bRecording = true;
	}
	return true;
}

void VoiceRecord_iOS::RecordStop()
{
	@autoreleasepool
	{
		if ( m_pEngine )
		{
			[[m_pEngine inputNode] removeTapOnBus:0];
			[m_pEngine stop];
			VR_RELEASE( m_pEngine );
			m_pEngine = nil;
		}
		if ( m_pConverter )
		{
			VR_RELEASE( m_pConverter );
			m_pConverter = nil;
		}
		if ( m_pFormat )
		{
			VR_RELEASE( m_pFormat );
			m_pFormat = nil;
		}
	}
	m_bRecording = false;
}

IVoiceRecord *CreateVoiceRecord_iOS( int nSampleRate )
{
	return new VoiceRecord_iOS( nSampleRate );
}
