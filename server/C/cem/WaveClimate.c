#include "utils.h"
#include "WaveClimate.h"

static double GetWaveHeight(struct WaveClimate *this, int timestep)
{
	return this->wave_heights[timestep];
}

static double GetWavePeriod(struct WaveClimate* this, int timestep)
{
	return this->wave_periods[timestep];
}

static double GetWaveAngle(struct WaveClimate* this, int timestep)
{
	return this->wave_angles[timestep];
}

static struct WaveClimate new(double* wave_periods, double* wave_angles, double* wave_heights, double asymmetry, double stability){
	return (struct WaveClimate) {
		.wave_periods = wave_periods,
		.wave_angles = wave_angles,
		.wave_heights = wave_heights,
		.asymmetry = asymmetry,
		.stability = stability,
		.GetWaveHeight = &GetWaveHeight,
		.GetWavePeriod = &GetWavePeriod,
		.GetWaveAngle = &GetWaveAngle
	};
}

const struct WaveClimateClass WaveClimate = { .new = &new };
