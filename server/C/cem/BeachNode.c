#include "BeachProperties.h"
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
	return this->properties->transport_potential;
}

static int GetBoundaryRow(struct BeachNode* this)
{
	if (this->row != EMPTY_INT) { return this->row; }

	struct BeachNode* otherNode;
	double angle, cdist;
	if (this->next)
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
	double rdist = cdist * tan(angle);
	return (int)trunc(otherNode->GetRow(otherNode) + rdist);
}

static int GetBoundaryCol(struct BeachNode* this)
{
	if (this->col != EMPTY_INT) { return this->col; }

	struct BeachNode* otherNode;
	double angle, rdist;
	if (this->next)
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

	double cdist = rdist / tan(angle);
	return (int)trunc(otherNode->GetCol(otherNode) + cdist);
}

static double GetBoundaryTransportPotential(struct BeachNode* this)
{
	if (this->next)
	{
		return this->next->properties->transport_potential;
	}
	return this->prev->properties->transport_potential;
}

static FLOW_DIR GetBoundaryFlowDirection(struct BeachNode* this)
{
	if (this->next) {
		return this->next->properties->transport_dir;
	}
	return this->prev->properties->transport_dir;
}

static FLOW_DIR GetFlowDirection(struct BeachNode* this)
{
	FLOW_DIR next_dir = this->properties->transport_dir;
	FLOW_DIR prev_dir = this->prev->properties->transport_dir == EMPTY_INT ? this->prev->GetFlowDirection(this->prev) : this->prev->properties->transport_dir;

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
	else if (prev_dir == LEFT)
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
	else
	{
		return next_dir;
	}
}

static struct BeachNode new(double frac_full, int r, int c){
	return (struct BeachNode) {
	.frac_full = frac_full,
		.is_boundary = FALSE,
		.row = r,
		.col = c,
		.next = NULL,
		.prev = NULL,
		.GetRow = &GetRow,
		.GetCol = &GetCol,
		.GetFlowDirection = &GetFlowDirection,
		.GetTransportPotential = &GetTransportPotential,
		.properties = NULL
	};
}

static struct BeachNode* boundary(int r, int c) {
	struct BeachNode* ptr = malloc(sizeof(struct BeachNode));
	struct BeachProperties* props = malloc(sizeof(struct BeachProperties));
	*props = BeachProperties.new();
	*ptr = (struct BeachNode){
		.frac_full = EMPTY_double,
		.is_boundary = TRUE,
		.row = r,
		.col = c,
		.next = NULL,
		.prev = NULL,
		.GetRow = &GetBoundaryRow,
		.GetCol = &GetBoundaryCol,
		.GetFlowDirection = &GetBoundaryFlowDirection,
		.GetTransportPotential = &GetBoundaryTransportPotential,
		.properties = props
	};

	return ptr;
}

/**
* Angle scheme:
* 90: left -> col1 == col2 && r1 > r2
* 0: up (seaward) -> col1 < col2 && r1 == r2
* -90: right -> col1 == col2 && r1 < r2
*/
static double GetAngle(struct BeachNode* node1, struct BeachNode* node2)
{
	double dR = node1->GetRow(node1) - node2->GetRow(node2);
	double dC = node2->GetCol(node2) - node1->GetCol(node1);
	double dF = node2->frac_full - node1->frac_full;

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
	double angle = atan2(dR * g_cell_length, dC * g_cell_width);

	while (angle > PI)
	{
		angle -= 2.0 * PI;
	}
	while (angle < -PI)
	{
		angle += 2.0 * PI;
	}
	return angle;
}

const struct BeachNodeClass BeachNode = { .new = &new,.boundary = &boundary,.GetAngle = &GetAngle };
