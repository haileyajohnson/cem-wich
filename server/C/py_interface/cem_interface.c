#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cem_interface.h"
#include "cem/config.h"

int run_test(Config config, int numTimesteps, int saveInterval);
int cem_initialize(Config config);
double* cem_update(int saveInterval);
int cem_finalize(void);

void test_LogShoreline();
void test_OutputGrid();

int run_test(Config config, int numTimesteps, int saveInterval)
{
	if (cem_initialize(config) != 0) { return FAILURE; }
	test_LogShoreline();
	int i = 0;
	while (i < numTimesteps)
	{
		printf("%d\n", i);
		int steps = (i + saveInterval) < numTimesteps ? saveInterval: (numTimesteps - i);
		double* out = cem_update(steps);
		test_OutputGrid();
		i += saveInterval;
	}
	if (cem_finalize(config) != 0) { return FAILURE; }
	return SUCCESS;
}

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
