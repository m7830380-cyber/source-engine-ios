//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#include "audio_pch.h"
#include "snd_mp3_source.h"
#include "vaudio/ivaudio.h"


#if defined( IOS )
//-----------------------------------------------------------------------------
// iOS MP3 decoding. The engine gets MP3 decoders from vaudio_miles (the Miles
// Sound System), which doesn't exist on iOS: vaudio stayed NULL, every MP3
// (menu and round music) failed with "Can't create mixer" and the main menu
// retried its music ~60 times a second. This IVAudio decodes with minimp3
// (thirdparty/minimp3, CC0).
//-----------------------------------------------------------------------------
#define MINIMP3_IMPLEMENTATION
#include "../../../thirdparty/minimp3/minimp3.h"

class CMiniMP3Stream : public IAudioStream
{
public:
	CMiniMP3Stream( IAudioStreamEvent *pEvent ) : m_pEvent( pEvent )
	{
		mp3dec_init( &m_dec );
		Reset( 0 );
		m_nRate = 0;
		m_nChannels = 0;
		// the mixer asks for channels and rate right after creation: decode the
		// first frame now and keep its samples for the first Decode()
		DecodeFrame();
	}

	virtual int Decode( void *pBuffer, unsigned int bufferSize )
	{
		unsigned int nOut = 0;
		while ( nOut + 1 < bufferSize )
		{
			if ( m_nPCMPos >= m_nPCMBytes && !DecodeFrame() )
				break;
			unsigned int nCopy = MIN( bufferSize - nOut, (unsigned int)( m_nPCMBytes - m_nPCMPos ) );
			nCopy &= ~1u;	// whole 16-bit samples
			memcpy( (char *)pBuffer + nOut, (char *)m_pcm + m_nPCMPos, nCopy );
			nOut += nCopy;
			m_nPCMPos += nCopy;
		}
		return (int)nOut;
	}

	virtual int GetOutputBits() { return 16; }
	virtual int GetOutputRate() { return m_nRate ? m_nRate : 44100; }
	virtual int GetOutputChannels() { return m_nChannels ? m_nChannels : 2; }
	virtual unsigned int GetPosition() { return m_nSourcePos; }
	virtual void SetPosition( unsigned int position )
	{
		mp3dec_init( &m_dec );
		Reset( position );
	}

private:
	void Reset( unsigned int nSourcePos )
	{
		m_nInBytes = m_nInPos = 0;
		m_nPCMBytes = m_nPCMPos = 0;
		m_bEOF = false;
		m_nSourcePos = nSourcePos;
		m_nNextRequestOffset = (int)nSourcePos;	// offset 0 lets the mixer skip an ID3 tag
	}

	void Fill()
	{
		if ( m_nInPos > 0 )
		{
			memmove( m_in, m_in + m_nInPos, m_nInBytes - m_nInPos );
			m_nInBytes -= m_nInPos;
			m_nInPos = 0;
		}
		int nWant = (int)sizeof( m_in ) - m_nInBytes;
		if ( nWant <= 0 )
			return;
		int nRead = m_pEvent->StreamRequestData( m_in + m_nInBytes, nWant, m_nNextRequestOffset );
		m_nNextRequestOffset = -1;
		if ( nRead <= 0 )
			m_bEOF = true;
		else
			m_nInBytes += nRead;
	}

	bool DecodeFrame()
	{
		m_nPCMBytes = m_nPCMPos = 0;
		for ( int nTries = 0; nTries < 64; nTries++ )
		{
			// keep at least a few frames of input buffered
			if ( !m_bEOF && m_nInBytes - m_nInPos < 8192 )
				Fill();
			int nAvail = m_nInBytes - m_nInPos;
			if ( nAvail <= 0 )
				return false;

			mp3dec_frame_info_t info;
			int nSamples = mp3dec_decode_frame( &m_dec, m_in + m_nInPos, nAvail, m_pcm, &info );
			if ( info.frame_bytes <= 0 )
			{
				// no frame in what we have: need more data, or the stream is done
				if ( m_bEOF )
					return false;
				if ( m_nInPos == 0 && m_nInBytes == (int)sizeof( m_in ) )
					m_nInPos = m_nInBytes;	// a buffer of garbage: drop it
				Fill();
				continue;
			}
			m_nInPos += info.frame_bytes;
			m_nSourcePos += info.frame_bytes;
			if ( nSamples <= 0 )
				continue;	// skipped data (tag, bad frame)

			if ( !m_nRate )
			{
				m_nRate = info.hz;
				m_nChannels = info.channels;
			}
			if ( info.channels != m_nChannels )
			{
				// keep the channel count the mixer was created with
				if ( info.channels == 2 && m_nChannels == 1 )
				{
					for ( int i = 0; i < nSamples; i++ )
						m_pcm[i] = (short)( ( m_pcm[i * 2] + m_pcm[i * 2 + 1] ) / 2 );
				}
				else if ( info.channels == 1 && m_nChannels == 2 )
				{
					for ( int i = nSamples - 1; i >= 0; i-- )
						m_pcm[i * 2] = m_pcm[i * 2 + 1] = m_pcm[i];
				}
			}
			m_nPCMBytes = nSamples * m_nChannels * (int)sizeof( short );
			return true;
		}
		return false;
	}

