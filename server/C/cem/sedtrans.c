#include "sedtrans.h"
#include "consts.h"
#include <stdlib.h>
#include <math.h>
#include "BeachNode.h"
#include "BeachGrid.h"


double GetTransportVolumePotential(double alpha, double wave_height, double timestep_length);
struct BeachNode* OopsImEmpty(struct BeachGrid* grid, struct BeachNode* node);
struct BeachNode* OopsImFull(struct BeachGrid* grid, struct BeachNode* node);
double GetDepthOfClosure(struct BeachNode* node, int ref_pos, double shelf_depth_at_ref_pos, double shelf_slope, double shoreface_slope, double shore_angle, double min_shelf_depth_at_closure, int cell_size);
double ModTowardZero(double a, double m);
double RoundRadians(double angle, double round_to, double bias);
double GetDir(double shore_angle);
struct BeachNode* GetNodeInDir(struct BeachGrid* grid, struct BeachNode* node, double dir);


void WaveTransformation(struct BeachGrid* grid, double wave_angle, double wave_period, double wave_height, double timestep_length)
{
	double start_depth = 3 * wave_height;       // (meters) depth to begin refraction calculations
	double refract_step = 0.2;                  // (meters) step size to iterate through depth
	double k_break = 0.5;                       // coefficient such that waves break at Hs > k_break*depth

	struct BeachNode* curr = grid->shoreline;

	while (!curr->is_boundary)
	{
		curr->properties->transport_potential = 0;

		double alpha_deep;
		double shore_angle = (*grid).GetAngleByDifferencingScheme(grid, curr, wave_angle);
		alpha_deep = abs(shore_angle - EMPTY_double) < 1 ? PI / 4 : wave_angle - shore_angle;

		if (fabs(alpha_deep) > (0.995 * PI / 2))
		{
			curr = curr->next;
			continue;
		}

		double local_wave_height = wave_height;
		double c_deep = (GRAVITY * wave_period) / (2 * PI);
		double l_deep = c_deep * wave_period;
		double local_depth = start_depth;
		double local_alpha;

		while (TRUE) {
			// non-iterative eqn or L, from Fenton & McKee
			double wave_length = l_deep * powf(tanh(powf(powf(2.0 * PI / wave_period, 2.0) * local_depth / GRAVITY, .75)), 2.0 / 3.0);
			double local_c = wave_length / wave_period;

			// n = 1/2(1+2kh/sinh(kh)) Komar 5.21
			// kh = 2 pi depth/L  from k = 2 pi/L
			double kh = 2 * PI * local_depth / wave_length;
			double n = 0.5 * (1 + 2.0 * kh / sinh(2.0 * kh));

			// Calculate angle, assuming shore parallel contours and no conv/div of rays from Komar 5.47
			local_alpha = asin(local_c / c_deep * sin(alpha_deep));

			// Determine wave height from refract calcs, from Komar 5.49
			local_wave_height = wave_height * sqrtf(fabs((c_deep * cos(alpha_deep)) / (local_c * 2.0 * n * cos(local_alpha))));

			// wave break condition
			if (local_wave_height > k_break * local_depth || local_depth <= refract_step)
			{
				break;
			}
			// iterate
			local_depth -= refract_step;
		}

		// sed transport_potential
		curr->properties->transport_potential = GetTransportVolumePotential(local_alpha, local_wave_height, timestep_length);

		curr = curr->next;
	}
}

double GetTransportVolumePotential(double alpha, double wave_height, double timestep_length)
{
	int rho = 1020;         // (kg/m^3) density of salt water
	return fabs(.67 * rho * powf(GRAVITY, 3.0 / 2.0) * powf(wave_height, 5.0 / 2.0) * cos(alpha) * sin(alpha) * timestep_length);
}

