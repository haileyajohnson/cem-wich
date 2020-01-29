#ifndef SEDTRANS_INCLUDED
#define SEDTRANS_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdlib.h>
#include "BeachNode.h"
#include "BeachGrid.h"

void WaveTransformation(struct BeachGrid *grid, double wave_angle, double wave_period, double wave_height, double timestep_length);
void GetAvailableSupply(struct BeachGrid *grid, int ref_pos, double ref_depth, double shelf_slope, double shoreface_slope, double min_depth, double depthOfClsoure);
void NetVolumeChange(struct BeachGrid *grid);
void TransportSediment(struct BeachGrid *grid, int ref_pos, double ref_depth, double shelf_slope, double shoreface_slope, double min_depth, double depthOfClosure);
void FixBeach(struct BeachGrid* grid);

#if defined(__cplusplus)
}
#endif

#endif
