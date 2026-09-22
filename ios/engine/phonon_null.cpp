//===========================================================================//
//
// Purpose: Steam Audio (phonon) entry points for builds without libphonon3d.
//
// Steam Audio ships only as a prebuilt library. The engine uses it for HRTF
// (snd_use_hrtf); when creating the context fails it logs, turns HRTF off and
// mixes normally, so reporting failure here is enough.
//
//===========================================================================//

#include "phonon/phonon_3d.h"

IPLerror iplCreate3DContext( IPLGlobalContext globalContext, IPLDspParams dspParams, IPLbyte *hrtfData, IPLhandle *context )
{
	if ( context )
	{
		*context = 0;
	}
	return IPL_STATUS_FAILURE;
}

IPLerror iplCreateBinauralEffect( IPLhandle context, IPLAudioFormat inputFormat, IPLAudioFormat outputFormat, IPLhandle *effect )
{
	if ( effect )
	{
		*effect = 0;
	}
	return IPL_STATUS_FAILURE;
}

IPLvoid iplDestroyBinauralEffect( IPLhandle *effect )
{
}

IPLvoid iplApplyBinauralEffect( IPLhandle effect, IPLAudioBuffer inputAudio, IPLVector3 direction, IPLHrtfInterpolation interpolation, IPLAudioBuffer outputAudio )
{
}
