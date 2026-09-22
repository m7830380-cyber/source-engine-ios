//========= Source Engine iOS modernization ==============================//
//
// Purpose: Plain C bridge to the AVAudioEngine backend.
//
//	This header must stay free of both Valve headers and Objective-C
//	headers so either side can include it. See the comment at the top of
//	snd_dev_ios_phase_backend.mm for why the two worlds cannot meet in one
//	translation unit (BOOL is `int` for Valve, `bool` for ObjC).
//
//=======================================================================//

#ifndef SND_DEV_IOS_PHASE_BACKEND_H
#define SND_DEV_IOS_PHASE_BACKEND_H
#pragma once

#define IOSPHASE_SAMPLE_RATE	44100
#define IOSPHASE_CHANNELS		2

#ifdef __cplusplus
extern "C" {
#endif

// Called on the real-time audio thread. Must not allocate or lock.
// pLeft/pRight are planar float32 buffers of nFrames samples each.
typedef void (*IOSPhaseRenderFunc)( void *pContext, float *pLeft, float *pRight, unsigned int nFrames );

void IOSPhase_ConfigureSession( void );
bool IOSPhase_Start( IOSPhaseRenderFunc pfnRender, void *pContext );
void IOSPhase_Stop( void );
void IOSPhase_Pause( void );
bool IOSPhase_Resume( void );
bool IOSPhase_IsHeadphone( void );

#ifdef __cplusplus
}
#endif

#endif // SND_DEV_IOS_PHASE_BACKEND_H
