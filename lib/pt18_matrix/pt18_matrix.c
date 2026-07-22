/* PT18 uses 16 grids for columns 0-15 skipping logical column 5
 * stitches logical column 5 the 17th column across the unused bit 7
 * positions of grids 8-15
 */

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <tm1640/tm1640.h>
#include <pt18_matrix/pt18_matrix.h>

LOG_MODULE_REGISTER(pt18_matrix, CONFIG_LOG_DEFAULT_LEVEL);

/* bit masks for logical to physical column mapping */
#define PT18_STANDARD_ROW_MASK  0x7F  /* bits 0-6 standard 7 rows */

/* individual row bit positions for flip */
#define PT18_ROW_1_BIT  0x01
#define PT18_ROW_2_BIT  0x02
#define PT18_ROW_3_BIT  0x04
#define PT18_ROW_4_BIT  0x08
#define PT18_ROW_5_BIT  0x10
#define PT18_ROW_6_BIT  0x20
#define PT18_ROW_7_BIT  0x40

/* physical grid layout */
#define PT18_SPLIT_COL            5   /* logical col scattered across bit 7 */
#define PT18_SPLIT_GRID_START     8
#define PT18_SPLIT_GRID_END      15
#define PT18_SPECIAL_ROW_COL_A   11   /* logical col whose bit 7 maps to grid 5 */
#define PT18_SPECIAL_ROW_GRID_A   5

static K_MUTEX_DEFINE(pt18_mutex);
static const struct device *tm_dev;

/*
 * mirrors the standard 7 row bits 0-6 to fix font orientation
 * preserves the special row -1 LED on bit 7
 */
static uint8_t flip_logical_rows(uint8_t col_data)
{
	uint8_t flipped = 0;

	if (col_data & PT18_ROW_1_BIT) flipped |= PT18_ROW_7_BIT;
	if (col_data & PT18_ROW_2_BIT) flipped |= PT18_ROW_6_BIT;
	if (col_data & PT18_ROW_3_BIT) flipped |= PT18_ROW_5_BIT;
	if (col_data & PT18_ROW_4_BIT) flipped |= PT18_ROW_4_BIT; /* center stays */
	if (col_data & PT18_ROW_5_BIT) flipped |= PT18_ROW_3_BIT;
	if (col_data & PT18_ROW_6_BIT) flipped |= PT18_ROW_2_BIT;
	if (col_data & PT18_ROW_7_BIT) flipped |= PT18_ROW_1_BIT;

	flipped |= (col_data & PT18_SPECIAL_ROW_BIT); /* preserve row -1 */

	return flipped;
}

/*
 * maps 17 logical columns to 16 physical grids for PT18 LED V0
 * cols 0-4   > grids 15-11 reversed bits 0-6
 * cols 6-16  > grids 10-0 reversed bits 0-6
 * col 5      > scattered across bit 7 of grids 8-15
 * col 11 bit 7 > grid 5 bit 7
 */
