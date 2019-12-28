#ifndef CEM_CONFIG_INCLUDED
#define CEM_CONFIG_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

	typedef struct _Config {
		float** grid;
		float* waveHeights;
		float* waveAngles;
		float* wavePeriods;
		float asymmetry;
		float stability;
		int nRows;
		int nCols;
		float cellWidth;
		float cellLength;
		float shelfSlope;
		float shorefaceSlope;
		int crossShoreReferencePos;
		float shelfDepthAtReferencePos;
		float minimumShelfDepthAtClosure;
		float lengthTimestep;
		int numTimesteps;
		int saveInterval;
	} Config;

#if defined(__cplusplus)
}
#endif

#endif
