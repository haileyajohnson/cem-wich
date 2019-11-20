#include "utils.h"
#include "WaveClimate.h"

static struct WaveClimate * UpdateWaveClimate(struct WaveClimate *this)
{
	return this;
}

static struct WaveClimate * SetWaveHeight(struct WaveClimate *this)
{
	this->wave_height = this->wave_height_avg;
	return this;
}

static double GetWaveHeight(struct WaveClimate *this)
{
	return this->wave_height;
}

static void FindWaveAngle(struct WaveClimate *this)
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
	this->wave_angle = angle;
}

static struct WaveClimate new(double asymmetry, double stability,  double wave_height_avg, double wave_period){
	return (struct WaveClimate) {
		.asymmetry = asymmetry,
		.stability = stability,
		.wave_height_avg = wave_height_avg,
		.wave_period = wave_period,
		.wave_angle = EMPTY_DOUBLE,
		.SetWaveHeight = &SetWaveHeight,
		.GetWaveHeight = &GetWaveHeight,
		.UpdateWaveClimate = &UpdateWaveClimate,
		.FindWaveAngle = &FindWaveAngle
	};
}

const struct WaveClimateClass WaveClimate = { .new = &new };
