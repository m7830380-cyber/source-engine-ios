//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Paint (gel) definitions for the P2ASW paint code on the CS:GO engine.
//
//=============================================================================//
#ifndef PORTAL2_PAINT_DEFS_H
#define PORTAL2_PAINT_DEFS_H
#ifdef _WIN32
#pragma once
#endif

#include "paint/paint_enum.h"

// P2ASW reaches Alien Swarm's engine through a macro (its IVEngineClient lacks the
// call). The CS:GO engine kept Portal 2's paint map support, client and server.
#define HASPAINTMAP ( engine->HasPaintmap() )

#endif // PORTAL2_PAINT_DEFS_H