static void pt18_map_logical_to_physical(const uint8_t *logical, uint8_t *physical)
{
	memset(physical, 0, TM1640_NUM_GRIDS);

	uint8_t flipped[PT18_MATRIX_COLUMNS];
	for (int i = 0; i < PT18_MATRIX_COLUMNS; i++) {
		flipped[i] = flip_logical_rows(logical[i]);
	}

	/* standard columns map to grids in reverse order */
	for (int i = 0; i < PT18_MATRIX_COLUMNS; i++) {
		if (i < PT18_SPLIT_COL) {
			physical[PT18_SPLIT_GRID_END - i] =
				flipped[i] & PT18_STANDARD_ROW_MASK;
		} else if (i > PT18_SPLIT_COL) {
			physical[TM1640_NUM_GRIDS - i] =
				flipped[i] & PT18_STANDARD_ROW_MASK;
		}
	}

	/* col 12 row -1 maps to grid 5 bit 7 */
	if (flipped[PT18_SPECIAL_ROW_COL_A] & PT18_SPECIAL_ROW_BIT) {
		physical[PT18_SPECIAL_ROW_GRID_A] |= PT18_SPECIAL_ROW_BIT;
	}

	/* col 6 scattered across bit 7 of grids 8-15 */
	if (flipped[PT18_SPLIT_COL] & PT18_SPECIAL_ROW_BIT)
		physical[PT18_SPLIT_GRID_START]     |= PT18_SPECIAL_ROW_BIT;
	if (flipped[PT18_SPLIT_COL] & PT18_ROW_1_BIT)
		physical[PT18_SPLIT_GRID_START + 1] |= PT18_SPECIAL_ROW_BIT;
	if (flipped[PT18_SPLIT_COL] & PT18_ROW_2_BIT)
		physical[PT18_SPLIT_GRID_START + 2] |= PT18_SPECIAL_ROW_BIT;
	if (flipped[PT18_SPLIT_COL] & PT18_ROW_3_BIT)
		physical[PT18_SPLIT_GRID_START + 3] |= PT18_SPECIAL_ROW_BIT;
	if (flipped[PT18_SPLIT_COL] & PT18_ROW_4_BIT)
		physical[PT18_SPLIT_GRID_START + 4] |= PT18_SPECIAL_ROW_BIT;
	if (flipped[PT18_SPLIT_COL] & PT18_ROW_5_BIT)
		physical[PT18_SPLIT_GRID_START + 5] |= PT18_SPECIAL_ROW_BIT;
	if (flipped[PT18_SPLIT_COL] & PT18_ROW_6_BIT)
		physical[PT18_SPLIT_GRID_START + 6] |= PT18_SPECIAL_ROW_BIT;
	if (flipped[PT18_SPLIT_COL] & PT18_ROW_7_BIT)
		physical[PT18_SPLIT_GRID_START + 7] |= PT18_SPECIAL_ROW_BIT;
}

/* public API */

int pt18_matrix_init(void)
{
	int ret;
	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(tm1640));

	if (!device_is_ready(dev)) {
		LOG_ERR("TM1640 device not ready");
		return -ENODEV;
	}

	tm_dev = dev;

	k_mutex_lock(&pt18_mutex, K_FOREVER);

	ret = tm1640_clear(tm_dev);
	if (ret < 0) {
		k_mutex_unlock(&pt18_mutex);
		return ret;
	}

	ret = tm1640_display_on(tm_dev);
	if (ret < 0) {
		k_mutex_unlock(&pt18_mutex);
		return ret;
	}

	ret = tm1640_set_brightness(tm_dev, 1);
	if (ret < 0) {
		k_mutex_unlock(&pt18_mutex);
		return ret;
	}

	k_mutex_unlock(&pt18_mutex);

	LOG_INF("PT18 matrix initialized");
	return 0;
}

int pt18_matrix_write(const uint8_t *buf, size_t len)
{
	uint8_t physical[TM1640_NUM_GRIDS];
	int ret;

	if (tm_dev == NULL || buf == NULL) {
		return -EINVAL;
	}

	/* pad to full 17 columns if caller provides fewer */
	uint8_t logical[PT18_MATRIX_COLUMNS] = {0};
	size_t copy_len = (len > PT18_MATRIX_COLUMNS) ? PT18_MATRIX_COLUMNS : len;
	memcpy(logical, buf, copy_len);

	pt18_map_logical_to_physical(logical, physical);

	k_mutex_lock(&pt18_mutex, K_FOREVER);
	ret = tm1640_write(tm_dev, physical, TM1640_NUM_GRIDS);
	k_mutex_unlock(&pt18_mutex);

	return ret;
}

int pt18_matrix_clear(void)
{
	int ret;

	if (tm_dev == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&pt18_mutex, K_FOREVER);
	ret = tm1640_clear(tm_dev);
	k_mutex_unlock(&pt18_mutex);

	return ret;
}

int pt18_matrix_set_brightness(uint8_t level)
{
	int ret;

	if (tm_dev == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&pt18_mutex, K_FOREVER);
	ret = tm1640_set_brightness(tm_dev, level);
	k_mutex_unlock(&pt18_mutex);

	return ret;
}
