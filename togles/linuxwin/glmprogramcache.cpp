//========= Source Engine iOS modernization ==============================//
//
// Purpose: On-disk GL program binary cache. See glmprogramcache.h for the
//          rationale and the safety model.
//
//=======================================================================//

#include "togles/rendermechanism.h"
#include "glmprogramcache.h"

#if defined( SRC_PRECOMPILED_SHADERS )

#include "tier0/icommandline.h"
#include "tier0/dbg.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlmap.h"
#include "tier1/utlstring.h"
#include "tier1/checksum_crc.h"
#include "filesystem.h"

#include <sys/stat.h>
#include <stdio.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Bump this whenever the on-disk layout changes so old caches are ignored
// rather than misread.
//-----------------------------------------------------------------------------
#define SHADERCACHE_VERSION		2
#define SHADERCACHE_MAGIC		MAKEID( 'G','L','P','C' )

// Refuse absurd blobs rather than trusting the file's own length field.
#define SHADERCACHE_MAX_BLOB	( 8 * 1024 * 1024 )

struct ShaderCacheEntry_t
{
	GLenum		m_nBinaryFormat;
	CUtlBuffer	m_Binary;
};

static bool									s_bCacheChecked  = false;
static bool									s_bCacheEnabled  = false;
static bool									s_bCacheInited   = false;
static bool									s_bCacheDirty    = false;
static bool									s_bDriverSupport = false;
static CUtlMap< uint64, ShaderCacheEntry_t * >	s_Cache( DefLessFunc( uint64 ) );
static char									s_szCachePath[ 512 ] = { 0 };

//-----------------------------------------------------------------------------
// Runtime gate. Default OFF: the user must pass -precompiledshaders.
//-----------------------------------------------------------------------------
bool ShaderCache_IsEnabled( void )
{
	if ( !s_bCacheChecked )
	{
		s_bCacheChecked = true;
		s_bCacheEnabled = ( CommandLine()->FindParm( "-precompiledshaders" ) != 0 );
	}
	return s_bCacheEnabled;
}

//-----------------------------------------------------------------------------
// Key a program on the exact text of both stages.
//
// A 64-bit key from two independent 32-bit CRCs. Collisions are not a
// correctness problem in the usual sense - a wrong binary fails to link
// and we fall back to compiling - but a wider key makes that vanishingly
// rare in the first place.
//-----------------------------------------------------------------------------
static uint64 ShaderCache_HashProgram( const char *pVertexText, const char *pFragmentText )
{
	if ( !pVertexText )   pVertexText = "";
	if ( !pFragmentText ) pFragmentText = "";

	const int nVertexLen   = V_strlen( pVertexText );
	const int nFragmentLen = V_strlen( pFragmentText );

	CRC32_t crcVertex = CRC32_ProcessSingleBuffer( pVertexText, nVertexLen );
	CRC32_t crcFrag   = CRC32_ProcessSingleBuffer( pFragmentText, nFragmentLen );

	// Fold the lengths in as well so two texts that collide on CRC still
	// have to match in size to collide overall.
	uint64 nKey = ( (uint64)crcVertex << 32 ) ^ (uint64)crcFrag;
	nKey ^= ( (uint64)nVertexLen << 16 ) ^ (uint64)nFragmentLen;
	return nKey;
}

//-----------------------------------------------------------------------------
// Fingerprint the driver. A program binary is only valid for the exact
// driver build that produced it; the binaryFormat token alone does not
// capture an OS/driver update.
//-----------------------------------------------------------------------------
static CRC32_t ShaderCache_DriverFingerprint( void )
{
	CRC32_t crc;
	CRC32_Init( &crc );

	const GLenum kStrings[] = { GL_VENDOR, GL_RENDERER, GL_VERSION };
	for ( int i = 0; i < ARRAYSIZE( kStrings ); ++i )
	{
		const GLubyte *pStr = gGL->glGetString( kStrings[ i ] );
		if ( pStr )
			CRC32_ProcessBuffer( &crc, pStr, V_strlen( (const char *)pStr ) );
	}

	const int nVersion = SHADERCACHE_VERSION;
	CRC32_ProcessBuffer( &crc, &nVersion, sizeof( nVersion ) );

	CRC32_Final( &crc );
	return crc;
}

//-----------------------------------------------------------------------------
static void ShaderCache_Clear( void )
{
	for ( unsigned short i = s_Cache.FirstInorder(); i != s_Cache.InvalidIndex(); i = s_Cache.NextInorder( i ) )
	{
		delete s_Cache[ i ];
	}
	s_Cache.RemoveAll();
}

