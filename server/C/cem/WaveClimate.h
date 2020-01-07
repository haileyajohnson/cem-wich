#ifndef CEM_WAVECLIMATE_INCLUDED
#define CEM_WAVECLIMATE_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

#include "consts.h"

	struct WaveClimate {
		float t_resolution;
		float asymmetry, stability;
		float* wave_periods;
		float* wave_angles;
		float* wave_heights;
		float (*GetWaveHeight)(struct WaveClimate *this, int timestep);
		float (*GetWavePeriod)(struct WaveClimate* this, int timestep);
		float (*GetWaveAngle)(struct WaveClimate* this, int timestep);
	};
	extern const struct WaveClimateClass {
		struct WaveClimate(*new)(float* wave_periods, float* wave_angles, float* wave_heights, 
			float asymmetry, float stability, int num_timesteps, int numWaveInputs);
	} WaveClimate;

#endif