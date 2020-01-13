#include "utils.h"
#include "WaveClimate.h"
#include <math.h>

static double GetWaveHeight(struct WaveClimate *this, int timestep)
{
	return this->wave_heights[(int)floor(timestep / this->t_resolution)];
}

static double GetWavePeriod(struct WaveClimate* this, int timestep)
{
	return this->wave_periods[(int)floor(timestep / this->t_resolution)];
}

static double GetWaveAngle(struct WaveClimate* this, int timestep)
{
	if (this->asymmetry >= 0 && this->stability >= 0)
	{
		double angle = RandZeroToOne() * (PI / 4);   // random angle 0 - pi/4
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

static struct WaveClimate new(double* wave_periods, double* wave_angles, double* wave_heights,
	double asymmetry, double stability, int num_timesteps, int num_wave_inputs){
	double t_resolution = ((double)num_timesteps) / num_wave_inputs;
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
