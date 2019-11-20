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

	int i = 0;
	while (i < grid->num_shorelines)
	{
		struct BeachNode* start = grid->shoreline[i];
		struct BeachNode* curr = start;

		do {
			curr->transport_potential = 0;
			if ((*grid).CheckIfInShadow(grid, curr, wave_angle))
			{
				curr = curr->next;
				continue;
			}

			double alpha_deep;
			double shore_angle = (*grid).GetAngleByDifferencingScheme(grid, curr, wave_angle);
			if (shore_angle == EMPTY_DOUBLE)
			{
				alpha_deep = PI / 4;
			}
			else
			{
				alpha_deep = wave_angle - shore_angle;
			}

			if (fabs(alpha_deep) > (0.995 * PI / 2))
			{
				curr = curr->next;
				continue;
			}

			double local_wave_height = wave_height;
			double c_deep = (GRAVITY * wave_period) / (2 * PI);
			double l_deep = c_deep * wave_period;
			double local_depth = start_depth + refract_step;
			double local_alpha;

			do {
				// iterate
				local_depth -= refract_step;

				// non-iterative eqn or L, from Fenton & McKee
				double wave_length = l_deep * powf(tanh(powf(powf(2.0 * PI / wave_period, 2.0) * (local_depth / GRAVITY), .75)), 2.0 / 3.0);
				double local_c = wave_length / wave_period;

				// n = 1/2(1+2kh/sinh(kh)) Komar 5.21
				// kh = 2 pi depth/L  from k = 2 pi/L
				double kh = 2 * PI * local_depth / wave_length;
				double n = 0.5 * (1 + 2.0 * kh / sinh(2.0 * kh));

				// Calculate angle, assuming shore parallel contours and no conv/div of rays from Komar 5.47
				local_alpha = asin(local_c / c_deep * sin(alpha_deep));

				// Determine wave height from refract calcs, from Komar 5.49
				local_wave_height = wave_height * sqrt(fabs((c_deep * cos(alpha_deep)) / (local_c * 2.0 * n * cos(local_alpha))));

			} while (local_wave_height <= k_break * local_depth && local_depth >= refract_step);

			curr->transport_dir = (wave_angle - (*grid).GetNextAngle(grid, curr)) > 0 ? RIGHT : LEFT;

			// sed transport_potential
			curr->transport_potential = GetTransportVolumePotential(local_alpha, local_wave_height, timestep_length);

			curr = curr->next;
		} while (curr != start && !curr->is_boundary);
		i++;
	}
}

double GetTransportVolumePotential(double alpha, double wave_height, double timestep_length)
{
	int rho = 1020;         // (kg/m^3) density of salt water
	return fabs(1.1 * rho * powf(GRAVITY, 3.0 / 2.0) * powf(wave_height, 5.0 / 2.0) * cos(alpha) * sin(alpha) * timestep_length);
}

void GetAvailableSupply(struct BeachGrid* grid, int ref_pos, double ref_depth, double shelf_slope, double shoreface_slope, double min_depth)
{
	double cell_area = grid->cell_width * grid->cell_length;

	int i = 0;
	while (i < grid->num_shorelines)
	{
		struct BeachNode* start = grid->shoreline[i];

		struct BeachNode* curr = start;

		do {
			double volume_needed_left = 0.0;
			double volume_needed_right = 0.0;

			struct BeachNode* prev = curr->prev;
			if (BeachNode.isEmpty(prev))
			{
				prev = curr;
			}

			FLOW_DIR dir = (*curr).GetFlowDirection(curr);
			switch (dir) {
			case RIGHT:
				volume_needed_right = curr->transport_potential;
				break;
			case DIVERGENT:
				volume_needed_right = curr->transport_potential;
				volume_needed_left = prev->transport_potential;
				break;
			case CONVERGENT:
				break;
			case LEFT:
				volume_needed_left = prev->transport_potential;
				break;
			}

			double total_volume_needed = volume_needed_left + volume_needed_right;
			double shore_angle = (*grid).GetNextAngle(grid, curr);
			double depth = GetDepthOfClosure(curr, ref_pos, ref_depth, shelf_slope, shoreface_slope, shore_angle, min_depth, grid->cell_length);
			double volume_available = curr->frac_full * cell_area * depth;
			struct BeachNode* node_behind = GetNodeInDir(grid, curr, GetDir(shore_angle));
			if (!BeachNode.isEmpty(node_behind))
			{
				volume_available += node_behind->frac_full * cell_area * depth;
			}

			if (total_volume_needed > volume_available)
			{
				curr->prev->transport_potential = total_volume_needed == 0 ? 0.0 : (volume_needed_left / total_volume_needed) * volume_available;
				curr->transport_potential = total_volume_needed == 0 ? 0.0 : (volume_needed_right / total_volume_needed) * volume_available;
			}

			curr = curr->next;
		} while (curr != start && !curr->is_boundary);
		i++;
	}
}