//-----------------------------------------------------------------------------
// File layout:
//   uint32  magic
//   uint32  version
//   uint32  driver fingerprint
//   uint32  entry count
//   [ uint64 key, uint32 binaryFormat, uint32 length, bytes... ] * count
//-----------------------------------------------------------------------------
static void ShaderCache_Load( void )
{
	FILE *pFile = fopen( s_szCachePath, "rb" );
	if ( !pFile )
		return;

	uint32 nMagic = 0, nVersion = 0, nFingerprint = 0, nCount = 0;

	if ( fread( &nMagic, sizeof( nMagic ), 1, pFile ) != 1 ||
		 fread( &nVersion, sizeof( nVersion ), 1, pFile ) != 1 ||
		 fread( &nFingerprint, sizeof( nFingerprint ), 1, pFile ) != 1 ||
		 fread( &nCount, sizeof( nCount ), 1, pFile ) != 1 )
	{
		fclose( pFile );
		return;
	}

	if ( nMagic != SHADERCACHE_MAGIC || nVersion != SHADERCACHE_VERSION )
	{
		DevMsg( "ShaderCache: format mismatch, discarding.\n" );
		fclose( pFile );
		return;
	}

	if ( nFingerprint != (uint32)ShaderCache_DriverFingerprint() )
	{
		// Driver changed under us - every stored binary is now invalid.
		DevMsg( "ShaderCache: driver changed, discarding cache.\n" );
		fclose( pFile );
		return;
	}

	uint32 nLoaded = 0;
	for ( uint32 i = 0; i < nCount; ++i )
	{
		uint64 nKey = 0;
		uint32 nFormat = 0, nLength = 0;

		if ( fread( &nKey, sizeof( nKey ), 1, pFile ) != 1 ||
			 fread( &nFormat, sizeof( nFormat ), 1, pFile ) != 1 ||
			 fread( &nLength, sizeof( nLength ), 1, pFile ) != 1 )
		{
			break;	// truncated file; keep whatever we already read
		}

		if ( nLength == 0 || nLength > SHADERCACHE_MAX_BLOB )
			break;	// corrupt length, stop trusting the rest

		ShaderCacheEntry_t *pEntry = new ShaderCacheEntry_t;
		pEntry->m_nBinaryFormat = (GLenum)nFormat;
		pEntry->m_Binary.EnsureCapacity( nLength );

		if ( fread( pEntry->m_Binary.Base(), 1, nLength, pFile ) != nLength )
		{
			delete pEntry;
			break;
		}
		pEntry->m_Binary.SeekPut( CUtlBuffer::SEEK_HEAD, nLength );

		if ( s_Cache.Find( nKey ) == s_Cache.InvalidIndex() )
		{
			s_Cache.Insert( nKey, pEntry );
			nLoaded++;
		}
		else
		{
			delete pEntry;
		}
	}

	fclose( pFile );
	DevMsg( "ShaderCache: loaded %u program binaries.\n", nLoaded );
}

//-----------------------------------------------------------------------------
static void ShaderCache_Save( void )
{
	if ( !s_bCacheDirty || s_Cache.Count() == 0 )
		return;

	// Write to a temporary file and rename, so a crash mid-write cannot
	// leave a half-written cache behind.
	char szTemp[ 576 ];
	V_snprintf( szTemp, sizeof( szTemp ), "%s.tmp", s_szCachePath );

	FILE *pFile = fopen( szTemp, "wb" );
	if ( !pFile )
		return;

	const uint32 nMagic       = SHADERCACHE_MAGIC;
	const uint32 nVersion     = SHADERCACHE_VERSION;
	const uint32 nFingerprint = (uint32)ShaderCache_DriverFingerprint();
	const uint32 nCount       = (uint32)s_Cache.Count();

	fwrite( &nMagic, sizeof( nMagic ), 1, pFile );
	fwrite( &nVersion, sizeof( nVersion ), 1, pFile );
	fwrite( &nFingerprint, sizeof( nFingerprint ), 1, pFile );
	fwrite( &nCount, sizeof( nCount ), 1, pFile );

	for ( unsigned short i = s_Cache.FirstInorder(); i != s_Cache.InvalidIndex(); i = s_Cache.NextInorder( i ) )
	{
		const uint64 nKey = s_Cache.Key( i );
		ShaderCacheEntry_t *pEntry = s_Cache[ i ];

		const uint32 nFormat = (uint32)pEntry->m_nBinaryFormat;
		const uint32 nLength = (uint32)pEntry->m_Binary.TellPut();

		fwrite( &nKey, sizeof( nKey ), 1, pFile );
		fwrite( &nFormat, sizeof( nFormat ), 1, pFile );
		fwrite( &nLength, sizeof( nLength ), 1, pFile );
		fwrite( pEntry->m_Binary.Base(), 1, nLength, pFile );
	}

	fclose( pFile );

	rename( szTemp, s_szCachePath );
	s_bCacheDirty = false;

	DevMsg( "ShaderCache: wrote %u program binaries.\n", nCount );
}

