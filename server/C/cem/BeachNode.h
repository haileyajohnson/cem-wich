#ifndef CEM_BEACHNODE_INCLUDED
#define CEM_BEACHNODE_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

#include "consts.h"

struct BeachNode {
    double frac_full, transport_potential, net_volume_change;
    int is_beach, row, col, cell_size;
    FLOW_DIR transport_dir;
    struct BeachNode* next;
    struct BeachNode* prev;
    FLOW_DIR (*GetFlowDirection)(struct BeachNode *this);
};
extern const struct BeachNodeClass {
    struct BeachNode (*new)(double frac_full, int row, int col, int cell_size);
    struct BeachNode (*empty)();
    int (*isEmpty)(struct BeachNode *node);
} BeachNode;

#endif