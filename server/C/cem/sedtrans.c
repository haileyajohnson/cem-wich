#include "sedtrans.h"
#include "consts.h"
#include <stdlib.h>
#include <math.h>
#include "BeachNode.h"
#include "BeachGrid.h"


float GetTransportVolumePotential(float alpha, float wave_height, float timestep_length);
void OopsImEmpty(struct BeachGrid* grid, struct BeachNode* node);
void OopsImFull(struct BeachGrid* grid, struct BeachNode* node);
float GetDepthOfClosure(struct BeachNode* node, int ref_pos, float shelf_depth_at_ref_pos, float shelf_slope, float shoreface_slope, float shore_angle, float min_shelf_depth_at_closure, int cell_size);
float ModTowardZero(float a, float m);
float RoundRadians(float angle, float round_to, float bias);
float GetDir(float shore_angle);
struct BeachNode* GetNodeInDir(struct BeachGrid* grid, struct BeachNode* node, float dir);


void WaveTransformation(struct BeachGrid* grid, float wave_angle, float wave_period, float wave_height, float timestep_length)
{
	float start_depth = 3 * wave_height;       // (meters) depth to begin refraction calculations
	float refract_step = 0.2;                  // (meters) step size to iterate through depth
	float k_break = 0.5;                       // coefficient such that waves break at Hs > k_break*depth

	int i = 0;
	while (i < grid->num_shorelines)
	{
		struct BeachNode* start = grid->shoreline[i];
		struct BeachNode* curr = start;

		do {
			curr->transport_potential = 0;

			float alpha_deep;
			float shore_angle = (*grid).GetAngleByDifferencingScheme(grid, curr, wave_angle);
			alpha_deep = wave_angle - shore_angle;

			if (fabs(alpha_deep) > (0.995 * PI / 2))
			{
				curr = curr->next;
				continue;
			}

			float local_wave_height = wave_height;
			float c_deep = (GRAVITY * wave_period) / (2 * PI);
			float l_deep = c_deep * wave_period;
			float local_depth = start_depth + refract_step;
			float local_alpha;

			do {
				// iterate
				local_depth -= refract_step;

				// non-iterative eqn or L, from Fenton & McKee
				float wave_length = l_deep * powf(tanh(powf(powf(2.0 * PI / wave_period, 2.0) * (local_depth / GRAVITY), .75)), 2.0 / 3.0);
				float local_c = wave_length / wave_period;

				// n = 1/2(1+2kh/sinh(kh)) Komar 5.21
				// kh = 2 pi depth/L  from k = 2 pi/L
				float kh = 2 * PI * local_depth / wave_length;
				float n = 0.5 * (1 + 2.0 * kh / sinh(2.0 * kh));

				// Calculate angle, assuming shore parallel contours and no conv/div of rays from Komar 5.47
				local_alpha = asin(local_c / c_deep * sin(alpha_deep));

				// Determine wave height from refract calcs, from Komar 5.49
				local_wave_height = wave_height * sqrt(fabs((c_deep * cos(alpha_deep)) / (local_c * 2.0 * n * cos(local_alpha))));

			} while (local_wave_height <= k_break * local_depth && local_depth >= refract_step);
			
			// sed transport_potential
			curr->transport_potential = GetTransportVolumePotential(local_alpha, local_wave_height, timestep_length);

			curr = curr->next;
		} while (curr != start && !curr->is_boundary);
		i++;
	}
}

float GetTransportVolumePotential(float alpha, float wave_height, float timestep_length)
{
	int rho = 1020;         // (kg/m^3) density of salt water
	return fabs(1.1 * rho * powf(GRAVITY, 3.0 / 2.0) * powf(wave_height, 5.0 / 2.0) * cos(alpha) * sin(alpha) * timestep_length);
}

