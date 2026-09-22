//========= Source Engine iOS modernization ==============================//
//
// Purpose: CoreHaptics feedback backend for iOS.
//
//	Enabled with the waf flag --haptics, which defines SRC_HAPTICS.
//	When it is not defined, every entry point below compiles to an empty
//	inline no-op and the engine behaves exactly as before.
//
//	The engine already has a rumble concept - IInputSystem::SetRumble takes
//	a left and a right motor strength, which came from the Xbox controller
//	model. iOS has no motors, it has a Taptic Engine driven by CoreHaptics,
//	so we translate: the two motor strengths are combined into a single
//	continuous haptic player whose intensity and sharpness we update live.
//	Low frequency (left) motor maps to intensity, high frequency (right)
//	maps to sharpness, which is the closest perceptual analogue.
//
//=======================================================================//

#ifndef SRC_HAPTICS_IOS_H
#define SRC_HAPTICS_IOS_H
#pragma once

#if defined( SRC_HAPTICS )

// Start up the haptic engine. Safe to call when the device has no haptic
// hardware - it simply reports failure and every later call becomes inert.
bool Haptics_Init( void );

// Tear down the engine and release the continuous player.
void Haptics_Shutdown( void );

// Drive the continuous haptic player. Both arguments are 0..1.
// Passing 0,0 stops playback rather than playing silence, so we do not
// keep the Taptic Engine spun up and draining battery.
void Haptics_SetRumble( float flLeftMotor, float flRightMotor );

// Immediate hard stop, used by StopRumble.
void Haptics_Stop( void );

#else // !SRC_HAPTICS

inline bool Haptics_Init( void )								{ return false; }
inline void Haptics_Shutdown( void )							{}
inline void Haptics_SetRumble( float, float )					{}
inline void Haptics_Stop( void )								{}

#endif // SRC_HAPTICS

#endif // SRC_HAPTICS_IOS_H
