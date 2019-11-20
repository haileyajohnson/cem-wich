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
void test_OutputGrid();

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
int cem_update() {

	g_wave_climate.FindWaveAngle(&g_wave_climate);

	SedimentTransport();

	Process();

	test_OutputGrid();

	current_time_step++;
	current_time += myConfig.lengthTimestep;

	return 0;
}

int cem_finalize() {
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
				struct BeachNode* endNode = startNode;

				// current cell
				int cur_r = r;
				int cur_c = c;
				struct BeachNode* curr = startNode;

				// direction
				int dir_r = 1; // downward
				int dir_c = 0;

				// search clockwise, then counterclockwise
				int search_dir = 1; // clockwise
				
				// while not done tracing landmass
				while (TRUE)
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
						// turn 45 degrees clockwise/counterclockwise to next neighbor
						double angle = atan2(temp[0] - cur_r, temp[1] - cur_c);
						angle += (search_dir) * (PI / 4);
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
					if (!foundNextBeach) {
						if (search_dir > 0) {
							search_dir = -1;
							curr = startNode;
							continue;
						}
						break; 
					}

					cur_r = temp[0];
					cur_c = temp[1];

					struct BeachNode* tempNode = g_beachGrid.TryGetNode(&g_beachGrid, cur_r, cur_c);
					if (BeachNode.isEmpty(tempNode)) { return -1; }
					if (search_dir > 0) { // searching clockwise
						curr->next = tempNode;
						tempNode->prev = curr;
						endNode = tempNode;
					}
					else { // backtracking
						curr->prev = tempNode;
						tempNode->next = curr;
						shorelines[num_landmasses - 1] = tempNode;
						startNode = tempNode;
					}
					// node already a beach cell, break
					if (tempNode->is_beach) {
						if (search_dir > 0) {
							search_dir = -1;
							curr = startNode;
							continue;
						}
						break;
					}
					curr = tempNode;
				}
				// check start boundary
				if (BeachNode.isEmpty(startNode->prev))
				{
					struct BeachNode* startBoundary = BeachNode.boundary();
					if (startNode->GetCol(startNode) == 0) { startBoundary->col = -1; }
					else if (startNode->GetCol(startNode) == myConfig.nCols - 1) { startBoundary->col = myConfig.nCols; }
					else if (startNode->GetRow(startNode) == 0) { startBoundary->row = -1; }
					else if (startNode->GetRow(startNode) == myConfig.nRows - 1) { startBoundary->row = myConfig.nRows; }
					else {
						// if not an edge node, break: invalid grid
						return -1;
					}
					startNode->prev = startBoundary;
					startBoundary->next = startNode;
				}
				// check end boundary
				if (BeachNode.isEmpty(endNode->next))
				{
					struct BeachNode* endBoundary = BeachNode.boundary();
					if (endNode->GetCol(endNode) == 0) { endBoundary->col = -1; }
					else if (endNode->GetCol(endNode) == myConfig.nCols - 1) { endBoundary->col = myConfig.nCols; }
					else if (endNode->GetRow(endNode) == 0) { endBoundary->row = -1; }
					else if (endNode->GetRow(endNode) == myConfig.nRows - 1) { endBoundary->row = myConfig.nRows; }
					else {
						// if not an edge node, break: invalid grid
						return -1;
					}
					endNode->next = endBoundary;
					endBoundary->prev = endNode;
				}

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
	// TODO
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
			fprintf(savefile, " %00d", node->is_beach);
		}
		fprintf(savefile, "\n");
	}

	fclose(savefile);
}

void test_OutputGrid() {
	char savefile_name[40];
	sprintf(savefile_name, "server/test/output/CEM_%06d.out", current_time_step);
	
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
			fprintf(savefile, " %00d", node->frac_full);
		}
		fprintf(savefile, "\n");
	}

	fclose(savefile);
}
