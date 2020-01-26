
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
	if (this->shoreline && !BeachNode.isEmpty(this->shoreline[0]))
	{
		struct BeachNode** shorelines = this->shoreline;
		// reset all shoreline nodes to is_beach = FALSE
		int i = 0;
		while (i < this->num_shorelines)
		{
			struct BeachNode* start = this->shoreline[i];
			struct BeachNode* curr = start;
			do {
				curr->is_beach = FALSE;
				if (BeachNode.isEmpty(curr)) { break; }
				if (!BeachNode.isEmpty(curr->prev) && curr->prev->is_boundary)
				{
					if (curr->prev->col == EMPTY_INT || curr->prev->row == EMPTY_INT)
					{
						free(curr->prev);
					}
					else {
						curr->prev->is_beach = FALSE;
						curr->prev->is_boundary = FALSE;
					}
				}
				curr->prev = NULL;
				struct BeachNode* next = curr->next;
				curr->next = NULL;
				curr = next;
			} while (curr != start && !curr->is_boundary);
			if (curr->is_boundary)
			{
				if (curr->col == EMPTY_INT || curr->row == EMPTY_INT)
				{
					free(curr);
				}
				else {
					curr->is_beach = FALSE;
					curr->is_boundary = FALSE;
				}
			}
			i++;
		}
		// free mem
		(*this).SetShorelines(this, NULL, 0);
		free(shorelines);
	}

	// empty shoreline pointer
	struct BeachNode** shorelines = malloc(sizeof(struct BeachNode*));
	int num_landmasses = 0;

	int r, c;
	for (c = 0; c < this->cols; c++) {
		for (r = 1; r < this->rows; r++) {
			// turn off search when we encounter boundary
			struct BeachNode* node = (*this).TryGetNode(this, r, c);
			if (node->is_beach)
			{
				// don't search below outer shoreline
				break;
			}

			// start tracing shoreline
			if ((*this).IsLandCell(this, r, c)) {
				// start cell
				struct BeachNode* startNode = node;
				if (BeachNode.isEmpty(startNode)) { return -1; }

				struct BeachNode* endNode = (*this).TraceLandmass(this, startNode, NULL, 1, 0, TRUE);
				if (!endNode || BeachNode.isEmpty(endNode))
				{
					return -1;
				}

				// check start boundary
				if (BeachNode.isEmpty(startNode->prev) && !BeachNode.isEmpty(startNode->next))
				{
					struct BeachNode* startBoundary;
					if (startNode->GetCol(startNode) == 0) { startBoundary = BeachNode.boundary(EMPTY_INT, -1); }
					else if (startNode->GetCol(startNode) == this->cols - 1) { startBoundary = BeachNode.boundary(EMPTY_INT, this->cols); }
					else if (startNode->GetRow(startNode) == 0) { startBoundary = BeachNode.boundary(-1, EMPTY_INT); }
					else if (startNode->GetRow(startNode) == this->rows - 1) { startBoundary = BeachNode.boundary(this->rows, EMPTY_INT); }
					else {
						startNode->is_boundary = TRUE;
						// TODO set boundary flow
						startBoundary = startNode;
						startNode = startNode->next;

					}
					startNode->prev = startBoundary;
					startBoundary->next = startNode;
				}
				// check end boundary
				if (BeachNode.isEmpty(endNode->next) && !BeachNode.isEmpty(endNode->prev))
				{
					struct BeachNode* endBoundary;
					if (endNode->GetCol(endNode) == 0) { endBoundary = BeachNode.boundary(EMPTY_INT, -1); }
					else if (endNode->GetCol(endNode) == this->cols - 1) { endBoundary = BeachNode.boundary(EMPTY_INT, this->cols); }
					else if (endNode->GetRow(endNode) == 0) { endBoundary = BeachNode.boundary(-1, EMPTY_INT); }
					else if (endNode->GetRow(endNode) == this->rows - 1) { endBoundary = BeachNode.boundary(this->rows, EMPTY_INT); }
					else {
						endNode->is_boundary = TRUE;
						// TODO set boundary flow
						endBoundary = endNode;
						endNode = endNode->prev;
					}
					endNode->next = endBoundary;
					endBoundary->prev = endNode;
				}

				if (!startNode->next || !startNode->prev)
				{
					r = startNode->GetRow(startNode) + 1;
					continue;
				}
				if (num_landmasses > 0)
				{
					// grow shorelines array
					struct BeachNode** temp = realloc(shorelines, (num_landmasses + 1) * sizeof(struct BeachNode*));
					if (temp)
					{
						shorelines = temp;
					}
					else {
						// TODO: handle error
						free(shorelines);
						return -1;
					}
				}

				shorelines[num_landmasses] = startNode;
				num_landmasses++;
				break;
			}
		}
	}

	// no shoreline found
	if (!shorelines || BeachNode.isEmpty(shorelines[0])) {
		// free shorelines var
		free(shorelines);
		return -1;
	}

	(*this).SetShorelines(this, shorelines, num_landmasses);
	return 0;
}

