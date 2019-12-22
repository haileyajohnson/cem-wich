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
		int nRows;
		int nCols;
		double cellWidth;
		double cellLength;
		double shelfSlope;
		double shorefaceSlope;
		int crossShoreReferencePos;
		double shelfDepthAtReferencePos;
		double minimumShelfDepthAtClosure;
		double lengthTimestep;
		int saveInterval;
	} Config;

#if defined(__cplusplus)
}
#endif

#endif
