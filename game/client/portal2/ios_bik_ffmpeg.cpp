//===========================================================================//
//
// Purpose: IBik for iOS. RAD's Bink SDK has no iOS build in this tree, so the
//			movies (menu backgrounds, elevator screens, video panels) are
//			decoded with FFmpeg's open-source Bink decoder instead and uploaded
//			as an RGBA procedural texture.
//
//===========================================================================//

#define __STDC_CONSTANT_MACROS
#include "cbase.h"

#if defined( IOS )

#include "avi/ibik.h"
#include "cdll_client_int.h"
#include "filesystem.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/itexture.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "vtf/vtf.h"
#include "pixelwriter.h"
#include "tier1/keyvalues.h"
#include "tier1/utlbuffer.h"
#include "tier1/utldict.h"
#include "tier1/utllinkedlist.h"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}

// Movie files shared between materials playing the same movie (elevator screens) and by PrecacheMovie
struct BikMovieData_t
{
	CUtlBuffer m_Data;
	int m_nRefs;
	bool m_bPrecached;
};

static CUtlDict< BikMovieData_t *, int > s_BikMovies;

static BikMovieData_t *Bik_AcquireMovie( const char *pFileName, const char *pPathID )
{
	int i = s_BikMovies.Find( pFileName );
	if ( i != s_BikMovies.InvalidIndex() )
	{
		s_BikMovies[i]->m_nRefs++;
		return s_BikMovies[i];
	}

	BikMovieData_t *pMovie = new BikMovieData_t;
	pMovie->m_nRefs = 1;
	pMovie->m_bPrecached = false;
	if ( !g_pFullFileSystem->ReadFile( pFileName, pPathID ? pPathID : "GAME", pMovie->m_Data ) || pMovie->m_Data.TellPut() <= 0 )
	{
		delete pMovie;
		return NULL;
	}
	s_BikMovies.Insert( pFileName, pMovie );
	return pMovie;
}

static void Bik_ReleaseMovie( BikMovieData_t *pMovie )
{
	if ( !pMovie || --pMovie->m_nRefs > 0 || pMovie->m_bPrecached )
		return;

	for ( int i = s_BikMovies.First(); i != s_BikMovies.InvalidIndex(); i = s_BikMovies.Next( i ) )
	{
		if ( s_BikMovies[i] == pMovie )
		{
			s_BikMovies.RemoveAt( i );
			break;
		}
	}
	delete pMovie;
}

//-----------------------------------------------------------------------------
// One playing movie
//-----------------------------------------------------------------------------
class CFFmpegBikMaterial : public ITextureRegenerator
{
public:
	CFFmpegBikMaterial();
	~CFFmpegBikMaterial();

	bool Init( const char *pMaterialName, const char *pFileName, const char *pPathID, int nFlags );
	void Shutdown();

	bool Update();
	bool ReadyForSwap() const;
	IMaterial *GetMaterial() { return m_Material; }
	void GetTexCoordRange( float *pMaxU, float *pMaxV );
	void GetFrameSize( int *pWidth, int *pHeight ) { *pWidth = m_nWidth; *pHeight = m_nHeight; }
	int GetFrameRate() const { return (int)( m_flFrameRate + 0.5f ); }
	int GetFrameCount() const { return m_nFrameCount; }
	int GetFrame() const { return m_nCurrentFrame; }
	void SetFrame( float flFrame );
	void Pause();
	void Unpause();

	// ITextureRegenerator
	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect );
	virtual void Release() {}

