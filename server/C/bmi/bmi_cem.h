#ifndef BMI_CEM_H_INCLUDED
#define BMI_CEM_H_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

#include "bmi.h"
#include "bmi_EXPORTS.h"

BMI_Model bmi_EXPORT *register_bmi_cem(BMI_Model *model);

typedef struct { double time; } bmi_EXPORT CemModel;

#if defined(__cplusplus)
}
#endif

#endif
