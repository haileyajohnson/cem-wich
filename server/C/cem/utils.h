#ifndef UTILS_INCLUDED
#define UTILS_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdlib.h>


void **malloc2d(size_t n_rows, size_t n_cols, size_t itemsize);
void free2d(void **mem);
float RandZeroToOne(void);
int *reverse(int *array, size_t n_cols);
int *rotate(int *array, size_t offset, size_t n_cols);


#if defined(__cplusplus)
}
#endif

#endif
