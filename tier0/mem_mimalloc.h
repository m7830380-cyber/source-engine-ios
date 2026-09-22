//========= Source Engine iOS modernization ==============================//
//
// Purpose: Optional mimalloc backend for the tier0 allocator.
//
//	Enabled with the waf flag --mimalloc, which defines SRC_MIMALLOC.
//	When it is NOT defined this header maps every entry point straight
//	back onto the libc allocator, so the engine keeps byte-for-byte the
//	same behaviour it had before.
//
//	Rationale: on iOS (POSIX) CStdMemAlloc's small block heap is compiled
//	out entirely - it is guarded by _WIN32 - so every single engine
//	allocation lands directly in libc malloc. mimalloc has a far better
//	size-class layout and per-thread free lists, which is exactly the
//	workload the engine produces (many small, short lived, cross-thread
//	allocations from the material/model streaming paths).
//
//=======================================================================//

#ifndef TIER0_MEM_MIMALLOC_H
#define TIER0_MEM_MIMALLOC_H
#pragma once

#if defined( SRC_MIMALLOC )

#include <mimalloc.h>

// Route the engine allocator onto mimalloc.
#define Src_InternalMalloc( nSize )				mi_malloc( ( nSize ) )

// NOTE: freeing and reallocating are deliberately NOT blind mi_* calls.
//
// tier0's allocator is not the only allocator in the process. SDL2, ANGLE,
// libc strdup() and friends all hand out blocks straight from libc malloc,
// and some of those blocks legitimately reach CStdMemAlloc::Free(). Calling
// mi_free() on a pointer mimalloc does not own is undefined behaviour and
// would crash on device, so we ask mimalloc whether the block is in one of
// its regions first and fall back to libc for anything foreign.
//
// mi_is_in_heap_region() is a cheap bitmap/range test, not a lookup.
inline void Src_InternalFree( void *pMem )
{
	if ( mi_is_in_heap_region( pMem ) )
		mi_free( pMem );
	else
		free( pMem );
}

inline void *Src_InternalRealloc( void *pMem, size_t nSize )
{
	if ( !pMem || mi_is_in_heap_region( pMem ) )
		return mi_realloc( pMem, nSize );

	// Foreign block: libc owns it, so let libc grow it.
	return realloc( pMem, nSize );
}

// mimalloc can tell us the usable size of a block, which the engine's
// heap reporting wants. This is exact, unlike the libc guesses.
inline size_t Src_InternalMallocSize( void *pMem )
{
	if ( mi_is_in_heap_region( pMem ) )
		return mi_usable_size( pMem );
#if defined( APPLE )
	return malloc_size( pMem );
#elif defined( _WIN32 )
	return _msize( pMem );
#else
	return malloc_usable_size( pMem );
#endif
}

#define SRC_MIMALLOC_ACTIVE 1

#else // !SRC_MIMALLOC - original behaviour

#define Src_InternalMalloc( nSize )				malloc( ( nSize ) )
#define Src_InternalRealloc( pMem, nSize )		realloc( ( pMem ), ( nSize ) )
#define Src_InternalFree( pMem )				free( ( pMem ) )

#if defined( APPLE )
#define Src_InternalMallocSize( pMem )			malloc_size( ( pMem ) )
#elif defined( _WIN32 )
#define Src_InternalMallocSize( pMem )			_msize( ( pMem ) )
#else
#define Src_InternalMallocSize( pMem )			malloc_usable_size( ( pMem ) )
#endif

#endif // SRC_MIMALLOC

#endif // TIER0_MEM_MIMALLOC_H