private:
	static int ReadPacket( void *pOpaque, uint8_t *pBuf, int nBufSize );
	static int64_t Seek( void *pOpaque, int64_t nOffset, int nWhence );

	bool OpenDecoder();
	void CloseDecoder();
	bool DecodeFrame();
	void ConvertFrame();

	BikMovieData_t *m_pMovie;
	int64_t m_nReadPos;

	AVFormatContext *m_pFormat;
	AVIOContext *m_pIO;
	AVCodecContext *m_pCodec;
	AVFrame *m_pFrame;
	AVPacket *m_pPacket;
	int m_nStream;
	bool m_bDraining;

	CUtlVector< unsigned char > m_RGBA;
	CTextureReference m_Texture;
	CMaterialReference m_Material;

	int m_nWidth;
	int m_nHeight;
	float m_flFrameRate;
	int m_nFrameCount;
	int m_nCurrentFrame;
	bool m_bLoop;
	bool m_bFinished;
	bool m_bPaused;
	double m_flNextFrameTime;
};

CFFmpegBikMaterial::CFFmpegBikMaterial() :
	m_pMovie( NULL ), m_nReadPos( 0 ), m_pFormat( NULL ), m_pIO( NULL ), m_pCodec( NULL ), m_pFrame( NULL ), m_pPacket( NULL ),
	m_nStream( -1 ), m_bDraining( false ), m_nWidth( 0 ), m_nHeight( 0 ), m_flFrameRate( 30.0f ), m_nFrameCount( 0 ),
	m_nCurrentFrame( -1 ), m_bLoop( false ), m_bFinished( false ), m_bPaused( false ), m_flNextFrameTime( 0.0 )
{
}

CFFmpegBikMaterial::~CFFmpegBikMaterial()
{
	Shutdown();
}

int CFFmpegBikMaterial::ReadPacket( void *pOpaque, uint8_t *pBuf, int nBufSize )
{
	CFFmpegBikMaterial *pThis = (CFFmpegBikMaterial *)pOpaque;
	const int64_t nSize = pThis->m_pMovie->m_Data.TellPut();
	const int64_t nLeft = nSize - pThis->m_nReadPos;
	if ( nLeft <= 0 )
		return AVERROR_EOF;

	const int nRead = (int)MIN( (int64_t)nBufSize, nLeft );
	V_memcpy( pBuf, (const unsigned char *)pThis->m_pMovie->m_Data.Base() + pThis->m_nReadPos, nRead );
	pThis->m_nReadPos += nRead;
	return nRead;
}

int64_t CFFmpegBikMaterial::Seek( void *pOpaque, int64_t nOffset, int nWhence )
{
	CFFmpegBikMaterial *pThis = (CFFmpegBikMaterial *)pOpaque;
	const int64_t nSize = pThis->m_pMovie->m_Data.TellPut();
	if ( nWhence & AVSEEK_SIZE )
		return nSize;

	int64_t nPos;
	switch ( nWhence & ~AVSEEK_FORCE )
	{
	case SEEK_SET: nPos = nOffset; break;
	case SEEK_CUR: nPos = pThis->m_nReadPos + nOffset; break;
	case SEEK_END: nPos = nSize + nOffset; break;
	default: return -1;
	}
	if ( nPos < 0 || nPos > nSize )
		return -1;
	pThis->m_nReadPos = nPos;
	return nPos;
}

