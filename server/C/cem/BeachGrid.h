#ifndef CEM_BEACHGRID_INCLUDED
#define CEM_BEACHGRID_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

#include "BeachNode.h"

struct BeachGrid {
    int rows, cols;
		double shoreline_change, cell_width, cell_length;
    struct BeachNode **cells;
    struct BeachNode *shoreline;
		int num_shorelines;
    struct BeachGrid* (*SetCells)(struct BeachGrid *this, struct BeachNode **cells);
    struct BeachNode* (*SetShorelines)(struct BeachGrid *this, struct BeachNode **shorelines, int num);
    struct BeachNode* (*TryGetNode)(struct BeachGrid *this, int row, int cool);
    double (*GetDistance)(struct BeachGrid *this, struct BeachNode node1, struct BeachNode node2);
    int (*GetColumnwiseDistance)(struct BeachGrid *this, struct BeachNode node1, struct BeachNode node2);
    double (*GetLeftAngle)(struct BeachGrid *this, struct BeachNode *node);
    double (*GetRightAngle)(struct BeachGrid *this, struct BeachNode *node);
    double (*GetSurroundingAngle)(struct BeachGrid *this, struct BeachNode *node);
    double (*GetAngleByDifferencingScheme)(struct BeachGrid *this, struct BeachNode *node, double wave_angle);
    double (*GetAngle)(struct BeachGrid *this, struct BeachNode *node1, struct BeachNode *node2);
    struct BeachNode* (*ReplaceNode)(struct BeachGrid *this, struct BeachNode *node, int dir);
    struct BeachNode* (*RemoveNode)(struct BeachGrid *this, struct BeachNode *node);
    struct BeachNode** (*Get4Neighbors)(struct BeachGrid *this, struct BeachNode *node);
    struct BeachNode** (*Get8Neighbors)(struct BeachGrid *this, struct BeachNode *node, int dir);
    int (*CheckIfInShadow)(struct BeachGrid *this, struct BeachNode *node, double wave_angle);
};
extern const struct BeachGridClass {
    struct BeachGrid (*new)(int rows, int cols, double cell_width, double cell_length);
} BeachGrid;

#if defined(__cplusplus)
}
#endif

#endif
