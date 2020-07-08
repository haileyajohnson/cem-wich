#ifndef CEM_CONFIG_INCLUDED
#define CEM_CONFIG_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

	typedef struct _Config {
		double** grid;
		double* waveHeights;
		double* waveAngles;
		double* wavePeriods;
		double asymmetry;
		double stability;
		int numWaveInputs;
		int nRows;
		int nCols;
		double cellWidth;
		double cellLength;
		double shelfSlope;
		double shorefaceSlope;
		int crossShoreReferencePos;
		double shelfDepthAtReferencePos;
		double minimumShelfDepthAtClosure;
		double depthOfClosure;
		double sedMobility;
		double lengthTimestep;
		int numTimesteps;
		int saveInterval;
	} Config;

#if defined(__cplusplus)
}
#endif

#endif