void NetVolumeChange(struct BeachGrid* grid)
{
	int i = 0;
	while (i < grid->num_shorelines)
	{
		struct BeachNode* start = grid->shoreline[i];
		struct BeachNode* curr = start;

		do {
			double volume_in = 0.0;
			double volume_out = 0.0;

			struct BeachNode* prev = curr->prev;
			if (BeachNode.isEmpty(prev))
			{
				prev = curr;
			}

			FLOW_DIR dir = (*curr).GetFlowDirection(curr);
			switch (dir)
			{
			case RIGHT:
				volume_in = prev->transport_potential;
				volume_out = curr->transport_potential;
				break;
			case DIVERGENT:
				volume_out = curr->transport_potential + prev->transport_potential;
				break;
			case CONVERGENT:
				volume_in = curr->transport_potential + prev->transport_potential;
				break;
			case LEFT:
				volume_in = curr->transport_potential;
				volume_out = prev->transport_potential;
				break;
			}
			curr->net_volume_change = volume_in - volume_out;
			curr = curr->next;
		} while (curr != start && !curr->is_boundary);
		i++;
	}
}

void TransportSediment(struct BeachGrid* grid, int ref_pos, double ref_depth, double shelf_slope, double shoreface_slope, double min_depth)
{
	double cell_area = grid->cell_width * grid->cell_length;
	int i = 0;
	while (i < grid->num_shorelines)
	{
		struct BeachNode* start = grid->shoreline[i];
		struct BeachNode* curr = start;

		do {
			double shore_angle = (*grid).GetNextAngle(grid, curr);
			double depth = GetDepthOfClosure(curr, ref_pos, ref_depth, shelf_slope, shoreface_slope, shore_angle, min_depth, grid->cell_length);
			double net_area_change = curr->net_volume_change / depth;
			curr->frac_full = curr->frac_full + net_area_change / cell_area;
			if (curr->frac_full < 0)
			{
				struct BeachNode* temp = OopsImEmpty(grid, curr);
				curr = temp;
			}
			else if (curr->frac_full > 1)
			{
				struct BeachNode* temp = OopsImFull(grid, curr);
				curr = temp;
			}
			curr = curr->next;
		} while (curr != start && !curr->is_boundary);
		i++;
	}
}

void FixBeach(struct BeachGrid* grid)
{
	// smooth corners
	int i = 0;
	while (i < grid->num_shorelines)
	{
		struct BeachNode* start = grid->shoreline[i];
		struct BeachNode* curr = start;

		do {
			struct BeachNode** neighbors = (*grid).Get4Neighbors(grid, curr);

			int needs_fix = TRUE;
			int num_cells = 0;
			int j;
			for (j = 0; j < 4; j++)
			{
				if (BeachNode.isEmpty(neighbors[j])) { continue; }
				if (neighbors[j]->frac_full >= 1.0)
				{
					needs_fix = FALSE;
				}
				else if (neighbors[j]->frac_full > 0.0)
				{
					num_cells++;
				}
			}
			// "floating bit"
			if (needs_fix && num_cells > 0)
			{
				double delta_fill = curr->frac_full / num_cells;
				double cell_area = grid->cell_width * grid->cell_length;
				for (j = 0; j < 4; j++)
				{
					if (BeachNode.isEmpty(neighbors[j])) { continue; }
					if (neighbors[j]->frac_full < 1.0 && neighbors[j]->frac_full > 0.0)
					{

						neighbors[j]->frac_full += delta_fill;
					}
				}

				curr->frac_full = 0;
				curr = (*grid).ReplaceNode(grid, curr, -1);
			}
			curr = curr->next;
			free(neighbors);
		} while (curr != start && !curr->is_boundary);


		// one more pass for over/under-filled cells
		curr = grid->shoreline[i];
		do {
			if (curr->frac_full < 0.0) {
				curr = OopsImEmpty(grid, curr);
			}
			else if (curr->frac_full > 1.0)
			{
				curr = OopsImFull(grid, curr);
			}
			curr = curr->next;
		} while (curr != start && !curr->is_boundary);
		i++;
	}
}