void GetAvailableSupply(struct BeachGrid* grid, int ref_pos, double ref_depth, double shelf_slope, double shoreface_slope, double min_depth, double depthOfClosure)
{
	double cell_area = g_cell_width * g_cell_length;

	struct BeachNode* curr = grid->shoreline;

	while (!curr->is_boundary)
	{
		double volume_needed_left = 0.0;
		double volume_needed_right = 0.0;

		struct BeachNode* prev = curr->prev;

		FLOW_DIR dir = (*curr).GetFlowDirection(curr);
		switch (dir) {
		case RIGHT:
			volume_needed_right = curr->GetTransportPotential(curr);
			break;
		case DIVERGENT:
			volume_needed_right = curr->GetTransportPotential(curr);
			volume_needed_left = prev->GetTransportPotential(prev);
			break;
		case CONVERGENT:
			break;
		case LEFT:
			volume_needed_left = prev->GetTransportPotential(prev);
			break;
		}

		double total_volume_needed = volume_needed_left + volume_needed_right;
		double shore_angle = (*grid).GetNextAngle(grid, curr);
		double depth = depthOfClosure ? depthOfClosure : GetDepthOfClosure(curr, ref_pos, ref_depth, shelf_slope, shoreface_slope, shore_angle, min_depth, g_cell_length);
		double volume_available = curr->frac_full * cell_area * depth;
		struct BeachNode* node_behind = GetNodeInDir(grid, curr, GetDir(shore_angle));
		if (node_behind && node_behind->frac_full >= 1.0)
		{
			volume_available += node_behind->frac_full * cell_area * depth;
		}

		if (total_volume_needed > volume_available)
		{
			if (dir == DIVERGENT)
			{
				curr->prev->properties->transport_potential = total_volume_needed == 0 ? 0.0 : (volume_needed_left / total_volume_needed) * volume_available;
				curr->properties->transport_potential = total_volume_needed == 0 ? 0.0 : (volume_needed_right / total_volume_needed) * volume_available;
			}
			else if (dir == RIGHT)
			{
				volume_available += prev->GetTransportPotential(prev);
				curr->properties->transport_potential = volume_available < curr->GetTransportPotential(curr) ? volume_available : curr->GetTransportPotential(curr);
			}
			else if (dir == LEFT)
			{
				volume_available += curr->GetTransportPotential(curr);
				prev->properties->transport_potential = volume_available < prev->GetTransportPotential(prev) ? volume_available : prev->GetTransportPotential(prev);
			}
		}

		curr = curr->next;
	}
}

void NetVolumeChange(struct BeachGrid* grid)
{
	struct BeachNode* curr = grid->shoreline;

	while (!curr->is_boundary) {
		double volume_in = 0.0;
		double volume_out = 0.0;

		struct BeachNode* prev = curr->prev;

		FLOW_DIR dir = (*curr).GetFlowDirection(curr);
		switch (dir)
		{
		case RIGHT:
			volume_in = prev->GetTransportPotential(prev);
			volume_out = curr->GetTransportPotential(curr);
			break;
		case DIVERGENT:
			volume_out = curr->GetTransportPotential(curr) + prev->GetTransportPotential(prev);
			break;
		case CONVERGENT:
			volume_in = curr->GetTransportPotential(curr) + prev->GetTransportPotential(prev);
			break;
		case LEFT:
			volume_in = curr->GetTransportPotential(curr);
			volume_out = prev->GetTransportPotential(prev);
			break;
		}
		curr->properties->net_volume_change = volume_in - volume_out;
		curr = curr->next;
	}
}

void TransportSediment(struct BeachGrid* grid, int ref_pos, double ref_depth, double shelf_slope, double shoreface_slope, double min_depth, double depthOfClosure)
{
	double cell_area = g_cell_width * g_cell_length;
	struct BeachNode* curr = grid->shoreline;

	while (!curr->is_boundary) {
		double shore_angle = (*grid).GetNextAngle(grid, curr);
		double depth = depthOfClosure ? depthOfClosure : GetDepthOfClosure(curr, ref_pos, ref_depth, shelf_slope, shoreface_slope, shore_angle, min_depth, g_cell_length);
		double net_area_change = curr->properties->net_volume_change / depth;
		curr->frac_full = curr->frac_full + net_area_change / cell_area;
		if (curr->frac_full < 0.0)
		{
			 curr = OopsImEmpty(grid, curr);
		}
		else if (curr->frac_full > 1.0)
		{
			curr = OopsImFull(grid, curr);
		}
		else
		{
			curr = curr->next;
		}
	}
}

