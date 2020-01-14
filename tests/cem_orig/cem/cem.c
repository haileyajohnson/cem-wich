#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "consts.h"
#include "config.h"
#include "utils.h"
#include "cem.h"

// replace consts with config vars
Config myConfig;
double cell_width;
double cell_length;
double SHELF_SLOPE;
double SHOREFACE_SLOPE;

// minimum depth of shoreface due to wave action (meters)
double DEPTH_SHOREFACE;
// cell where intial conditions changes from beach to ocean
double INIT_BEACH;
// theoretical depth in meters of continental shelf at x = INIT_BEACH
double INITIAL_DEPTH;

// Global variables to be share with other files.
int current_time_step = 0;  // Time step of current calculation
double current_time = 0.;  // Current model time


// Global variables to be used only within this file.
static const double kAngleFactor = 1.;

// Overall Shoreface Configuration Arrays - Data file information
// Fractional amount of cell full of sediment LMV
double** PercentFullSand = NULL;

static FILE* SaveSandFile;
static FILE* ReadSandFile;
static FILE* ReadRealWaveFile;

// Computational Arrays (determined for each time step)  -- will eventually be
// in a structure for BMI

static int X[MaxBeachLength]; // X Position of ith beach element
static int Y[MaxBeachLength]; // Y Position of ith beach element
static int XBehind[MaxBeachLength]; // Cell that is "behind" X[i] LMV
static int YBehind[MaxBeachLength]; // Cell that is "behind" Y[i] LMV
static char InShadow[MaxBeachLength]; // Is ith beach element in shadow?
static double ShorelineAngle[MaxBeachLength]; // Angle between cell and right (z+1) neighbor
static double SurroundingAngle[MaxBeachLength]; // Angle between left and right neighbors
static char UpWind[MaxBeachLength]; // Upwind or downwind condition used to calculate sediment transport
static double VolumeIn[MaxBeachLength]; // Sediment volume into ith beach element
static double VolumeOut[MaxBeachLength]; // Sediment volume out of ith beach element
static double VolumeAcrossBorder[MaxBeachLength]; // Sediment volume across border of ith beach element in m^3/day LMV
/* amount sediment needed, not necessarily amount a cell gets */
static double ActualVolumeAcross[MaxBeachLength]; // Sediment volume that actually gets passed across border LMV
/* amount sed is limited by volumes across border upstream and downstream */
static char DirectionAcrossBorder[MaxBeachLength]; // Flag to indicate if transport across border is L or R LMV
static char FlowThroughCell[MaxBeachLength]; // Is flow through ith cell Left, Right, Convergent, or Divergent LMV

static double WvHeight;
static double Angle;
static double WvPeriod;

// Miscellaneous Global Variables

static int NextX; // Global variables used to iterate FindNextCell in global array -
static int NextY; // would've used pointer but wouldn't work
static int BehindX;
static int BehindY;
static int TotalBeachCells; // Number of cells describing beach at particular iteration
static int ShadowXMax; // used to determine maximum extent of beach cells
static double WaveAngle; // wave angle for current time step
static int FindStart; // Used to tell FindBeach at what Y value to start looking
static char FellOffArray; // Flag used to determine if accidentally went off array
static int NumWaveBins; // For Input Wave - number of bins
static double WaveMax[36]; // Max Angle for specific bin
static double WaveProb[36]; // Probability of Certain Bin
static double WaveAngleIn;
static double WaveHeightIn;
static double WavePeriodIn;

static double Period = 10.0; // seconds
static double OffShoreWvHt = 2.0; // meters

// Fractional portion of waves coming from positive (left) direction
static double Asym = 0.70;
// .5 = even dist, < .5 high angle domination. NOTE Highness actually
// determines lowness!
static double Highness = 0.35;

// Factor applied to the input wave height; 1 = no change, 0.5 = half
// wave height, and 2 = double wave height
static double waveheightchange = 1.;
// Factor applied to the input wave period; 1 = no change, 0.5 = half
// wave period, and 2 = double wave period
static double waveperiodchange = 1.;

void AdjustShore(int i);
void DetermineAngles(void);
void DetermineSedTransport(void);
void FindBeachCells(int YStart);
char FindIfInShadow(int xin, int yin, int ShadMax);
void FindNextCell(int x, int y, int z);
double FindWaveAngle(void);
void FixBeach(void);
void FixFlow(void);
void FlowInCell(void);
void OopsImEmpty(int x, int y);
void OopsImFull(int x, int y);
void periodic_boundary_copy(void);
double Raise(double b, double e);
double RandZeroToOne(void);
void ReadSandFromFile(void);
void RealWaveIn(void);
void SaveSandToFile(void);
void SaveLineToFile(void);
void SedTrans(int From, int To, double ShoreAngle, char MaxT, int Last);
void ShadowSweep(void);
void TransportSedimentSweep(void);
void WaveOutFile(void);
void ZeroVars(void);

int run_test(Config config, int numTimesteps, int saveInterval);
int initialize(Config config);
int update(int saveInterval);
int finalize();

int run_test(Config config, int numTimesteps, int saveInterval) {
	if (initialize(config) != 0) { exit(1); }
	int i = 0;
	while (i < numTimesteps)
	{
		int steps = (i + saveInterval) < numTimesteps ? saveInterval : (numTimesteps - i);
		if (update(steps) != 0) { exit(1); }
		i += saveInterval;
	}
	if (finalize() != 0) { exit(1); }
	return 0;
}

int initialize(Config config) {
	myConfig = config;
	cell_width = config.cellWidth;
	cell_length = config.cellLength;
	SHELF_SLOPE = config.shelfSlope;
	SHOREFACE_SLOPE = config.shorefaceSlope;
	DEPTH_SHOREFACE = config.minimumShelfDepthAtClosure;
	INIT_BEACH = X_MAX - config.crossShoreReferencePos -1;
	INITIAL_DEPTH = config.shelfDepthAtReferencePos;

	ShadowXMax = X_MAX; /* RCL: was X_MAX-5; */

	if (SEED == -999)
		srand(time(NULL));
	else
		srand(SEED);

	{ // Allocate memory for arrays.
		const int n_rows = X_MAX;
		const int n_cols = 2 * Y_MAX;
		PercentFullSand = (double**)malloc2d(n_rows, n_cols, sizeof(double));
	}

	ReadSandFromFile();

	/* Set Periodic Boundary Conditions */
	periodic_boundary_copy();

	/* Fix the Beach (?) */
	FixBeach();

	FindBeachCells(0);
	SaveLineToFile();

	current_time_step = 0;

	return 0;
}

// Update the CEM by a single time step.
int update(int saveInterval) {
	// duration loop variable, moved from former (now deleted) 'main.c' above
	int xx;
		/*  Time Step iteration - compute same wave angle for Duration time steps */
	
	/*  Loop for Duration at the current wave sign and wave angle */
	for (xx = 0; xx < saveInterval; xx++) {
		periodic_boundary_copy(); /* Copy visible data to external model - creates
															 boundaries
		/*  Calculate Wave Angle */
		WaveAngle = FindWaveAngle();
		ZeroVars();

		/* Initialize for Find Beach Cells  (make sure strange beach does not
		 * cause trouble */
		FellOffArray = 'y';
		int FindStart = 0;
		/*  Look for beach - if you fall off of array, bump over a little and try
		 * again */
		while (FellOffArray == 'y') {
			FindBeachCells(FindStart);
			FindStart += FindCellError;

			/* Get Out if no good beach spots exist - finish program */
			if (FindStart > Y_MAX / 2 + 1) {
				SaveSandToFile();
				return 1;
			}
		}

		/*LMV*/ ShadowSweep();

		DetermineAngles();

		DetermineSedTransport();

		FlowInCell();
		/*LMV*/ FixFlow();
		/*LMV*/ TransportSedimentSweep();

		FixBeach();
		current_time_step++;
		current_time += myConfig.lengthTimestep;
	}

	SaveSandToFile();

	return 0;
}

int finalize() {
	free2d((void**)PercentFullSand);

	printf("Run Complete.");

	return 0;
}

/* -----FUNCTIONS ARE BELOW----- */

double FindWaveAngle(void)
/* calculates wave angle for given time step */
{
	double Angle;
	
	RealWaveIn(); /*Read real data (angle/period/height) */

	Angle = ((double)(WaveAngleIn));
	if (Angle >= PI)
		Angle -= (2*PI); /* conversion to CEM angles (-180 -- 0 -- +180) */
	//Angle *= -1; /*-1 is there as wave angle is from 90 in the west to -90 	the east*/

	Period = WavePeriodIn * waveperiodchange;
	OffShoreWvHt = WaveHeightIn * waveheightchange;

	if (fabs(Angle) > PI/2) {
		Angle = 0; /*if waves were to come from behind coast */
		OffShoreWvHt =
			1e-10; /*if waves were to come from behind coast set to inf small */
	}
	return Angle;
}


