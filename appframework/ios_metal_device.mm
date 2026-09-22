//========= Source Engine iOS modernization ==============================//
//
// Purpose: Native Metal device query. See ios_metal_device.h for why this
//          is a capability query and not a rendering backend.
//
//	IMPORTANT: this file includes Objective-C framework headers and must
//	therefore NOT include any Valve header. public/tier0/basetypes.h does
//	`typedef int BOOL` while ObjC's objc.h does `typedef bool BOOL`, and a
//	translation unit that sees both fails to compile. (Same issue the repo
//	fixed in commit 14f6ad3d for glmrendererinfo.) Reporting therefore
//	goes through printf rather than Msg/Warning.
//
//=======================================================================//

#include "ios_metal_device.h"

#if defined( SRC_METAL )

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

#include <string.h>
#include <stdio.h>

//-----------------------------------------------------------------------------
bool IOSMetal_GetDeviceInfo( IOSMetalDeviceInfo_t *pOut )
{
	if ( !pOut )
		return false;

	memset( pOut, 0, sizeof( *pOut ) );

	id<MTLDevice> pDevice = MTLCreateSystemDefaultDevice();
	if ( pDevice == nil )
		return false;

	const char *pszName = [[pDevice name] UTF8String];
	if ( pszName )
	{
		strncpy( pOut->m_szDeviceName, pszName, sizeof( pOut->m_szDeviceName ) - 1 );
		pOut->m_szDeviceName[ sizeof( pOut->m_szDeviceName ) - 1 ] = '\0';
	}

	// Walk the families downwards and record the highest one supported.
	if ( @available( iOS 13.0, * ) )
	{
		const MTLGPUFamily kFamilies[] = {
			MTLGPUFamilyApple7, MTLGPUFamilyApple6, MTLGPUFamilyApple5,
			MTLGPUFamilyApple4, MTLGPUFamilyApple3, MTLGPUFamilyApple2,
			MTLGPUFamilyApple1,
		};
		const int kFamilyNumbers[] = { 7, 6, 5, 4, 3, 2, 1 };

		for ( size_t i = 0; i < sizeof( kFamilies ) / sizeof( kFamilies[ 0 ] ); ++i )
		{
			if ( [pDevice supportsFamily:kFamilies[ i ]] )
			{
				pOut->m_nAppleGPUFamily = kFamilyNumbers[ i ];
				break;
			}
		}

		pOut->m_bSupportsFamilyApple4 = [pDevice supportsFamily:MTLGPUFamilyApple4] ? true : false;
		pOut->m_bSupportsFamilyApple7 = [pDevice supportsFamily:MTLGPUFamilyApple7] ? true : false;
	}

	// Apple4 (A11) and later guarantee 16384; earlier hardware caps at 8192.
	pOut->m_nMaxTextureSize = pOut->m_bSupportsFamilyApple4 ? 16384 : 8192;

	if ( @available( iOS 11.0, * ) )
	{
		pOut->m_nRecommendedMaxWorkingSetSize =
			(unsigned long long)[pDevice recommendedMaxWorkingSetSize];
	}

	if ( @available( iOS 13.0, * ) )
	{
		pOut->m_bUnifiedMemory = [pDevice hasUnifiedMemory] ? true : false;
	}
	else
	{
		// Every iOS device before that was unified memory anyway.
		pOut->m_bUnifiedMemory = true;
	}

	// 4x MSAA is the baseline on all Metal-capable iOS hardware.
	pOut->m_bSupportsMSAA = [pDevice supportsTextureSampleCount:4] ? true : false;

	return true;
}

//-----------------------------------------------------------------------------
void IOSMetal_ReportDevice( void )
{
	IOSMetalDeviceInfo_t info;
	if ( !IOSMetal_GetDeviceInfo( &info ) )
	{
		printf( "Metal: no device available.\n" );
		return;
	}

	printf( "Metal device: %s\n", info.m_szDeviceName );
	printf( "  Apple GPU family : %d\n", info.m_nAppleGPUFamily );
	printf( "  Max texture size : %d\n", info.m_nMaxTextureSize );
	printf( "  Unified memory   : %s\n", info.m_bUnifiedMemory ? "yes" : "no" );
	printf( "  4x MSAA          : %s\n", info.m_bSupportsMSAA ? "yes" : "no" );

	if ( info.m_nRecommendedMaxWorkingSetSize > 0 )
	{
		printf( "  Recommended VRAM : %llu MB\n",
			(unsigned long long)( info.m_nRecommendedMaxWorkingSetSize / ( 1024 * 1024 ) ) );
	}
}

#endif // SRC_METAL