	IAudioStreamEvent	*m_pEvent;
	mp3dec_t			m_dec;
	unsigned char		m_in[ 16 * 1024 ];
	int					m_nInBytes, m_nInPos;
	short				m_pcm[ MINIMP3_MAX_SAMPLES_PER_FRAME ];
	int					m_nPCMBytes, m_nPCMPos;
	int					m_nRate, m_nChannels;
	bool				m_bEOF;
	unsigned int		m_nSourcePos;
	int					m_nNextRequestOffset;
};

class CMiniMP3VAudio : public IVAudio
{
public:
	virtual IAudioStream *CreateMP3StreamDecoder( IAudioStreamEvent *pEventHandler )
	{
		CMiniMP3Stream *pStream = new CMiniMP3Stream( pEventHandler );
		return pStream;
	}
	virtual void DestroyMP3StreamDecoder( IAudioStream *pDecoder ) { delete pDecoder; }
	virtual void *CreateMilesAudioEngine() { return NULL; }
	virtual void DestroyMilesAudioEngine( void *pEngine ) {}
};

IVAudio *IOS_CreateMP3VAudio()
{
	return new CMiniMP3VAudio;
}
#endif // IOS

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IVAudio *vaudio;

static const int MP3_BUFFER_SIZE = 16384;

//-----------------------------------------------------------------------------
// Purpose: Mixer for ADPCM encoded audio
//-----------------------------------------------------------------------------
class CAudioMixerWaveMP3 : public CAudioMixerWave, public IAudioStreamEvent
{
public:
	CAudioMixerWaveMP3( IWaveData *data );
	~CAudioMixerWaveMP3( void );
	
	virtual void Mix( channel_t *pChannel, void *pData, int outputOffset, int inputOffset, fixedint fracRate, int outCount, int timecompress );
	virtual int	 GetOutputData( void **pData, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] );

	// need to override this to fixup blocks
	// UNDONE: This doesn't quite work with MP3 - we need a MP3 position, not a sample position
	void SetSampleStart( int newPosition );

	int GetPositionForSave() { return m_pStream->GetPosition(); }
	void SetPositionFromSaved(int position) { m_pStream->SetPosition(position); }

	// IAudioStreamEvent
	virtual int StreamRequestData( void *pBuffer, int bytesRequested, int offset );

	virtual void SetStartupDelaySamples( int delaySamples );
	virtual int GetMixSampleSize() { return CalcSampleSize( 16, m_channelCount ); }

	bool IsValid() { return m_pStream != NULL; }

	virtual int GetStreamOutputRate() { return m_pStream->GetOutputRate(); }

private:
	bool					DecodeBlock( void );
	void					GetID3HeaderOffset();


	IAudioStream			*m_pStream;
	char					m_samples[MP3_BUFFER_SIZE];
	int						m_sampleCount;
	int						m_samplePosition;
	int						m_channelCount;
	int						m_offset;
	int						m_delaySamples;
	int						m_headerOffset;
};


CAudioMixerWaveMP3::CAudioMixerWaveMP3( IWaveData *data ) : CAudioMixerWave( data ) 
{
	m_sampleCount = 0;
	m_samplePosition = 0;
	m_offset = 0;
	m_delaySamples = 0;
	m_headerOffset = 0;
	m_pStream = NULL;
	if ( vaudio )
		m_pStream = vaudio->CreateMP3StreamDecoder( static_cast<IAudioStreamEvent *>(this) );
	if ( m_pStream )
	{
		m_channelCount = m_pStream->GetOutputChannels();
		//Assert( m_pStream->GetOutputRate() == m_pData->Source().SampleRate() );
	}
}


CAudioMixerWaveMP3::~CAudioMixerWaveMP3( void )
{
	if ( m_pStream )
	{
		vaudio->DestroyMP3StreamDecoder( m_pStream );
		m_pStream = NULL;
	}
}


void CAudioMixerWaveMP3::Mix( channel_t *pChannel, void *pData, int outputOffset, int inputOffset, fixedint fracRate, int outCount, int timecompress )
{
	if ( m_channelCount == 1 )
	{
		Device_Mix16Mono( pChannel, (short *)pData, outputOffset, inputOffset, fracRate, outCount, timecompress );
	}
	else
	{
		Device_Mix16Stereo( pChannel, (short *)pData, outputOffset, inputOffset, fracRate, outCount, timecompress );
	}

}


