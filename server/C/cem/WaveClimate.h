#ifndef CEM_WAVECLIMATE_INCLUDED
#define CEM_WAVECLIMATE_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

#include "consts.h"

	struct WaveClimate {
		double* wave_periods;
		double* wave_angles;
		double* wave_heights;
		double (*GetWaveHeight)(struct WaveClimate *this, int timestep);
		double (*GetWavePeriod)(struct WaveClimate* this, int timestep);
		double (*GetWaveAngle)(struct WaveClimate* this, int timestep);
	};
	extern const struct WaveClimateClass {
		struct WaveClimate(*new)(double* wave_periods, double* wave_angles, double* wave_heights);
	} WaveClimate;

#endif