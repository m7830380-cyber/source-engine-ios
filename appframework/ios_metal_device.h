//========= Source Engine iOS modernization ==============================//
//
// Purpose: Native Metal device query for iOS.
//
//	Build gate: --metal (defines SRC_METAL).
//
//	READ THIS BEFORE EXTENDING
//	--------------------------
//	This is NOT a Metal rendering backend, and the --metal flag does not
//	switch the renderer. Here is the honest state of things.
//
//	The renderer already reaches Metal today. The stack is:
//
//	    engine -> IDirect3DDevice9 (dxabstract) -> ToGLES -> EGL/GLES
//	           -> ANGLE (Metal backend) -> CAMetalLayer -> Metal
//
//	scripts/ios/build-angle.sh ships ANGLE's Metal-backed EGL/GLESv2
//	frameworks, and appframework/ios_metal_layer.mm already configures a
//	CAMetalLayer for presentation. So "port the renderer to Metal" is not
//	a matter of adding a Metal path where none exists - it means deleting
//	a translation layer that already ends up in Metal anyway.
//
//	The cost of doing that properly:
//	  * ~30,600 lines across togles/ would have to be replaced
//	  * 139 D3D9 entry points in dxabstract.h reimplemented on Metal
//	  * DX9 shader assembly -> MSL translation, replacing dx9asmtogl2.cpp
//	    (3,958 lines of DX-asm -> GLSL that has been debugged for years)
//	  * every shader permutation revalidated against the new path
//
//	That is a multi-month project with a high regression risk, and the
//	realistic gain is the ANGLE translation overhead - not the 30-40%
//	that a naive "OpenGL is deprecated, Metal is faster" reading suggests,
//	because the GPU work is already going through Metal.
//
//	What this file does provide is the groundwork that such a port needs
//	and that is independently useful right now: authoritative device
//	capability reporting straight from MTLDevice, rather than the guesses
//	ANGLE reports through the GL strings. Texture limits, memory budget
//	and GPU family drive quality defaults, and today those are inferred.
//
//=======================================================================//

#ifndef IOS_METAL_DEVICE_H
#define IOS_METAL_DEVICE_H
#pragma once

#if defined( SRC_METAL )

struct IOSMetalDeviceInfo_t
{
	char	m_szDeviceName[ 128 ];

	// Highest Apple GPU family supported (1..7); 0 if unknown.
	// Capped at 7 because Xcode 15.4's SDK is the build floor here and
	// MTLGPUFamilyApple8/9 are not guaranteed to be declared.
	int		m_nAppleGPUFamily;

	// Largest 2D texture dimension the hardware accepts.
	int		m_nMaxTextureSize;

	// Recommended working set in bytes. Exceeding it makes iOS start
	// evicting our resources, which is the usual cause of texture
	// popping on memory-constrained devices.
	// Plain C type: this header is included from an ObjC translation unit
	// that must not pull in Valve's basetypes.h.
	unsigned long long	m_nRecommendedMaxWorkingSetSize;

	bool	m_bSupportsFamilyApple4;	// A11 and later
	bool	m_bSupportsFamilyApple7;	// A14/M1 and later
	bool	m_bSupportsMSAA;
	bool	m_bUnifiedMemory;
};

// Query the default Metal device. Returns false if Metal is unavailable,
// in which case the struct is zeroed.
bool IOSMetal_GetDeviceInfo( IOSMetalDeviceInfo_t *pOut );

// Log what we found. Called once during renderer startup.
void IOSMetal_ReportDevice( void );

#else // !SRC_METAL

struct IOSMetalDeviceInfo_t;
inline bool IOSMetal_GetDeviceInfo( IOSMetalDeviceInfo_t * )	{ return false; }
inline void IOSMetal_ReportDevice( void )						{}

#endif // SRC_METAL

#endif // IOS_METAL_DEVICE_H
