#ifndef BMI_CEM_H_INCLUDED
#define BMI_CEM_H_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif


#define SUCCESS (0)
#define FAILURE (1)

#include "cem_EXPORTS.h"
#include "cem/config.h"

cem_EXPORT int initialize(Config config);
cem_EXPORT double** update(int saveInterval);
cem_EXPORT int finalize();
cem_EXPORT int testFunc(int a, int b);

#if defined(__cplusplus)
}
#endif

#endif