bool CFFmpegBikMaterial::OpenDecoder()
{
	CloseDecoder();
	m_nReadPos = 0;

	const int nIOBufferSize = 32 * 1024;
	unsigned char *pIOBuffer = (unsigned char *)av_malloc( nIOBufferSize );
	if ( !pIOBuffer )
		return false;
	m_pIO = avio_alloc_context( pIOBuffer, nIOBufferSize, 0, this, &CFFmpegBikMaterial::ReadPacket, NULL, &CFFmpegBikMaterial::Seek );
	if ( !m_pIO )
	{
		av_free( pIOBuffer );
		return false;
	}

	m_pFormat = avformat_alloc_context();
	if ( !m_pFormat )
		return false;
	m_pFormat->pb = m_pIO;
	m_pFormat->flags |= AVFMT_FLAG_CUSTOM_IO;

	const AVInputFormat *pInputFormat = av_find_input_format( "bink" );
	if ( avformat_open_input( &m_pFormat, "", pInputFormat, NULL ) < 0 )
	{
		// avformat_open_input frees the context on failure
		m_pFormat = NULL;
		return false;
	}
	if ( avformat_find_stream_info( m_pFormat, NULL ) < 0 )
		return false;

	m_nStream = av_find_best_stream( m_pFormat, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0 );
	if ( m_nStream < 0 )
		return false;

	AVStream *pStream = m_pFormat->streams[m_nStream];
	const AVCodec *pDecoder = avcodec_find_decoder( pStream->codecpar->codec_id );
	if ( !pDecoder )
		return false;

	m_pCodec = avcodec_alloc_context3( pDecoder );
	if ( !m_pCodec || avcodec_parameters_to_context( m_pCodec, pStream->codecpar ) < 0 )
		return false;
	if ( avcodec_open2( m_pCodec, pDecoder, NULL ) < 0 )
		return false;

	m_pFrame = av_frame_alloc();
	m_pPacket = av_packet_alloc();
	if ( !m_pFrame || !m_pPacket )
		return false;

	m_nWidth = m_pCodec->width;
	m_nHeight = m_pCodec->height;

	AVRational rate = pStream->avg_frame_rate;
	if ( rate.num <= 0 || rate.den <= 0 )
		rate = pStream->r_frame_rate;
	m_flFrameRate = ( rate.num > 0 && rate.den > 0 ) ? (float)av_q2d( rate ) : 30.0f;
	m_flFrameRate = clamp( m_flFrameRate, 1.0f, 120.0f );

	m_nFrameCount = (int)pStream->nb_frames;
	if ( m_nFrameCount <= 0 && pStream->duration > 0 )
		m_nFrameCount = (int)pStream->duration;

	m_bDraining = false;
	m_nCurrentFrame = -1;
	return m_nWidth > 0 && m_nHeight > 0;
}

void CFFmpegBikMaterial::CloseDecoder()
{
	if ( m_pPacket )
		av_packet_free( &m_pPacket );
	if ( m_pFrame )
		av_frame_free( &m_pFrame );
	if ( m_pCodec )
		avcodec_free_context( &m_pCodec );
	if ( m_pFormat )
		avformat_close_input( &m_pFormat );
	if ( m_pIO )
	{
		av_freep( &m_pIO->buffer );
		avio_context_free( &m_pIO );
	}
	m_nStream = -1;
}

// Decodes the next video frame into m_pFrame. Loops back to the start when the movie is set to loop.
bool CFFmpegBikMaterial::DecodeFrame()
{
	if ( !m_pCodec )
		return false;

	int nRestarts = 0;
	for ( int nGuard = 0; nGuard < 4096; ++nGuard )
	{
		int nResult = avcodec_receive_frame( m_pCodec, m_pFrame );
		if ( nResult == 0 )
		{
			++m_nCurrentFrame;
			return true;
		}

		if ( nResult == AVERROR_EOF )
		{
			if ( !m_bLoop || nRestarts++ > 0 || !OpenDecoder() )
				return false;
			continue;
		}

		if ( nResult != AVERROR( EAGAIN ) )
			return false;

		if ( m_bDraining )
			return false;

		nResult = av_read_frame( m_pFormat, m_pPacket );
		if ( nResult < 0 )
		{
			// End of file: flush the frames the decoder still holds
			m_bDraining = true;
			avcodec_send_packet( m_pCodec, NULL );
			continue;
		}

		if ( m_pPacket->stream_index == m_nStream )
			avcodec_send_packet( m_pCodec, m_pPacket );
		av_packet_unref( m_pPacket );
	}
	return false;
}