void FindBeachCells(int YStart)
/* Determines locations of beach cells moving from left to right direction
	 */
	 /* This function will affect and determine the global arrays:  X[] and Y[]
			*/
			/* This function calls FindNextCell */
			/* This will define TotalBeachCells for this time step */
{
	int y, z, xstart; /* local iterators */

	/* Starting at left end, find the x - value for first cell that is 'allbeach'
	 */

	xstart = X_MAX - 1;
	y = YStart;

	while (!(PercentFullSand[xstart][y] > 0)) {
		xstart -= 1;
	}

	//xstart += 1; /* Step back to where partially full beach */


	X[0] = xstart;
	Y[0] = YStart;

	z = 0;
	X[z - 1] = X[z];
	Y[z - 1] = Y[z] - 1;

	while ((Y[z] < 2 * Y_MAX - 1) && (z < MaxBeachLength - 1)) {
		z++;
		NextX = -2;
		NextY = -2;
		BehindX = -2;
		BehindY = -2;

		FindNextCell(X[z - 1], Y[z - 1], z - 1);
		X[z] = NextX;
		Y[z] = NextY;
		XBehind[z] = BehindX;
		YBehind[z] = BehindY;

		/* If return to start point or go off left side of array, going the wrong
		 * direction     */
		 /* Jump off and start again closer to middle */

		if ((NextY < 1) || ((NextY == Y[0]) && (NextX == X[0])) ||
			(z > MaxBeachLength - 2)) {
			FellOffArray = 'y';
			ZeroVars();
			return;
		}
	}

	TotalBeachCells = z;
	FellOffArray = 'n';
}

void FindNextCell(int x, int y, int z)

/* Function to find next cell that is beach moving in the general positive X
	 direction */
	 /* changes global variables NextX and NextY, coordinates for the next beach cell
			*/
			/* This function will use but not affect the global arrays:  AllBeach [][], X[],
				 and Y[] */
				 /* New thinking...5/04 LMV */
{

	if (x == X_MAX - 1 || y == 2 * Y_MAX - 1)
	{
		return;
	}
	if ((X[z - 1] == X[z]) && (Y[z - 1] == Y[z] - 1))
		/* came from left */
	{
		if ((PercentFullSand[x + 1][y]) > 0.0) {
			NextX = x + 1;
			NextY = y;
			BehindX = NextX - 1;
			BehindY = NextY;
			return;
		}
		else if ((PercentFullSand[x + 1][y + 1]) >
			0.0) {
			NextX = x + 1;
			NextY = y + 1;
			BehindX = NextX - 1;
			BehindY = NextY;
			return;
		}
		else if ((PercentFullSand[x][y + 1]) > 0.0) {
			NextX = x;
			NextY = y + 1;
			BehindX = NextX - 1;
			BehindY = NextY;
			return;
		}
		else if ((PercentFullSand[x - 1][y + 1]) >
			0.0) {
			NextX = x - 1;
			NextY = y + 1;
			BehindX = NextX - 1;
			BehindY = NextY;
			return;
		}
		else if ((PercentFullSand[x - 1][y]) > 0.0) {
			NextX = x - 1;
			NextY = y;
			BehindX = NextX - 1;
			BehindY = NextY;
			return;
		}
		else if ((PercentFullSand[x - 1][y - 1]) >
			0.0) {
			NextX = x - 1;
			NextY = y - 1;
			BehindX = NextX - 1;
			BehindY = NextY;
			return;
		}
		else
			printf("Should've found next cell (Left): %d, %d \n", x, y);
	}

	else if ((X[z - 1] == X[z] + 1) && (Y[z - 1] == Y[z] - 1))
		/* came from upper left */
	{
		if ((PercentFullSand[x + 1][y + 1]) > 0.0) {
			NextX = x + 1;
			NextY = y + 1;
			BehindX = NextX - 1;
			BehindY = NextY;
			return;
		}
		else if ((PercentFullSand[x][y + 1]) > 0.0) {
			NextX = x;
			NextY = y + 1;
			BehindX = NextX - 1;
			BehindY = NextY;
			return;
		}
		else if ((PercentFullSand[x - 1][y + 1]) > 0.0) {
			NextX = x - 1;
			NextY = y + 1;
			BehindX = NextX - 1;
			BehindY = NextY;
			return;
		}
		else if ((PercentFullSand[x - 1][y]) > 0.0) {
			NextX = x - 1;
			NextY = y;
			BehindX = NextX;
			BehindY = NextY - 1;
			return;
		}
		else if ((PercentFullSand[x - 1][y - 1]) > 0.0) {
			NextX = x - 1;
			NextY = y - 1;
			BehindX = NextX;
			BehindY = NextY - 1;
			return;
		}
		else if ((PercentFullSand[x][y - 1]) > 0.0) {
			NextX = x;
			NextY = y - 1;
			BehindX = NextX;
			BehindY = NextY - 1;
			return;
		}
		else {
			printf("Should've found next cell (Upper Left): %d, %d \n", x, y);
		}
	}

	else if ((X[z - 1] == X[z] + 1) && (Y[z - 1] == Y[z]))
		/* came from above */
	{
		if ((PercentFullSand[x][y + 1]) > 0.0) {
			NextX = x;
			NextY = y + 1;
			BehindX = NextX;
			BehindY = NextY - 1;
			return;
		}
		else if ((PercentFullSand[x - 1][y + 1]) > 0.0) {
			NextX = x - 1;
			NextY = y + 1;
			BehindX = NextX;
			BehindY = NextY - 1;
			return;
		}
		else if ((PercentFullSand[x - 1][y]) > 0.0) {
			NextX = x - 1;
			NextY = y;
			BehindX = NextX;
			BehindY = NextY - 1;
			return;
		}
		else if ((PercentFullSand[x - 1][y - 1]) > 0.0) {
			NextX = x - 1;
			NextY = y - 1;
			BehindX = NextX;
			BehindY = NextY - 1;
			return;
		}
		else if (PercentFullSand[x][y - 1] > 0.0) {
			NextX = x;
			NextY = y - 1;
			BehindX = NextX;
			BehindY = NextY - 1;
			return;
		}
		else if ((PercentFullSand[x + 1][y - 1]) > 0.0) {
			NextX = x + 1;
			NextY = y - 1;
			BehindX = NextX;
			BehindY = NextY - 1;
			return;
		}
		else if ((PercentFullSand[x + 1][y - 1]) < 1.0) { /* RCL adds ad hoc case */
			NextX = x + 1;
			NextY = y - 1;
			BehindX = NextX;
			BehindY = NextY - 1;
			return;
		}
		else {
			printf("Should've found next cell (Above): %d, %d \n", x, y);
		}
	}

	else if ((X[z - 1] == X[z] + 1) && (Y[z - 1] == Y[z] + 1))
		/* came from upper right */
	{
		if ((PercentFullSand[x - 1][y + 1]) > 0.0) {
			NextX = x - 1;
			NextY = y + 1;
			BehindX = NextX;
			BehindY = NextY - 1;
			return;
		}
		else if ((PercentFullSand[x - 1][y]) > 0.0) {
			NextX = x - 1;
			NextY = y;
			BehindX = NextX;
			BehindY = NextY - 1;
			return;
		}
		else if ((PercentFullSand[x - 1][y - 1]) > 0.0) {
			NextX = x - 1;
			NextY = y - 1;
			BehindX = NextX + 1;
			BehindY = NextY;
			return;
		}
		else if ((PercentFullSand[x][y - 1]) > 0.0) {
			NextX = x;
			NextY = y - 1;
			BehindX = NextX + 1;
			BehindY = NextY;
			return;
		}
		else if ((PercentFullSand[x + 1][y - 1]) > 0.0) {
			NextX = x + 1;
			NextY = y - 1;
			BehindX = NextX + 1;
			BehindY = NextY;
			return;
		}
		else if ((PercentFullSand[x + 1][y]) > 0.0) {
			NextX = x + 1;
			NextY = y;
			BehindX = NextX + 1;
			BehindY = NextY;
			return;
		}
		else {
			printf("Should've found next cell (Upper Right): %d, %d \n", x, y);
		}
	}

	else if ((X[z - 1] == X[z]) && (Y[z - 1] == Y[z] + 1))
		/* came from right */
	{
		if ((PercentFullSand[x - 1][y]) > 0.0) {
			NextX = x - 1;
			NextY = y;
			BehindX = NextX + 1;
			BehindY = NextY;
			return;
		}
		else if ((PercentFullSand[x - 1][y - 1]) > 0.0) {
			NextX = x - 1;
			NextY = y - 1;
			BehindX = NextX + 1;
			BehindY = NextY;
			return;
		}
		else if ((PercentFullSand[x][y - 1]) > 0.0) {
			NextX = x;
			NextY = y - 1;
			BehindX = NextX + 1;
			BehindY = NextY;
			return;
		}
		else if ((PercentFullSand[x + 1][y - 1]) > 0.0) {
			NextX = x + 1;
			NextY = y - 1;
			BehindX = NextX + 1;
			BehindY = NextY;
			return;
		}
		else if ((PercentFullSand[x + 1][y]) > 0.0) {
			NextX = x + 1;
			NextY = y;
			BehindX = NextX + 1;
			BehindY = NextY;
			return;
		}
		else if ((PercentFullSand[x + 1][y + 1]) > 0.0) {
			NextX = x + 1;
			NextY = y + 1;
			BehindX = NextX + 1;
			BehindY = NextY;
			return;
		}
		else {
			printf("Should've found next cell (Right): %d, %d \n", x, y);
		}
	}

	else if ((X[z - 1] == X[z] - 1) && (Y[z - 1] == Y[z] + 1))
		/* came from lower right */
	{
		if ((PercentFullSand[x - 1][y - 1]) > 0.0) {
			NextX = x - 1;
			NextY = y - 1;
			BehindX = NextX + 1;
			BehindY = NextY;
			return;
		}
		else if ((PercentFullSand[x][y - 1]) > 0.0) {
			NextX = x;
			NextY = y - 1;
			BehindX = NextX + 1;
			BehindY = NextY;
			return;
		}
		else if ((PercentFullSand[x + 1][y - 1]) > 0.0) {
			NextX = x + 1;
			NextY = y - 1;
			BehindX = NextX + 1;
			BehindY = NextY;
			return;
		}
		else if ((PercentFullSand[x + 1][y]) > 0.0) {
			NextX = x + 1;
			NextY = y;
			BehindX = NextX;
			BehindY = NextY + 1;
			return;
		}
		else if ((PercentFullSand[x + 1][y + 1]) > 0.0) {
			NextX = x + 1;
			NextY = y + 1;
			BehindX = NextX;
			BehindY = NextY + 1;
			return;
		}
		else if ((PercentFullSand[x][y + 1]) > 0.0) {
			NextX = x;
			NextY = y + 1;
			BehindX = NextX;
			BehindY = NextY + 1;
			return;
		}
		else if (PercentFullSand[x][y + 2] > 0.0 || PercentFullSand[x - 1][y + 2] > 0.0) {
			/* RCL adds new ad hoc case */
			NextX = x;
			NextY = y + 1;
			BehindX = NextX;
			BehindY = NextY + 1;
			return;
		}

		else {
			printf("Should've found next cell (Lower Right): %d, %d \n", x, y);
		}
	}

	else if ((X[z - 1] == X[z] - 1) && (Y[z - 1] == Y[z]))
		/* came from below */
	{
		if ((PercentFullSand[x][y - 1]) > 0.0) {
			NextX = x;
			NextY = y - 1;
			BehindX = NextX;
			BehindY = NextY + 1;
			return;
		}
		else if ((PercentFullSand[x + 1][y - 1]) > 0.0) {
			NextX = x + 1;
			NextY = y - 1;
			BehindX = NextX;
			BehindY = NextY + 1;
			return;
		}
		else if ((PercentFullSand[x + 1][y]) > 0.0) {
			NextX = x + 1;
			NextY = y;
			BehindX = NextX;
			BehindY = NextY + 1;
			return;
		}
		else if ((PercentFullSand[x + 1][y + 1]) > 0.0) { /* RCL fixed typo */
			NextX = x + 1;
			NextY = y + 1;
			BehindX = NextX;
			BehindY = NextY + 1;
			return;
		}
		else if ((PercentFullSand[x][y + 1]) > 0.0) { /* RCL changed behind */
			NextX = x;
			NextY = y + 1;
			BehindX = NextX - 1;
			BehindY = NextY;
			return;
		}
		else if ((PercentFullSand[x - 1][y + 1]) > 0.0)
			/* RCL messed with all the 42.X cases */
		{
			NextX = x - 1;
			NextY = y + 1;
			BehindX = NextX - 1;
			BehindY = NextY;
			return;
		}
		else if ((PercentFullSand[x - 1][y - 1]) > 0.0) {
			NextX = x - 1;
			NextY = y - 1;
			BehindX = NextX + 1;
			BehindY = NextY - 1;
			return;
		}
		else if ((PercentFullSand[x][y + 1]) < 1.0)
			/* RCL added this case */
		{
			NextX = x;
			NextY = y + 1;
			BehindX = NextX - 1;
			BehindY = NextY;
			return;
		}
		else if ((PercentFullSand[x - 1][y + 1]) < 1.0)
			/* RCL added this case */
		{
			NextX = x - 1;
			NextY = y + 1;
			BehindX = NextX - 1;
			BehindY = NextY;
			return;
		}
		else {
			printf("Should've found next cell (Below): %d, %d \n", x, y);
		}
	}
	else if ((X[z - 1] == X[z] - 1) && (Y[z - 1] == Y[z] - 1))
		/* came from lower left */
	{
		if ((PercentFullSand[x + 1][y - 1]) > 0.0) {
			NextX = x + 1;
			NextY = y - 1;
			BehindX = NextX;
			BehindY = NextY + 1;
			return;
		}
		else if ((PercentFullSand[x + 1][y]) > 0.0) {
			NextX = x + 1;
			NextY = y;
			BehindX = NextX;
			BehindY = NextY + 1;
			return;
		}
		else if ((PercentFullSand[x + 1][y + 1]) > 0.0) {
			NextX = x + 1;
			NextY = y + 1;
			BehindX = NextX - 1;
			BehindY = NextY;
			return;
		}
		else if ((PercentFullSand[x][y + 1]) > 0.0) {
			NextX = x;
			NextY = y + 1;
			BehindX = NextX - 1;
			BehindY = NextY;
			return;
		}
		else if ((PercentFullSand[x - 1][y + 1]) > 0.0) {
			NextX = x - 1;
			NextY = y + 1;
			BehindX = NextX - 1;
			BehindY = NextY;
			return;
		}
		else if ((PercentFullSand[x - 1][y]) > 0.0) {
			NextX = x - 1;
			NextY = y;
			BehindX = NextX - 1;
			BehindY = NextY;
			return;
		}
		else {
			printf("Should've found next cell (Lower Left): %d, %d \n", x, y);
		}
	}

	printf("Should've found next cell : %d, %d \n", x, y);
}

