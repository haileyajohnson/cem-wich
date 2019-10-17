#include "utils.h"
#include "WaveClimate.h"

#define ASYMMETRY_INCREMENT (0.0049)
#define INSTABILITY_INCREMENT (0.0048)
#define WAVE_INCREMENT (0.1)
#define WAVE_DISTRIBUTION (TRUE)

const float myWaveDistribution[5] = { 0.86, 1.34, 1.94, 2.83, 3.75 };

static struct WaveClimate * UpdateWaveClimate(struct WaveClimate *this)
{
	// update asymmetry
	if (this->p_asymmetry_decrease + this->p_asymmetry_increase > 0)
	{
		float v = RandZeroToOne();
		if (v <= this->p_asymmetry_decrease)
		{
			this->asymmetry -= ASYMMETRY_INCREMENT;
			if (this->asymmetry < 0) { this->asymmetry = 0; }
		}
		else if (v <= this->p_asymmetry_decrease + this->p_asymmetry_increase)
		{
			this->asymmetry += ASYMMETRY_INCREMENT;
			if (this->asymmetry > 1) { this->asymmetry = 1; }
		}
	}
	// update stability
	if (this->p_stability_decrease + this->p_stability_increase > 0)
	{
		float v = RandZeroToOne();
		if (v <= this->p_stability_decrease)
		{
			this->stability -= INSTABILITY_INCREMENT;
			if (this->stability < 0) { this->stability = 0; }
		}
		else if (v <= this->p_stability_decrease + this->p_stability_increase)
		{
			this->stability += INSTABILITY_INCREMENT;
			if (this->stability > 1) { this->stability = 1; }
		}
	}
	return this;
}

static 
struct WaveClimate * SetWaveHeight(struct WaveClimate *this)
{
	float v = RandZeroToOne();
	if (WAVE_DISTRIBUTION)
	{
		if (v >= 0.99) { this->wave_height = myWaveDistribution[4]; }
		else if (v >= 0.95) { this->wave_height = myWaveDistribution[3]; }
		else if (v >= 0.75) { this->wave_height = myWaveDistribution[2]; }
		else if (v >= 0.50) { this->wave_height = myWaveDistribution[1]; }
		else { this->wave_height = myWaveDistribution[0]; }
	}
	else
	{
		this->wave_height = v < this->p_high_energy_wave ? this->wave_height_high : this->wave_height_avg;
	}
	return this;
}

static double GetWaveHeight(struct WaveClimate *this)
{
	return this->wave_height;
}

static struct WaveClimate new(double asymmetry, double stability, double p_asymmetry_increase, double p_asymmetry_decrease,
	double p_stability_increase, double p_stability_decrease, double wave_height_avg, double wave_height_high, double p_high_energy_wave, double wave_period, int temporal_resolution){
	return (struct WaveClimate) {
		.asymmetry = asymmetry,
		.stability = stability,
		.p_asymmetry_increase = p_asymmetry_increase,
		.p_asymmetry_decrease = p_asymmetry_decrease,
		.p_stability_increase = p_stability_increase,
		.p_stability_decrease = p_stability_decrease,
		.wave_height_avg = wave_height_avg,
		.wave_height_high = wave_height_high,
		.p_high_energy_wave = p_high_energy_wave,
		.wave_period = wave_period,
		.wave_angle = EMPTY_DOUBLE,
		.temporal_resolution = temporal_resolution,
		.SetWaveHeight = &SetWaveHeight,
		.GetWaveHeight = &GetWaveHeight,
		.UpdateWaveClimate = &UpdateWaveClimate
	};
}

const struct WaveClimateClass WaveClimate = { .new = &new };
