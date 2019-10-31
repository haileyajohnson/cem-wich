#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

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

/*TODO constants */
const int c_cross_shore_reference_pos = 10;
const double c_shelf_depth_at_reference_pos = 10;
const double c_minimum_shelf_depth_at_closure = 10;

/* Functions */
void InitializeBeachGrid();
int FindBeach();
void SedimentTransport();

/* Helpers */
int IsLandCell(int row, int col);

// Main steps
int cem_initialize(Config config);
int cem_update(void);
int cem_finalize(void);

/* Logging and Debugging */
void Process();
void test_LogShoreline();

// TODO: Add error and data return

int add_nums(int a, int b) {
	return a + b;
}

int cem_initialize(Config config)
{
	srand(time(NULL));
	current_time_step = 0;
	current_time = 0.0;

	myConfig = config;
	g_wave_climate = WaveClimate.new(myConfig.asymmetry, myConfig.stability, myConfig.waveHeight, myConfig.wavePeriod);

	InitializeBeachGrid();

 	if (FindBeach() < 0)
	{
		return -1;
	}

	test_LogShoreline();

	return 0;
}

// Update the CEM by a single time step.
int cem_update(void) {

	g_wave_climate.FindWaveAngle(&g_wave_climate);

	SedimentTransport();

	//Process();

	current_time_step++;
	current_time += myConfig.lengthTimestep;

	return 0;
}

int cem_finalize(void) {
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
			float val = myConfig.grid[r][c];
			nodes[r][c] = BeachNode.new(val, r, c, myConfig.cellWidth, myConfig.cellLength);
		}
	}

	g_beachGrid.SetCells(&g_beachGrid, nodes);
}

int FindBeach()
{
	// empty shoreline pointer
	struct BeachNode** shorelines = malloc(sizeof(struct BeachNode*));
	int num_landmasses = 0;

	int r, c;
	for (c = 0; c < myConfig.nCols; c++) {
		// flag to skip cells within landmasses
		int search = TRUE;
		for (r = 0; r < myConfig.nRows; r++) {
			// turn off search when we encounter boundary
			struct BeachNode* node = g_beachGrid.TryGetNode(&g_beachGrid, r, c);
			if (node->is_beach)
			{
				search = FALSE;
			}
			// turn on search when we encounter ocean cell
			if (!search && node->frac_full == 0)
			{
				search = TRUE;
			}

			// start tracing shoreline
			if (search && IsLandCell(r, c)) {
				// start cell
				struct BeachNode* startNode = node;
				if (BeachNode.isEmpty(startNode)) { return -1; }

				// add to shorelines array
				num_landmasses++;
				if (num_landmasses > 1)
				{
					// grow shorelines array
					struct BeachNode** temp = realloc(shorelines, num_landmasses * sizeof(struct BeachNode*));
					if (temp)
					{
						shorelines = temp;
					}
					else {
						// deal with error
					}
				}

				shorelines[num_landmasses - 1] = startNode;

				// current cell
				int cur_r = r;
				int cur_c = c;
				struct BeachNode* curr = startNode;

				// direction
				int dir_r = 1; // downward
				int dir_c = 0;
				
				// while not done tracing landmass
				do
				{
					// mark as beach
					if (BeachNode.isEmpty(curr)) { return -1; }
					curr->is_beach = TRUE;

					// backtrack
					dir_r = -dir_r;
					dir_c = -dir_c;
					int backtrack[2] = { cur_r + dir_r, cur_c + dir_c };
					int temp[2] = { backtrack[0], backtrack[1] };

					int foundNextBeach = FALSE;
					do
					{
						// turn 45 degrees clockwise to next neighbor
						double angle = atan2(temp[0] - cur_r, temp[1] - cur_c);
						angle += (PI / 4);
						int next[2] = { cur_r + round(sin(angle)), cur_c + round(cos(angle)) };

						// break if running off edge of grid
						if (next[0] < 0 || next[0] >= myConfig.nRows || next[1] < 0 || next[1] > myConfig.nCols) { break; }

						// track dir relative to previous neighbor for backtracking
						dir_r = next[0] - temp[0];
						dir_c = next[1] - temp[1];
						temp[0] = next[0];
						temp[1] = next[1];
						if (IsLandCell(temp[0], temp[1])) {
							foundNextBeach = TRUE;
							break;
						}
					} while (temp[0] != backtrack[0] || temp[1] != backtrack[1]);

					// end landmass
					if (!foundNextBeach) { break; }

					cur_r = temp[0];
					cur_c = temp[1];

					struct BeachNode* tempNode = g_beachGrid.TryGetNode(&g_beachGrid, cur_r, cur_c);
					if (BeachNode.isEmpty(tempNode)) { return -1; }
					// node already a beach cell, break
					if (tempNode->is_beach) { break; }
					curr->next = tempNode;
					tempNode->prev = curr;
					curr = tempNode;

				} while (cur_r != r || cur_c != c);
				// back track so that search will cross land boundary and set flag to false
				r--;
			}
		}
	}

	// no shoreline found
	if (!shorelines) {
		// free shorelines var
		free(shorelines);
		return -1;
	}

	g_beachGrid.SetShorelines(&g_beachGrid, shorelines, num_landmasses);
	return 1;
}