void ShadowSweep(void)
/*  Moves along beach and tests to see if cells are in shadow 		*/
/*  This function will use and determine the Global array:  InShadow[]	*/
/*  This function will use and adjust the variable:   ShadowXMax        */
/*  This function will use but not adjust the variable:  TotalBeachCells */
{
	int i;
	
	/* Determine if beach cells are in shadow */
	for (i = 0; i <= TotalBeachCells; i++) {
		InShadow[i] = FindIfInShadow(X[i], Y[i], ShadowXMax);
	}
}

char FindIfInShadow(int xin, int yin, int ShadMax)
/*  Function to determine if particular cell xin,yin is in shadow */
/*  Returns a character 'y' or 'n' to indicate so */
/*  This function will use but not affect the global arrays: */
/*      AllBeach[][] and PercentFull[][] */
/*  This function refers to global variables:  WaveAngle, ShadowStepDistance */
{
	/* Initialize local variables */
	
	// TEMP TO TEST SEDTRANS COMPARISON
	// don't shadow outside main region
	int offset = Y_MAX / 2;
	int ymin = offset;
	int ymax = offset + Y_MAX;

	double xdistance, ydistance;

	int xcheck = xin;
	int ycheck = yin;
	int iteration = 1;

	while ((xcheck < ShadMax) && (ycheck >= ymin) && (ycheck < ymax)) {
		/*  Find a cell along the projection line moving against wave direction */

		xdistance = iteration * ShadowStepDistance * cos(WaveAngle);
		xcheck = xin + rint(xdistance);

		ydistance = -iteration * ShadowStepDistance * sin(WaveAngle);
		ycheck = yin + rint(ydistance);

		if (xcheck >= X_MAX - 1 || ycheck >= 2 * Y_MAX - 1) return 'n';

		/* If AllBeach is along the way, and not next neighbor                  */
		/* Probably won't get to this one, though                               */

		if ((PercentFullSand[xcheck][ycheck] == 1.0) &&
			(/* (abs(ycheck-yin) !=1 && abs(xcheck-xin) !=1) && */
			((xcheck + 1) >
				(xin + (PercentFullSand[xin][yin]) +
					fabs((ycheck - yin) / tan(WaveAngle))))) &&
					(ycheck > -1) && (ycheck < 2 * Y_MAX - 1)) {
			return 'y';
		}
		/* Compare a partially full cell's x - distance to a line projected     */
		/* from the starting beach cell's x-distance                            */
		/* This assumes that beach projection is in x-direction (not too bad)   */
		/* RCL this could be contributing to the jaggedness...need to take into account
		the way it's facing
		or could just ignore partially full cells completely...
		*/
#ifdef FGFG
		else if (((PercentFullSand[xcheck][ycheck] +
			PercentFullRock[xcheck][ycheck]) > 0) &&
			((xcheck + (PercentFullSand[xcheck][ycheck] +
				PercentFullRock[xcheck][ycheck])) >
				(xin + (PercentFullSand[xin][yin] + PercentFullRock[xin][yin]) +
					fabs((ycheck - yin) / tan(WaveAngle)))) &&
					(ycheck > -1) && (ycheck < 2 * Y_MAX - 1)
			/* && !(xcheck == xin) */
			) {
			if (debug2a)
				printf(
					"finishing on a partially full with Xcheck %d, Xin %d, PFS+R %f  "
					"\n",
					xcheck, xin, (PercentFullSand[xcheck][ycheck] +
						PercentFullSand[xcheck][ycheck]));
			return 'y';
		}
#endif
		iteration++;
	}

	/*  No shadow was found */
	return 'n';
}

