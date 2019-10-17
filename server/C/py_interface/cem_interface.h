#ifndef BMI_CEM_H_INCLUDED
#define BMI_CEM_H_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif


#define SUCCESS (0)
#define FAILURE (1)

#include "cem_EXPORTS.h"

cem_EXPORT int initialize();
cem_EXPORT int update();
cem_EXPORT int finalize();
cem_EXPORT int testFunc(int a, int b);

#if defined(__cplusplus)
}
#endif

#endif
