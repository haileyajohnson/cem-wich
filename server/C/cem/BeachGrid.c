
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

	while (col >= this->cols)
	{
		col = col - this->cols;
	}
	while (col < 0)
	{
		col = col + this->cols;
	}

	return &(this->cells[row][col]);
}

static double GetDistance(struct BeachGrid* this, struct BeachNode node1, struct BeachNode node2)
{
	return sqrt(raise((node2.row - node2.row), 2) + raise(GetColumnwiseDistance(this, node1, node2), 2));
}

static int GetColumnwiseDistance(struct BeachGrid* this, struct BeachNode node1, struct BeachNode node2)
{
	int c1 = node1.col;
	int c2 = node2.col;
	if (abs(c2 - c1) >= this->cols / 2)
	{
		if (c2 > c1)
		{
			c1 += this->cols;
		}
		else
		{
			c2 += this->cols;
		}
	}
	return c2 - c1;
}

static double GetLeftAngle(struct BeachGrid* this, struct BeachNode* node)
{
	if (BeachNode.isEmpty(node->prev))
	{
		return EMPTY_DOUBLE;
	}
	return (*this).GetAngle(this, node->prev, node);
}

static double GetRightAngle(struct BeachGrid* this, struct BeachNode* node)
{
	if (BeachNode.isEmpty(node->next))
	{
		return EMPTY_DOUBLE;
	}
	return (*this).GetAngle(this, node, node->next);
}

static double GetSurroundingAngle(struct BeachGrid* this, struct BeachNode* node)
{
	if (BeachNode.isEmpty(node->prev) || BeachNode.isEmpty(node->next))
	{
		return EMPTY_DOUBLE;
	}
	return ((*this).GetLeftAngle(this, node) + (*this).GetRightAngle(this, node)) / 2;
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
		upwind_angle = (*this).GetLeftAngle(this, node);
		downwind_angle = (*this).GetRightAngle(this, node);
		dir = RIGHT;
	}
	else
	{
		downwind_node = node->next;
		upwind_angle = (*this).GetRightAngle(this, node);
		downwind_angle = (*this).GetLeftAngle(this, node);
		dir = LEFT;
	}

	double instability_threshold = 42 * DEG_TO_RAD;
	int U = fabs(alpha) >= instability_threshold;
	int U_downwind = fabs(wave_angle - (*this).GetSurroundingAngle(this, downwind_node)) >= instability_threshold
		&& !(*this).CheckIfInShadow(this, downwind_node, wave_angle);

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

static double GetAngle(struct BeachGrid* this, struct BeachNode* node1, struct BeachNode* node2)
{
	double angle = atan2((node2->row + node2->frac_full) - (node1->row + node1->frac_full), GetColumnwiseDistance(this, *node1, *node2));
	if (node1->col == node2->col)
	{
		angle = atan2(node2->row - node1->row, (1 - node2->frac_full) - (1 - node1->frac_full));
	}

	while (angle > PI)
	{
		angle -= 2 * PI;
	}
	while (angle < -PI)
	{
		angle += 2 * PI;
	}
	return angle;
}

static struct BeachNode* ReplaceNode(struct BeachGrid* this, struct BeachNode* node, int dir)
{
	struct BeachNode** neighbors = (*this).Get8Neighbors(this, node, dir);

	struct BeachNode* firstNode = node->prev;
	struct BeachNode* curr = firstNode;
	int i;
	for (i = 0; i < 8; i++)
	{
		struct BeachNode* temp = neighbors[i];
		if (temp == node->next)
		{
			break;
		}
		if (BeachNode.isEmpty(temp) || temp->frac_full == 1 || temp->frac_full == 0 || temp->is_beach)
		{
			continue;
		}

		temp->is_beach = TRUE;
		curr->next = temp;
		temp->prev = curr;
		curr = temp;
	}

	curr->next = node->next;
	node->next->prev = curr;
	struct BeachNode* lastNode = curr;

	// remove inset corner nodes
	curr = firstNode;
	struct BeachNode* stopNode = lastNode->next->next;
	while (curr != stopNode)
	{
		if (abs(curr->next->col - curr->prev->col) == 1 && abs(curr->next->row - curr->prev->row) == 1)
		{
			int r = curr->prev->row == curr->row ? curr->next->row : curr->prev->row;
			int c = curr->prev->col == curr->col ? curr->next->col : curr->prev->col;
			struct BeachNode* temp = TryGetNode(this, r, c);
			if (!BeachNode.isEmpty(temp) && temp->frac_full <= 0)
			{
				if (curr == lastNode) { lastNode = lastNode->prev; }
				struct BeachNode* next_node = (*this).RemoveNode(this, curr);
				if (curr == this->shoreline) { this->shoreline = next_node; }
				curr = next_node;
			}
		}
		curr = curr->next;
	}