void DetermineAngles(void)

/*  Function to determine beach angles for all beach cells from left to right */
/*  By convention, the ShorelineAngle will apply to current cell and right
	 neighbor     */
	 /*  This function will determine global arrays: */
	 /*              ShorelineAngle[], UpWind[], SurroundingAngle[] */
	 /*  This function will use but not affect the following arrays and values: */
	 /*              X[], Y[], PercentFull[][], AllBeach[][], WaveAngle */
	 /*  ADA Revised underside, SurroundingAngle 6/03 */
	 /*  ADA Rerevised, 3/04 */
{
	int i, j, k; /* Local loop variables */

	/* Compute ShorelineAngle[]  */
	/*      not equal to TotalBeachCells because angle between cell and rt
	 * neighbor */

	for (i = 1; i < TotalBeachCells - 2; i++) {
		/* function revised 1-04 */
		if (Y[i] > Y[i + 1])
			/* On bottom side of Spit (assuming we must be if going right)  */
		{
			/* math revised 6-03 */

			ShorelineAngle[i] = atan2((X[i + 1] - X[i] - (PercentFullSand[X[i + 1]][Y[i + 1]] + PercentFullSand[X[i]][Y[i]])) * cell_length,
				(Y[i + 1] - Y[i]) * cell_width);

			if (ShorelineAngle[i] > PI) {
				ShorelineAngle[i] -= 2.0 * PI;
			}
		}

		else if ((Y[i] < Y[i + 1]))
			/* function revised 1-04 - asusme if goin right, on regular shore */
			/*  'regular' beach */
		{
			ShorelineAngle[i] = atan2((X[i + 1] - X[i] + (PercentFullSand[X[i + 1]][Y[i + 1]] - PercentFullSand[X[i]][Y[i]])) * cell_length,
				(Y[i + 1] - Y[i]) * cell_width);
		}

		else if (Y[i] == Y[i + 1] && X[i] > X[i + 1])
			/*  Shore up and down, on right side */
		{
			ShorelineAngle[i] = atan2((X[i + 1] - X[i]) * cell_length,
				(Y[i + 1] - Y[i] + (PercentFullSand[X[i + 1]][Y[i + 1]] - PercentFullSand[X[i]][Y[i]])) * cell_width);
		}

		else if (Y[i] == Y[i + 1] && X[i] < X[i + 1])
			/* Shore up and down, on left side */
		{
			ShorelineAngle[i] = atan2((X[i + 1] - X[i]) * cell_length,
				(Y[i + 1] - Y[i] - (PercentFullSand[X[i + 1]][Y[i + 1]] + PercentFullSand[X[i]][Y[i]])) * cell_width);
		}

		else {
			printf("Should've found ShorelineAngle): %d, %d \n", X[i], Y[i]);
		}
	}

	ShorelineAngle[0] = ShorelineAngle[1];
	ShorelineAngle[TotalBeachCells - 2] = ShorelineAngle[TotalBeachCells - 3];
	ShorelineAngle[TotalBeachCells - 1] = ShorelineAngle[TotalBeachCells - 3];

	for (k = 1; k < TotalBeachCells - 1; k++) {
		/* compute SurroundingAngle array */
		/* 02/04 AA averaging doesn't work on bottom of spits */
		/* Use trick that x is less if on bottom of spit - angles must be different
		 * signs as well */

		if ((Y[k - 1] - Y[k + 1] == 2) &&
			(copysign(ShorelineAngle[k - 1], ShorelineAngle[k]) !=
				ShorelineAngle[k - 1])) {
			SurroundingAngle[k] =
				(ShorelineAngle[k - 1] + ShorelineAngle[k]) / 2 + PI;
			if (SurroundingAngle[k] > PI) {
				SurroundingAngle[k] -= 2.0 * PI;
			}
		}
		else {
			SurroundingAngle[k] = (ShorelineAngle[k - 1] + ShorelineAngle[k]) / 2;
		}
	}

	/* Determine Upwind/downwind condition */
	/* Note - Surrounding angle is based upon left and right cell neighbors, */
	/* and is centered on cell, not on right boundary */

	for (j = 1; j < TotalBeachCells - 1; j++) {
		if (fabs(WaveAngle - SurroundingAngle[j]) >= 42.0 / RAD_TO_DEG) {
			UpWind[j] = 'u';
		}
		else {
			UpWind[j] = 'd';
		}
	}
	UpWind[0] = UpWind[1];
	UpWind[TotalBeachCells - 1] = UpWind[TotalBeachCells - 2];
}

void DetermineSedTransport(void)

/*  Loop function to determine which neigbor/situation to use for sediment
	 transport calcs      */
	 /*  Once situation is determined, will use function SedTrans to determine actual
			transport      */
			/*  This function will call SedTrans which will determine global array:
				 VolumeAcrossBorder[] LMV        */
				 /*  This function will use but not affect the following arrays and values: */
				 /*              X[], Y[], InShadow[], UpWind[], ShorelineAngle[] */
				 /*              PercentFullSand[][], AllBeach[][], WaveAngle */
{
	int i; /* Loop variable LMV also sent to SedTrans to determine VolAcrossBorder
						*/
	double ShoreAngleUsed; /* Temporary holder for shoreline angle */
	int CalcCell; /* Cell sediment coming from to go across boundary i */
	int Next, Last; /* Indicators so test can go both left/right */
	int Correction; /* Term needed for shoreline angle and i+1 case, angle stored
										 at i      */
	char UpWindLocal; /* Local holder for upwind/downwind condition */
	char MaxTrans; /* Do we need to compute using maximum transport ? */
	int DoFlux; /* Skip sed transport calcs (added 02/04 AA) */

	for (i = 1; i < TotalBeachCells - 1; i++) {
		MaxTrans = 'n';

		/*  Is littoral transport going left or right?  */
		if ((WaveAngle - ShorelineAngle[i]) > 0) {
			/*  Transport going right, center on cell to left side of border
				*/
				/*  Next cell in positive direction, no correction term needed */
			CalcCell = i;
			Next = 1;
			Last = -1;
			Correction = 0;
			DirectionAcrossBorder[i] = 'r'; /*LMV*/
			DirectionAcrossBorder[i] = 'r'; /*LMV*/
		}
		else {
			/*  Transport going left, center on cell to right side of border
				*/
				/*  Next cell in negative direction, correction term needed */
			CalcCell = i + 1;
			Next = -1;
			Last = 1;
			Correction = -1;
			DirectionAcrossBorder[i] = 'l'; /*LMV*/
		}

		if (InShadow[CalcCell] == 'n') {
			/*  Adjustment for maximum transport when passing through 45 degrees */
			/*  This adjustment is only made for moving from downwind to upwind
			 * conditions  */
			 /*                                                                              */
			 /*  purposefully done before shadow adjustment, only use maxtran when */
			 /*      transition from dw to up not because of shadow */
			 /* keeping transition from uw to dw - does not seem to be big deal (04/02
				* AA) */

			if (((UpWind[CalcCell] == 'd') && (UpWind[CalcCell + Next] == 'u') &&
				(InShadow[CalcCell + Next] == 'n')) ||
				((UpWind[CalcCell + Last] == 'u') && (UpWind[CalcCell] == 'd') &&
				(InShadow[CalcCell + Last] == 'n'))) {
				MaxTrans = 'y';
			}

			/*  Upwind/Downwind adjustment Make sure sediment is put into shadows */
			/*  If Next cell is in shadow, use UpWind condition */

			DoFlux = 1;
			UpWindLocal = UpWind[CalcCell];

			if (InShadow[CalcCell + Next] == 'y') {
				UpWindLocal = 'u';
			}

			/*  If coming out of shadow, downwind should be used            */
			/*  HOWEVER- 02/04 AA - if high angle, will result in same flux in/out
			 * problem */
			 /*      solution  - no flux for high angle waves */

			if ((InShadow[CalcCell + Last] == 'y') && (UpWindLocal == 'u')) {
				DoFlux = 0;
			}

			/*  Use upwind or downwind shoreline angle for calcs                    */

			if (UpWindLocal == 'u') {
				ShoreAngleUsed = ShorelineAngle[CalcCell + Last + Correction];
			}
			else if (UpWindLocal == 'd') {
				ShoreAngleUsed = ShorelineAngle[CalcCell + Correction];
			}

			/* !!! Do not do transport on unerneath c'cause it gets all messed up */
			if (fabs(ShoreAngleUsed) > PI / 2.0) {
				DoFlux = 0;
			}

			/* Send to SedTrans to calculate VolumeIn and VolumeOut */

			if (DoFlux) {
				/* SedTrans (i, ShoreAngleUsed, MaxTrans); */
				/*LMV*/
				SedTrans(i, CalcCell, ShoreAngleUsed, MaxTrans, Last);
			}
		}
	}
}

