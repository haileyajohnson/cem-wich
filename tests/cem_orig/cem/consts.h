#ifndef CEM_CONSTS_INCLUDED
#define CEM_CONSTS_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

// Aspect Parameters
// number of cells in x (cross-shore) direction
#define X_MAX (50)
// number of cells in y (longshore) direction
#define Y_MAX (100)

// maximum length of arrays that contain beach data at each time step
#define MaxBeachLength (8 * Y_MAX)

// if we run off of array, how far over do we try again?
#define FindCellError (5)
// step size for shadow cell checking
#define ShadowStepDistance (0.2)

#ifndef TRUE
# define TRUE 1
#endif
#ifndef FALSE
# define FALSE 0
#endif

// random seed:  control value = 1 completely random = -999
#define SEED (1973)

/* Universal Constants */
#define PI (3.1415927)
#define GRAVITY (9.80665)
#define RAD_TO_DEG (180. / PI) /* transform rads to degrees */

#if defined(__cplusplus)
}
#endif

#endif
