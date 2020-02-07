#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
//#include <Windows.h>
//#include <Psapi.h>

#include "consts.h"
#include "BeachGrid.h"
#include "BeachNode.h"
#include "WaveClimate.h"
#include "sedtrans.h"
#include "utils.h"
#include "config.h"


/* Globals */
struct BeachGrid g_beachGrid;
int current_time_step;
double current_time;

/* Wave climate object */
struct WaveClimate g_wave_climate;

/* Config parameter */
Config myConfig;

/* Functions */
void InitializeBeachGrid();
void SedimentTransport();

/* Main steps */
int cem_initialize(Config config);
double* cem_update(int saveInterval);
int cem_finalize(void);

/* Logging and Debugging */
void SaveOutputGrid();
double* outputGrid;
void test_LogShoreline();
void test_OutputGrid();

// TODO: Add error and data return
int cem_initialize(Config config)
{
	srand(time(NULL));
	current_time_step = 0;
	current_time = 0.0;

	myConfig = config;
	g_wave_climate = WaveClimate.new(myConfig.wavePeriods, myConfig.waveAngles, myConfig.waveHeights,
		myConfig.asymmetry, myConfig.stability, myConfig.numTimesteps, myConfig.numWaveInputs);

	InitializeBeachGrid();

 	if (g_beachGrid.FindBeach(&g_beachGrid) < 0)
	{
		return -1;
	}
	outputGrid = malloc(myConfig.nRows * myConfig.nCols * sizeof(double));

	//t = clock() - t;
	//printf("initialize (new): %f\n", ((double)t) / CLOCKS_PER_SEC);

	return 0;
}

// Update the CEM by given steps
double* cem_update(int saveInterval) {
	int i;
	for (i = 0; i < saveInterval; i++)
	{
		g_beachGrid.current_time = current_time_step;
		SedimentTransport();
		current_time_step++;
		current_time += myConfig.lengthTimestep;
	}

	SaveOutputGrid();
	return outputGrid;
}

int cem_finalize() {
	//PROCESS_MEMORY_COUNTERS pmc;
	//if (GetProcessMemoryInfo(GetCurrentProcess(),	&pmc, sizeof(pmc)))
	//{
	//	printf("\WorkingSetSize (new): 0x%08X - %u\n", pmc.PeakWorkingSetSize, pmc.PeakWorkingSetSize / 1024);
	//}
	// free everything
	struct BeachNode* curr = g_beachGrid.shoreline;
	free(curr->prev);
	curr->prev = NULL;
	while (!curr->next->is_boundary) {
		curr = curr->next;
	}
	free(curr->next);
	curr->next = NULL;

	struct BeachNode** cells = g_beachGrid.cells;
	free2d(cells);
	free(outputGrid);
	return 0;
}

/* -----MAIN FUNCTIONS---- */

void InitializeBeachGrid()
{
	g_beachGrid = BeachGrid.new(myConfig.nRows, myConfig.nCols, myConfig.cellWidth, myConfig.cellLength);
	struct BeachNode** nodes = (struct BeachNode**)malloc2d(myConfig.nRows, myConfig.nCols, sizeof(struct BeachNode));

	int r, c;
	for (r = 0; r < myConfig.nRows; r++)
	{
		for (c = 0; c < myConfig.nCols; c++)
		{
			double val = myConfig.grid[r][c];
			nodes[r][c] = BeachNode.new(val, r, c, myConfig.cellWidth, myConfig.cellLength);
		}
	}

	g_beachGrid.SetCells(&g_beachGrid, nodes);
}

void SedimentTransport()
{
	WaveTransformation(&g_beachGrid,
		g_wave_climate.GetWaveAngle(&g_wave_climate, current_time_step),
		g_wave_climate.GetWavePeriod(&g_wave_climate, current_time_step),
		g_wave_climate.GetWaveHeight(&g_wave_climate, current_time_step),
		myConfig.lengthTimestep);

	GetAvailableSupply(&g_beachGrid,
		myConfig.crossShoreReferencePos,
		myConfig.shelfDepthAtReferencePos,
		myConfig.shelfSlope,
		myConfig.shorefaceSlope,
		myConfig.minimumShelfDepthAtClosure,
		myConfig.depthOfClosure);
	NetVolumeChange(&g_beachGrid);

	TransportSediment(&g_beachGrid,
		myConfig.crossShoreReferencePos,
		myConfig.shelfDepthAtReferencePos,
		myConfig.shelfSlope,
		myConfig.shorefaceSlope,
		myConfig.minimumShelfDepthAtClosure,
		myConfig.depthOfClosure);

	FixBeach(&g_beachGrid);
}

/* ----- CONFIGURATION AND OUTPUT FUNCTIONS -----*/
void SaveOutputGrid()
{
	int r, c;
	for (r = 0; r < myConfig.nRows; r++)
	{
		for (c = 0; c < myConfig.nCols; c++)
		{
			struct BeachNode* node = g_beachGrid.TryGetNode(&g_beachGrid, r, c);
			if (!node)
			{
				continue;
			}
			int i = r * myConfig.nCols + c;
			outputGrid[i] = node->frac_full;
		}
	}
}

void test_LogShoreline() {
	char savefile_name[40] = "test/output/new_shoreline.txt";

	FILE* savefile = fopen(savefile_name, "w");

	int r, c;
	for (r = 0; r < myConfig.nRows; r++)
	{
		for (c = 0; c < myConfig.nCols; c++)
		{
			struct BeachNode* node = g_beachGrid.TryGetNode(&g_beachGrid, r, c);
			if (!node)
			{
				fprintf(savefile, " --");
				continue;
			}
			fprintf(savefile, " %d", node->is_beach);
		}
		fprintf(savefile, "\n");
	}

	fclose(savefile);
}

void test_OutputGrid() {
	char savefile_name[40];
	sprintf(savefile_name, "test/output/new/CEM_%06d.out", current_time_step-1);
	
	FILE* savefile = fopen(savefile_name, "w");
	int r, c;
	for (r = 0; r < myConfig.nRows; r++)
	{
		for (c = 0; c < myConfig.nCols; c++)
		{
			struct BeachNode* node = g_beachGrid.TryGetNode(&g_beachGrid, r, c);
			if (!node)
			{
				fprintf(savefile, " --");
				continue;
			}
			fprintf(savefile, " %lf", node->frac_full);
		}
		fprintf(savefile, "\n");
	}

	fclose(savefile);
}
