//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: LAN profile pictures (iOS). Each client uploads its profile_avatar
// (cfg/user.cfg) as a small RGB image with "ios_avatar begin/data/end"; the
// server keeps them in the "IOSAvatars" string table (key: account ID) that
// every client receives, and clients show them as that player's avatar.
//
//=============================================================================//

#ifndef IOS_AVATAR_SHARE_H
#define IOS_AVATAR_SHARE_H

#define IOS_AVATAR_SIZE			64									// pixels, square
#define IOS_AVATAR_BYTES		( IOS_AVATAR_SIZE * IOS_AVATAR_SIZE * 3 )	// RGB, rows top to bottom
#define IOS_AVATAR_BASE64_MAX	( ( IOS_AVATAR_BYTES + 2 ) / 3 * 4 )
#define IOS_AVATAR_CHUNK		480									// base64 chars per command (512 max per command)

// base64url ('-' '_', no padding): no characters the command parser treats specially
static const char s_szIOSAvatarBase64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

inline int IOSAvatar_Base64Encode( const unsigned char *pData, int nBytes, char *pszOut, int nOutSize )
{
	int n = 0;
	for ( int i = 0; i < nBytes; i += 3 )
	{
		unsigned int v = pData[i] << 16;
		if ( i + 1 < nBytes ) v |= pData[i + 1] << 8;
		if ( i + 2 < nBytes ) v |= pData[i + 2];
		int nChars = ( i + 2 < nBytes ) ? 4 : ( i + 1 < nBytes ) ? 3 : 2;
		for ( int k = 0; k < nChars; k++ )
		{
			if ( n >= nOutSize - 1 )
				return -1;
			pszOut[n++] = s_szIOSAvatarBase64[ ( v >> ( 18 - 6 * k ) ) & 63 ];
		}
	}
	pszOut[n] = 0;
	return n;
}

// returns the number of bytes, or -1 on bad input / too little room
inline int IOSAvatar_Base64Decode( const char *pszIn, unsigned char *pOut, int nOutSize )
{
	int nBytes = 0;
	unsigned int v = 0;
	int nBits = 0;
	for ( const char *p = pszIn; *p; p++ )
	{
		const char *pPos = strchr( s_szIOSAvatarBase64, *p );
		if ( !pPos || !*p )
			return -1;
		v = ( v << 6 ) | (unsigned int)( pPos - s_szIOSAvatarBase64 );
		nBits += 6;
		if ( nBits >= 8 )
		{
			nBits -= 8;
			if ( nBytes >= nOutSize )
				return -1;
			pOut[nBytes++] = (unsigned char)( ( v >> nBits ) & 0xFF );
		}
	}
	return nBytes;
}

#if defined( CLIENT_DLL )
// Queues the upload of the local profile picture (throttled under the server's
// string command limit). Called when the local player entity appears.
void IOSAvatar_SendLocal();
// Bumped whenever a received picture was stored; panels showing avatars reload them.
int IOSAvatar_GetVersion();
#endif

#endif // IOS_AVATAR_SHARE_H
