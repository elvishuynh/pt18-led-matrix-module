/* Text rendering */

#ifndef PT18_MATRIX_PT18_MATRIX_TEXT_H_
#define PT18_MATRIX_PT18_MATRIX_TEXT_H_

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

int pt18_matrix_print(const char *str, int offset);

#ifdef __cplusplus
}
#endif

#endif