void SedTrans(int i, int From, double ShoreAngle, char MaxT, int Last)

/*  This central function will calcualte the sediment transported from the cell
	 i-1 to          */
	 /*  the cell at i, using the input ShoreAngle   LMV */
	 /*  This function will caluclate and determine the global arrays: */
	 /*              VolumeAcrossBorder[]    LMV */
	 /*  This function will use the global values defining the wave field: */
	 /*      WaveAngle, Period, OffShoreWvHt */
	 /*  Revised 6/02 - New iterative calc for refraction and breaking, parameters
			revised           */
{
	/* Coefficients - some of these are important */

	double StartDepth = 3 * OffShoreWvHt; /* m, depth to begin refraction calcs
																					(needs to be beyond breakers) */
	double RefractStep =
		.2; /* m, step size to iterate depth for refraction calcs */
	double KBreak = 0.5; /* coefficient for wave breaking threshold */
	double rho = 1020; /* kg/m3 - density of water and dissolved matter */

	/* Variables */

	double AngleDeep;          /* rad, Angle of waves to shore at inner shelf  */
	double Depth = StartDepth; /* m, water depth for current iteration         */
	double Angle;              /* rad, calculation angle                       */
	double CDeep;              /* m/s, phase velocity in deep water            */
	double LDeep;              /* m, offhsore wavelength                       */
	double C;                  /* m/s, current step phase velocity             */
	double kh;                 /* wavenumber times depth                       */
	double n;                  /* n                                            */
	double WaveLength;         /* m, current wavelength                        */

	/* Primary assumption is that waves refract over shore-parallel contours */
	/* New algorithm 6/02 iteratively takes wiave onshore until they break, then
	 * computes Qs        */
	 /* See notes 06/05/02 */

	AngleDeep = WaveAngle - ShoreAngle;

	if (MaxT == 'y') {
		AngleDeep = PI / 4.0;
	}

	/*  Don't do calculations if over 90 degrees, should be in shadow  */

	if (AngleDeep > 0.995 * PI / 2.0 || AngleDeep < -0.995 * PI / 2.0) {
		return;
	}
	/* Calculate Deep Water Celerity & Length, Komar 5.11 c = gT / PI, L = CT */

	CDeep = GRAVITY * Period / (2.0 * PI);
	LDeep = CDeep * Period;

	while (TRUE) {
		/* non-iterative eqn for L, from Fenton & McKee                 */

		WaveLength =
			LDeep *
			Raise(tanh(Raise(Raise(2.0 * PI / Period, 2) * Depth / GRAVITY, .75)),
				2.0 / 3.0);
		C = WaveLength / Period;

		/* Determine n = 1/2(1+2kh/sinh(kh)) Komar 5.21                 */
		/* First Calculate kh = 2 pi Depth/L  from k = 2 pi/L           */

		kh = 2 * PI * Depth / WaveLength;
		n = 0.5 * (1 + 2.0 * kh / sinh(2.0 * kh));

		/* Calculate angle, assuming shore parallel contours and no conv/div of
		 * rays    */
		 /* from Komar 5.47 */

		Angle = asin(C / CDeep * sin(AngleDeep));

		/* Determine Wave height from refract calcs - Komar 5.49 */

		WvHeight = OffShoreWvHt *
			Raise(CDeep * cos(AngleDeep) / (C * 2.0 * n * cos(Angle)), .5);

		if (WvHeight > Depth * KBreak || Depth <= RefractStep)
			break;
		else
			Depth -= RefractStep;
	}

	/* Now Determine Transport */
	/* eq. 9.6b (10.8) Komar, including assumption of sed density = 2650 kg/m3 */
	/* additional accuracy here will not improve an already suspect eqn for sed
	 * transport   */
	 /* (especially with poorly constrained coefficients), */
	 /* so no attempt made to make this a more perfect imperfection */

	VolumeAcrossBorder[i] =
		fabs(0.67 * rho * Raise(GRAVITY, 3.0 / 2.0) * Raise(WvHeight, 2.5) *
			cos(Angle) * sin(Angle) * myConfig.lengthTimestep); /*LMV - now global array*/

/*LMV VolumeIn/Out is now calculated below in AdjustShore */
}

void FlowInCell(void)
/* LMV */
/* This function will determine if sediment transport into a cell is from right,
	 from left,     */
	 /*                                              converging into the cell, or
			diverging out      */
			/* Purpose is to address problems with insufficent sand, fix losing sand problem
				 */
				 /* Moves from left to right direction */
				 /* This function will affect and determine the global arrays:  FlowThroughCell[]
						*/
						/* This function uses TotalBeachCells */
						/* Uses but does not affect the global arrays:  DirectionAcrossBorder[] */
{
	int i;

	/* float AverageFull; */
	double TotalPercentInBeaches = 0.;

	for (i = 1; i <= TotalBeachCells; i++) {
		if ((DirectionAcrossBorder[i - 1] == 'r') &&
			(DirectionAcrossBorder[i] == 'r'))
			FlowThroughCell[i] = 'R'; /*Right */
		if ((DirectionAcrossBorder[i - 1] == 'r') &&
			(DirectionAcrossBorder[i] == 'l'))
			FlowThroughCell[i] = 'C'; /*Convergent */
		if ((DirectionAcrossBorder[i - 1] == 'l') &&
			(DirectionAcrossBorder[i] == 'r'))
			FlowThroughCell[i] = 'D'; /*Divergent */
		if ((DirectionAcrossBorder[i - 1] == 'l') &&
			(DirectionAcrossBorder[i] == 'l'))
			FlowThroughCell[i] = 'L'; /*Left */

		TotalPercentInBeaches +=
			(PercentFullSand[X[i]][Y[i]]);
	}
}

