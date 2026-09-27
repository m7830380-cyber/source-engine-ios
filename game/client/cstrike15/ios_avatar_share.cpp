//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: LAN profile pictures (iOS), client side. See ios_avatar_share.h.
//
//   upload:   profile_avatar (PNG/JPG in the csgo folder) -> center square ->
//             64x64 RGB -> base64url -> "ios_avatar begin/data/end", a few
//             commands per second (the server kicks above 40 string commands/s)
//   download: the IOSAvatars string table -> cache/avatars/<account ID>.tga,
//             which the Scaleform avatar loader ("img://avatar_<xuid>") shows
//
//=============================================================================//
#include "cbase.h"

#if defined( IOS )

#include "ios_avatar_share.h"
#include "networkstringtabledefs.h"
#include "filesystem.h"
#include "tier1/checksum_crc.h"
#include "igamesystem.h"

#define STBI_NO_HDR
#define STBI_NO_WRITE
#include "bitmap/stb_image.c"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern INetworkStringTableContainer *networkstringtable;

static CUtlVector< CUtlString > s_AvatarCommands;	// upload queue
static int s_nAvatarVersion = 0;

int IOSAvatar_GetVersion()
{
	return s_nAvatarVersion;
}

// profile_avatar -> IOS_AVATAR_SIZE square RGB (the center square of the picture, box-filtered)
static bool LoadAvatarPixels( unsigned char *pOut )
{
	static ConVarRef profile_avatar( "profile_avatar" );
	const char *pszFile = profile_avatar.IsValid() ? profile_avatar.GetString() : "";
	if ( !pszFile || !pszFile[0] )
		return false;

	CUtlBuffer buf;
	if ( !g_pFullFileSystem->ReadFile( pszFile, "GAME", buf ) )
	{
		Warning( "[avatar] couldn't read profile_avatar '%s'\n", pszFile );
		return false;
	}

	int w = 0, h = 0, comp = 0;
	unsigned char *pImage = stbi_load_from_memory( (unsigned char *)buf.Base(), buf.TellPut(), &w, &h, &comp, 3 );
	if ( !pImage || w <= 0 || h <= 0 )
	{
		Warning( "[avatar] couldn't decode profile_avatar '%s' (PNG or JPG)\n", pszFile );
		if ( pImage )
			stbi_image_free( pImage );
		return false;
	}

	int nSide = MIN( w, h );
	int x0 = ( w - nSide ) / 2, y0 = ( h - nSide ) / 2;
	for ( int y = 0; y < IOS_AVATAR_SIZE; y++ )
	{
		int sy0 = y0 + y * nSide / IOS_AVATAR_SIZE, sy1 = MAX( sy0 + 1, y0 + ( y + 1 ) * nSide / IOS_AVATAR_SIZE );
		for ( int x = 0; x < IOS_AVATAR_SIZE; x++ )
		{
			int sx0 = x0 + x * nSide / IOS_AVATAR_SIZE, sx1 = MAX( sx0 + 1, x0 + ( x + 1 ) * nSide / IOS_AVATAR_SIZE );
			unsigned int sum[3] = { 0, 0, 0 }, n = 0;
			for ( int sy = sy0; sy < sy1; sy++ )
			{
				for ( int sx = sx0; sx < sx1; sx++ )
				{
					const unsigned char *p = pImage + ( sy * w + sx ) * 3;
					sum[0] += p[0]; sum[1] += p[1]; sum[2] += p[2];
					n++;
				}
			}
			unsigned char *pd = pOut + ( y * IOS_AVATAR_SIZE + x ) * 3;
			pd[0] = (unsigned char)( sum[0] / n );
			pd[1] = (unsigned char)( sum[1] / n );
			pd[2] = (unsigned char)( sum[2] / n );
		}
	}
	stbi_image_free( pImage );
	return true;
}