struct BeachNode* OopsImEmpty(struct BeachGrid* grid, struct BeachNode* node)
{
	if (node->frac_full >= -0.000001)
	{
		node->frac_full = 0.0;
		return (*grid).ReplaceNode(grid, node);
	}
	struct BeachNode** neighbors = (*grid).Get4Neighbors(grid, node);

	int num_cells = 0;
	double total_sed = 0.0;
	int none_full = FALSE;
	int i;
	for (i = 0; i < 4; i++)
	{
		if (neighbors[i] && neighbors[i]->frac_full >= 1.0)
		{
			num_cells++;
			total_sed += neighbors[i]->frac_full;
		}
	}

	if (num_cells == 0)
	{
		none_full = TRUE;
		for (i = 0; i < 4; i++)
		{
			if (neighbors[i] && neighbors[i]->frac_full > 0.0)
			{
				num_cells++;
				total_sed += neighbors[i]->frac_full;
			}
		}
	}

	for (i = 0; i < 4; i++)
	{
		struct BeachNode* neighbor = neighbors[i];
		if (neighbor && neighbor->frac_full > 0.0 && (none_full || neighbor->frac_full >= 1.0))
		{
			double percent_available = neighbor->frac_full / total_sed;
			double delta_fill = node->frac_full * percent_available;
			neighbor->frac_full += delta_fill;
			if (neighbor->is_boundary) {
				free(neighbor->properties);
				free(neighbor);
			}
		}
	}

	if (num_cells > 0)
	{
		node->frac_full = 0;
	}
	free(neighbors);

	return (*grid).ReplaceNode(grid, node);
}

struct BeachNode* OopsImFull(struct BeachGrid* grid, struct BeachNode* node)
{
	struct BeachNode** neighbors = (*grid).Get4Neighbors(grid, node);

	int num_cells = 0;
	double total_space = 0.0;
	int none_empty = FALSE;
	int i;
	for (i = 0; i < 4; i++)
	{
		if (neighbors[i] && neighbors[i]->frac_full <= 0.0)
		{
			num_cells++;
			total_space += (1 - neighbors[i]->frac_full);
		}
	}

	if (num_cells == 0)
	{
		none_empty = TRUE;
		for (i = 0; i < 4; i++)
		{
			if (neighbors[i] && neighbors[i]->frac_full < 1.0)
			{
				num_cells++;
				total_space += (1 - neighbors[i]->frac_full);
			}
		}
	}

	for (i = 0; i < 4; i++)
	{
		struct BeachNode* neighbor = neighbors[i];
		if (neighbor && neighbor->frac_full < 1.0 && (none_empty || neighbor->frac_full <= 0.0))
		{
			double percent_available = (1 - neighbor->frac_full) / total_space;
			double delta_fill = (node->frac_full - 1) * percent_available;
			neighbor->frac_full += delta_fill;
			if (neighbor->is_boundary) {
				free(neighbor->properties);
				free(neighbor);
			}
		}
	}

	if (num_cells > 0)
	{
		node->frac_full = 1;
	}

	free(neighbors);

	return (*grid).ReplaceNode(grid, node);
}

