#ifndef IOS_AVATAR_SHARE_H
#define IOS_AVATAR_SHARE_H
#ifdef _WIN32
#pragma once
#endif

#define IOS_AVATAR_BYTES 4096

inline void IOSAvatar_StorePicture( uint32 unAccount, const void *pData, int nBytes ) { (void)unAccount; (void)pData; (void)nBytes; }

#endif
