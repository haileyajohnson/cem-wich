
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

static double GetDistance(struct BeachGrid* this, struct BeachNode node1, struct BeachNode node2)
{
	return sqrt(raise((node2.GetRow(&node2) - node1.GetRow(&node1)), 2) + raise((node2.GetCol(&node2) - node1.GetCol(&node1)), 2));
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

static struct BeachNode* ReplaceNode(struct BeachGrid* this, struct BeachNode* node, int dir)
{
	struct BeachNode* curr = node->prev;
	int row = node->GetRow(node);
	int col = node->GetCol(node);

	int curr_row = curr->GetRow(curr);
	int curr_col = curr->GetCol(curr);

	struct BeachNode* stopNode = node->next;
	int stop_row = stopNode->GetRow(stopNode);
	int stop_col = stopNode->GetCol(stopNode);

	//while (curr_row != stop_row || curr_col != stop_col)
	while(TRUE)
	{
		double angle = atan2(curr_row - row, curr_col - col);
		angle += (dir * PI / 4);
		curr_row = row + round(sin(angle));
		curr_col = col + round(cos(angle));

		if (curr_row < 0 || curr_row >= this->rows || curr_col < 0 || curr_col >= this->cols) { continue; }

		struct BeachNode* temp = this->TryGetNode(this, curr_row, curr_col);
		// TODO check if member of another shoreline
		if (temp->is_beach) {
			if (temp->row == stop_row && temp->col == stop_col) {
				curr->next = temp;
				temp->prev = curr;
				curr = temp;
				break;
			}
			continue;
		}
		if (temp->frac_full > 0)
		{
			temp->is_beach = TRUE;
			curr->next = temp;
			temp->prev = curr;
			curr = temp;
		}
	}

	node->is_beach = FALSE;
	node->prev = NULL;
	node->next = NULL;
	// return end node of inserted segment
	return stopNode->prev;
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
				.GetDistance = &GetDistance,
				.GetPrevAngle = &GetPrevAngle,
				.GetNextAngle = &GetNextAngle,
				.GetSurroundingAngle = &GetSurroundingAngle,
				.GetAngleByDifferencingScheme = &GetAngleByDifferencingScheme,
				.ReplaceNode = &ReplaceNode,
				.Get4Neighbors = &Get4Neighbors,
				.CheckIfInShadow = &CheckIfInShadow
				};
}

const struct BeachGridClass BeachGrid = { .new = &new };
