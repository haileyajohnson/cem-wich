#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "consts.h"
#include "globals.h"
#include "BeachGrid.h"
#include "BeachNode.h"
#include "WaveClimate.h"
#include "sedtrans.h"
#include "utils.h"


/* Globals */
struct BeachGrid g_beachGrid;
int g_current_time_step = 0;
double g_current_time = 0.0;
/* Wave climate object */
struct WaveClimate g_wave_climate;

int c_tidal_flag = FALSE;

/* Grid constants (with default values) */
int c_rows;
int c_cols;
int c_cell_size = 100;                            // width and length of each cell (meters)
int c_cross_shore_reference_pos = 10;             // cross-shore distance to a referance point (# cells)
double c_shelf_depth_at_reference_pos = 10;       // depth from surface to shelf at reference point (meters)
double c_minimum_shelf_depth_at_closure = 10;     // minimum shelf depth maintained by wave action (meters)
double c_shelf_slope = 0.001;                     // linear slope of continental shelf
double c_shoreface_slope = 0.01;                  // linear slope of shoreface

/* Model control constants (With default values) */
int c_save_interval = 50;                           // interval of timesteps to save output
double c_num_timesteps = 1000.;                         // number of timesteps for model run
double c_timestep_length = 1.;                          // time represented by each timestep (days)
char *c_input_file;                                 // name of initial conditions file
const char *c_log_file = "../output/CEM_LOG";

/* Functions */
void ReadConfigFile();
double** SetupCells();
double** InitConds();
void InitializeBeachGrid(double** cells, int cellSize);
void InitializeDebugger();
int FindBeach();
double FindWaveAngle();
void SedimentTransport();
void Process();

/* Helpers */
int IsBeachCell(int row, int col);
void GetStartCell(int *row, int *col);
void TurnCounterClockwise(int *dir_r, int *dir_c, int num_turns);

int cem_initialize(void);
int cem_update(void);
int cem_finalize(void);

/* Logging and Debugging */
void LogError(char *message, int fatal);
void LogMessage(char *message);
void LogData(char *name, void *data, size_t itemsize);
enum Debug {

};

enum Debug g_debug;

int add_nums(int a, int b) {
	return a + b;
}

int cem_initialize(void)
{
  srand(time(NULL));

  ReadConfigFile();

  double** cells = SetupCells();
  
  InitializeBeachGrid(cells, c_cell_size);

  if (FindBeach() < 0) 
  {
    return -1;
  }
  
  return 0;
}

// Update the CEM by a single time step.
int cem_update(void) {

  g_wave_climate.wave_angle = FindWaveAngle();

  SedimentTransport();

  Process();

  g_current_time_step++;
  g_current_time += c_timestep_length;

  return 0;
}

int cem_finalize(void) {
  return 0;
}

/* -----MAIN FUNCTIONS---- */


void InitializeBeachGrid(double** cells, int cellSize)
{
    g_beachGrid = BeachGrid.new(c_rows, c_cols, cellSize);
    struct BeachNode **nodes = (struct BeachNode**)malloc2d(c_rows, c_cols, sizeof(struct BeachNode));

    int r, c;
    for (r = 0; r < c_rows; r++)
    {
        for (c = 0; c < c_cols; c++)
        {
            nodes[r][c] = BeachNode.new(cells[r][c], r, c, cellSize);
        }
    }

    g_beachGrid.SetCells(&g_beachGrid, nodes);
}

int FindBeach()
{
  // Find starting point
  int start_r = 0;
  int start_c = 0;
  GetStartCell(&start_r, &start_c);
  if (start_c >= g_beachGrid.cols)
  {
    return -1;
  }

  struct BeachNode *shoreline = g_beachGrid.TryGetNode(&g_beachGrid, start_r, start_c);
  if (BeachNode.isEmpty(shoreline)) { return -1; }
  shoreline->is_beach = TRUE;
  int cells = 1;
  struct BeachNode *curr = shoreline;

  // Find beach cells
  int dir_r = 1; // downward
  int dir_c = 0;

  int r = start_r;
  int c = start_c;
  // while still in grid bounds
  while (r >= 0 && r < g_beachGrid.rows && c >= 0 && c < g_beachGrid.cols)
  {
    // rotate direction of search to the right 90 degrees
    TurnCounterClockwise(&dir_r, &dir_c, 6);
    // get cell to check
    int next_r = r + dir_r;
    int next_c = c + dir_c;
    while (!IsBeachCell(next_r, next_c))
    {
      // rotate direction of search to the left 45 degrees
      TurnCounterClockwise(&dir_r, &dir_c, 1);
      next_r = r + dir_r;
      next_c = c + dir_c;
    }

    if (!BeachNode.isEmpty(curr->prev))
    {
      // break if our next cell is also our previous cell; no other beach cells adjacent
      if (next_r == curr->prev->row && next_c == curr->prev->col)
      {
        break;
      }
    }

    r = next_r;
    c = next_c;
    struct BeachNode *nextNode = g_beachGrid.TryGetNode(&g_beachGrid, r, c);
    if (BeachNode.isEmpty(nextNode)) { return -1; }
    curr->next = nextNode;
    nextNode->prev = curr;
    curr = nextNode;
    curr->is_beach = TRUE;
	cells++;

    // break if we make a complete loop
    if (r == start_r && c == start_c) { break; }
  }

  g_beachGrid.SetShoreline(&g_beachGrid, shoreline);
  return cells;
}

