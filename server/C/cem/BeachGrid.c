#include "BeachProperties.h"
#include "BeachNode.h"
#include "BeachGrid.h"
#include "BeachProperties.h"
#include "consts.h"
#include "utils.h"
#include <math.h>

double g_cell_length, g_cell_width;

static struct BeachGrid* SetCells(struct BeachGrid* this, struct BeachNode** cells)
{
	this->cells = cells;
	return this;
}

static struct BeachNode* SetShoreline(struct BeachGrid* this, struct BeachNode* shoreline)
{
	this->shoreline = shoreline;
	return shoreline;
}

void FreeShoreline(struct BeachGrid* this)
{
	struct BeachNode* shoreline = this->shoreline;
	struct BeachNode* curr = this->shoreline;
	do {
		free(curr->properties);
		curr->properties = NULL;
		if (!curr) { break; }
		if (curr->prev && curr->prev->is_boundary)
		{
			free(curr->prev->properties);
			free(curr->prev);
		}
		curr->prev = NULL;
		struct BeachNode* next = curr->next;
		curr->next = NULL;
		curr = next;
	} while (!curr->is_boundary);

	if (curr->is_boundary)
	{
		free(curr->properties);
		free(curr);
	}
	// free mem
	(*this).SetShoreline(this, NULL);

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

static double GetPrevAngle(struct BeachGrid* this, struct BeachNode* node)
{
	// error: node is null
	if (!node)
	{
		return EMPTY_double;
	}

	if (node->properties->prev_timestamp == this->current_time)
	{
		return node->properties->prev_angle;
	}

	double angle;
	if (node->is_boundary)
	{
		// start boundary node: prev is same as next->next
		if (!node->prev && node->next)
		{
			angle = (*this).GetNextAngle(this, node->next);
		}
		// end boundary node: prev is same as prev->prev
		else if (!node->next && node->prev)
		{
			angle = (*this).GetPrevAngle(this, node->prev);
		}
		// error: no valid neighbors
		else
		{
			return EMPTY_double;
		}
	}
	else if (node->prev->is_boundary)
	{
		// error: no valid neighbors
		if (!node->next || node->next->is_boundary)
		{
			return EMPTY_double;
		}
		// start node: prev is same as next
		angle = BeachNode.GetAngle(node, node->next);
	}
	else
	{
		// main case
		angle = BeachNode.GetAngle(node->prev, node);
	}
	node->properties->prev_angle = angle;
	node->properties->prev_timestamp = this->current_time;
	return angle;
}

static double GetNextAngle(struct BeachGrid* this, struct BeachNode* node)
{
	// error: node is null
	if (!node)
	{
		return EMPTY_double;
	}

	if (node->properties->next_timestamp == this->current_time)
	{
		return node->properties->next_angle;
	}

	double angle;
	if (node->is_boundary)
	{
		// start boundary node: next is same as next->next
		if (!node->prev && node->next)
		{
			angle = (*this).GetNextAngle(this, node->next);
		}
		// end boundary node: next is same as prev->prev
		else if (!node->next && node->prev)
		{
			angle = (*this).GetPrevAngle(this, node->prev);
		}
		else
		{
			// error: no valid neighbors
			return EMPTY_double;
		}
	}
	else if (node->next->is_boundary)
	{
		// error: no valid neighbors
		if (!node->prev || node->prev->is_boundary)
		{
			return EMPTY_double;
		}
		// end node: next is same as prev
		angle = BeachNode.GetAngle(node->prev, node);
	}
	else
	{
		// main case
		angle = BeachNode.GetAngle(node, node->next);
	}
	node->properties->next_angle = angle;
	node->properties->next_timestamp = this->current_time;
	return angle;
}

static double GetSurroundingAngle(struct BeachGrid* this, struct BeachNode* node)
{
	// error: node is null
	if (!node)
	{
		return EMPTY_double;
	}

	if (node->properties->surrounding_timestamp == this->current_time)
	{
		return node->properties->surrounding_angle;
	}

	double angle;
	if (node->is_boundary)
	{
		if (!node->prev)
		{
			// error: next node is null
			if (!node->next || node->next->is_boundary)
			{
				return EMPTY_double;
			}
			// start boundary: surrounding is same as next
			angle = (*this).GetNextAngle(this, node->next);
		}
		else if (!node->next)
		{
			// error: prev node is null
			if (!node->prev || node->prev->is_boundary)
			{
				return EMPTY_double;
			}
			// end boundary: surrounding is same as prev
			angle = (*this).GetPrevAngle(this, node->prev);
		}
	}
	// error: next or prev is null
	else if (!node->prev || !node->next)
	{
		return EMPTY_double;
	}
	else
	{
		// main case
		angle = ((*this).GetPrevAngle(this, node) + (*this).GetNextAngle(this, node)) / 2;
	}
	node->properties->surrounding_angle = angle;
	node->properties->surrounding_timestamp = this->current_time;
	return angle;
}

static double GetAngleByDifferencingScheme(struct BeachGrid* this, struct BeachNode* node, double wave_angle)
{
	double alpha = wave_angle - (*this).GetNextAngle(this, node);
	struct BeachNode* downwind_node;
	struct BeachNode* upwind_node;
	struct BeachNode* calc_node;
	double upwind_angle, downwind_angle;
	if (alpha > 0) // transport going right
	{
		calc_node = node;
		downwind_node = node->next;
		upwind_node = node->prev;
		upwind_angle = (*this).GetPrevAngle(this, calc_node);
		downwind_angle = (*this).GetNextAngle(this, calc_node);
		node->properties->transport_dir = RIGHT;
	}
	else
	{
		calc_node = node->next;
		downwind_node = node;
		upwind_node = calc_node->is_boundary ? calc_node : calc_node->next;
		upwind_angle = (*this).GetNextAngle(this, calc_node);
		downwind_angle = (*this).GetPrevAngle(this, calc_node);
		node->properties->transport_dir = LEFT;
	}

	if ((*this).CheckIfInShadow(this, calc_node, wave_angle))
	{
		return wave_angle - (PI / 2);
	}

	int downwindInShadow = (*this).CheckIfInShadow(this, downwind_node, wave_angle);
	int upwindInShadow = (*this).CheckIfInShadow(this, upwind_node, wave_angle);

	double instability_threshold = 42 * DEG_TO_RAD;
	int U = fabs(wave_angle - (*this).GetSurroundingAngle(this, calc_node)) >= instability_threshold;
	int U_downwind = !downwind_node->is_boundary ? (fabs(wave_angle - (*this).GetSurroundingAngle(this, downwind_node)) >= instability_threshold) : U;
	int U_upwind = !upwind_node->is_boundary ? (fabs(wave_angle - (*this).GetSurroundingAngle(this, upwind_node)) >= instability_threshold) : U;

	if (!U && ((U_downwind && !downwindInShadow) || (U_upwind && !upwindInShadow)))
	{
		return EMPTY_double;
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
		if (neighbors[i]) {
			struct BeachNode* neighbor = BeachNode.boundary(EMPTY_INT, EMPTY_INT);
			neighbor->frac_full = node->frac_full > 1.0 ? 0.0 : 1.0;
			neighbors[i] = neighbor;
		}
	}

	return neighbors;
}

static int CheckIfInShadow(struct BeachGrid* this, struct BeachNode* node, double wave_angle)
{
	if (node->is_boundary)
	{
		return FALSE;
	}

	if (node->properties->shadow_timestamp == this->current_time)
	{
		return node->properties->in_shadow;
	}

	// start at corner TODO: switch to centroid
	int node_r = node->GetRow(node);
	int node_c = node->GetCol(node);
	double r = (double)node_r;
	double c = (double)node_c;
	double cos_angle = cos(wave_angle);
	double sin_angle = sin(wave_angle);
	int r_sign = cos_angle >= 0 ? -1 : 1;
	int c_sign = sin_angle >= 0 ? -1 : 1;

	node->properties->shadow_timestamp = this->current_time;

	while (TRUE)
	{
		int next_r = ceil(r + r_sign);
		int next_c = ceil(c + c_sign);
		double d_r = fabs(((next_r - r) * g_cell_length) / cos_angle);
		double d_c = fabs(((next_c - c) * g_cell_width) / sin_angle);

		if (d_r < d_c)
		{
			r = (double)next_r;
			c += (c_sign * fabs(d_r * sin_angle)) / g_cell_width;
		}
		else
		{
			c = (double)next_c;
			r += (r_sign * fabs(d_c * cos_angle)) / g_cell_length;
		}

		if (r < 0 || r >= (*this).rows || c < 0 || c >= (*this).cols)
		{
			node->properties->in_shadow = FALSE;
			break;
		}

		int row = floor(r);
		int col = floor(c);

		struct BeachNode* temp = TryGetNode(this, row, col);
		if (!temp)
		{
			node->properties->in_shadow = FALSE;
			break;
		}

		if (temp->frac_full == 1 && (row - 1) < (node_r - (node->frac_full + fabs((col - node_c) / tan(wave_angle)))))
		{
			node->properties->in_shadow = TRUE;
			break;
		}
	}
	return node->properties->in_shadow;
}

int FindBeach(struct BeachGrid* this)
{
	// clear current shoreline if grid already has one
	if (this->shoreline)
	{
		(*this).FreeShoreline;
	}
	
	// start search downward from top left
	int r, c;
	for (c = 0; c < this->cols; c++) {
		for (r = 1; r < this->rows; r++) {
			struct BeachNode* node = (*this).TryGetNode(this, r, c);

			// start tracing shoreline
			if ((*this).IsLandCell(this, r, c)) {
				struct BeachNode* startNode = node;
				if (!startNode) { return -1; }

				struct BeachNode* endNode = (*this).GetShoreline(this, startNode, NULL, 1, 0);
				if (!endNode)
				{
					return -1;
				}

				// set start boundary
				struct BeachNode* startBoundary;
				int startCol = startNode->GetCol(startNode);
				int startRow = startNode->GetRow(startNode);
				if (startCol == 0) { startBoundary = BeachNode.boundary(EMPTY_INT, -1); }
				else if (startCol == this->cols - 1) { startBoundary = BeachNode.boundary(EMPTY_INT, this->cols); }
				else if (startRow == 0) { startBoundary = BeachNode.boundary(-1, EMPTY_INT); }
				else if (startRow == this->rows - 1) { startBoundary = BeachNode.boundary(this->rows, EMPTY_INT); }
				else { // start not at grid boundary, invalid grid
					return -1;
				}
				startNode->prev = startBoundary;
				startBoundary->next = startNode;

				// set end boundary
				struct BeachNode* endBoundary;
				int endCol = endNode->GetCol(endNode);
				int endRow = endNode->GetRow(endNode);
				if (endCol == 0) { endBoundary = BeachNode.boundary(EMPTY_INT, -1); }
				else if (endCol == this->cols - 1) { endBoundary = BeachNode.boundary(EMPTY_INT, this->cols); }
				else if (endRow == 0) { endBoundary = BeachNode.boundary(-1, EMPTY_INT); }
				else if (endRow == this->rows - 1) { endBoundary = BeachNode.boundary(this->rows, EMPTY_INT); }
				else { // end not at grid boundary, invalid grid
					return -1;
				}
				endNode->next = endBoundary;
				endBoundary->prev = endNode;

				(*this).SetShoreline(this, startNode);
				return 0;
			}
		}
	}
	return -1;
}

static struct BeachNode* ReplaceNode(struct BeachGrid* this, struct BeachNode* node)
{
	if (!node || node->is_boundary)
	{
		return NULL;
	}
	struct BeachNode* start = node->prev;
	struct BeachNode* stop = node->next;

	struct BeachNode* curr = node;

	// get start dir - default to left if prev is null
	int dir_r = 0;
	int dir_c = -1;
	struct BeachNode* prev = start->prev;
	if (prev)
	{
		// start from 45 clockwise of prev
		double angle = atan2(prev->GetRow(prev) - start->GetRow(start), prev->GetCol(prev) - start->GetCol(start));
		angle += (PI / 4);
		dir_r = -round(sin(angle));
		dir_c = -round(cos(angle));
	}

	struct BeachNode* endNode = (*this).GetShoreline(this, start, stop, dir_r, dir_c);
	if (endNode != stop)
	{
		// edge of grid - add boundary node
		if (endNode->GetCol(endNode) == this->cols - 1)
		{
			while (!stop->is_boundary)
			{
				struct BeachNode* next = stop->next;
				free(stop->properties);
				stop->properties = NULL;
				stop->prev = NULL;
				stop->next = NULL;
				stop = next;
			}
			endNode->next = stop;
			stop->prev = endNode;
		}
		// error: didn't replace shoreline
		else
		{
			return NULL;
		}
	}	

	// reset shoreline head node if necessary
	if (this->shoreline == node)
	{
		this->shoreline = start;
	}

	// remove node from shoreline
	free(node->properties);
	node->properties = NULL;
	node->prev = NULL;
	node->next = NULL;

	return stop;
}


/**
* Find shoreline between start and end cols using Moore tracing algorithm
*/
struct BeachNode* GetShoreline(struct BeachGrid* this, struct BeachNode* startNode, struct BeachNode* stopNode, int dir_r,  int dir_c)
{
	struct BeachNode* curr = startNode;

	// while not done tracing boundary
	while (TRUE)
	{
		// mark as beach
		if (!curr) { return NULL; }
		struct BeachProperties* props = BeachProperties.new();
		curr->properties = props;

		// backtrack
		dir_r = -dir_r;
		dir_c = -dir_c;
		int currRow = curr->GetRow(curr);
		int currCol = curr->GetCol(curr);
		int backtrack[2] = { currRow + dir_r, currCol + dir_c };
		int temp[2] = { backtrack[0], backtrack[1] };

		int foundNextBeach = FALSE;
		do
		{
			// turn 45 degrees clockwise to next neighbor
			double angle = atan2(temp[0] - currRow, temp[1] - currCol);
			angle += (PI / 4);
			int next[2] = { currRow + round(sin(angle)), currCol + round(cos(angle)) };

			// break if running off edge of grid
			if (next[0] < 0 || next[0] >= this->rows || next[1] < 0 || next[1] >= this->cols)
			{
				break;
			}

			// track dir relative to previous neighbor for backtracking
			dir_r = next[0] - temp[0];
			dir_c = next[1] - temp[1];
			temp[0] = next[0];
			temp[1] = next[1];
			if ((*this).IsLandCell(this, temp[0], temp[1]))
			{
				foundNextBeach = TRUE;
				break;
			}
		} while (temp[0] != backtrack[0] || temp[1] != backtrack[1]);

		// end landmass
		if (!foundNextBeach) {
			break;
		}

		struct BeachNode* tempNode = (*this).TryGetNode(this, temp[0], temp[1]);
		// temp node invalid, return NULL
		if (!tempNode)
		{
			return NULL;
		}
		
		// curr node is stop node
		if (tempNode == stopNode)
		{
			curr->next = tempNode;
			tempNode->prev = curr;
			return tempNode;
		}

		// node already a beach cell, invalid grid // change to curr not tempNode
		if (tempNode->properties) {
			if (!stopNode || abs(curr->GetRow(curr) - stopNode->GetRow(stopNode)) > 1 || abs(curr->GetCol(curr) - stopNode->GetCol(stopNode)) > 1)
			{
				return NULL;
			}

			curr->next = stopNode;
			stopNode->prev = curr;
			return stopNode;
		}

		curr->next = tempNode;
		tempNode->prev = curr;
		curr = tempNode;
	}
	return curr;
}

/**
* Return true if cell at row, col contains land
*/
static int IsLandCell(struct BeachGrid* this, int row, int col)
{
	if (row < 0 || row > this->rows || col < 0 || col > this->cols)
	{
		return FALSE;
	}

	struct BeachNode* temp = this->TryGetNode(this, row, col);
	if (!temp)
	{
		return FALSE;
	}

	return (temp->frac_full > 0.0);
}

static struct BeachGrid new(int rows, int cols, double cell_width, double cell_length){
	g_cell_width = cell_width;
	g_cell_length = cell_length;

		return (struct BeachGrid) {
				.rows = rows,
				.cols = cols,
				.current_time = 0,
				.cells = NULL,
				.shoreline = NULL,
				.SetCells = &SetCells,
				.SetShoreline = &SetShoreline,
				.FreeShoreline = &FreeShoreline,
				.TryGetNode = &TryGetNode,
				.GetPrevAngle = &GetPrevAngle,
				.GetNextAngle = &GetNextAngle,
				.GetSurroundingAngle = &GetSurroundingAngle,
				.GetAngleByDifferencingScheme = &GetAngleByDifferencingScheme,
				.ReplaceNode = &ReplaceNode,
				.Get4Neighbors = &Get4Neighbors,
				.CheckIfInShadow = &CheckIfInShadow,
				.FindBeach = &FindBeach,
				.GetShoreline = &GetShoreline,
				.IsLandCell = &IsLandCell
				};
}

const struct BeachGridClass BeachGrid = { .new = &new };