//-----------------------------------------------------------------------------
void ShaderCache_Init( void )
{
	if ( s_bCacheInited || !ShaderCache_IsEnabled() )
		return;

	s_bCacheInited = true;

	// The driver must actually expose the entry points. They are declared
	// optional, so check before using them.
	s_bDriverSupport = ( gGL->glGetProgramBinary != NULL ) && ( gGL->glProgramBinary != NULL );
	if ( !s_bDriverSupport )
	{
		Warning( "ShaderCache: driver has no program binary support, disabling.\n" );
		s_bCacheEnabled = false;
		return;
	}

	// Writable location on iOS. Fall back to /tmp rather than failing.
	const char *pBase = getenv( "HOME" );
	if ( pBase && *pBase )
		V_snprintf( s_szCachePath, sizeof( s_szCachePath ), "%s/Library/Caches/glprogramcache.bin", pBase );
	else
		V_snprintf( s_szCachePath, sizeof( s_szCachePath ), "/tmp/glprogramcache.bin" );

	ShaderCache_Load();
}

//-----------------------------------------------------------------------------
void ShaderCache_Shutdown( void )
{
	if ( !s_bCacheInited )
		return;

	ShaderCache_Save();
	ShaderCache_Clear();
	s_bCacheInited = false;
}

//-----------------------------------------------------------------------------
bool ShaderCache_TryLoadProgram( uint nProgram, const char *pVertexText, const char *pFragmentText )
{
	if ( !ShaderCache_IsEnabled() || !s_bDriverSupport )
		return false;

	ShaderCache_Init();
	if ( !s_bDriverSupport )
		return false;

	const uint64 nKey = ShaderCache_HashProgram( pVertexText, pFragmentText );

	unsigned short nIndex = s_Cache.Find( nKey );
	if ( nIndex == s_Cache.InvalidIndex() )
		return false;

	ShaderCacheEntry_t *pEntry = s_Cache[ nIndex ];

	// Clear any pending error so we can attribute the next one correctly.
	while ( gGL->glGetError() != GL_NO_ERROR ) {}

	gGL->glProgramBinary( nProgram,
						  pEntry->m_nBinaryFormat,
						  pEntry->m_Binary.Base(),
						  (GLsizei)pEntry->m_Binary.TellPut() );

	// glProgramBinary is permitted to fail for any reason at all - a
	// driver update, a format it no longer accepts, anything. Failure is
	// reported through the link status, not just glGetError.
	GLint nLinked = 0;
	gGL->glGetProgramiv( nProgram, GL_LINK_STATUS, &nLinked );

	if ( gGL->glGetError() != GL_NO_ERROR || nLinked == GL_FALSE )
	{
		// Drop the bad entry so we do not retry it every single time.
		delete pEntry;
		s_Cache.RemoveAt( nIndex );
		s_bCacheDirty = true;
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Tell the driver we intend to read this program's binary back. Must be
// called before glLinkProgram; without it glGetProgramBinary is allowed to
// return nothing at all.
//-----------------------------------------------------------------------------
void ShaderCache_MarkRetrievable( uint nProgram )
{
	if ( !ShaderCache_IsEnabled() )
		return;

	ShaderCache_Init();

	if ( !s_bDriverSupport || gGL->glProgramParameteri == NULL )
		return;

	gGL->glProgramParameteri( nProgram, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE );

	// Not all drivers accept the hint; it is advisory, so swallow the error.
	while ( gGL->glGetError() != GL_NO_ERROR ) {}
}

//-----------------------------------------------------------------------------
void ShaderCache_StoreProgram( uint nProgram, const char *pVertexText, const char *pFragmentText )
{
	if ( !ShaderCache_IsEnabled() || !s_bDriverSupport )
		return;

	const uint64 nKey = ShaderCache_HashProgram( pVertexText, pFragmentText );
	if ( s_Cache.Find( nKey ) != s_Cache.InvalidIndex() )
		return;	// already have it

	GLint nLength = 0;
	gGL->glGetProgramiv( nProgram, GL_PROGRAM_BINARY_LENGTH, &nLength );

	if ( nLength <= 0 || nLength > SHADERCACHE_MAX_BLOB )
		return;

	ShaderCacheEntry_t *pEntry = new ShaderCacheEntry_t;
	pEntry->m_Binary.EnsureCapacity( nLength );

	GLsizei nWritten = 0;
	GLenum  nFormat  = 0;

	gGL->glGetProgramBinary( nProgram, nLength, &nWritten, &nFormat, pEntry->m_Binary.Base() );

	if ( nWritten <= 0 )
	{
		delete pEntry;
		return;
	}

	pEntry->m_nBinaryFormat = nFormat;
	pEntry->m_Binary.SeekPut( CUtlBuffer::SEEK_HEAD, nWritten );

	s_Cache.Insert( nKey, pEntry );
	s_bCacheDirty = true;
}

#endif // SRC_PRECOMPILED_SHADERS
