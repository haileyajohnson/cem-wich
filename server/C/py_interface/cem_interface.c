#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cem_interface.h"
#include "cem/config.h"

int cem_initialize(Config config);
double* cem_update(void);
int cem_finalize(void);


int initialize(Config config) {
	int status = cem_initialize(config);

	if (status == 0)
		return SUCCESS;
	else
		return FAILURE;
}

double* update(int saveInterval) {
	return cem_update(saveInterval);
}

int finalize() {
	cem_finalize();
	return SUCCESS;
}
