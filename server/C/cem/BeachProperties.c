#include "BeachProperties.h"
#include "consts.h"

static struct BeachProperties new(void) {
	return (struct BeachProperties) {
		.transport_dir = NONE,
		.transport_potential = 0,
		.net_volume_change = 0,
		.prev_angle = EMPTY_double,
		.next_angle = EMPTY_double,
		.surrounding_angle = EMPTY_double,
		.prev_timestamp = EMPTY_INT,
		.next_timestamp = EMPTY_INT,
		.surrounding_timestamp = EMPTY_INT,
		.in_shadow = EMPTY_INT,
		.shadow_timestamp = EMPTY_INT
		};
}

const struct BeachPropertiesClass BeachProperties = { .new = &new };

