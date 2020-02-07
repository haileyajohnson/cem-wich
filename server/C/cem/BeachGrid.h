#ifndef CEM_BEACHGRID_INCLUDED
#define CEM_BEACHGRID_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

#include "BeachNode.h"

double g_cell_length, g_cell_width;

struct BeachGrid {
    int rows, cols, current_time;
    struct BeachNode **cells;
    struct BeachNode *shoreline;
    struct BeachGrid* (*SetCells)(struct BeachGrid *this, struct BeachNode **cells);
    struct BeachNode* (*SetShoreline)(struct BeachGrid *this, struct BeachNode *shoreline);
		void (*FreeShoreline)(struct BeachGrid* this);
    struct BeachNode* (*TryGetNode)(struct BeachGrid *this, int row, int col);
    double (*GetPrevAngle)(struct BeachGrid *this, struct BeachNode *node);
    double (*GetNextAngle)(struct BeachGrid *this, struct BeachNode *node);
    double (*GetSurroundingAngle)(struct BeachGrid *this, struct BeachNode *node);
    double (*GetAngleByDifferencingScheme)(struct BeachGrid *this, struct BeachNode *node, double wave_angle);
		struct BeachNode* (*ReplaceNode)(struct BeachGrid *this, struct BeachNode* node);
    struct BeachNode** (*Get4Neighbors)(struct BeachGrid *this, struct BeachNode *node);
    int (*CheckIfInShadow)(struct BeachGrid *this, struct BeachNode *node, double wave_angle);
		int (*FindBeach)(struct BeachGrid* this);
		struct BeachNode* (*GetShoreline)(struct BeachGrid *this, struct BeachNode *startNode, struct BeachNode* stopNode, int dir_r, int dir_c);
		int (*IsLandCell) (struct BeachGrid* this, int row, int col);
};
extern const struct BeachGridClass {
    struct BeachGrid (*new)(int rows, int cols, double cell_width, double cell_length);
} BeachGrid;

#if defined(__cplusplus)
}
#endif

#endif