double FindWaveAngle()
{
  double angle = RandZeroToOne() * (PI/4);   // random angle 0 - pi/4
  if (RandZeroToOne() >= g_wave_climate.stability)        // random variable determining above or below 45 degrees
  {
    angle += PI/4;
  }
  if (RandZeroToOne() >= g_wave_climate.asymmetry)        // random variable determining direction of approach (positive = left)
  {
    angle = -angle;
  }

  if (g_wave_climate.temporal_resolution >= 0 && g_current_time_step % g_wave_climate.temporal_resolution == 0)
  {
	  g_wave_climate.UpdateWaveClimate(&g_wave_climate);
	  printf("\nA=%f\tU=%f\n", g_wave_climate.asymmetry, g_wave_climate.stability);
  }
  return angle;
}

void SedimentTransport()
{
  g_wave_climate.SetWaveHeight(&g_wave_climate);
  WaveTransformation(&g_beachGrid, g_wave_climate.wave_angle, g_wave_climate.wave_period, g_wave_climate.GetWaveHeight(&g_wave_climate));
  GetAvailableSupply(&g_beachGrid, c_cross_shore_reference_pos, c_shelf_depth_at_reference_pos, c_shelf_slope, c_shoreface_slope, c_minimum_shelf_depth_at_closure);
  NetVolumeChange(&g_beachGrid);
  TransportSediment(&g_beachGrid, c_cross_shore_reference_pos, c_shelf_depth_at_reference_pos, c_shelf_slope, c_shoreface_slope, c_minimum_shelf_depth_at_closure);
  FixBeach(&g_beachGrid);
}

