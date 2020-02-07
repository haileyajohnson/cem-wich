#ifndef UTILS_INCLUDED
#define UTILS_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdlib.h>

void **malloc2d(size_t n_rows, size_t n_cols, size_t itemsize);
void free2d(void **mem);
double RandZeroToOne(void);

#if defined(__cplusplus)
}
#endif

#endif