void FixFlow(void)
/* LMV */
/* Checks and distributes sediment in adjacent cells based on FlowThroughCell[]
	 */
	 /* Sweeps left to right to check cells where DirectionAcrossBorder='r' (D to R,
			D to C, R to R, R to C) */
			/* Sweeps right to left to check cells where DirectionAcrossBorder='l' (D to L,
				 D to C, L to L, L to C) */
				 /* This function uses TotalBeachCells */
				 /* Uses but does not affect the global arrays: FlowThroughCell[],
						VolumeAcrossBorder[], PFS[][]         */
						/* reminder--VolumeAcrossBorder[] is always on right side of cell */
						/* revised 5/04 LMV */
{
	int i;
	double AmountSand; /* amount of sand availible for transport (includes amount
											 in cell behind) LMV */

	double Depth; /* Depth of current cell */
	/* float        DeltaArea;       Holds change in area for cell (m^2) */
	double Distance;   /* distance from shore to intercept of equilib. profile and
											 overall slope (m) */
	double Xintercept; /* X position of intercept of equilib. profile and overall
											 slope */
	double DepthEffective; /* depth at x intercept, Brad: where slope becomes <
													 equilib slope, */
													 /*                             so sand stays piled against shoreface */

	for (i = 1; i <= TotalBeachCells - 1; i++)
		ActualVolumeAcross[i] = VolumeAcrossBorder[i];

	for (i = 1; i <= TotalBeachCells - 1;	i++) { /*get the actual volumes across for left and right borders of D
							cells */

							/* calculate effective depth */
		Depth = INITIAL_DEPTH + ((X[i] - INIT_BEACH) * cell_length * SHELF_SLOPE);
		Distance = Depth / (SHOREFACE_SLOPE - SHELF_SLOPE * cos(ShorelineAngle[i]));
		Xintercept = X[i] - Distance * cos(ShorelineAngle[i]) / cell_length;
		DepthEffective =
			INITIAL_DEPTH - ((Xintercept - INIT_BEACH) * cell_length * SHELF_SLOPE);

		if (DepthEffective < DEPTH_SHOREFACE) DepthEffective = DEPTH_SHOREFACE;

		/*set the D's first - left and right border set */

		if (FlowThroughCell[i] == 'D') { /*Flow from a Divergent cell (to a
																				Convergent cell or a Right cell) */

			if (PercentFullSand[XBehind[i]][YBehind[i]] >= 1.0) {
				AmountSand = (PercentFullSand[X[i]][Y[i]] +
					PercentFullSand[XBehind[i]][YBehind[i]]) *
					cell_width * cell_length * DepthEffective;
			}

			else { /*I don't know how much I have, just figure it out and give me some
								sand */

								/* printf("Check Amount sand X: %d, Y: %d \n", X[i], Y[i]); */
				AmountSand = (PercentFullSand[X[i]][Y[i]]) * cell_width * cell_length * DepthEffective;
			}

			if ((VolumeAcrossBorder[i - 1] + VolumeAcrossBorder[i]) <= AmountSand) {
				ActualVolumeAcross[i] = VolumeAcrossBorder[i];
				ActualVolumeAcross[i - 1] = VolumeAcrossBorder[i - 1];
			}
			if ((VolumeAcrossBorder[i - 1] + VolumeAcrossBorder[i]) > AmountSand)
				/*divide up proportionally */
			{
				ActualVolumeAcross[i] =
					((VolumeAcrossBorder[i] /
					(VolumeAcrossBorder[i - 1] + VolumeAcrossBorder[i])) *
						AmountSand);
				ActualVolumeAcross[i - 1] =
					((VolumeAcrossBorder[i - 1] /
					(VolumeAcrossBorder[i - 1] + VolumeAcrossBorder[i])) * AmountSand);
			}
		}
	}

	for (i = 1; i <= TotalBeachCells - 1; i++) { /*get the actual volumes across right borders for R cells */
 /* calculate effective depth */
		Depth = INITIAL_DEPTH + ((X[i] - INIT_BEACH) * cell_width * SHELF_SLOPE);
		Distance = Depth / (SHOREFACE_SLOPE - SHELF_SLOPE * cos(ShorelineAngle[i]));
		Xintercept = X[i] - Distance * cos(ShorelineAngle[i]) / cell_width;
		DepthEffective = INITIAL_DEPTH - ((Xintercept - INIT_BEACH) * cell_width * SHELF_SLOPE);

		if (DepthEffective < DEPTH_SHOREFACE) DepthEffective = DEPTH_SHOREFACE;

		if (FlowThroughCell[i] == 'R') { /*Flow from a Right cell (to a Right cell
																				or a Convergent cell) */

			if (PercentFullSand[XBehind[i]][YBehind[i]] >= 1.0) {
				AmountSand = (PercentFullSand[X[i]][Y[i]] +
					PercentFullSand[XBehind[i]][YBehind[i]]) *
					cell_width * cell_length * DepthEffective;
			}

			else { /*I don't know how much I have, just figure it out and give me some
								sand */

								/* printf("Check 2 Amount sand X: %d, Y: %d \n", X[i], Y[i]); */
				AmountSand = (PercentFullSand[X[i]][Y[i]]) * cell_width * cell_length * DepthEffective;
			}

			if ((ActualVolumeAcross[i - 1] + AmountSand) >= VolumeAcrossBorder[i]) {
				ActualVolumeAcross[i] = VolumeAcrossBorder[i];
			}
			if ((ActualVolumeAcross[i - 1] + AmountSand) < VolumeAcrossBorder[i]) {
				ActualVolumeAcross[i] = ActualVolumeAcross[i - 1] + AmountSand;
			}
		}
	}

	for (i = TotalBeachCells - 1; i >= 1; i--) { /*get the volumes across left side of cell for L cells */

		Depth = INITIAL_DEPTH + ((X[i] - INIT_BEACH) * cell_width * SHELF_SLOPE);
		Distance = Depth / (SHOREFACE_SLOPE - SHELF_SLOPE * cos(ShorelineAngle[i]));
		Xintercept = X[i] - Distance * cos(ShorelineAngle[i]) / cell_width;
		DepthEffective =
			INITIAL_DEPTH - ((Xintercept - INIT_BEACH) * cell_width * SHELF_SLOPE);

		if (DepthEffective < DEPTH_SHOREFACE) DepthEffective = DEPTH_SHOREFACE;

		if (FlowThroughCell[i] =='L') { /*Flow from a Left cell (to a Convergent cell or a Left cell) */

			if (PercentFullSand[XBehind[i]][YBehind[i]] >= 1.0) {
				AmountSand = (PercentFullSand[X[i]][Y[i]] +
					PercentFullSand[XBehind[i]][YBehind[i]]) *
					cell_width * cell_length * DepthEffective;
			}

			else { /*I don't know how much I have, just figure it out and give me some
								sand */

								/* printf("Check 3 Amount sand X: %d, Y: %d \n", X[i], Y[i]); */
				AmountSand = (PercentFullSand[X[i]][Y[i]]) * cell_width * cell_length *
					DepthEffective;
			}

			if ((ActualVolumeAcross[i] + AmountSand) >= VolumeAcrossBorder[i - 1]) {
				ActualVolumeAcross[i - 1] = VolumeAcrossBorder[i - 1];
			}
			if ((ActualVolumeAcross[i] + AmountSand) < VolumeAcrossBorder[i - 1]) {
				ActualVolumeAcross[i - 1] = ActualVolumeAcross[i] + AmountSand;
			}
		}
	}
}

void TransportSedimentSweep(void)
/*  Sweep through cells to place transported sediment */
/*  Call function AdjustShore() to move sediment. */
/*  If cell full or overempty, call OopsImFull or OopsImEmpty() */
/*  This function doesn't change any values, but the functions it calls do */
/*  Uses but doesn't change:  X[], Y[], PercentFullSand[] */
/*  sweepsign added to ensure that direction of actuating changes does not */
/*      produce unwanted artifacts (e.g. make sure symmetrical */
{
	int i, ii, sweepsign;

	if (RandZeroToOne() * 2 > 1)
		sweepsign = 1;
	else
		sweepsign = 0;

	/*if (debug7) printf("\n\n TransSedSweep  Ang %f  %d\n", WaveAngle * RAD_TO_DEG,
	 * current_time_step); */

	for (i = 0; i <= TotalBeachCells - 1; i++) {
		if (sweepsign == 1)
			ii = i;
		else
			ii = TotalBeachCells - 1 - i;
		AdjustShore(ii);
		if ((PercentFullSand[X[ii]][Y[ii]]) < -0.000001) { /* RCL changes 0.0 to 1E-6
																 whenever testing for
																 oopsimempty */

			OopsImEmpty(X[ii], Y[ii]);
			/*printf("Empty called in TSS\n"); */
		}

		else if ((PercentFullSand[X[ii]][Y[ii]]) >
			1.0) {
			OopsImFull(X[ii], Y[ii]);
			/*printf("Full called in TSS\n"); */
		}
		if (PercentFullSand[X[ii]][Y[ii]] < -0.000001) { /* RCL */
			OopsImEmpty(X[ii], Y[ii]);
		}
	}
}

void AdjustShore(int i)

/*  Complete mass balance for incoming and ougoing sediment */
/*  This function will change the global data array PercentFullSand[][] */
/*  Uses but does not adjust arrays: */
/*              X[], Y[], ShorelineAngle[], ActualVolumeAcross  LMV */
/*  Uses global variables: SHELF_SLOPE, CELL_WIDTH, SHOREFACE_SLOPE, INITIAL_DEPTH
	 */
{
	double Depth;      /* Depth of current cell */
	double DeltaArea;  /* Holds change in area for cell (m^2) */
	double Distance;   /* distance from shore to intercept of equilib. profile and
											 overall slope (m) */
	double Xintercept; /* X position of intercept of equilib. profile and overall
											 slope */
	double DepthEffective; /* depth at x intercept, Brad: where slope becomes <
													 equilib slope, */
													 /*                             so sand stays piled against shoreface */

													 /*  FIND EFFECTIVE DEPTH, Deff, HERE */
													 /*  Deff = FROM SOLUTION TO: */
													 /*      A*Distance^n = D(X) - OverallSlope*cos(ShorelineAngle[X][Y])*Distance
														*/
														/*  Where Distance is from shore, perpendicular to ShorelineAngle; */
														/* Brad's stuff not being used right now (linear slope for now) : */
														/*      Xintercept = X + Distance*cos(ShorelineAngle), and n = 2/3. */
														/*      THEN Deff = D(Xintercept); */
														/*      FIND "A" FROM DEPTH OF 10METERS AT ABOUT 1000METERS. */
														/*      FOR NOW, USE n = 1: */

	Depth = INITIAL_DEPTH + ((X[i] - INIT_BEACH) * cell_width * SHELF_SLOPE);

	Distance = Depth / (SHOREFACE_SLOPE - SHELF_SLOPE * cos(ShorelineAngle[i]));

	Xintercept = X[i] - Distance * cos(ShorelineAngle[i]) / cell_width;

	DepthEffective =
		INITIAL_DEPTH - ((Xintercept - INIT_BEACH) * cell_width * SHELF_SLOPE);

	if (DepthEffective < DEPTH_SHOREFACE) DepthEffective = DEPTH_SHOREFACE;
	/*LMV*/ if (FlowThroughCell[i] == 'L') {
		VolumeIn[i] = ActualVolumeAcross[i];
		VolumeOut[i] = ActualVolumeAcross[i - 1];
	}
	if (FlowThroughCell[i] == 'C') {
		VolumeIn[i] = ActualVolumeAcross[i];
		VolumeOut[i] = -ActualVolumeAcross[i - 1];
	}
	if (FlowThroughCell[i] == 'D') {
		VolumeIn[i] = -ActualVolumeAcross[i - 1];
		VolumeOut[i] = ActualVolumeAcross[i];
	}
	if (FlowThroughCell[i] == 'R') {
		VolumeIn[i] = ActualVolumeAcross[i - 1];
		VolumeOut[i] = ActualVolumeAcross[i];
	}

	DeltaArea = (VolumeIn[i] - VolumeOut[i]) / DepthEffective;
	/* if(you want to do sinks this way && this cell is sink) DeltaArea = 0;
		 RCL addded sinks but won't do them like this probably */
	PercentFullSand[X[i]][Y[i]] += DeltaArea / (cell_width * cell_length);
}

