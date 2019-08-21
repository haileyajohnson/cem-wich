#ifndef BMI_CEM_H_INCLUDED
#define BMI_CEM_H_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

#include "bmi.h"
#include "bmi_EXPORTS.h"

bmi_EXPORT BMI_Model *register_bmi_cem(BMI_Model *model);

bmi_EXPORT int testFunc(int a, int b);

bmi_EXPORT typedef struct { double time; } CemModel;

#if defined(__cplusplus)
}
#endif

#endif