void IOSAvatar_SendLocal()
{
	s_AvatarCommands.RemoveAll();

	static unsigned char s_Pixels[ IOS_AVATAR_BYTES ];
	if ( !LoadAvatarPixels( s_Pixels ) )
		return;

	static char s_szBase64[ IOS_AVATAR_BASE64_MAX + 1 ];
	int nChars = IOSAvatar_Base64Encode( s_Pixels, IOS_AVATAR_BYTES, s_szBase64, sizeof( s_szBase64 ) );
	if ( nChars <= 0 )
		return;

	s_AvatarCommands.AddToTail( CUtlString( "ios_avatar begin\n" ) );
	for ( int i = 0; i < nChars; i += IOS_AVATAR_CHUNK )
	{
		char szChunk[ IOS_AVATAR_CHUNK + 1 ];
		V_strncpy( szChunk, s_szBase64 + i, MIN( IOS_AVATAR_CHUNK, nChars - i ) + 1 );
		s_AvatarCommands.AddToTail( CUtlString( CFmtStr( "ios_avatar data %s\n", szChunk ) ) );
	}
	s_AvatarCommands.AddToTail( CUtlString( "ios_avatar end\n" ) );
}

// 24-bit uncompressed TGA, top-left origin (Scaleform reads TGA)
static void WriteAvatarTGA( const char *pszKey, const unsigned char *pRGB )
{
	CUtlBuffer buf;
	unsigned char header[18] = {};
	header[2] = 2;											// uncompressed true-color
	header[12] = IOS_AVATAR_SIZE & 0xFF; header[13] = IOS_AVATAR_SIZE >> 8;
	header[14] = IOS_AVATAR_SIZE & 0xFF; header[15] = IOS_AVATAR_SIZE >> 8;
	header[16] = 24;
	header[17] = 0x20;										// rows top to bottom
	buf.Put( header, sizeof( header ) );
	for ( int i = 0; i < IOS_AVATAR_SIZE * IOS_AVATAR_SIZE; i++ )
	{
		const unsigned char *p = pRGB + i * 3;
		unsigned char bgr[3] = { p[2], p[1], p[0] };
		buf.Put( bgr, 3 );
	}

	g_pFullFileSystem->CreateDirHierarchy( "cache/avatars", "MOD" );
	char szFile[MAX_PATH];
	V_snprintf( szFile, sizeof( szFile ), "cache/avatars/%s.tga", pszKey );
	g_pFullFileSystem->WriteFile( szFile, "MOD", buf );
}

class CIOSAvatarShare : public CAutoGameSystemPerFrame
{
public:
	CIOSAvatarShare() : CAutoGameSystemPerFrame( "CIOSAvatarShare" ), m_flNextSend( 0.0f ), m_flNextPoll( 0.0f ) {}

	virtual void Update( float frametime )
	{
		if ( !engine->IsConnected() )
		{
			s_AvatarCommands.RemoveAll();
			return;
		}

		double flNow = Plat_FloatTime();

		// upload: 10 commands a second, well under the server's limit (40)
		if ( s_AvatarCommands.Count() && flNow >= m_flNextSend )
		{
			m_flNextSend = flNow + 0.1;
			engine->ServerCmd( s_AvatarCommands[0].Get(), true );
			s_AvatarCommands.Remove( 0 );
		}

		// download: look for new or changed pictures twice a second
		if ( flNow >= m_flNextPoll )
		{
			m_flNextPoll = flNow + 0.5;
			Poll();
		}
	}

private:
	void Poll()
	{
		INetworkStringTable *pTable = networkstringtable ? networkstringtable->FindTable( "IOSAvatars" ) : NULL;
		if ( !pTable )
			return;

		for ( int i = 0; i < pTable->GetNumStrings(); i++ )
		{
			const char *pszKey = pTable->GetString( i );
			int nBytes = 0;
			const unsigned char *pData = (const unsigned char *)pTable->GetStringUserData( i, &nBytes );
			if ( !pszKey || !pszKey[0] || !pData || nBytes != IOS_AVATAR_BYTES )
				continue;

			CRC32_t crc = CRC32_ProcessSingleBuffer( pData, nBytes );
			if ( m_Stored.Defined( pszKey ) && m_Stored[ pszKey ] == crc )
				continue;

			WriteAvatarTGA( pszKey, pData );
			m_Stored[ pszKey ] = crc;
			s_nAvatarVersion++;
		}
	}

	double m_flNextSend;
	double m_flNextPoll;
	CUtlStringMap< CRC32_t > m_Stored;		// account ID -> CRC of the picture written
};

static CIOSAvatarShare s_IOSAvatarShare;

#endif // IOS
