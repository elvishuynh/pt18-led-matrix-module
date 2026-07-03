/* The PT18 uses 16 grids for columns 0-15 skipping logical column 5
 * stitches logical column 5 the 17th column across the unused bit-7
 * positions of grids 8-15
 */

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include <tm1640/tm1640.h>
#include <pt18_matrix/pt18_matrix.h>

LOG_MODULE_REGISTER(pt18_matrix, CONFIG_LOG_DEFAULT_LEVEL);

/*
 * mirrors the standard 7 rows bits 0-6 to fix the font orientation
 * preserves the special Row -1 LED bit 7
 */
static uint8_t flip_logical_rows(uint8_t col_data)
{
	uint8_t flipped = 0;
	if (col_data & 0x01) flipped |= 0x40; /* Font Top > Hardware Top Row 7 */
	if (col_data & 0x02) flipped |= 0x20;
	if (col_data & 0x04) flipped |= 0x10;
	if (col_data & 0x08) flipped |= 0x08; /* Center stays center */
	if (col_data & 0x10) flipped |= 0x04;
	if (col_data & 0x20) flipped |= 0x02;
	if (col_data & 0x40) flipped |= 0x01; /* Font Bottom > Hardware Bottom Row 1 */

	/* Preserve the special bit 7 Row -1 */
	flipped |= (col_data & 0x80);

	return flipped;
}

/*
 * Maps 17 logical columns to 16 physical grids for the PT18 LED V0 board
 * Columns 0-4   > Grids 15-11 reversed,bits 0-6 only
 * Columns 6-16  > Grids 10-0  reversed, bits 0-6 only
 * Column 5      > Scattered across bit 7 of Grids 8-15
 * Column 11 bit 7 > Grid 5 bit 7
 */
static void pt18_map_logical_to_physical(const uint8_t *logical, uint8_t *physical)
{
	memset(physical, 0, TM1640_NUM_GRIDS);

	/* Create a row flipped version of the logical buffer */
	uint8_t flipped_logical[PT18_MATRIX_COLUMNS];
	for (int i = 0; i < PT18_MATRIX_COLUMNS; i++) {
		flipped_logical[i] = flip_logical_rows(logical[i]);
	}

	for (int i = 0; i < PT18_MATRIX_COLUMNS; i++) {
		if (i < 5) {
			physical[15 - i] = flipped_logical[i] & 0x7F;
		} else if (i > 5) {
			physical[16 - i] = flipped_logical[i] & 0x7F;
		}
	}

	/* Col 12 logical[11] Row -1 bit 7 */
	if (flipped_logical[11] & 0x80) {
		physical[5] |= 0x80;
	}

	/* Col 6 logical[5] mapping to grid 0x08-0x0F */
	/* Because we flipped the bits above font Top (0x40) routes to physical[15] (Row 7) */
	if (flipped_logical[5] & 0x80) physical[8]  |= 0x80;  /* Row -1 */
	if (flipped_logical[5] & 0x01) physical[9]  |= 0x80;  /* Row 1 (Bottom) */
	if (flipped_logical[5] & 0x02) physical[10] |= 0x80;  /* Row 2 */
	if (flipped_logical[5] & 0x04) physical[11] |= 0x80;  /* Row 3 */
	if (flipped_logical[5] & 0x08) physical[12] |= 0x80;  /* Row 4 */
	if (flipped_logical[5] & 0x10) physical[13] |= 0x80;  /* Row 5 */
	if (flipped_logical[5] & 0x20) physical[14] |= 0x80;  /* Row 6 */
	if (flipped_logical[5] & 0x40) physical[15] |= 0x80;  /* Row 7 (Top) */
}

/* API */

int pt18_matrix_init(const struct device *dev)
{
	int ret;

	if (dev == NULL) {
		return -EINVAL;
	}

	ret = tm1640_clear(dev);
	if (ret < 0) {
		return ret;
	}

	ret = tm1640_display_on(dev);
	if (ret < 0) {
		return ret;
	}

	LOG_INF("PT18 matrix initialized");
	return 0;
}

int pt18_matrix_write(const struct device *dev, const uint8_t *buf, size_t len)
{
	uint8_t physical[TM1640_NUM_GRIDS];

	if (dev == NULL || buf == NULL) {
		return -EINVAL;
	}

	/* Pad to full 17 columns if caller provides fewer */
	uint8_t logical[PT18_MATRIX_COLUMNS] = {0};
	size_t copy_len = (len > PT18_MATRIX_COLUMNS) ? PT18_MATRIX_COLUMNS : len;
	memcpy(logical, buf, copy_len);

	pt18_map_logical_to_physical(logical, physical);

	return tm1640_write(dev, physical, TM1640_NUM_GRIDS);
}

int pt18_matrix_clear(const struct device *dev)
{
	if (dev == NULL) {
		return -EINVAL;
	}

	return tm1640_clear(dev);
}

int pt18_matrix_set_brightness(const struct device *dev, uint8_t level)
{
	if (dev == NULL) {
		return -EINVAL;
	}

	return tm1640_set_brightness(dev, level);
}
