#ifndef CEM_WAVECLIMATE_INCLUDED
#define CEM_WAVECLIMATE_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

#include "consts.h"

	struct WaveClimate {
		double t_resolution;
		double asymmetry, stability;
		double* wave_periods;
		double* wave_angles;
		double* wave_heights;
		double (*GetWaveHeight)(struct WaveClimate *this, int timestep);
		double (*GetWavePeriod)(struct WaveClimate* this, int timestep);
		double (*GetWaveAngle)(struct WaveClimate* this, int timestep);
	};
	extern const struct WaveClimateClass {
		struct WaveClimate(*new)(double* wave_periods, double* wave_angles, double* wave_heights, 
			double asymmetry, double stability, int num_timesteps, int numWaveInputs);
	} WaveClimate;

#endif