void SedimentTransport()
{
	g_wave_climate.SetWaveHeight(&g_wave_climate);
	WaveTransformation(&g_beachGrid, g_wave_climate.wave_angle, g_wave_climate.wave_period, g_wave_climate.GetWaveHeight(&g_wave_climate), myConfig.lengthTimestep);
	GetAvailableSupply(&g_beachGrid, c_cross_shore_reference_pos, c_shelf_depth_at_reference_pos, myConfig.shelfslope, myConfig.shorefaceSlope, c_minimum_shelf_depth_at_closure);
	NetVolumeChange(&g_beachGrid);
	TransportSediment(&g_beachGrid, c_cross_shore_reference_pos, c_shelf_depth_at_reference_pos, myConfig.shelfslope, myConfig.shorefaceSlope, c_minimum_shelf_depth_at_closure);
	FixBeach(&g_beachGrid);
}

/* ----- HELPER FUNCTIONS ----- */
int IsLandCell(int row, int col)
{
	if (row < 0 || row > g_beachGrid.rows || col < 0 || col > g_beachGrid.cols)
	{
		return FALSE;
	}

	struct BeachNode* temp = g_beachGrid.TryGetNode(&g_beachGrid, row, col);
	if (BeachNode.isEmpty(temp))
	{
		return FALSE;
	}

	return (temp->frac_full > 0);
}

/* ----- CONFIGURATION AND OUTPUT FUNCTIONS -----*/
void Process()
{
	// rock grid
	// sand grid
	// shoreline position
	// wave characteristics
	// miscellaneous data
	if (current_time_step % myConfig.saveInterval != 0) { return; }

	char savefile_name[40];
	sprintf(savefile_name, "../output/CEM_%06d.out", current_time_step);
	printf("Saving as: %s \n \n \n", savefile_name);

	FILE* savefile = fopen(savefile_name, "w");
	if (!savefile) { // TODO
	}

	fprintf(savefile, "%f %f\n%f %f\n%f\n\n%d %d\n", g_wave_climate.asymmetry, g_wave_climate.stability, g_wave_climate.wave_angle, g_wave_climate.GetWaveHeight(&g_wave_climate), g_beachGrid.shoreline_change, g_beachGrid.rows, g_beachGrid.cols);

	int r, c;
	for (r = (g_beachGrid.rows - 1); r >= 0; r--)
	{
		for (c = 0; c < g_beachGrid.cols; c++)
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

	printf("file save!\n");
}

void test_LogShoreline() {
	char savefile_name[40] = "server/test/shoreline.txt";

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
