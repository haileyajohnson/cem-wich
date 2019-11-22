
#include "BeachNode.h"
#include "BeachGrid.h"
#include "consts.h"
#include "utils.h"
#include <math.h>

static struct BeachGrid* SetCells(struct BeachGrid* this, struct BeachNode** cells)
{
	this->cells = cells;
	return this;
}

static struct BeachNode* SetShorelines(struct BeachGrid* this, struct BeachNode** shorelines, int num)
{
	this->shoreline = shorelines;
	this->num_shorelines = num;
	return shorelines;
}

static struct BeachNode* TryGetNode(struct BeachGrid* this, int row, int col)
{
	if (row < 0 || row >= this->rows)
	{
		return NULL;
	}

	if (col < 0 || col >= this->cols)
	{
		return NULL;
	}

	return &(this->cells[row][col]);
}

static int FindInShorelines(struct BeachGrid* this, struct BeachNode* node) {
	int i = 0;
	while (i < this->num_shorelines)
	{
		if (this->shoreline[i] == node)
		{
			return i;
		}
		i++;
	}
	return -1;
}

static double GetPrevAngle(struct BeachGrid* this, struct BeachNode* node)
{
	if (BeachNode.isEmpty(node->prev)) { return EMPTY_DOUBLE; }
	return BeachNode.GetAngle(node->prev, node);
}

static double GetNextAngle(struct BeachGrid* this, struct BeachNode* node)
{
	if (BeachNode.isEmpty(node->next)) { return EMPTY_DOUBLE; }
	return BeachNode.GetAngle(node, node->next);
}

static double GetSurroundingAngle(struct BeachGrid* this, struct BeachNode* node)
{
	if (BeachNode.isEmpty(node->prev) || BeachNode.isEmpty(node->next)) { return EMPTY_DOUBLE; }
	return ((*this).GetPrevAngle(this, node) + (*this).GetNextAngle(this, node)) / 2;
}

static double GetAngleByDifferencingScheme(struct BeachGrid* this, struct BeachNode* node, double wave_angle)
{
	double alpha = wave_angle - (*this).GetSurroundingAngle(this, node);

	if (alpha == 0)
	{
		return 0;
	}

	struct BeachNode* downwind_node;
	double upwind_angle, downwind_angle;
	FLOW_DIR dir;
	if (alpha > 0)
	{
		downwind_node = node->prev;
		upwind_angle = (*this).GetPrevAngle(this, node);
		downwind_angle = (*this).GetNextAngle(this, node);
		dir = RIGHT;
	}
	else
	{
		downwind_node = node->next;
		upwind_angle = (*this).GetNextAngle(this, node);
		downwind_angle = (*this).GetPrevAngle(this, node);
		dir = LEFT;
	}

	double instability_threshold = 42 * DEG_TO_RAD;
	int U = fabs(alpha) >= instability_threshold;
	
	int U_downwind = !downwind_node->is_boundary ? (fabs(wave_angle - (*this).GetSurroundingAngle(this, downwind_node)) >= instability_threshold
		&& !(*this).CheckIfInShadow(this, downwind_node, wave_angle)) : U;

	if ((U && !U_downwind) || (!U && U_downwind))
	{
		return EMPTY_DOUBLE;
	}
	else if (U)
	{
		return upwind_angle;
	}
	return downwind_angle;
}

static struct BeachNode** Get4Neighbors(struct BeachGrid* this, struct BeachNode* node)
{
	struct BeachNode** neighbors = malloc(4 * sizeof(struct BeachNode*));
	int myCol = node->GetCol(node);
	int myRow = node->GetRow(node);
	int cols[4] = { myCol - 1, myCol, myCol + 1, myCol };
	int rows[4] = { myRow, myRow + 1, myRow, myRow - 1 };

	int i;
	for (i = 0; i < 4; i++)
	{
		neighbors[i] = TryGetNode(this, rows[i], cols[i]);
	}

	return neighbors;
}

static int CheckIfInShadow(struct BeachGrid* this, struct BeachNode* node, double wave_angle)
{
	int row = node->GetRow(node);
	int i = 1;
	// TODO revisit
	int max_i = (ShadowMax * 1000) / (ShadowStepDistance * this->cell_width);

	while (row < this->rows && i < max_i)
	{
		row = node->GetRow(node) + trunc(i * ShadowStepDistance * cos(wave_angle));
		int col = node->GetCol(node) + trunc(-i * ShadowStepDistance * sin(wave_angle));

		struct BeachNode* temp = TryGetNode(this, row, col);
		if (BeachNode.isEmpty(temp))
		{
			return FALSE;
		}

		if (temp->frac_full == 1 && (temp->GetRow(temp) + 1) > node->GetRow(node) + node->frac_full + fabs((node->GetCol(node) - temp->GetCol(temp)) / tan(wave_angle)))
		{
			return TRUE;
		}

		i++;
	}
	return FALSE;
}

static struct BeachGrid new(int rows, int cols, double cell_width, double cell_length){
		return (struct BeachGrid) {
				.cell_width = cell_width,
				.cell_length = cell_length,
				.rows = rows,
				.cols = cols,
				.cells = NULL,
				.shoreline = NULL,
				.num_shorelines = 0,
				.SetCells = &SetCells,
				.SetShorelines = &SetShorelines,
				.TryGetNode = &TryGetNode,
				.FindInShorelines = &FindInShorelines,
				.GetPrevAngle = &GetPrevAngle,
				.GetNextAngle = &GetNextAngle,
				.GetSurroundingAngle = &GetSurroundingAngle,
				.GetAngleByDifferencingScheme = &GetAngleByDifferencingScheme,
				.Get4Neighbors = &Get4Neighbors,
				.CheckIfInShadow = &CheckIfInShadow
				};
}

const struct BeachGridClass BeachGrid = { .new = &new };
