// iOS: diagnostics printed only when the game runs with -verbose (tier0's
// Plat_IsVerboseLogging; tier0 is linked into every module that links GFx).
#pragma once
#include <stdio.h>
#ifndef VERBOSE_PRINTF
extern "C" bool Plat_IsVerboseLogging();
#define VERBOSE_PRINTF( ... ) ( Plat_IsVerboseLogging() ? printf( __VA_ARGS__ ) : 0 )
#endif
