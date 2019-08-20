#ifndef CEM_WAVECLIMATE_INCLUDED
#define CEM_WAVECLIMATE_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

#include "consts.h"

	struct WaveClimate {
		double asymmetry, stability, p_asymmetry_increase, p_asymmetry_decrease, p_stability_increase, p_stability_decrease, wave_height_avg, wave_height_high, p_high_energy_wave, wave_period, wave_angle, wave_height;
		int temporal_resolution;
		struct WaveClimate* (*SetWaveHeight)(struct WaveClimate *this);
		double(*GetWaveHeight)(struct WaveClimate *this);
		struct WaveClimate* (*UpdateWaveClimate)(struct WaveClimate *this);
	};
	extern const struct WaveClimateClass {
		struct WaveClimate(*new)(double asymmetry, double stability, double p_asymmetry_increase, double p_asymmetry_decrease,
		double p_stability_increase, double p_stability_decrease, double wave_height_avg, double wave_height_high, double p_high_energy_wave, double wave_period, int temporal_resolution);
	} WaveClimate;

#endif