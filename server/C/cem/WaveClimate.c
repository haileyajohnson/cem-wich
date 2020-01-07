#include "utils.h"
#include "WaveClimate.h"
#include <math.h>

static float GetWaveHeight(struct WaveClimate *this, int timestep)
{
	return this->wave_heights[(int)floor(timestep / this->t_resolution)];
}

static float GetWavePeriod(struct WaveClimate* this, int timestep)
{
	return this->wave_periods[(int)floor(timestep / this->t_resolution)];
}

static float GetWaveAngle(struct WaveClimate* this, int timestep)
{
	if (this->asymmetry >= 0 && this->stability >= 0)
	{
		float angle = RandZeroToOne() * (PI / 4);   // random angle 0 - pi/4
		if (RandZeroToOne() >= this->stability)        // random variable determining above or below 45 degrees
		{
			angle += PI / 4;
		}
		if (RandZeroToOne() >= this->asymmetry)        // random variable determining direction of approach (positive = left)
		{
			angle = -angle;
		}
		return angle;
	}

	return this->wave_angles[(int)floor(timestep / this->t_resolution)];
}

static struct WaveClimate new(float* wave_periods, float* wave_angles, float* wave_heights,
	float asymmetry, float stability, int num_timesteps, int num_wave_inputs){
	float t_resolution = ((float)num_timesteps) / num_wave_inputs;
	return (struct WaveClimate) {
		.t_resolution = t_resolution,
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
