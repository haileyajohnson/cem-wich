
#include "BeachNode.h"
#include "consts.h"
#include "utils.h"

static FLOW_DIR GetFlowDirection(struct BeachNode *this)
{   
    if (BeachNode.isEmpty(this->prev) || BeachNode.isEmpty(this->next))
    {
        return EMPTY_INT;
    }

    // get left cell border
    struct BeachNode *leftBorder = this->prev;
    while (!BeachNode.isEmpty(leftBorder->prev) && leftBorder != this && leftBorder->transport_dir == NONE)
    {
        leftBorder = leftBorder->prev;
    }

    //get right cell border
    struct BeachNode *rightBorder = this;
    while (!BeachNode.isEmpty(rightBorder->next) && rightBorder->next != this && rightBorder->transport_dir == NONE)
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

static struct BeachNode new(double frac_full, int r, int c, double cell_width, double cell_length) {
    return (struct BeachNode){
        .frac_full = frac_full,
        .transport_dir = NONE,
        .transport_potential = 0,
        .net_volume_change = 0,
        .is_beach = FALSE,
        .row = r,
        .col = c,
        .cell_width = cell_width,
				.cell_length = cell_length,
        .next = NULL,
        .prev = NULL,
        .GetFlowDirection=&GetFlowDirection
        };
}

static struct BeachNode empty(){
    return (struct BeachNode){        
        .frac_full = EMPTY_DOUBLE,
        .transport_dir = EMPTY_INT,
        .transport_potential = EMPTY_DOUBLE,
        .net_volume_change = EMPTY_DOUBLE,
        .is_beach = EMPTY_INT,
        .row = EMPTY_INT,
        .col = EMPTY_INT,
        .cell_width = EMPTY_DOUBLE,
				.cell_length = EMPTY_DOUBLE,
        .next = NULL,
        .prev = NULL,
        .GetFlowDirection=NULL
        };
}

static int isEmpty(struct BeachNode *node)
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
    || node->GetFlowDirection != NULL)
    {
        return FALSE;
    }
    
    return TRUE;
}

const struct BeachNodeClass BeachNode={.new=&new, .empty=&empty, .isEmpty=&isEmpty};
