#ifndef PORTAL2_SERVER_COMPAT_H
#define PORTAL2_SERVER_COMPAT_H
#ifdef _WIN32
#pragma once
#endif

#if defined( PORTAL2 )

#ifndef TEAM_RED
#define TEAM_RED 2
#endif
#ifndef TEAM_BLUE
#define TEAM_BLUE 3
#endif
#ifndef TEAM_CT
#define TEAM_CT 3
#endif
#ifndef TEAM_TERRORIST
#define TEAM_TERRORIST 2
#endif

#ifndef CS_PLAYER_DUCK_SPEED_IDEAL
#define CS_PLAYER_DUCK_SPEED_IDEAL 8.0f
#endif
#ifndef PLAYERDECALS_COOLDOWN_SECONDS
#define PLAYERDECALS_COOLDOWN_SECONDS 60.0f
#endif

// CPlayerPickupController comes from P2ASW (portal_grabcontroller_shared.h).

#endif // PORTAL2

#endif