void OopsImEmpty(int x, int y)

/*  If a cell is under-full, this will find source for desparity and move brach
	 in      */
	 /*  Function completly changed 5/21/02 sandrevt.c */
	 /*              New Approach - steal from all neighboring AllBeach cells */
	 /*              Backup plan - steal from all neighboring percent full > 0 */
	 /*  Function adjusts primary data arrays: */
	 /*              AllBeach[][] and PercentFullSand[][] */
	 /* LMV Needs to be adjusted -- can't take sand from rock cells, uses AllRock[][]
			*/
{
	int emptycells = 0;
	int emptycells2 = 0;

	/* find out how many AllBeaches to take from */

	if ((PercentFullSand[x - 1][y] >= 1.0))
		/*LMV*/ emptycells += 1;
	if ((PercentFullSand[x + 1][y] >= 1.0))
		emptycells += 1;
	if ((PercentFullSand[x][y - 1] >= 1.0))
		emptycells += 1;
	if ((PercentFullSand[x][y + 1] >= 1.0))
		emptycells += 1;

	if (emptycells > 0) {
		/* Now Move Sediment */

		if ((PercentFullSand[x - 1][y] >= 1.0))
			/*LMV*/ {
			PercentFullSand[x - 1][y] += PercentFullSand[x][y] / emptycells;
		}

		if ((PercentFullSand[x + 1][y] >= 1.0)) {
			PercentFullSand[x + 1][y] += PercentFullSand[x][y] / emptycells;
		}

		if ((PercentFullSand[x][y - 1] >= 1.0)) {
			PercentFullSand[x][y - 1] += PercentFullSand[x][y] / emptycells;
		}

		if ((PercentFullSand[x][y + 1] >= 1.0)) {
			PercentFullSand[x][y + 1] += PercentFullSand[x][y] / emptycells;
		}
	}

	else {
		/* No full neighbors, so take away from partially full neighbors */

		if (PercentFullSand[x - 1][y] > 0.0) emptycells2 += 1;
		if (PercentFullSand[x + 1][y] > 0.0) emptycells2 += 1;
		if (PercentFullSand[x][y - 1] > 0.0) emptycells2 += 1;
		if (PercentFullSand[x][y + 1] > 0.0) emptycells2 += 1;

		if (emptycells2 > 0) {
			if (PercentFullSand[x - 1][y] > 0.0) {
				PercentFullSand[x - 1][y] += PercentFullSand[x][y] / emptycells2;
			}

			if (PercentFullSand[x + 1][y] > 0.0) {
				PercentFullSand[x + 1][y] += PercentFullSand[x][y] / emptycells2;
			}

			if (PercentFullSand[x][y - 1] > 0.0) {
				PercentFullSand[x][y - 1] += PercentFullSand[x][y] / emptycells2;
			}

			if (PercentFullSand[x][y + 1] > 0.0) {
				PercentFullSand[x][y + 1] += PercentFullSand[x][y] / emptycells2;
			}
		}
		else {
			printf("@@@ Didn't find anywhere to steal sand from!! x: %d  y: %d\n", x, y);
		}
	}

	/*LMV*/ PercentFullSand[x][y] = 0.0;
}

void OopsImFull(int x, int y)

/*  If a cell is overfull, push beach out in new direction */
/*  Completely revised 5/20/02 sandrevt.c to resolve 0% full problems, etc. */
/*  New approach:       put sand wherever 0% full in adjacent cells */
/*                      if not 0% full, then fill all non-allbeach */
/*  Function adjusts primary data arrays: */
/*              AllBeach[][] and PercentFullSand[][] */
/*  LMV add rock adjustments */
{
	int fillcells = 0;
	int fillcells2 = 0;

	/* find out how many cells will be filled up    */

	if ((PercentFullSand[x - 1][y]) <= 0.000001)
		fillcells += 1;
	if ((PercentFullSand[x + 1][y]) <= 0.000001)
		fillcells += 1;
	if ((PercentFullSand[x][y - 1]) <= 0.000001)
		fillcells += 1;
	if ((PercentFullSand[x][y + 1]) <= 0.000001)
		fillcells += 1;

	if (fillcells != 0) {
		/* Now Move Sediment */

		if ((PercentFullSand[x - 1][y]) <= 0.000001)
			/*LMV*/ {
			PercentFullSand[x - 1][y] +=
				((PercentFullSand[x][y]) - 1.0) / fillcells;
		}

		if ((PercentFullSand[x + 1][y]) <= 0.000001) {
			PercentFullSand[x + 1][y] +=
				((PercentFullSand[x][y]) - 1.0) / fillcells;
		}

		if ((PercentFullSand[x][y - 1]) <= 0.000001) {
			PercentFullSand[x][y - 1] +=
				((PercentFullSand[x][y]) - 1.0) / fillcells;
		}

		if ((PercentFullSand[x][y + 1]) <= 0.000001) {
			PercentFullSand[x][y + 1] +=
				((PercentFullSand[x][y]) - 1.0) / fillcells;
		}
	}
	else {
		/* No fully empty neighbors, so distribute to partially full neighbors */

		if ((PercentFullSand[x - 1][y]) < 1.0)
			fillcells2 += 1;
		if ((PercentFullSand[x + 1][y]) < 1.0)
			fillcells2 += 1;
		if ((PercentFullSand[x][y - 1]) < 1.0)
			fillcells2 += 1;
		if ((PercentFullSand[x][y + 1]) < 1.0)
			fillcells2 += 1;

		if (fillcells2 > 0) {
			if ((PercentFullSand[x - 1][y]) < 1.0) {
				PercentFullSand[x - 1][y] +=
					((PercentFullSand[x][y]) - 1.0) /
					fillcells2;
			}

			if ((PercentFullSand[x + 1][y]) < 1.0) {
				PercentFullSand[x + 1][y] +=
					((PercentFullSand[x][y]) - 1.0) /
					fillcells2;
			}

			if ((PercentFullSand[x][y - 1]) < 1.0) {
				PercentFullSand[x][y - 1] +=
					((PercentFullSand[x][y]) - 1.0) /
					fillcells2;
			}
		}
	}

	if (PercentFullSand[x][y] >= 1.0)
	/*LMV*/ PercentFullSand[x][y] = 1.0;
}

void FixBeach(void)