void FixBeach(struct BeachGrid* grid)
{
	int done = TRUE;
	struct BeachNode* curr = grid->shoreline;

	// pass for over/under-filled cells
	while (!curr->is_boundary)
	{
		if (curr->frac_full < 0.0) {
			curr = OopsImEmpty(grid, curr);
			done = FALSE;
		}
		else if (curr->frac_full > 1.0)
		{
			curr = OopsImFull(grid, curr);
			done = FALSE;
		}
		else
		{
			curr = curr->next;
		}
	}

	curr = grid->shoreline;

	// smooth outset corners
	while (!curr->is_boundary)
	{
		struct BeachNode** neighbors = (*grid).Get4Neighbors(grid, curr);

		int num_cells = 0;
		double total_space = 0.0;
		int needs_fix = TRUE;
		int j;
		for (j = 0; j < 4; j++)
		{
			if (!neighbors[j]) { continue; }
			if (neighbors[j]->frac_full >= 1.0)
			{
				needs_fix = FALSE;
				break;
			}
			else if (neighbors[j]->frac_full > 0.0)
			{
				num_cells++;
				total_space += (1 - neighbors[j]->frac_full);
			}
		}

		if (needs_fix)
		{
			// distribute to beach neighbors
			if (num_cells > 0)
			{
				double delta_fill = min(curr->frac_full, total_space);
				curr->frac_full -= delta_fill;
				for (j = 0; j < 4; j++)
				{
					struct BeachNode* neighbor = neighbors[j];
					if (neighbor) { continue; }
					if (neighbor->frac_full < 1.0 && neighbor->frac_full > 0.0)
					{
						double percent_fill = delta_fill * ((1 - neighbor->frac_full) / total_space);
						neighbor->frac_full += percent_fill;
						if (neighbor->is_boundary) {
							free(neighbor->properties);
							free(neighbor);
						}
					}
				}
				if (curr->frac_full <= 0.0)
				{
					done = FALSE;
					curr = (*grid).ReplaceNode(grid, curr);
					curr = curr->prev; // backtrack
				}
			}
			else // error
			{
				free(neighbors);
				return;
			}
		}
		curr = curr->next;
		free(neighbors);
	}

	// remove inset corners
	curr = grid->shoreline;
	while (!curr->is_boundary)
	{
		struct BeachNode* prev = curr->prev;
		struct BeachNode* next = curr->next;
		if (abs(next->GetRow(next) - prev->GetRow(prev)) == 1 && abs(next->GetCol(next) - prev->GetCol(prev)) == 1)
		{
			done = FALSE;
			if (curr == grid->shoreline)
			{
				grid->shoreline = next;
			}
			prev->next = next;
			next->prev = prev;
			curr->next = NULL;
			curr->prev = NULL;
			free(curr->properties);
			curr->properties = NULL;
			curr = prev;
		}
		curr = curr->next;
	}

	if (!done)
	{
		FixBeach(grid);
	}
}


/* ---- SEDIMENT TRANSPORT HELPERS ------- */
double GetDepthOfClosure(struct BeachNode* node, int ref_pos, double shelf_depth_at_ref_pos, double shelf_slope, double shoreface_slope, double shore_angle, double min_shelf_depth_at_closure, int cell_length)
{
	int x = node->row;
	// Eq 1
	double local_shelf_depth = shelf_depth_at_ref_pos + ((ref_pos - x) * cell_length * shelf_slope);

	// Eq 2
	double cross_shore_distance_to_closure = local_shelf_depth / (shoreface_slope - (cos(shore_angle) * shelf_slope));

	// Eq 3
	double cross_shore_pos_of_closure = x + cos(shore_angle) * cross_shore_distance_to_closure / cell_length;

	// Eq 4
	double closure_depth = shelf_depth_at_ref_pos + (cross_shore_pos_of_closure - ref_pos) * cell_length * shelf_slope;
	if (closure_depth < min_shelf_depth_at_closure)
	{
		closure_depth = min_shelf_depth_at_closure;
	}

	return closure_depth;
}

double ModTowardZero(double a, double m)
{
	int sign = a / fabs(a);
	a = fabs(a);
	int c = floor(a / m);
	c = c * sign;
	a = a * sign;
	return a - m * c;
}

double RoundRadians(double angle, double round_to, double bias)
{
	double m90 = ModTowardZero(angle, PI / 2);
	double m180 = ModTowardZero(angle, PI);

	if (m90 != m180)
	{
		bias = (PI / 2) - bias;
	}

	double rounded_angle = angle - m90;
	if (fabs(m90) >= bias)
	{
		rounded_angle += (m90 / fabs(m90)) * (PI / 2);
	}
	return rounded_angle;
}

double GetDir(double shore_angle)
{
	double shore_normal = shore_angle - PI / 2;
	return RoundRadians(shore_normal, PI / 2, PI / 6);
}

struct BeachNode* GetNodeInDir(struct BeachGrid* grid, struct BeachNode* node, double dir)
{
	int r = node->row;
	int c = node->col;

	int row, col;

	if (cos(dir) > 1e-6)
	{
		col = c - 1;
	}
	else if (cos(dir) < -1e-6)
	{
		col = c + 1;
	}
	else
	{
		col = c;
	}

	if (sin(dir) > 1e-6)
	{
		row = r - 1;
	}
	else if (sin(dir) < -1e-6)
	{
		row = r + 1;
	}
	else
	{
		row = r;
	}

	return (*grid).TryGetNode(grid, row, col);
}