static struct BeachNode* ReplaceNode(struct BeachGrid* this, struct BeachNode* node)
{
	struct BeachNode* start = node;
	struct BeachNode* stop = node;
	if (!BeachNode.isEmpty(node->prev))
	{
		start = BeachNode.isEmpty(node->prev->prev) ? node->prev : node->prev->prev;
	}
	if (!BeachNode.isEmpty(node->next))
	{
		stop = BeachNode.isEmpty(node->next->next) ? node->next : node->next->next;
	}

	struct BeachNode* curr = start->next;
	// set flag to replace shoreline head if necessary
	int shoreline_head = -1;
	if (start->is_boundary) {
		int i;
		for (i = 0; i < this->num_shorelines; i++)
		{
			if (this->shoreline[i] == curr)
			{
				shoreline_head = i;
				break;
			}
		}
	}

	// detach shoreline segment
	start->next = NULL;
	while (curr != stop) {
		curr->is_beach = FALSE;
		curr->is_boundary = FALSE;
		struct BeachNode* temp = curr->next;
		curr->prev = NULL;
		curr->next = NULL;
		curr = temp;
	}
	stop->prev = NULL;

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

	struct BeachNode* endNode = (*this).TraceLandmass(this, start, stop, dir_r, dir_c, FALSE);
	// complex shoreline change, need to do full FindShoreline
	if (endNode != stop)
	{
		// attach start and stop nodes to allow for shoreline cleanup
		start->next = stop;
		stop->prev = start;
		// TODO: return ????
		return NULL;
	}	

	// reset shoreline head node
	if (shoreline_head >= 0)
	{
		this->shoreline[shoreline_head] = start->next;
	}

	// TODO: return ???
	return node;
}


/**
* Find shoreline between start and end cols using Moore tracing algorithm
*/
struct BeachNode* TraceLandmass(struct BeachGrid* this, struct BeachNode* startNode, struct BeachNode* stopNode, int dir_r,  int dir_c, int reverse)
{
	struct BeachNode* endNode = startNode;
	struct BeachNode* curr = startNode;

	// search clockwise, then counterclockwise if `reverse` is TRUE
	int search_dir = 1; // clockwise

	// while not done tracing landmass
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
			// turn 45 degrees clockwise/counterclockwise to next neighbor
			double angle = atan2(temp[0] - currRow, temp[1] - currCol);
			angle += (search_dir) * (PI / 4);
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
			// search counterclockwise
			if (search_dir > 0 && reverse)
			{
				curr = startNode;
				search_dir = -1;
				continue;
			}
			break;
		}

		struct BeachNode* tempNode = (*this).TryGetNode(this, temp[0], temp[1]);
		// temp node invalid, return NULL
		if (!tempNode || BeachNode.isEmpty(tempNode))
		{
			return NULL;
		}
		// temp node is stop node
		if (tempNode == stopNode)
		{
			return tempNode;
		}

		// searching clockwise
		if (search_dir > 0) {
		// node already a beach cell, add boundary cell and break
			if (tempNode->is_beach) {
				curr->next = NULL;
				curr = startNode;
				search_dir = -1;
				continue;
			}
			curr->next = tempNode;
			tempNode->prev = curr;

			//// end if undercutting
			//if (tempNode->GetCol(tempNode) < curr->GetCol(curr) && tempNode->GetRow(tempNode) >= curr->GetRow(curr))
			//{
			//	tempNode->is_beach = TRUE;
			//	tempNode->is_boundary = TRUE;
			//	// TODO set get flow to boundary get flow
			//	search_dir = -1;
			//	dir_r = 1;
			//	dir_c = 0;
			//	curr = startNode;
			//	continue;
			//}
			endNode = tempNode;
		}
		// searching counterclockwise
		else {
		// node already a beach cell, add boundary cell and break
			if (tempNode->is_beach) {
				curr->prev = NULL;
				break;
			}
			curr->prev = tempNode;
			tempNode->next = curr;
			//// end if undercutting
			//if (tempNode->GetCol(tempNode) > curr->GetCol(curr) && tempNode->GetRow(tempNode) >= curr->GetRow(curr))
			//{
			//	tempNode->is_beach = TRUE;
			//	tempNode->is_boundary = TRUE;
			//	// TODO set get flow to boundary get flow
			//	break;
			//}
			startNode = tempNode;
		}
		curr = tempNode;
	}
	return endNode;
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
				.num_shorelines = 0,
				.SetCells = &SetCells,
				.SetShorelines = &SetShorelines,
				.TryGetNode = &TryGetNode,
				.GetPrevAngle = &GetPrevAngle,
				.GetNextAngle = &GetNextAngle,
				.GetSurroundingAngle = &GetSurroundingAngle,
				.GetAngleByDifferencingScheme = &GetAngleByDifferencingScheme,
				.ReplaceNode = &ReplaceNode,
				.Get4Neighbors = &Get4Neighbors,
				.CheckIfInShadow = &CheckIfInShadow,
				.FindBeach = &FindBeach,
				.TraceLandmass = &TraceLandmass,
				.IsLandCell = &IsLandCell
				};
}

const struct BeachGridClass BeachGrid = { .new = &new };
