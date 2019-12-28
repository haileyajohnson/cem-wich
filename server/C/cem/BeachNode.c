
#include "BeachNode.h"
#include "BeachGrid.h"
#include "consts.h"
#include "utils.h"
#include <math.h>

static int GetRow(struct BeachNode* this)
{
	return this->row;
}

static int GetCol(struct BeachNode* this)
{
	return this->col;
}

static float GetTransportPotential(struct BeachNode* this)
{
	return this->transport_potential;
}

static int GetBoundaryRow(struct BeachNode* this)
{
	if (this->row != EMPTY_INT) { return this->row; }

	struct BeachNode* otherNode;
	float angle, cdist;
	if (!BeachNode.isEmpty(this->next))
	{
		otherNode = this->next;
		angle = BeachNode.GetAngle(otherNode, otherNode->next);
		cdist = otherNode->GetCol(otherNode) - this->col;
	}
	else
	{
		otherNode = this->prev;
		angle = BeachNode.GetAngle(otherNode->prev, otherNode);
		cdist = this->col - otherNode->GetCol(otherNode);
	}
	float rdist = cdist * tan(angle);
	return (int) trunc(otherNode->GetRow(otherNode) + rdist);
}

static int GetBoundaryCol(struct BeachNode* this)
{
	if (this->col != EMPTY_INT) { return this->col; }

	struct BeachNode* otherNode;
	float angle, rdist;
	if (!BeachNode.isEmpty(this->next))
	{
		otherNode = this->next;
		angle = BeachNode.GetAngle(otherNode, otherNode->next);
		rdist = otherNode->GetRow(otherNode) - this->row;
	}
	else
	{
		otherNode = this->prev;
		angle = BeachNode.GetAngle(otherNode->prev, otherNode);
		rdist = this->row - otherNode->GetRow(otherNode);
	}

	float cdist = rdist / tan(angle);
	return (int)trunc(otherNode->GetCol(otherNode) + cdist);
}

static float GetBoundaryTransportPotential(struct BeachNode* this)
{
	if (this->next)
	{
		return this->next->transport_potential;
	}
	return this->prev->transport_potential;
}

static FLOW_DIR GetBoundaryFlowDirection(struct BeachNode* this)
{
	if (this->next) {
		return this->next->transport_dir;
	}
	return this->prev->transport_dir;
}

static FLOW_DIR GetFlowDirection(struct BeachNode* this)
{
	FLOW_DIR next_dir = this->transport_dir;
	FLOW_DIR prev_dir = this->prev->is_boundary ? this->prev->GetFlowDirection(this->prev) : this->prev->transport_dir;

	if (prev_dir == RIGHT)
	{
		if (next_dir == RIGHT)
		{
			return RIGHT;
		}
		else
		{
			return CONVERGENT;
		}
	}
	else
	{
		if (next_dir == RIGHT)
		{
			return DIVERGENT;
		}
		else
		{
			return LEFT;
		}
	}
}

static struct BeachNode new(float frac_full, int r, int c, float cell_width, float cell_length){
		return (struct BeachNode) {
				.frac_full = frac_full,
				.transport_dir = NONE,
				.transport_potential = 0,
				.net_volume_change = 0,
				.is_beach = FALSE,
				.is_boundary = FALSE,
				.row = r,
				.col = c,
				.cell_width = cell_width,
				.cell_length = cell_length,
				.next = NULL,
				.prev = NULL,
				.GetRow = &GetRow,
				.GetCol = &GetCol,
				.GetFlowDirection = &GetFlowDirection,
				.GetTransportPotential = &GetTransportPotential
				};
}

static struct BeachNode empty() {
	return (struct BeachNode) {
		.frac_full = EMPTY_float,
			.transport_dir = EMPTY_INT,
			.transport_potential = EMPTY_float,
			.net_volume_change = EMPTY_float,
			.is_beach = EMPTY_INT,
			.is_boundary = EMPTY_INT,
			.row = EMPTY_INT,
			.col = EMPTY_INT,
			.cell_width = EMPTY_float,
			.cell_length = EMPTY_float,
			.next = NULL,
			.prev = NULL,
			.GetRow = NULL,
			.GetCol = NULL,
			.GetFlowDirection = NULL,
			.GetTransportPotential = NULL
	};
}

static struct BeachNode* boundary(int r, int j) {
	struct BeachNode* ptr = malloc(sizeof(struct BeachNode));
	*ptr = (struct BeachNode) {
		.frac_full = EMPTY_float,
			.transport_dir = EMPTY_INT,
			.transport_potential = EMPTY_float,
			.net_volume_change = EMPTY_float,
			.is_beach = TRUE,
			.is_boundary = TRUE,
			.row = r,
			.col = j,
			.cell_width = EMPTY_float,
			.cell_length = EMPTY_float,
			.next = NULL,
			.prev = NULL,
			.GetRow = &GetBoundaryRow,
			.GetCol = &GetBoundaryCol,
			.GetFlowDirection = &GetBoundaryFlowDirection,
			.GetTransportPotential = &GetBoundaryTransportPotential
	};

	return ptr;
}

static int isEmpty(struct BeachNode* node)
{
	if (!node)
	{
		return TRUE;
	}

	if (node->frac_full != EMPTY_float
		|| node->transport_dir != EMPTY_INT
		|| node->transport_potential != EMPTY_float
		|| node->net_volume_change != EMPTY_float
		|| node->is_beach != EMPTY_INT
		|| node->row != EMPTY_INT
		|| node->col != EMPTY_INT
		|| node->cell_width != EMPTY_float
		|| node->cell_length != EMPTY_float
		|| node->next != NULL
		|| node->prev != NULL
		|| node->GetRow != NULL
		|| node->GetCol != NULL
		|| node->GetFlowDirection != NULL
		|| node->GetTransportPotential != NULL)
	{
		return FALSE;
	}

	return TRUE;
}

/**
* Angle scheme:
* 90: left -> col1 == col2 && r1 > r2
* 0: up (seaward) -> col1 < col2 && r1 == r2
* -90: right -> col1 == col2 && r1 < r2
*/
static float GetAngle(struct BeachNode* node1, struct BeachNode* node2)
{
	float dR = node1->GetRow(node1) - node2->GetRow(node2);
	float dC = node2->GetCol(node2) - node1->GetCol(node1);
	float dF = node2->frac_full - node1->frac_full;

	// vertical orientation
	if (dC == 0)
	{
		// subtract if left edge (dR > 0), add if right edge (dR < 0) 
		dC += (-dR / fabs(dR)) * dF;
	}
	// horizontal orientation
	else
	{
		// subtract if bottom edge (dC < 0), add if top edge (dC > 0) 
		dR += (dC / fabs(dC)) * dF;
	}
	float angle = atan2(dR * node1->cell_length, dC * node1->cell_width);

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

const struct BeachNodeClass BeachNode = { .new = &new, .empty = &empty, .boundary = &boundary, .isEmpty = &isEmpty, .GetAngle = &GetAngle };