void Process()
{
	// rock grid
	// sand grid
	// shoreline position
	// wave characteristics
	// miscellaneous data
  if (g_current_time_step % c_save_interval != 0) { return; }

  char savefile_name[40];
  sprintf(savefile_name, "../output/CEM_%06d.out", g_current_time_step);
  printf("Saving as: %s \n \n \n", savefile_name);

  FILE *savefile = fopen(savefile_name, "w");
  if (!savefile) { LogError("problem opening output file", TRUE); }

  fprintf(savefile, "%f %f\n%f %f\n%f\n\n%d %d\n", g_wave_climate.asymmetry, g_wave_climate.stability, g_wave_climate.wave_angle, g_wave_climate.GetWaveHeight(&g_wave_climate), g_beachGrid.shoreline_change, g_beachGrid.rows, g_beachGrid.cols);

  int r, c;
  for (r = (g_beachGrid.rows - 1); r >=0; r--)
  {
    for (c = 0; c < g_beachGrid.cols; c++)
    {
      struct BeachNode *node = g_beachGrid.TryGetNode(&g_beachGrid, r, c);
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

/* ----- HELPER FUNCTIONS ----- */
int IsBeachCell(int row, int col)
{
  if (row < 0 || row > g_beachGrid.rows || col < 0 || col > g_beachGrid.cols)
  {
    return FALSE;
  }

  struct BeachNode *temp = g_beachGrid.TryGetNode(&g_beachGrid, row, col);
  if (BeachNode.isEmpty(temp))
  {
    return FALSE;
  }

  return (temp->frac_full > 0 && temp->frac_full < 1);
}

void GetStartCell(int *row, int *col)
{
  while (*col < g_beachGrid.cols)
  {
    if (IsBeachCell(*row, *col))
    {
      return;
    }

    *row  = *row + 1;
    if (*row > g_beachGrid.rows)
    {
      *row = 0;
	  *col = *col + 1;
    }
  }
}

void TurnCounterClockwise(int *dir_r, int *dir_c, int num_turns)
{
  double angle = atan2(-(*dir_r), *dir_c);
  angle += num_turns * (PI/4);
  *dir_r = -round(sin(angle));
  *dir_c = round(cos(angle));
}

/* ----- CONFIGURATION AND OUTPUT FUNCTIONS -----*/
struct table_element
{
    struct table_element *next;
    char *key;
    char *val;
};

#define HASHSIZE 101
static struct table_element *hash_table[HASHSIZE];

unsigned hash(char *str)
{
    unsigned hashval;
    for (hashval = 0; *str != '\0'; str++)
    {
      hashval = *str + 31 * hashval;
    }
    return hashval % HASHSIZE;
}

struct table_element *lookup(char *key)
{
    struct table_element *elem;
    for (elem = hash_table[hash(key)]; elem != NULL; elem = elem->next)
    {
        if (strcmp(key, elem->key) == 0)
          return elem;
    }
    return NULL;
}

char *table_strdup(char *str) /* make a duplicate of s */
{
    char *p;
    p = (char *) malloc(strlen(str)+1); /* +1 for ’\0’ */
    if (p != NULL)
       strcpy(p, str);
    return p;
}

struct table_element *install(char *key, char *val)
{
    struct table_element *elem;
    unsigned hashval;
    if ((elem = lookup(key)) == NULL)
    {
        elem = malloc(sizeof(struct table_element));
        if (elem == NULL || (elem->key = table_strdup(key)) == NULL) { return NULL; }
        hashval = hash(key);
        elem->next = hash_table[hashval];
        hash_table[hashval] = elem;
    }
    else
    {
        free((void *) elem->val);
    }
    if ((elem->val = table_strdup(val)) == NULL) { return NULL; }
    return elem;
}

void ReadConfigFile(){
  // open file
  char read_file_name[] = "../input/config.txt";
  FILE *file = fopen(read_file_name, "r");
  if (file != NULL)
  {
    char *key;
    char *val;

	char *delimiter = "=";
	char line[256];
	// read key value pairs from file
	while (fgets(line, sizeof(line), file) != NULL)
	{
		key = strtok(line, delimiter);
		val = strtok(NULL, delimiter);

		if (install(key, val) == NULL) { printf("WARNING: Invalid key-value input, {%s, %s}", key, val); }
	}

    // assign values to model variables
    struct table_element *elem;
    if ((elem = lookup("cell_size")) != NULL) { c_cell_size = atoi(elem->val); }
    if ((elem = lookup("cross_shore_reference_pos")) != NULL) { c_cross_shore_reference_pos = atoi(elem->val); }
    if ((elem = lookup("shelf_depth_at_reference_pos")) != NULL) { c_shelf_depth_at_reference_pos = atof(elem->val); }
    if ((elem = lookup("minimum_shelf_depth_at_closure")) != NULL) { c_minimum_shelf_depth_at_closure = atof(elem->val); }
    if ((elem = lookup("shelf_slope")) != NULL) { c_shelf_slope = atof(elem->val); }
    if ((elem = lookup("shore_face_slope")) != NULL) { c_shoreface_slope = atof(elem->val); }
	    
    if ((elem = lookup("save_interval")) != NULL) { c_save_interval = atoi(elem->val); }
    if ((elem = lookup("num_timesteps")) != NULL) { c_num_timesteps = atof(elem->val); }
    if ((elem = lookup("timestep_length")) != NULL) { c_timestep_length = atof(elem->val); }
    if ((elem = lookup("input_file")) != NULL) { c_input_file = strtok(elem->val, " \n"); }
	if ((elem = lookup("tidal_flag")) != NULL) { c_tidal_flag = atoi(elem->val); }

	// build wave climate object
	double asymmetry, stability, p_asymmetry_increase, p_asymmetry_decrease, p_stability_increase, p_stability_decrease, wave_height_avg, wave_height_high, p_high_energy_wave, wave_period;
	int temporal_resolution = -1;
	if ((elem = lookup("asymmetry")) != NULL) { asymmetry = atof(elem->val); }
	else { asymmetry = 0.5; }
	if ((elem = lookup("stability")) != NULL) { stability = atof(elem->val); }
	else { stability = 0.5; }
	if ((elem = lookup("p_asymmetry_increase")) != NULL) { p_asymmetry_increase = atof(elem->val); }
	else { p_asymmetry_increase = 0.0; }
	if ((elem = lookup("p_asymmetry_decrease")) != NULL) { p_asymmetry_decrease = atof(elem->val); }
	else { p_asymmetry_decrease = 0.0; }
	if ((elem = lookup("p_stability_increase")) != NULL) { p_stability_increase = atof(elem->val); }
	else { p_stability_increase = 0.0; }
	if ((elem = lookup("p_stability_decrease")) != NULL) { p_stability_decrease = atof(elem->val); }
	else { p_stability_decrease = 0.0; }
	if ((elem = lookup("wave_height_avg")) != NULL) { wave_height_avg = atof(elem->val); }
	else { wave_height_avg = 2.0; }
	if ((elem = lookup("high_energy_wave_height")) != NULL) { wave_height_high = atof(elem->val); }
	else { wave_height_high = 2.0; }
	if ((elem = lookup("p_high_energy_wave")) != NULL) { p_high_energy_wave = atof(elem->val); }
	else { p_high_energy_wave = 0.0; }
	if ((elem = lookup("wave_period")) != NULL) { wave_period = atof(elem->val); }
	else { wave_period = 10; }
	if ((elem = lookup("temporal_resolution")) != NULL) { temporal_resolution = atof(elem->val); }
	g_wave_climate = WaveClimate.new(asymmetry, stability, p_asymmetry_increase, p_asymmetry_decrease, p_stability_increase, p_stability_decrease, wave_height_avg, wave_height_high, p_high_energy_wave, wave_period, temporal_resolution);

    InitializeDebugger();
    fclose(file);
  }
  else
  {
	  LogError("No config.txt file found", TRUE);
  }
}

void InitializeDebugger()
{
	return;
}

double** SetupCells()
{
	if (c_input_file != NULL)
	{
		FILE *file = fopen(strcat("../input/", c_input_file), "r");
		if (file != NULL)
		{
			char line[2048];
			char* ptr;
			// read grid dimensions
			if (fgets(line, sizeof(line), file) == NULL) { LogError("Input file is empty", TRUE); }
			if ((c_rows = strtol(strtok(line, " "), &ptr, 10)) == 0 || (c_cols = strtol(strtok(NULL, " "), &ptr, 10)) == 0) { LogError("Invalid input grid size", TRUE); }

			double** cells = (double**)malloc2d(c_rows, c_cols, sizeof(double));
			// read values
			int r;
			for (r = 0; r < c_rows; r++)
			{
				if (fgets(line, sizeof(line), file) == NULL) { LogError("Expected more rows in input grid", TRUE); }
				char *temp = strtok(line, "\t");
				int c;
				for (c = 0; c < c_cols; c++)
				{
					if (!temp) { LogError("Expected more columns in input grid", TRUE); }
					cells[r][c] = strtod(temp, &ptr);
					temp = strtok(NULL, "\t");
				}
			}
			fclose(file);
			return cells;
		}
		else
		{
			LogError("Input file not found", TRUE);
			return NULL;
		}
	}
	else
	{
		return InitConds();
	}
}

double** InitConds()
{
	return NULL;
}

void LogError(char *message, int fatal)
{
	const char* ERROR_MESSAGE = fatal ? "FATAL ERROR: " : "ERROR: ";
	FILE *file = fopen(c_log_file, "a");
	char message_str[128];
	sprintf(message_str, "\nError caught at timestep %d\n%s%s\n", g_current_time_step, ERROR_MESSAGE, message);
	fprintf(file, message_str);
	fclose(file);
	if (fatal) { exit(-1); }
}

void LogMessage(char *message)
{
	const char *USER_MESSAGE = "USER MESSAGE: ";
	FILE *file = fopen(c_log_file, "a");
	char message_str[128];
	sprintf(message_str, "\nMessage logged at timestep %d\n%s%s\n", g_current_time_step, USER_MESSAGE, message);
	fprintf(file, message_str);
	fclose(file);
}

// TODO: finish data log
void LogData(char *name, void **data, size_t itemsize)
{
	char savefile_name[32];
	sprintf(savefile_name, "../output/CEM_%06d.out", g_current_time_step);
	printf("Saving as: %s \n \n \n", savefile_name);
	FILE *file = fopen(savefile_name, "a");
	char metadata[32];
	sprintf(metadata, "%s: \n", name);
	fprintf(file, metadata);

	fclose(file);
}