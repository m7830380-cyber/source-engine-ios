//========= Source Engine iOS modernization ==============================//
//
// Purpose: AVAudioEngine/PHASE audio device for iOS.
//
//	Only built when the --phase-audio waf flag is set (SRC_PHASE_AUDIO).
//
//=====================================================================================//

#ifndef SND_DEV_IOS_PHASE_H
#define SND_DEV_IOS_PHASE_H
#pragma once

class IAudioDevice;
IAudioDevice *Audio_CreateIOSPhaseDevice( void );

#endif // SND_DEV_IOS_PHASE_H