void GetAvailableSupply(struct BeachGrid* grid, int ref_pos, float ref_depth, float shelf_slope, float shoreface_slope, float min_depth)
{
	float cell_area = grid->cell_width * grid->cell_length;

	int i = 0;
	while (i < grid->num_shorelines)
	{
		struct BeachNode* start = grid->shoreline[i];

		struct BeachNode* curr = start;

		do {
			float volume_needed_left = 0.0;
			float volume_needed_right = 0.0;

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

			float total_volume_needed = volume_needed_left + volume_needed_right;
			float shore_angle = (*grid).GetNextAngle(grid, curr);
			float depth = GetDepthOfClosure(curr, ref_pos, ref_depth, shelf_slope, shoreface_slope, shore_angle, min_depth, grid->cell_length);
			float volume_available = curr->frac_full * cell_area * depth;
			struct BeachNode* node_behind = GetNodeInDir(grid, curr, GetDir(shore_angle));
			if (!BeachNode.isEmpty(node_behind) && node_behind->frac_full >= 1.0)
			{
				volume_available += node_behind->frac_full * cell_area * depth;
			}

			if (total_volume_needed > volume_available)
			{
				if (dir == DIVERGENT)
				{
					curr->prev->transport_potential = total_volume_needed == 0 ? 0.0 : (volume_needed_left / total_volume_needed) * volume_available;
					curr->transport_potential = total_volume_needed == 0 ? 0.0 : (volume_needed_right / total_volume_needed) * volume_available;
				}
				else if (dir == RIGHT)
				{
					volume_available += prev->GetTransportPotential(prev);
					curr->transport_potential = volume_available < curr->GetTransportPotential(curr) ? volume_available : curr->GetTransportPotential(curr);
				}
				else if (dir == LEFT)
				{
					volume_available += curr->GetTransportPotential(curr);
					prev->transport_potential = volume_available < prev->GetTransportPotential(prev) ? volume_available : prev->GetTransportPotential(prev);
				}
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
			float volume_in = 0.0;
			float volume_out = 0.0;

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
			curr->net_volume_change = volume_in - volume_out;
			curr = curr->next;
		} while (curr != start && !curr->is_boundary);
		i++;
	}
}

int TransportSediment(struct BeachGrid* grid, int ref_pos, float ref_depth, float shelf_slope, float shoreface_slope, float min_depth)
{

	int FIND_BEACH_FLAG = FALSE;
	float cell_area = grid->cell_width * grid->cell_length;
	int i = 0;
	while (i < grid->num_shorelines)
	{
		struct BeachNode* start = grid->shoreline[i];
		struct BeachNode* curr = start;

		do {
			float shore_angle = (*grid).GetNextAngle(grid, curr);
			float depth = GetDepthOfClosure(curr, ref_pos, ref_depth, shelf_slope, shoreface_slope, shore_angle, min_depth, grid->cell_length);
			float net_area_change = curr->net_volume_change / depth;
			curr->frac_full = curr->frac_full + net_area_change / cell_area;
			if (curr->frac_full < -0.000001)
			{
				OopsImEmpty(grid, curr);
				FIND_BEACH_FLAG = TRUE;
			}
			else if (curr->frac_full > 1)
			{
				OopsImFull(grid, curr);
				FIND_BEACH_FLAG = TRUE;
			}
			else if (curr->frac_full == 0)
			{
				FIND_BEACH_FLAG = TRUE;
			}
			curr = curr->next;
		} while (curr != start && !curr->is_boundary);
		i++;
	}
	return FIND_BEACH_FLAG;
}

void OopsImEmpty(struct BeachGrid* grid, struct BeachNode* node)
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

	float delta_fill = node->frac_full / num_cells;
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

	// recurse through neighbors
	for (i = 0; i < 4; i++)
	{
		if (!BeachNode.isEmpty(neighbors[i]) && neighbors[i]->frac_full < -0.000001)
		{
			OopsImEmpty(grid, neighbors[i]);
		}
	}
	free(neighbors);

	return;
}

void OopsImFull(struct BeachGrid* grid, struct BeachNode* node)
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

	float delta_fill = (node->frac_full - 1) / num_cells;
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

	// recurse through neighbors
	for (i = 0; i < 4; i++)
	{
		if (!BeachNode.isEmpty(neighbors[i]) && neighbors[i]->frac_full > 1)
		{
			OopsImFull(grid, neighbors[i]);
		}
	}

	free(neighbors);

	return;
}

