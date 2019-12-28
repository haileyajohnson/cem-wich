#ifndef CEM_BEACHNODE_INCLUDED
#define CEM_BEACHNODE_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

#include "consts.h"
	
struct BeachNode {
    float frac_full, transport_potential, net_volume_change, cell_width, cell_length;
		int is_beach, is_boundary, row, col;
		int (*GetRow)(struct BeachNode* this);
		int (*GetCol)(struct BeachNode* this);
    FLOW_DIR transport_dir;
    struct BeachNode* next;
    struct BeachNode* prev;
    FLOW_DIR (*GetFlowDirection)(struct BeachNode *this);
		float (*GetTransportPotential)(struct BeachNode* this);
};
extern const struct BeachNodeClass {
    struct BeachNode (*new)(float frac_full, int row, int col, float cell_width, float cell_length);
    struct BeachNode (*empty)();
		struct BeachNode* (*boundary)(int r, int c);
    int (*isEmpty)(struct BeachNode *node);
		float (*GetAngle)(struct BeachNode* node1, struct BeachNode* node2);
} BeachNode;

#if defined(__cplusplus)
}
#endif

#endif