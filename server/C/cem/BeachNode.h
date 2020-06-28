#ifndef CEM_BEACHNODE_INCLUDED
#define CEM_BEACHNODE_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

#include "consts.h"
#include "BeachProperties.h"
	
struct BeachNode {
	double frac_full;
	int  is_boundary, row, col;
	int (*GetRow)(struct BeachNode* this);
	int (*GetCol)(struct BeachNode* this);
  struct BeachNode* next;
  struct BeachNode* prev;
  FLOW_DIR (*GetFlowDirection)(struct BeachNode *this);
	double (*GetTransportPotential)(struct BeachNode* this);
	struct BeachProperties* properties;
};
extern const struct BeachNodeClass {
    struct BeachNode (*new)(double frac_full, int row, int col);
		struct BeachNode* (*boundary)(int r, int c);
		double (*GetAngle)(struct BeachNode* node1, struct BeachNode* node2);
} BeachNode;

#if defined(__cplusplus)
}
#endif

#endif