	// remove node
	node->prev = NULL;
	node->next = NULL;
	node->is_beach = FALSE;
	if (node == this->shoreline) { this->shoreline = lastNode; }
	free(neighbors);

	// return end node of inserted segment
	return lastNode;
}

static struct BeachNode* RemoveNode(struct BeachGrid* this, struct BeachNode* node)
{
	struct BeachNode* next_node = node->next;
	node->prev->next = next_node;
	node->next->prev = node->prev;
	node->prev = NULL;
	node->next = NULL;
	node->is_beach = FALSE;
	return next_node;
}

static struct BeachNode** Get4Neighbors(struct BeachGrid* this, struct BeachNode* node)
{
	struct BeachNode** neighbors = malloc(4 * sizeof(struct BeachNode*));
	int cols[4] = { node->col - 1, node->col, node->col + 1, node->col };
	int rows[4] = { node->row, node->row + 1, node->row, node->row - 1 };

	int i;
	for (i = 0; i < 4; i++)
	{
		neighbors[i] = TryGetNode(this, rows[i], cols[i]);
	}

	return neighbors;
}

static struct BeachNode** Get8Neighbors(struct BeachGrid* this, struct BeachNode* node, int dir)
{
	struct BeachNode** neighbors = malloc(8 * sizeof(struct BeachNode*));
	int cols[8] = { node->col - 1, node->col - 1, node->col, node->col + 1, node->col + 1, node->col + 1, node->col, node->col - 1 };
	int rows[8] = { node->row, node->row + 1, node->row + 1, node->row + 1, node->row, node->row - 1, node->row - 1, node->row - 1 };

	// reorder
	int angle = round((*this).GetAngle(this, node->prev, node) / (PI / 4));
	int start_index = 0;
	switch (angle) {
	case (1):
		start_index = 7;
		break;
	case (2):
		start_index = 6;
		break;
	case (3):
		start_index = 5;
		break;
	case (4):
		start_index = 4;
		break;
	case (-4):
		start_index = 4;
		break;
	case (-1):
		start_index = 1;
		break;
	case (-2):
		start_index = 2;
		break;
	case (-3):
		start_index = 3;
		break;

	}

	int* new_cols = rotate(cols, start_index, 8);
	int* new_rows = rotate(rows, start_index, 8);

	// reverse order if shoreline is eroding rather than prograding
	if (dir < 0)
	{
		new_cols = reverse(cols, 8);
		new_rows = reverse(rows, 8);
	}

	// get neighbores
	int i;
	for (i = 0; i < 8; i++)
	{
		neighbors[i] = TryGetNode(this, new_rows[i], new_cols[i]);
	}

	return neighbors;
}

static int CheckIfInShadow(struct BeachGrid* this, struct BeachNode* node, double wave_angle)
{
	int row = node->row;
	int i = 1;
	// TODO revisit
	int max_i = (ShadowMax * 1000) / (ShadowStepDistance * this->cell_width);

	while (row < this->rows && i < max_i)
	{
		row = node->row + trunc(i * ShadowStepDistance * cos(wave_angle));
		int col = node->col + trunc(-i * ShadowStepDistance * sin(wave_angle));

		struct BeachNode* temp = TryGetNode(this, row, col);
		if (BeachNode.isEmpty(temp))
		{
			return FALSE;
		}

		if (temp->frac_full == 1 && (temp->row + 1) > node->row + node->frac_full + fabs(GetColumnwiseDistance(this, *node, *temp) / tan(wave_angle)))
		{
			return TRUE;
		}
		//if (temp->frac_full > 0 && temp->row + temp->frac_full > node->row + node->frac_full + fabs(GetColumnwiseDistance(this, *node, *temp) / tan(wave_angle)))
		//{
		//    return TRUE;
		//}
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
				.shoreline_change = 0,
				.shoreline = NULL,
				.num_shorelines = 0,
				.SetCells = &SetCells,
				.SetShorelines = &SetShorelines,
				.TryGetNode = &TryGetNode,
				.GetDistance = &GetDistance,
				.GetRightAngle = &GetRightAngle,
				.GetLeftAngle = &GetLeftAngle,
				.GetSurroundingAngle = &GetSurroundingAngle,
				.GetAngleByDifferencingScheme = &GetAngleByDifferencingScheme,
				.GetAngle = &GetAngle,
				.ReplaceNode = &ReplaceNode,
				.RemoveNode = &RemoveNode,
				.Get4Neighbors = &Get4Neighbors,
				.Get8Neighbors = &Get8Neighbors,
				.CheckIfInShadow = &CheckIfInShadow
				};
}

const struct BeachGridClass BeachGrid = { .new = &new };
