#ifndef CEM_WAVECLIMATE_INCLUDED
#define CEM_WAVECLIMATE_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

#include "consts.h"

	struct WaveClimate {
		double asymmetry, stability, wave_height_avg, wave_period, wave_angle, wave_height;
		struct WaveClimate* (*SetWaveHeight)(struct WaveClimate *this);
		double (*GetWaveHeight)(struct WaveClimate *this);
		struct WaveClimate* (*UpdateWaveClimate)(struct WaveClimate *this);
		void (*FindWaveAngle)(struct WaveClimate* this);
	};
	extern const struct WaveClimateClass {
		struct WaveClimate(*new)(double asymmetry, double stability, double wave_height_g, double wave_period);
	} WaveClimate;

#endif