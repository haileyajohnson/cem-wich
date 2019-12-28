
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

static float GetPrevAngle(struct BeachGrid* this, struct BeachNode* node)
{
	// error: node is null
	if (BeachNode.isEmpty(node))
	{
		return EMPTY_float;
	}
	if (node->is_boundary)
	{
		// start boundary node: prev is same as next->next
		if (BeachNode.isEmpty(node->prev) && !BeachNode.isEmpty(node->next))
		{
			return (*this).GetNextAngle(this, node->next);
		}
		// end boundary node: prev is same as prev->prev
		if (BeachNode.isEmpty(node->next) && !BeachNode.isEmpty(node->prev))
		{
			return (*this).GetPrevAngle(this, node->prev);
		}
				// error: no valid neighbors
		return EMPTY_float;
	}
	if (node->prev->is_boundary)
	{
		// error: no valid neighbors
		if (BeachNode.isEmpty(node->next) || node->next->is_boundary)
		{
			return EMPTY_float;
		}
		// start node: prev is same as next
		return BeachNode.GetAngle(node, node->next);
	}
	// main case
	return BeachNode.GetAngle(node->prev, node);
}

static float GetNextAngle(struct BeachGrid* this, struct BeachNode* node)
{
	// error: node is null
	if (BeachNode.isEmpty(node))
	{
		return EMPTY_float;
	}
	if (node->is_boundary)
	{
		// start boundary node: next is same as next->next
		if (BeachNode.isEmpty(node->prev) && !BeachNode.isEmpty(node->next))
		{
			return (*this).GetNextAngle(this, node->next);
		}
		// end boundary node: next is same as prev->prev
		if (BeachNode.isEmpty(node->next) && !BeachNode.isEmpty(node->prev))
		{
			return (*this).GetPrevAngle(this, node->prev);
		}
		// error: no valid neighbors
		return EMPTY_float;
	}
	if (node->next->is_boundary)
	{
		// error: no valid neighbors
		if (BeachNode.isEmpty(node->prev) || node->prev->is_boundary)
		{
			return EMPTY_float;
		}
		// end node: next is same as prev
		return BeachNode.GetAngle(node->prev, node);
	}
	// main case
	return BeachNode.GetAngle(node, node->next);
}

static float GetSurroundingAngle(struct BeachGrid* this, struct BeachNode* node)
{
	// error: node is null
	if (BeachNode.isEmpty(node))
	{
		return EMPTY_float;
	}
	if (node->is_boundary)
	{
		if (BeachNode.isEmpty(node->prev))
		{
			// error: next node is null
			if (BeachNode.isEmpty(node->next) || node->next->is_boundary)
			{
				return EMPTY_float;
			}
			// start boundary: surrounding is same as next
			return (*this).GetNextAngle(this, node->next);
		}
		if (BeachNode.isEmpty(node->next))
		{
			// error: prev node is null
			if (BeachNode.isEmpty(node->prev) || node->prev->is_boundary)
			{
				return EMPTY_float;
			}
			// end boundary: surrounding is same as prev
			return (*this).GetPrevAngle(this, node->prev);
		}
	}
	// error: next or prev is null
	if (BeachNode.isEmpty(node->prev) || BeachNode.isEmpty(node->next))
	{
		return EMPTY_float;
	}
	// main case
	return ((*this).GetPrevAngle(this, node) + (*this).GetNextAngle(this, node)) / 2;
}

static float GetAngleByDifferencingScheme(struct BeachGrid* this, struct BeachNode* node, float wave_angle)
{
	float alpha = wave_angle - (*this).GetNextAngle(this, node);
	struct BeachNode* downwind_node;
	struct BeachNode* upwind_node;
	struct BeachNode* calc_node;
	float upwind_angle, downwind_angle;
	if (alpha > 0) // transport going right
	{
		calc_node = node;
		downwind_node = node->next;
		upwind_node = node->prev;
		upwind_angle = (*this).GetPrevAngle(this, calc_node);
		downwind_angle = (*this).GetNextAngle(this, calc_node);
		node->transport_dir = RIGHT;
	}
	else
	{
		calc_node = node->next;
		downwind_node = node;
		upwind_node = calc_node->is_boundary ? calc_node : calc_node->next;
		upwind_angle = (*this).GetNextAngle(this, calc_node);
		downwind_angle = (*this).GetPrevAngle(this, calc_node);
		node->transport_dir = LEFT;
	}

	if ((*this).CheckIfInShadow(this, calc_node, wave_angle))
	{
		return wave_angle - (PI / 2);
	}

	int downwindInShadow = (*this).CheckIfInShadow(this, downwind_node, wave_angle);
	int upwindInShadow = (*this).CheckIfInShadow(this, upwind_node, wave_angle);

	float instability_threshold = 42 * DEG_TO_RAD;
	int U = fabs(wave_angle - (*this).GetSurroundingAngle(this, calc_node)) >= instability_threshold;
	int U_downwind = !downwind_node->is_boundary ? (fabs(wave_angle - (*this).GetSurroundingAngle(this, downwind_node)) >= instability_threshold) : U;
	int U_upwind = !upwind_node->is_boundary ? (fabs(wave_angle - (*this).GetSurroundingAngle(this, upwind_node)) >= instability_threshold) : U;

	if (!U && ((U_downwind && !downwindInShadow) || (U_upwind && ! upwindInShadow)))
	{
		return wave_angle - (45 * DEG_TO_RAD);
	}

	if (downwindInShadow)
	{
		U = TRUE;
	}

	if (U && upwindInShadow)
	{
		return wave_angle - (PI / 2);
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

static int CheckIfInShadow(struct BeachGrid* this, struct BeachNode* node, float wave_angle)
{
	if (node->is_boundary)
	{
		return FALSE;
	}
	int row = node->GetRow(node);
	int col = node->GetCol(node);
	int i = 1;

	float shadow_step = 0.2;
	
	while (TRUE)
	{
		int r = row - (int)rint(i * shadow_step * cos(wave_angle));
		int c = col - (int)rint(i * shadow_step * sin(wave_angle));

		if (r < 0 || r >= (*this).rows || c < 0 || c >= (*this).cols)
		{
			return FALSE;
		}

		struct BeachNode* temp = TryGetNode(this, r, c);
		if (BeachNode.isEmpty(temp))
		{
			return FALSE;
		}

		if (temp->frac_full == 1 && (r - 1) < (row - (node->frac_full + fabs((c - col) / tan(wave_angle)))))
		{
			return TRUE;
		}

		i++;
	}
	return FALSE;
}

static struct BeachGrid new(int rows, int cols, float cell_width, float cell_length){
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
				.GetPrevAngle = &GetPrevAngle,
				.GetNextAngle = &GetNextAngle,
				.GetSurroundingAngle = &GetSurroundingAngle,
				.GetAngleByDifferencingScheme = &GetAngleByDifferencingScheme,
				.Get4Neighbors = &Get4Neighbors,
				.CheckIfInShadow = &CheckIfInShadow
				};
}

const struct BeachGridClass BeachGrid = { .new = &new };
