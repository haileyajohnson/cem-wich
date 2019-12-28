#ifndef SEDTRANS_INCLUDED
#define SEDTRANS_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdlib.h>
#include "BeachNode.h"
#include "BeachGrid.h"

void WaveTransformation(struct BeachGrid *grid, float wave_angle, float wave_period, float wave_height, float timestep_length);
void GetAvailableSupply(struct BeachGrid *grid, int ref_pos, float ref_depth, float shelf_slope, float shoreface_slope, float min_depth);
void NetVolumeChange(struct BeachGrid *grid);
int TransportSediment(struct BeachGrid *grid, int ref_pos, float ref_depth, float shelf_slope, float shoreface_slope, float min_depth);
int FixBeach(struct BeachGrid* grid);

#if defined(__cplusplus)
}
#endif

#endif
