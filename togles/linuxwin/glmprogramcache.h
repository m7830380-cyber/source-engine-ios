//========= Source Engine iOS modernization ==============================//
//
// Purpose: On-disk GL program binary cache.
//
//	Build gate:   --precompiled-shaders  (defines SRC_PRECOMPILED_SHADERS)
//	Runtime gate: -precompiledshaders    (OFF by default, as requested)
//
//	Both gates must be satisfied. With the build flag off this header is
//	a set of empty inlines; with the flag on but the launch argument
//	absent, every call returns false/no-ops and the engine links shaders
//	exactly as it does today.
//
//	WHAT THIS ACTUALLY DOES
//	-----------------------
//	The original idea was ".metallib offline compilation". That only
//	applies once there is a Metal backend - the current renderer is
//	GLES via ANGLE, and it has no notion of a metallib. The equivalent
//	win for the GLES path is glGetProgramBinary/glProgramBinary, which
//	is core in GLES 3.0 and lets the driver skip the entire
//	GLSL -> IR -> native compile on subsequent runs.
//
//	The engine links thousands of shader permutations during map load,
//	and each link is a full driver compile. Caching the linked binaries
//	turns the second and later loads into a memcpy plus a driver upload.
//
//	SAFETY
//	------
//	Program binaries are only valid for the exact driver that produced
//	them. GL gives us a binaryFormat token, but that is not sufficient
//	on its own: an OS update can change the driver while keeping the
//	format token. We therefore key every entry on a fingerprint built
//	from GL_VENDOR, GL_RENDERER and GL_VERSION, and discard the whole
//	cache when it changes. glProgramBinary failure is also handled - we
//	simply fall back to a normal compile and link, so a stale or corrupt
//	cache can never be fatal.
//
//=======================================================================//

#ifndef GLM_PROGRAM_CACHE_H
#define GLM_PROGRAM_CACHE_H
#pragma once

#if defined( SRC_PRECOMPILED_SHADERS )

#include "tier0/platform.h"

// Reads -precompiledshaders once and caches the answer.
// Returns false unless the user explicitly asked for it.
bool ShaderCache_IsEnabled( void );

// Prepare the cache directory and validate the driver fingerprint.
// Safe to call more than once.
void ShaderCache_Init( void );

// Flush the in-memory index to disk. Called at shutdown.
void ShaderCache_Shutdown( void );

// Try to populate an already-created program object from the cache.
// Returns true only if the program is fully linked and usable.
bool ShaderCache_TryLoadProgram( uint nProgram, const char *pVertexText, const char *pFragmentText );

// Store a successfully linked program.
void ShaderCache_StoreProgram( uint nProgram, const char *pVertexText, const char *pFragmentText );

// Hint to the driver, before linking, that the binary will be read back.
void ShaderCache_MarkRetrievable( uint nProgram );

#else // !SRC_PRECOMPILED_SHADERS

inline bool ShaderCache_IsEnabled( void )													{ return false; }
inline void ShaderCache_Init( void )														{}
inline void ShaderCache_Shutdown( void )													{}
inline bool ShaderCache_TryLoadProgram( unsigned int, const char *, const char * )			{ return false; }
inline void ShaderCache_StoreProgram( unsigned int, const char *, const char * )			{}
inline void ShaderCache_MarkRetrievable( unsigned int )										{}

#endif // SRC_PRECOMPILED_SHADERS

#endif // GLM_PROGRAM_CACHE_H
