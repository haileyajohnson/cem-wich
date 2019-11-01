#ifndef CEM_CONFIG_INCLUDED
#define CEM_CONFIG_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

	typedef struct _Config {
		float** grid;
		int nRows;
		int nCols;
		float cellWidth;
		float cellLength;
		double asymmetry;
		double stability;
		double waveHeight;
		double wavePeriod;
		double shelfSlope;
		double shorefaceSlope;
		int numTimesteps;
		double lengthTimestep;
		int saveInterval;
	} Config;

#if defined(__cplusplus)
}
#endif

#endif