/* Hopefully addresses strange problems caused by filling/emptying of cells */
/* Looks at entire data set */
/* Find unattached pieces of sand and moves them back to the shore */
/* Takes care of 'floating bits' of sand */
/* Also takes care of over/under filled beach pieces */
/* Revised 5/21/02 to move sand to all adjacent neighbors sandrevt.c */
/* Changes global variable PercentFullSand[][] */
/* Uses and can change AllBeach[][] */
/* sandrevx.c - added sweepsign to reduce chances of asymmetrical artifacts */
/* LMV rock fix - do not fix bare bedrock cells */
{
	int i, x, y, sweepsign, done, counter;
	int fillcells3 = 0;
	int Corner = 0;
	/*LMV*/ int xstart;

	if (RandZeroToOne() * 2 > 1)
		sweepsign = 1;
	else
		sweepsign = 0;

	for (x = 1; x < ShadowXMax - 1; x++) {
		for (i = 1; i < 2 * Y_MAX - 1; i++) { /* RCL: changing loops to ignore border */
			if (sweepsign == 1)
				y = i;
			else
				y = 2 * Y_MAX - i;

			/*Take care of corner problem?
				 if  (((AllBeach[x][y] == 'n') && (PercentFullRock[x][y] > 0.0)) &&
				 (((AllBeach[x][y-1] == 'n') && (AllBeach[x-1][y] == 'n') &&
				 (AllRock[x-1][y-1] == 'y'))
				 ||((AllBeach[x-1][y] == 'n') && (AllBeach[x][y+1] == 'n') &&
				 (AllRock[x-1][y+1] == 'y'))))

				 Corner = 1;
				 else */
			Corner = 0;

			/* Take care of situations that shouldn't exist */

			if ((PercentFullSand[x][y]) < -0.000001) {
				/* RCL changed  0.0 to 10E-6--floating-pt equality is messy */
				OopsImEmpty(x, y);
			}

			if ((PercentFullSand[x][y]) > 1.0) {
				OopsImFull(x, y);
			}
			
			/* Take care of 'loose' bits of sand */

			if (Corner == 0) {
				fillcells3 = 0;

				if (((PercentFullSand[x][y]) != 0.0) &&
					((PercentFullSand[x - 1][y]) < 1.0) &&
					((PercentFullSand[x + 1][y]) < 1.0) &&
					((PercentFullSand[x][y + 1]) < 1.0) &&
					((PercentFullSand[x][y - 1]) < 1.0) &&
					(PercentFullSand[x][y] < 1.0))

					/* Beach in cell, but bottom, top, right, and left neighbors not all
						 full */
				{
					/* distribute to partially full neighbors */

					if (((PercentFullSand[x - 1][y]) < 1.0) &&
						((PercentFullSand[x - 1][y]) > 0.0))
						fillcells3 += 1;
					if (((PercentFullSand[x + 1][y]) < 1.0) &&
						((PercentFullSand[x + 1][y]) > 0.0))
						fillcells3 += 1;
					if (((PercentFullSand[x][y + 1]) < 1.0) &&
						((PercentFullSand[x][y + 1]) > 0.0))
						fillcells3 += 1;
					if (((PercentFullSand[x][y - 1]) < 1.0) &&
						((PercentFullSand[x][y - 1]) > 0.0))
						fillcells3 += 1;

					if ((fillcells3 > 0) && (fillcells3 <= 4)) {
						if (((PercentFullSand[x - 1][y]) < 1.0) && ((PercentFullSand[x - 1][y]) > 0.0)) {
							PercentFullSand[x - 1][y] += PercentFullSand[x][y] / fillcells3; /* RCL */
						}

						if (((PercentFullSand[x + 1][y]) < 1.0) && ((PercentFullSand[x + 1][y]) > 0.0)) {
							PercentFullSand[x + 1][y] += PercentFullSand[x][y] / fillcells3; /* RCL */
						}

						if (((PercentFullSand[x][y - 1]) < 1.0) && ((PercentFullSand[x][y - 1]) > 0.0)) {
							PercentFullSand[x][y - 1] += PercentFullSand[x][y] / fillcells3; /* RCL */
						}

						if (((PercentFullSand[x][y + 1]) < 1.0) && ((PercentFullSand[x][y + 1]) > 0.0)) {
							PercentFullSand[x][y + 1] += PercentFullSand[x][y] / fillcells3; /* RCL */
						}

					}
					else {

						xstart = X_MAX - 1;
						while (!(PercentFullSand[xstart][y] > 0.0)) {
							xstart -= 1;
						}
						//xstart += 1;

						PercentFullSand[xstart][y] += PercentFullSand[x][y];
					}

					PercentFullSand[x][y] = 0.0;

					/* If we have overfilled any of the cells in this loop, need to
					 * OopsImFull() */

					if ((PercentFullSand[x - 1][y]) > 1.0) {
						OopsImFull(x - 1, y);
					}
					if ((PercentFullSand[x][y - 1]) > 1.0) {
						OopsImFull(x, y - 1);
					}
					if ((PercentFullSand[x][y + 1]) > 1.0) {
						OopsImFull(x, y + 1);
					}
					if ((PercentFullSand[x + 1][y]) > 1.0) {
						OopsImFull(x + 1, y);
					}
				}
			}
		}
	}
	/* now make sure there aren't any overfull or underfull cells */
	done = FALSE;
	for (counter = 0; counter < 10 && !done; counter++) {
		done = TRUE;
		for (x = 1; x < X_MAX - 1; x++) {
			for (y = 1; y < 2 * Y_MAX - 1; y++) {
				if ((PercentFullSand[x][y] > 1.0) || PercentFullSand[x][y] < -0.000001)
					done = FALSE;
				if (PercentFullSand[x][y] > 1.0) {
					OopsImFull(x, y);
				}
				if (PercentFullSand[x][y] < -0.000001) {
					OopsImEmpty(x, y);
				}
			}
		}
	}
}

double Raise(double b, double e)
/**
 * function calulates b to the e power. pow has problems if b <= 0
 * 	note you can't use this at all when b is negative--it'll be
 * 	wrong in cases when the answer returned should have been
 * 	positive, as x^2 when x < 0.
 */
{
	if (b > 0)
		return powf(b, e);
	else {
		printf("Raise: can't handle negative base \n");
		return 0;
	}
}

// Apply periodic boundary conditions to CEM arrays.
void periodic_boundary_copy(void)
{
	const int buff_len = 2 * Y_MAX;
	int row;

	for (row = 0; row < X_MAX; row++) {
		apply_periodic_boundary(PercentFullSand[row], sizeof(double), buff_len);
	}
}


void ZeroVars(void)
/* Resets all arrays recalculated at each time step to 'zero' conditions */
{
	int z;

	for (z = 0; z < MaxBeachLength; z++) {
		X[z] = -1;
		Y[z] = -1;
		/*LMV*/ XBehind[z] = -1;
		YBehind[z] = -1;
		InShadow[z] = '?';
		ShorelineAngle[z] = -999;
		UpWind[z] = '?';

		VolumeAcrossBorder[z] = 0.0;
		/*LMV*/ ActualVolumeAcross[z] = 0.0;
		/*LMV*/
	}
}

void ReadSandFromFile(void)
/*  Reads saved output file, AllBeach[][] & PercentFullSand[][]	 */
/*  LMV - Also reads saved output file, AllRock[][], PercentFullRock[][],
	 type_of_rock[][]  */
{
	int x, y;
	int offset = Y_MAX / 2;

	for (x = (X_MAX - 1); x >= 0; x--) {
		for (y = offset; y < 3 * offset; y++) {
			PercentFullSand[x][y] = myConfig.grid[x][y - offset];
		}
	}
}

void SaveSandToFile(void)
/*  Saves current AllBeach[][] and PercentFullSand[][] data arrays to file
	 */
	 /*  Save file name will add extension '.' and the current_time_step
			*/
			/*  LMV - Save AllRock[][], PercentFullRock[][], type_of_rock[][]
				 */
{
	int x, y;
	char savefile_name[40];
	sprintf(savefile_name, "test/output/orig/CEM_%06d.out", current_time_step-1);

	SaveSandFile = fopen(savefile_name, "w");

	if (!SaveSandFile) {
		printf("problem opening output file\n");
		exit(1);
	}


	for (x = 0; x < X_MAX; x++) {
		for (y = Y_MAX / 2; y < 3 * Y_MAX / 2; y++) {
			fprintf(SaveSandFile, " %lf", PercentFullSand[x][y]);
		}
		fprintf(SaveSandFile, "\n");
	}

	fclose(SaveSandFile);

	periodic_boundary_copy();
}

void SaveLineToFile(void)
/*  Saves data line of shoreline position rather than entire array */
/*  Main concern is to have only one data point at each alongshore location
	 */
	 /*  Save file name will add extension '.' and the current_time_step
			*/
{
	int is_beach[X_MAX][Y_MAX];

	int x, y;
	int offset = Y_MAX / 2;
	for (x = 0; x < myConfig.nRows; x++)
	{
		for (y = 0; y < myConfig.nCols; y++)
		{
			is_beach[x][y] = 0;
		}
	}

	char savefile_name[40] = "test/orig_shoreline.txt";
	FILE* savefile = fopen(savefile_name, "w");

	if (!savefile) {
		printf("problem opening output file\n");
		exit(1);
	}

	int i;
	for (i = 1; i < TotalBeachCells - 1; i++) {
		if (Y[i] >= offset && Y[i] < offset + Y_MAX)
		{
			is_beach[X[i]][Y[i] - offset] = 1;
		}
	}


	for (x = 0; x < myConfig.nRows; x++)
	{
		for (y = 0; y < myConfig.nCols; y++)
		{
			fprintf(savefile, " %d", is_beach[x][y]);
		}
		fprintf(savefile, "\n");
	}

	fclose(savefile);
}

void RealWaveIn(void)
/* Input Wave Distribution */
{
	WaveAngleIn = myConfig.waveAngles[current_time_step];
	WavePeriodIn = myConfig.wavePeriods[current_time_step];
	WaveHeightIn = myConfig.waveHeights[current_time_step];
}