int FixBeach(struct BeachGrid* grid)
{
	int FIND_BEACH_FLAG = FALSE;
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
				else if (neighbors[j]->frac_full > 0)
				{
					num_cells++;
				}
			}

			if (needs_fix) // no solid land in neighborhood
			{
				// distribute to beach neighbors
				if (num_cells > 0)
				{
					float delta_fill = curr->frac_full / num_cells;
					curr->frac_full = 0;
					for (j = 0; j < 4; j++)
					{
						if (BeachNode.isEmpty(neighbors[j])) { continue; }
						if (neighbors[j]->frac_full < 1.0 && neighbors[j]->frac_full > 0)
						{
							neighbors[j]->frac_full += delta_fill;
							if (neighbors[j]->frac_full > 1.0)
							{
								OopsImFull(grid, neighbors[j]);
								FIND_BEACH_FLAG = TRUE;
							}
						}
					}
				}
				else // push sed back to shoreline
				{
					int r = curr->GetRow(curr) + 1;
					int c = curr->GetCol(curr);
					struct BeachNode* temp = (*grid).TryGetNode(grid, r, c);

					while (!BeachNode.isEmpty(temp) && !(temp->frac_full > 0))
					{
						r--;
						temp = (*grid).TryGetNode(grid, r, c);
					}
					if (!BeachNode.isEmpty(temp))
					{
						temp->frac_full == curr->frac_full;
						curr->frac_full = 0;
						if (temp->frac_full > 1.0)
						{
							OopsImFull(grid, temp);
							FIND_BEACH_FLAG = TRUE;
						}
					}
				}
			}
			curr = curr->next;
			free(neighbors);
		} while (curr != start && !curr->is_boundary);


		// one more pass for over/under-filled cells
		curr = grid->shoreline[i];
		do {
			if (curr->frac_full < -0.000001) {
				OopsImEmpty(grid, curr);
				FIND_BEACH_FLAG = TRUE;
			}
			else if (curr->frac_full > 1.0)
			{
				OopsImFull(grid, curr);
				FIND_BEACH_FLAG = TRUE;
			}
			curr = curr->next;
		} while (curr != start && !curr->is_boundary);
		i++;
	}
	return FIND_BEACH_FLAG;
}


/* ---- SEDIMENT TRANSPORT HELPERS ------- */
float GetDepthOfClosure(struct BeachNode* node, int ref_pos, float shelf_depth_at_ref_pos, float shelf_slope, float shoreface_slope, float shore_angle, float min_shelf_depth_at_closure, int cell_length)
{
	int x = node->row;
	// Eq 1
	float local_shelf_depth = shelf_depth_at_ref_pos + ((ref_pos - x) * cell_length * shelf_slope);

	// Eq 2
	float cross_shore_distance_to_closure = local_shelf_depth / (shoreface_slope - (cos(shore_angle) * shelf_slope));

	// Eq 3
	float cross_shore_pos_of_closure = x + cos(shore_angle) * cross_shore_distance_to_closure / cell_length;

	// Eq 4
	float closure_depth = shelf_depth_at_ref_pos + (cross_shore_pos_of_closure - ref_pos) * cell_length * shelf_slope;
	if (closure_depth < min_shelf_depth_at_closure)
	{
		closure_depth = min_shelf_depth_at_closure;
	}

	return closure_depth;
}

float ModTowardZero(float a, float m)
{
	int sign = a / fabs(a);
	a = fabs(a);
	int c = floor(a / m);
	c = c * sign;
	a = a * sign;
	return a - m * c;
}

float RoundRadians(float angle, float round_to, float bias)
{
	float m90 = ModTowardZero(angle, PI / 2);
	float m180 = ModTowardZero(angle, PI);

	if (m90 != m180)
	{
		bias = (PI / 2) - bias;
	}

	float rounded_angle = angle - m90;
	if (fabs(m90) >= bias)
	{
		rounded_angle += (m90 / fabs(m90)) * (PI / 2);
	}
	return rounded_angle;
}

float GetDir(float shore_angle)
{
	float shore_normal = shore_angle - PI / 2;
	return RoundRadians(shore_normal, PI / 2, PI / 6);
}

struct BeachNode* GetNodeInDir(struct BeachGrid* grid, struct BeachNode* node, float dir)
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

