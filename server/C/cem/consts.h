#ifndef CEM_CONSTS_INCLUDED
#define CEM_CONSTS_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

// IMPORTANT -- specify wave transformation routine
#undef WITH_SWAN

// Aspect Parameters
// size of cells (meters)
#define CELL_WIDTH (100.)
// number of cells in x (cross-shore) direction
#define X_MAX (103)
// number of cells in y (longshore) direction
#define Y_MAX (110)


// slope of continental shelf
#define SHELF_SLOPE (0.001)
// for now, linear slope of shoreface
#define SHOREFACE_SLOPE (0.01)
// minimum depth of shoreface due to wave action (meters)
#define DEPTH_SHOREFACE (10.)
// cell where intial conditions changes from beach to ocean
#define INIT_BEACH (10)
// cell where initial conditions change from beach to rock LMV
#define INIT_ROCK (0)
// Initial pattern of rock types
#define INITIAL_ROCK_TYPE (1)
// theoretical depth in meters of continental shelf at x = INIT_BEACH
#define INITIAL_DEPTH (10.)


// step size for shadow cell checking
#define ShadowStepDistance (0.2)
// max distance for an obstacle to shadow a cell (km)
#define ShadowMax (10)

typedef enum {
  RIGHT,
  LEFT,
  DIVERGENT,
  CONVERGENT,
  NONE
} FLOW_DIR;


#ifndef TRUE
# define TRUE 1
#endif
#ifndef FALSE
# define FALSE 0
#endif

#ifndef EMPTY_INT
# define EMPTY_INT -9999
#endif
#ifndef EMPTY_DOUBLE
# define EMPTY_DOUBLE -9999.9
#endif

/* Universal Constants */
#define PI (3.1415927)
#define GRAVITY (9.80665)
#define RAD_TO_DEG (180. / PI) /* transform rads to degrees */
#define DEG_TO_RAD (PI / 180.) /* transform degrees to rads */

#if defined(__cplusplus)
}
#endif

#endif
