#ifndef CEM_BEACHGRID_INCLUDED
#define CEM_BEACHGRID_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

#include "BeachNode.h"

struct BeachGrid {
    int rows, cols;
		float cell_width, cell_length;
    struct BeachNode **cells;
    struct BeachNode **shoreline;
		int num_shorelines;
    struct BeachGrid* (*SetCells)(struct BeachGrid *this, struct BeachNode **cells);
    struct BeachNode* (*SetShorelines)(struct BeachGrid *this, struct BeachNode **shorelines, int num);
    struct BeachNode* (*TryGetNode)(struct BeachGrid *this, int row, int col);
    float (*GetPrevAngle)(struct BeachGrid *this, struct BeachNode *node);
    float (*GetNextAngle)(struct BeachGrid *this, struct BeachNode *node);
    float (*GetSurroundingAngle)(struct BeachGrid *this, struct BeachNode *node);
    float (*GetAngleByDifferencingScheme)(struct BeachGrid *this, struct BeachNode *node, float wave_angle);
    struct BeachNode** (*Get4Neighbors)(struct BeachGrid *this, struct BeachNode *node);
    int (*CheckIfInShadow)(struct BeachGrid *this, struct BeachNode *node, float wave_angle);
};
extern const struct BeachGridClass {
    struct BeachGrid (*new)(int rows, int cols, float cell_width, float cell_length);
} BeachGrid;

#if defined(__cplusplus)
}
#endif

#endif
