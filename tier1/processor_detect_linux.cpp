//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: linux dependant ASM code for CPU capability detection
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//


// NOTE: This has to be the last file included! (turned off below, since this is included like a header)
#include "tier0/memdbgon.h"




// Turn off memdbg macros (turned on up top) since this is included like a header
#include "tier0/memdbgoff.h"

static void cpuid(uint32 function, uint32& out_eax, uint32& out_ebx, uint32& out_ecx, uint32& out_edx)
{
#if defined( __aarch64__ )
	// arm64: no cpuid. Report the SSE levels sse2neon provides.
	out_eax = out_ebx = out_ecx = out_edx = 0;
	if ( function == 0 )
	{
		out_eax = 1;			// highest standard function
		out_ebx = 0x756e6547;	// "Genu"
		out_edx = 0x49656e69;	// "ineI"
		out_ecx = 0x6c65746e;	// "ntel"
	}
	else if ( function == 1 )
	{
		out_edx = ( 1 << 23 ) | ( 1 << 25 ) | ( 1 << 26 );	// MMX, SSE, SSE2
		out_ecx = ( 1 << 0 );								// SSE3
	}
#elif defined(PLATFORM_64BITS)
	asm("mov %%rbx, %%rsi\n\t"
		"cpuid\n\t"
		"xchg %%rsi, %%rbx"
		: "=a" (out_eax),
		  "=S" (out_ebx),
		  "=c" (out_ecx),
		  "=d" (out_edx)
		: "a" (function) 
	);
#else
	asm("mov %%ebx, %%esi\n\t"
		"cpuid\n\t"
		"xchg %%esi, %%ebx"
		: "=a" (out_eax),
		  "=S" (out_ebx),
		  "=c" (out_ecx),
		  "=d" (out_edx)
		: "a" (function) 
	);
#endif
}

bool CheckMMXTechnology(void)
{
    uint32 eax,ebx,edx,unused;
    cpuid(1,eax,ebx,unused,edx);

    return edx & 0x800000;
}

bool CheckSSETechnology(void)
{
    uint32 eax,ebx,edx,unused;
    cpuid(1,eax,ebx,unused,edx);

    return edx & 0x2000000L;
}

bool CheckSSE2Technology(void)
{
    uint32 eax,ebx,edx,unused;
    cpuid(1,eax,ebx,unused,edx);

    return edx & 0x04000000;
}

bool Check3DNowTechnology(void)
{
    uint32 eax, unused;
    cpuid(0x80000000,eax,unused,unused,unused);

    if ( eax > 0x80000000L )
    {
     	cpuid(0x80000001,unused,unused,unused,eax);
		return ( eax & 1<<31 );
    }
    return false;
}