// YUV (BT.601 studio range, the same matrix as the Bik shader) to RGBA
void CFFmpegBikMaterial::ConvertFrame()
{
	const AVFrame *pFrame = m_pFrame;
	const int nWidth = MIN( pFrame->width, m_nWidth );
	const int nHeight = MIN( pFrame->height, m_nHeight );
	const bool bYUV = ( pFrame->format == AV_PIX_FMT_YUV420P || pFrame->format == AV_PIX_FMT_YUVA420P );
	if ( !bYUV || !pFrame->data[0] || !pFrame->data[1] || !pFrame->data[2] )
		return;
	const bool bAlpha = ( pFrame->format == AV_PIX_FMT_YUVA420P ) && pFrame->data[3];

	for ( int y = 0; y < nHeight; ++y )
	{
		const unsigned char *pY = pFrame->data[0] + y * pFrame->linesize[0];
		const unsigned char *pU = pFrame->data[1] + ( y >> 1 ) * pFrame->linesize[1];
		const unsigned char *pV = pFrame->data[2] + ( y >> 1 ) * pFrame->linesize[2];
		const unsigned char *pA = bAlpha ? pFrame->data[3] + y * pFrame->linesize[3] : NULL;
		unsigned char *pOut = m_RGBA.Base() + y * m_nWidth * 4;

		for ( int x = 0; x < nWidth; ++x )
		{
			const int c = 298 * ( pY[x] - 16 );
			const int d = pU[x >> 1] - 128;
			const int e = pV[x >> 1] - 128;
			const int r = ( c + 409 * e + 128 ) >> 8;
			const int g = ( c - 100 * d - 208 * e + 128 ) >> 8;
			const int b = ( c + 516 * d + 128 ) >> 8;
			pOut[0] = (unsigned char)clamp( r, 0, 255 );
			pOut[1] = (unsigned char)clamp( g, 0, 255 );
			pOut[2] = (unsigned char)clamp( b, 0, 255 );
			pOut[3] = pA ? pA[x] : 255;
			pOut += 4;
		}
	}
}

bool CFFmpegBikMaterial::Init( const char *pMaterialName, const char *pFileName, const char *pPathID, int nFlags )
{
	m_pMovie = Bik_AcquireMovie( pFileName, pPathID );
	if ( !m_pMovie )
	{
		Msg( "[bik] %s: file not found\n", pFileName );
		return false;
	}

	m_bLoop = ( nFlags & BIK_LOOP ) != 0;
	if ( !OpenDecoder() || !DecodeFrame() )
	{
		Msg( "[bik] %s: FFmpeg could not open or decode it\n", pFileName );
		return false;
	}

	m_RGBA.SetCount( m_nWidth * m_nHeight * 4 );
	V_memset( m_RGBA.Base(), 0, m_RGBA.Count() );
	ConvertFrame();

	char szTextureName[MAX_PATH];
	V_snprintf( szTextureName, sizeof( szTextureName ), "%s_rgba", pMaterialName );
	m_Texture.InitProceduralTexture( szTextureName, "bik", m_nWidth, m_nHeight, IMAGE_FORMAT_RGBA8888,
		TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_NOLOD | TEXTUREFLAGS_PROCEDURAL | TEXTUREFLAGS_SINGLECOPY );
	if ( !m_Texture.IsValid() )
		return false;
	m_Texture->SetTextureRegenerator( this );
	m_Texture->Download();

	// Raw read and write like the Bik shader, which has sRGB write off
	KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetString( "$basetexture", szTextureName );
	pVMTKeyValues->SetInt( "$nofog", 1 );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	pVMTKeyValues->SetInt( "$gammacolorread", 1 );
	pVMTKeyValues->SetInt( "$linearwrite", 1 );
	m_Material.Init( pMaterialName, pVMTKeyValues );
	m_Material->Refresh();

	m_flNextFrameTime = Plat_FloatTime() + 1.0 / m_flFrameRate;

	Msg( "[bik] %s: %dx%d, %.2f fps, %d frames%s, %d KB\n", pFileName, m_nWidth, m_nHeight, m_flFrameRate, m_nFrameCount,
		m_bLoop ? ", looping" : "", m_pMovie->m_Data.TellPut() / 1024 );
	return true;
}

