/* 17-column × 7-row logic framebuffer on top of the TM1640 16-grid physical driver */

#ifndef PT18_MATRIX_PT18_MATRIX_H_
#define PT18_MATRIX_PT18_MATRIX_H_

#include <zephyr/device.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PT18 LED V0 logical display dimensions */
#define PT18_MATRIX_COLUMNS  17
#define PT18_MATRIX_ROWS     7
#define PT18_SPECIAL_ROW_BIT 0x80

/*
 * Initialize
 * Clears display and turn it on at full brightness
 */
int pt18_matrix_init(void);

/*
 * Each byte represents one columnn. bits 0-6 map to rows 1-7.
 * bit 7 is the Row -1 in my notes indicator LED
 */
int pt18_matrix_write(const uint8_t *buf, size_t len);

/*
 * Clear display
 */
int pt18_matrix_clear(void);

/*
 * Set display brightness 0-7
 */
int pt18_matrix_set_brightness(uint8_t level);

/*
 * Power off display
 * Clears display and turns off brightness
 */
int pt18_matrix_power_off(void);

/*
 * Power on display
 * Turns on display at current brightness level
 */
int pt18_matrix_power_on(void);

#ifdef __cplusplus
}
#endif

#endif
