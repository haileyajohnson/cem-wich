#ifndef CEM_GLOBALS_INCLUDED
#define CEM_GLOBALS_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

#include "consts.h"

extern int g_current_time_step; // Time step of current calculation
extern double g_current_time; // The current model time
extern double c_num_timesteps; // Stop after what number of time steps
extern double c_timestep_length; // days; reflects rate of sediment transport per time step

#if defined(__cplusplus)
}
#endif

#endif