void CFFmpegBikMaterial::Shutdown()
{
	m_Material.Shutdown();
	if ( m_Texture.IsValid() )
	{
		m_Texture->SetTextureRegenerator( NULL );
		m_Texture.Shutdown( true );
	}
	CloseDecoder();
	Bik_ReleaseMovie( m_pMovie );
	m_pMovie = NULL;
}

bool CFFmpegBikMaterial::ReadyForSwap() const
{
	return !m_bPaused && Plat_FloatTime() >= m_flNextFrameTime;
}

bool CFFmpegBikMaterial::Update()
{
	if ( m_bFinished )
		return false;
	if ( m_bPaused )
		return true;

	const double flNow = Plat_FloatTime();
	if ( flNow < m_flNextFrameTime )
		return true;

	// Catch up by at most two frames; after a long stall (alt-tab, loading) just resync
	const double flFrameTime = 1.0 / m_flFrameRate;
	const double flLate = flNow - m_flNextFrameTime;
	const bool bResync = flLate > 0.25;
	const int nWanted = bResync ? 1 : MIN( 2, 1 + (int)( flLate / flFrameTime ) );
	int nDecoded = 0;
	while ( nDecoded < nWanted )
	{
		if ( !DecodeFrame() )
		{
			m_bFinished = true;
			break;
		}
		++nDecoded;
	}

	if ( nDecoded )
	{
		ConvertFrame();
		m_Texture->Download();
	}

	m_flNextFrameTime = bResync ? flNow + flFrameTime : m_flNextFrameTime + flFrameTime * nDecoded;
	if ( m_flNextFrameTime <= flNow )
		m_flNextFrameTime = flNow + flFrameTime;

	return !m_bFinished;
}

void CFFmpegBikMaterial::SetFrame( float flFrame )
{
	int nTarget = MAX( 0, (int)flFrame );
	if ( m_nFrameCount > 0 )
		nTarget = MIN( nTarget, m_nFrameCount - 1 );

	if ( nTarget == m_nCurrentFrame )
		return;

	if ( nTarget < m_nCurrentFrame && !OpenDecoder() )
	{
		m_bFinished = true;
		return;
	}

	m_bFinished = false;
	while ( m_nCurrentFrame < nTarget )
	{
		if ( !DecodeFrame() )
			break;
	}
	ConvertFrame();
	m_Texture->Download();
	m_flNextFrameTime = Plat_FloatTime() + 1.0 / m_flFrameRate;
}

void CFFmpegBikMaterial::Pause()
{
	m_bPaused = true;
}

void CFFmpegBikMaterial::Unpause()
{
	if ( m_bPaused )
		m_flNextFrameTime = Plat_FloatTime() + 1.0 / m_flFrameRate;
	m_bPaused = false;
}

void CFFmpegBikMaterial::GetTexCoordRange( float *pMaxU, float *pMaxV )
{
	int nTextureWidth = m_Texture.IsValid() ? m_Texture->GetActualWidth() : 0;
	int nTextureHeight = m_Texture.IsValid() ? m_Texture->GetActualHeight() : 0;
	*pMaxU = nTextureWidth ? (float)m_nWidth / (float)nTextureWidth : 1.0f;
	*pMaxV = nTextureHeight ? (float)m_nHeight / (float)nTextureHeight : 1.0f;
}

