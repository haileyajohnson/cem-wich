
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

static struct BeachNode* SetShoreline(struct BeachGrid* this, struct BeachNode* shoreline)
{
	this->shoreline = shoreline;
	return shoreline;
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
	if (BeachNode.isEmpty(node))
	{
		return EMPTY_double;
	}

	if (node->prev_timestamp == this->current_time)
	{
		return node->prev_angle;
	}

	double angle;
	if (node->is_boundary)
	{
		// start boundary node: prev is same as next->next
		if (BeachNode.isEmpty(node->prev) && !BeachNode.isEmpty(node->next))
		{
			angle = (*this).GetNextAngle(this, node->next);
		}
		// end boundary node: prev is same as prev->prev
		else if (BeachNode.isEmpty(node->next) && !BeachNode.isEmpty(node->prev))
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
		if (BeachNode.isEmpty(node->next) || node->next->is_boundary)
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
	node->prev_angle = angle;
	node->prev_timestamp = this->current_time;
	return angle;
}

static double GetNextAngle(struct BeachGrid* this, struct BeachNode* node)
{
	// error: node is null
	if (BeachNode.isEmpty(node))
	{
		return EMPTY_double;
	}

	if (node->next_timestamp == this->current_time)
	{
		return node->next_angle;
	}

	double angle;
	if (node->is_boundary)
	{
		// start boundary node: next is same as next->next
		if (BeachNode.isEmpty(node->prev) && !BeachNode.isEmpty(node->next))
		{
			angle = (*this).GetNextAngle(this, node->next);
		}
		// end boundary node: next is same as prev->prev
		else if (BeachNode.isEmpty(node->next) && !BeachNode.isEmpty(node->prev))
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
		if (BeachNode.isEmpty(node->prev) || node->prev->is_boundary)
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
	node->next_angle = angle;
	node->next_timestamp = this->current_time;
	return angle;
}

static double GetSurroundingAngle(struct BeachGrid* this, struct BeachNode* node)
{
	// error: node is null
	if (BeachNode.isEmpty(node))
	{
		return EMPTY_double;
	}

	if (node->surrounding_timestamp == this->current_time)
	{
		return node->surrounding_angle;
	}

	double angle;
	if (node->is_boundary)
	{
		if (BeachNode.isEmpty(node->prev))
		{
			// error: next node is null
			if (BeachNode.isEmpty(node->next) || node->next->is_boundary)
			{
				return EMPTY_double;
			}
			// start boundary: surrounding is same as next
			angle = (*this).GetNextAngle(this, node->next);
		}
		else if (BeachNode.isEmpty(node->next))
		{
			// error: prev node is null
			if (BeachNode.isEmpty(node->prev) || node->prev->is_boundary)
			{
				return EMPTY_double;
			}
			// end boundary: surrounding is same as prev
			angle = (*this).GetPrevAngle(this, node->prev);
		}
	}
	// error: next or prev is null
	else if (BeachNode.isEmpty(node->prev) || BeachNode.isEmpty(node->next))
	{
		return EMPTY_double;
	}
	else
	{
		// main case
		angle = ((*this).GetPrevAngle(this, node) + (*this).GetNextAngle(this, node)) / 2;
	}
	node->surrounding_angle = angle;
	node->surrounding_timestamp = this->current_time;
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
		if (BeachNode.isEmpty(neighbors[i])) {
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
	int row = node->GetRow(node);
	int col = node->GetCol(node);
	int i = 1;

	double shadow_step = 0.2;

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

int FindBeach(struct BeachGrid* this)
{
	// clear current shoreline if grid already has one
	if (!BeachNode.isEmpty(this->shoreline))
	{
		struct BeachNode* shoreline = this->shoreline;
		struct BeachNode* curr = this->shoreline;
		do {
			curr->is_beach = FALSE;
			if (BeachNode.isEmpty(curr)) { break; }
			if (!BeachNode.isEmpty(curr->prev) && curr->prev->is_boundary)
			{
				free(curr->prev);
			}
			curr->prev = NULL;
			struct BeachNode* next = curr->next;
			curr->next = NULL;
			curr = next;
		} while (!curr->is_boundary);

		if (curr->is_boundary)
		{
			free(curr);
		}
		// free mem
		(*this).SetShoreline(this, NULL);
	}
	
	// start search downward from top left
	int r, c;
	for (c = 0; c < this->cols; c++) {
		for (r = 1; r < this->rows; r++) {
			struct BeachNode* node = (*this).TryGetNode(this, r, c);

			// start tracing shoreline
			if ((*this).IsLandCell(this, r, c)) {
				struct BeachNode* startNode = node;
				if (BeachNode.isEmpty(startNode)) { return -1; }

				struct BeachNode* endNode = (*this).GetShoreline(this, startNode, NULL, 1, 0);
				if (!endNode || BeachNode.isEmpty(endNode))
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
	if (BeachNode.isEmpty(node) || node->is_boundary)
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
	if (!BeachNode.isEmpty(prev))
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
				stop->is_beach = FALSE;
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
	node->is_beach = FALSE;
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
		if (BeachNode.isEmpty(curr)) { return NULL; }
		curr->is_beach = TRUE;

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
		if (!tempNode || BeachNode.isEmpty(tempNode))
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
		if (tempNode->is_beach) {
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
	if (BeachNode.isEmpty(temp))
	{
		return FALSE;
	}

	return (temp->frac_full > 0.0);
}

static struct BeachGrid new(int rows, int cols, double cell_width, double cell_length){
		return (struct BeachGrid) {
				.cell_width = cell_width,
				.cell_length = cell_length,
				.rows = rows,
				.cols = cols,
				.current_time = 0,
				.cells = NULL,
				.shoreline = NULL,
				.SetCells = &SetCells,
				.SetShoreline = &SetShoreline,
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
