#ifndef CEM_BEACHDATA_INCLUDED
#define CEM_BEACHDATA_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

#include "consts.h"

struct BeachProperties {
	double transport_potential, net_volume_change, prev_angle, next_angle, surrounding_angle;
	int prev_timestamp, next_timestamp, surrounding_timestamp, in_shadow, shadow_timestamp;
	FLOW_DIR transport_dir;
};

extern const struct BeachPropertiesClass {
	struct BeachProperties (*new)(void);
} BeachProperties;

#if defined(__cplusplus)
}
#endif

#endif