void CFFmpegBikMaterial::RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect )
{
	const int nWidth = MIN( pVTFTexture->Width(), m_nWidth );
	const int nHeight = MIN( pVTFTexture->Height(), m_nHeight );
	if ( m_RGBA.Count() < m_nWidth * m_nHeight * 4 )
		return;

	if ( pVTFTexture->Format() == IMAGE_FORMAT_RGBA8888 )
	{
		unsigned char *pDest = pVTFTexture->ImageData();
		const int nDestStride = pVTFTexture->RowSizeInBytes( 0 );
		for ( int y = 0; y < nHeight; ++y )
			V_memcpy( pDest + y * nDestStride, m_RGBA.Base() + y * m_nWidth * 4, nWidth * 4 );
		return;
	}

	CPixelWriter pixelWriter;
	pixelWriter.SetPixelMemory( pVTFTexture->Format(), pVTFTexture->ImageData(), pVTFTexture->RowSizeInBytes( 0 ) );
	for ( int y = 0; y < nHeight; ++y )
	{
		pixelWriter.Seek( 0, y );
		const unsigned char *pSrc = m_RGBA.Base() + y * m_nWidth * 4;
		for ( int x = 0; x < nWidth; ++x, pSrc += 4 )
			pixelWriter.WritePixel( pSrc[0], pSrc[1], pSrc[2], pSrc[3] );
	}
}

//-----------------------------------------------------------------------------
// IBik
//-----------------------------------------------------------------------------
class CFFmpegBik : public CBaseAppSystem< IBik >
{
public:
	CFFmpegBik() : m_nAllocationNumber( 0 ) {}

	virtual BIKMaterial_t CreateMaterial( const char *pMaterialName, const char *pFileName, const char *pPathID, int flags );
	virtual void DestroyMaterial( BIKMaterial_t hMaterial );
	virtual bool Update( BIKMaterial_t hMaterial ) { CFFmpegBikMaterial *p = Get( hMaterial ); return p ? p->Update() : false; }
	virtual bool ReadyForSwap( BIKMaterial_t hMaterial ) { CFFmpegBikMaterial *p = Get( hMaterial ); return p ? p->ReadyForSwap() : false; }
	virtual IMaterial *GetMaterial( BIKMaterial_t hMaterial ) { CFFmpegBikMaterial *p = Get( hMaterial ); return p ? p->GetMaterial() : NULL; }
	virtual void GetTexCoordRange( BIKMaterial_t hMaterial, float *pMaxU, float *pMaxV );
	virtual void GetFrameSize( BIKMaterial_t hMaterial, int *pWidth, int *pHeight );
	virtual int GetFrameRate( BIKMaterial_t hMaterial ) { CFFmpegBikMaterial *p = Get( hMaterial ); return p ? p->GetFrameRate() : -1; }
	virtual int GetFrameCount( BIKMaterial_t hMaterial ) { CFFmpegBikMaterial *p = Get( hMaterial ); return p ? p->GetFrameCount() : -1; }
	virtual int GetFrame( BIKMaterial_t hMaterial ) { CFFmpegBikMaterial *p = Get( hMaterial ); return p ? p->GetFrame() : -1; }
	virtual void SetFrame( BIKMaterial_t hMaterial, float flFrame ) { CFFmpegBikMaterial *p = Get( hMaterial ); if ( p ) p->SetFrame( flFrame ); }
	virtual void Pause( BIKMaterial_t hMaterial ) { CFFmpegBikMaterial *p = Get( hMaterial ); if ( p ) p->Pause(); }
	virtual void Unpause( BIKMaterial_t hMaterial ) { CFFmpegBikMaterial *p = Get( hMaterial ); if ( p ) p->Unpause(); }
	virtual int GetGlobalMaterialAllocationNumber( void ) { return m_nAllocationNumber; }
	virtual bool PrecacheMovie( const char *pFileName, const char *pPathID );
	virtual void *GetPrecachedMovie( const char *pFileName );
	virtual void EvictPrecachedMovie( const char *pFileName );
	virtual void EvictAllPrecachedMovies();
	virtual void UpdateVolume( BIKMaterial_t hMaterial ) {}
	virtual bool IsMovieResidentInMemory( BIKMaterial_t hMaterial ) { return Get( hMaterial ) != NULL; }

private:
	CFFmpegBikMaterial *Get( BIKMaterial_t hMaterial )
	{
		return m_Materials.IsValidIndex( hMaterial ) ? m_Materials[hMaterial] : NULL;
	}