// Some MP3 files are wrapped in ID3
void CAudioMixerWaveMP3::GetID3HeaderOffset()
{
	char copyBuf[AUDIOSOURCE_COPYBUF_SIZE];
	byte *pData;

	int bytesRead = m_pData->ReadSourceData( (void **)&pData, 0, 10, copyBuf );
	if ( bytesRead < 10 )
		return;

	m_headerOffset = 0;
	if (( pData[ 0 ] == 0x49 ) &&
		( pData[ 1 ] == 0x44 ) &&
		( pData[ 2 ] == 0x33 ) &&
		( pData[ 3 ] < 0xff ) &&
		( pData[ 4 ] < 0xff ) &&
		( pData[ 6 ] < 0x80 ) &&
		( pData[ 7 ] < 0x80 ) &&
		( pData[ 8 ] < 0x80 ) &&
		( pData[ 9 ] < 0x80 ) )
	{
		// this is in id3 file
		// compute the size of the wrapper and skip it
		m_headerOffset = 10 + ( pData[9] | (pData[8]<<7) | (pData[7]<<14) | (pData[6]<<21) );
   }
}

int CAudioMixerWaveMP3::StreamRequestData( void *pBuffer, int bytesRequested, int offset )
{
	if ( offset < 0 )
	{
		offset = m_offset;
	}
	else
	{
		m_offset = offset;
	}
	// read the data out of the source
	int totalBytesRead = 0;

	if ( offset == 0 )
	{
		// top of file, check for ID3 wrapper
		GetID3HeaderOffset();
	}

	offset += m_headerOffset; // skip any id3 header/wrapper
	
	while ( bytesRequested > 0 )
	{
		char *pOutputBuffer = (char *)pBuffer;
		pOutputBuffer += totalBytesRead;

		void *pData = NULL;
		int bytesRead = m_pData->ReadSourceData( &pData, offset + totalBytesRead, bytesRequested, pOutputBuffer );
		
		if ( !bytesRead )
			break;
		if ( bytesRead > bytesRequested )
		{
			bytesRead = bytesRequested;
		}
		// if the source is buffering it, copy it to the MP3 decomp buffer
		if ( pData != pOutputBuffer )
		{
			memcpy( pOutputBuffer, pData, bytesRead );
		}
		totalBytesRead += bytesRead;
		bytesRequested -= bytesRead;
	}

	m_offset += totalBytesRead;
	return totalBytesRead;
}

bool CAudioMixerWaveMP3::DecodeBlock()
{
	m_sampleCount = m_pStream->Decode( m_samples, sizeof(m_samples) );
	m_samplePosition = 0;
	return m_sampleCount > 0;
}

//-----------------------------------------------------------------------------
// Purpose: Read existing buffer or decompress a new block when necessary
// Input  : **pData - output data pointer
//			sampleCount - number of samples (or pairs)
// Output : int - available samples (zero to stop decoding)
//-----------------------------------------------------------------------------
int CAudioMixerWaveMP3::GetOutputData( void **pData, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] )
{
	if ( m_samplePosition >= m_sampleCount )
	{
		if ( !DecodeBlock() )
			return 0;
	}

	if ( m_samplePosition < m_sampleCount )
	{
		int sampleSize = m_channelCount * 2;
		*pData = (void *)(m_samples + m_samplePosition);
		int available = m_sampleCount - m_samplePosition;
		int bytesRequired = sampleCount * sampleSize;
		if ( available > bytesRequired )
			available = bytesRequired;

		m_samplePosition += available;
		
		int samples_loaded = available / sampleSize;

		// update count of max samples loaded in CAudioMixerWave

		CAudioMixerWave::m_sample_max_loaded += samples_loaded;

		// update index of last sample loaded

		CAudioMixerWave::m_sample_loaded_index += samples_loaded;

		return samples_loaded;
	}

	return 0;
}



//-----------------------------------------------------------------------------
// Purpose: Seek to a new position in the file
//			NOTE: In most cases, only call this once, and call it before playing
//			any data.
// Input  : newPosition - new position in the sample clocks of this sample
//-----------------------------------------------------------------------------
void CAudioMixerWaveMP3::SetSampleStart( int newPosition )
{
	// UNDONE: Implement this?
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : delaySamples - 
//-----------------------------------------------------------------------------
void CAudioMixerWaveMP3::SetStartupDelaySamples( int delaySamples )
{
	m_delaySamples = delaySamples;
}

//-----------------------------------------------------------------------------
// Purpose: Abstract factory function for MP3 mixers
// Input  : *data - wave data access object
//			channels - 
// Output : CAudioMixer
//-----------------------------------------------------------------------------
CAudioMixer *CreateMP3Mixer( IWaveData *data, int *pSampleRate )
{
	CAudioMixerWaveMP3 *pMixer = new CAudioMixerWaveMP3( data );
	if ( pMixer->IsValid() )
	{
		// pass the sample rate back just in time to save parsing the MP3 file twice to get sample rate
		if ( pSampleRate )
		{
			*pSampleRate = pMixer->GetStreamOutputRate();
		}
		return pMixer;
	}

	delete pMixer;
	return NULL;
}
