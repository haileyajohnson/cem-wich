#ifndef CEM_CONSTS_INCLUDED
#define CEM_CONSTS_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

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