struct BeachNode* OopsImEmpty(struct BeachGrid* grid, struct BeachNode* node)
{
	struct BeachNode** neighbors = (*grid).Get4Neighbors(grid, node);

	int num_cells = 0;
	int none_full = FALSE;
	int i;
	for (i = 0; i < 4; i++)
	{
		if (!BeachNode.isEmpty(neighbors[i]) && neighbors[i]->frac_full >= 1)
		{
			num_cells++;
		}
	}

	if (num_cells == 0)
	{
		none_full = TRUE;
		for (i = 0; i < 4; i++)
		{
			if (!BeachNode.isEmpty(neighbors[i]) && neighbors[i]->frac_full > 0)
			{
				num_cells++;
			}
		}
	}

	double delta_fill = node->frac_full / num_cells;
	double cell_area = grid->cell_width * grid->cell_length;
	for (i = 0; i < 4; i++)
	{
		if (!BeachNode.isEmpty(neighbors[i]) && neighbors[i]->frac_full > 0 && (none_full || neighbors[i]->frac_full >= 1))
		{
			neighbors[i]->frac_full += delta_fill;
		}
	}

	if (num_cells > 0)
	{
		node->frac_full = 0;
	}
	free(neighbors);

	return (*grid).ReplaceNode(grid, node, -1);
}

struct BeachNode* OopsImFull(struct BeachGrid* grid, struct BeachNode* node)
{
	struct BeachNode** neighbors = (*grid).Get4Neighbors(grid, node);

	int num_cells = 0;
	int none_empty = FALSE;
	int i;
	for (i = 0; i < 4; i++)
	{
		if (!BeachNode.isEmpty(neighbors[i]) && neighbors[i]->frac_full <= 0)
		{
			num_cells++;
		}
	}

	if (num_cells == 0)
	{
		none_empty = TRUE;
		for (i = 0; i < 4; i++)
		{
			if (!BeachNode.isEmpty(neighbors[i]) && neighbors[i]->frac_full < 1)
			{
				num_cells++;
			}
		}
	}

	double delta_fill = (node->frac_full - 1) / num_cells;
	double cell_area = grid->cell_width * grid->cell_length;
	for (i = 0; i < 4; i++)
	{
		if (!BeachNode.isEmpty(neighbors[i]) && neighbors[i]->frac_full < 1 && (none_empty || neighbors[i]->frac_full <= 0))
		{
			neighbors[i]->frac_full += delta_fill;
		}
	}

	if (num_cells > 0)
	{
		node->frac_full = 1;
	}
	free(neighbors);

	return (*grid).ReplaceNode(grid, node, 1);
}


/* ---- SEDIMENT TRANSPORT HELPERS ------- */
double GetDepthOfClosure(struct BeachNode* node, int ref_pos, double shelf_depth_at_ref_pos, double shelf_slope, double shoreface_slope, double shore_angle, double min_shelf_depth_at_closure, int cell_length)
{
	int x = node->row;
	// Eq 1
	double local_shelf_depth = shelf_depth_at_ref_pos + ((x - ref_pos) * cell_length * shelf_slope);

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
		row = r + 1;
	}
	else if (sin(dir) < -1e-6)
	{
		row = r - 1;
	}
	else
	{
		row = r;
	}

	return (*grid).TryGetNode(grid, row, col);
}