	CUtlLinkedList< CFFmpegBikMaterial *, BIKMaterial_t > m_Materials;
	int m_nAllocationNumber;
};

BIKMaterial_t CFFmpegBik::CreateMaterial( const char *pMaterialName, const char *pFileName, const char *pPathID, int flags )
{
	if ( !pFileName || !pFileName[0] )
		return BIKMATERIAL_INVALID;

	CFFmpegBikMaterial *pMaterial = new CFFmpegBikMaterial;
	if ( !pMaterial->Init( pMaterialName, pFileName, pPathID, flags ) )
	{
		delete pMaterial;
		return BIKMATERIAL_INVALID;
	}

	++m_nAllocationNumber;
	return m_Materials.AddToTail( pMaterial );
}

void CFFmpegBik::DestroyMaterial( BIKMaterial_t hMaterial )
{
	CFFmpegBikMaterial *pMaterial = Get( hMaterial );
	if ( !pMaterial )
		return;

	m_Materials.Remove( hMaterial );
	delete pMaterial;
}

void CFFmpegBik::GetTexCoordRange( BIKMaterial_t hMaterial, float *pMaxU, float *pMaxV )
{
	CFFmpegBikMaterial *p = Get( hMaterial );
	if ( p )
		p->GetTexCoordRange( pMaxU, pMaxV );
	else
		*pMaxU = *pMaxV = 1.0f;
}

void CFFmpegBik::GetFrameSize( BIKMaterial_t hMaterial, int *pWidth, int *pHeight )
{
	CFFmpegBikMaterial *p = Get( hMaterial );
	if ( p )
		p->GetFrameSize( pWidth, pHeight );
	else
		*pWidth = *pHeight = 0;
}

bool CFFmpegBik::PrecacheMovie( const char *pFileName, const char *pPathID )
{
	int i = s_BikMovies.Find( pFileName );
	if ( i != s_BikMovies.InvalidIndex() && s_BikMovies[i]->m_bPrecached )
		return true;

	// The precache holds one reference until it is evicted
	BikMovieData_t *pMovie = Bik_AcquireMovie( pFileName, pPathID );
	if ( !pMovie )
		return false;
	pMovie->m_bPrecached = true;
	return true;
}

void *CFFmpegBik::GetPrecachedMovie( const char *pFileName )
{
	int i = s_BikMovies.Find( pFileName );
	if ( i == s_BikMovies.InvalidIndex() || !s_BikMovies[i]->m_bPrecached )
		return NULL;
	return s_BikMovies[i]->m_Data.Base();
}

void CFFmpegBik::EvictPrecachedMovie( const char *pFileName )
{
	int i = s_BikMovies.Find( pFileName );
	if ( i == s_BikMovies.InvalidIndex() || !s_BikMovies[i]->m_bPrecached )
		return;

	BikMovieData_t *pMovie = s_BikMovies[i];
	pMovie->m_bPrecached = false;
	Bik_ReleaseMovie( pMovie );
}

void CFFmpegBik::EvictAllPrecachedMovies()
{
	CUtlVector< CUtlString > names;
	for ( int i = s_BikMovies.First(); i != s_BikMovies.InvalidIndex(); i = s_BikMovies.Next( i ) )
	{
		if ( s_BikMovies[i]->m_bPrecached )
			names.AddToTail( s_BikMovies.GetElementName( i ) );
	}
	for ( int i = 0; i < names.Count(); ++i )
		EvictPrecachedMovie( names[i].Get() );
}

static CFFmpegBik s_FFmpegBik;

// Called after the client and GameUI connect their interfaces, which leave the Bink pointers NULL on iOS
void IOS_InstallFFmpegBik()
{
	g_pBIK = &s_FFmpegBik;
	bik = &s_FFmpegBik;
}

#endif // IOS
