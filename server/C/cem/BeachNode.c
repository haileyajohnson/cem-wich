
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

static double GetTransportPotential(struct BeachNode* this)
{
	return this->transport_potential;
}

static int GetBoundaryRow(struct BeachNode* this)
{
	if (this->row != EMPTY_INT) { return this->row; }

	struct BeachNode* otherNode;
	double angle;
	if (!BeachNode.isEmpty(this->next))
	{
		otherNode = this->next;
		angle = BeachNode.GetAngle(otherNode, otherNode->next);
	}
	else
	{
		otherNode = this->prev;
		angle = BeachNode.GetAngle(otherNode, otherNode->prev);
	}

	double cdist = otherNode->GetCol(otherNode) - this->col;
	double rdist = cdist * tan(angle);
	return otherNode->GetRow(otherNode) + rdist;
}

static int GetBoundaryCol(struct BeachNode* this)
{
	if (this->col != EMPTY_INT) { return this->col; }

	struct BeachNode* otherNode;
	double angle;
	if (!BeachNode.isEmpty(this->next))
	{
		otherNode = this->next;
		angle = BeachNode.GetAngle(otherNode, otherNode->next);
	}
	else
	{
		otherNode = this->prev;
		angle = BeachNode.GetAngle(otherNode, otherNode->prev);
	}

	double rdist = otherNode->GetRow(otherNode) - this->row;
	double cdist = rdist / tan(angle);
	return otherNode->GetCol(otherNode) + cdist;
}

static double GetBoundaryTransportPotential(struct BeachNode* this)
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
		return RIGHT;
	}
	return LEFT;
}

static FLOW_DIR GetFlowDirection(struct BeachNode* this)
{
	// get left cell border
	struct BeachNode* leftBorder = this;
	while (!leftBorder->prev->is_boundary && leftBorder->prev != this && leftBorder->transport_dir == NONE)
	{
		leftBorder = leftBorder->prev;
	}

	//get right cell border
	struct BeachNode* rightBorder = this;
	while (!rightBorder->next->is_boundary && rightBorder->next != this && rightBorder->transport_dir == NONE)
	{
		rightBorder = rightBorder->next;
	}

	if (rightBorder->transport_dir == NONE || leftBorder->transport_dir == NONE)
	{
		return NONE;
	}

	if (rightBorder->transport_dir == RIGHT)
	{
		if (leftBorder->transport_dir == RIGHT)
		{
			return RIGHT;
		}
		else
		{
			return DIVERGENT;
		}
	}
	else
	{
		if (leftBorder->transport_dir == RIGHT)
		{
			return CONVERGENT;
		}
		else
		{
			return LEFT;
		}
	}
}

static struct BeachNode new(double frac_full, int r, int c, double cell_width, double cell_length){
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
		.frac_full = EMPTY_DOUBLE,
			.transport_dir = EMPTY_INT,
			.transport_potential = EMPTY_DOUBLE,
			.net_volume_change = EMPTY_DOUBLE,
			.is_beach = EMPTY_INT,
			.is_boundary = EMPTY_INT,
			.row = EMPTY_INT,
			.col = EMPTY_INT,
			.cell_width = EMPTY_DOUBLE,
			.cell_length = EMPTY_DOUBLE,
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
		.frac_full = EMPTY_DOUBLE,
			.transport_dir = EMPTY_INT,
			.transport_potential = EMPTY_DOUBLE,
			.net_volume_change = EMPTY_DOUBLE,
			.is_beach = TRUE,
			.is_boundary = TRUE,
			.row = r,
			.col = j,
			.cell_width = EMPTY_DOUBLE,
			.cell_length = EMPTY_DOUBLE,
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
	if (node == NULL)
	{
		return TRUE;
	}

	if (node->frac_full != EMPTY_DOUBLE
		|| node->transport_dir != EMPTY_INT
		|| node->transport_potential != EMPTY_DOUBLE
		|| node->net_volume_change != EMPTY_DOUBLE
		|| node->is_beach != EMPTY_INT
		|| node->row != EMPTY_INT
		|| node->col != EMPTY_INT
		|| node->cell_width != EMPTY_DOUBLE
		|| node->cell_length != EMPTY_DOUBLE
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

static double GetAngle(struct BeachNode* node1, struct BeachNode* node2)
{
	double rdist, cdist;
	if (node1->GetCol(node1) == node2->GetCol(node2))
	{
		rdist = (node2->GetRow(node2) - node1->GetRow(node1)) * node1->cell_length;
		cdist = ((node2->GetCol(node2) + node2->frac_full) - (node1->GetCol(node1) + node1->frac_full)) * node1->cell_width;
	}
	else
	{
		rdist = ((node2->GetRow(node2) + node2->frac_full) - (node1->GetRow(node1) + node1->frac_full)) * node1->cell_length;
		cdist = (node2->GetCol(node2) - node1->GetCol(node1)) * node1->cell_width;
	}
	double angle = atan2(rdist, cdist);

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
