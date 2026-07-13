/* rasterizes strings into the 17 column framebuffer using proportional widths */

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <pt18_matrix/pt18_matrix.h>
#include <pt18_matrix/pt18_matrix_text.h>
#include <pt18_matrix/pt18_font_5x7.h>

LOG_MODULE_REGISTER(pt18_matrix_text, CONFIG_LOG_DEFAULT_LEVEL);

int pt18_matrix_print(const struct device *dev, const char *str, int offset)
{
	uint8_t logical[PT18_MATRIX_COLUMNS] = {0};
	int cursor = offset;

	if (dev == NULL || str == NULL) {
		return -EINVAL;
	}

	for (int i = 0; str[i] != '\0'; i++) {
		uint8_t ch = (uint8_t)str[i];
		const uint8_t *glyph;
		uint8_t width;

		/* solid block for unprintable chars */
		if (ch < PT18_FONT_FIRST_CHAR || ch > PT18_FONT_LAST_CHAR) {
			static const uint8_t placeholder[PT18_FONT_WIDTH] = {
				0x7F, 0x7F, 0x7F, 0x7F, 0x7F
			};
			glyph = placeholder;
			width = PT18_FONT_WIDTH;
		} else {
			uint8_t idx = ch - PT18_FONT_FIRST_CHAR;
			glyph = pt18_font_data[idx];
			width = pt18_font_widths[idx];
		}

		/* hardware decimal point logic */
		if (ch == '.') {
			if (cursor == 0) {
				/* leading decimal gets standard grid pixels */
				uint8_t idx = ch - PT18_FONT_FIRST_CHAR;
				logical[0] = pt18_font_data[idx][2];
				cursor += 2;
			} else if (cursor > 0) {
				/* hardware LED overlay onto previous chars spacing gap */
				int dot_col = cursor - 1;
				if (dot_col < PT18_MATRIX_COLUMNS) {
					logical[dot_col] |= PT18_SPECIAL_ROW_BIT;
				}
			}
			/* cursor < 0 means dot is off screen so skip */
			continue;
		}

		/* past the right edge */
		if (cursor >= PT18_MATRIX_COLUMNS) {
			break;
		}

		/* find first non-zero column for left trim */
		int start = 0;
		for (int j = 0; j < PT18_FONT_WIDTH; j++) {
			if (glyph[j] != 0x00) {
				start = j;
				break;
			}
		}

		/* entirely off the left edge */
		if (cursor + (int)width <= 0) {
			cursor += width + PT18_FONT_CHAR_SPACING;
			continue;
		}

		/* blit trimmed glyph columns */
		for (int j = 0; j < (int)width; j++) {
			int col = cursor + j;
			if (col < 0) {
				continue;
			}
			if (col >= PT18_MATRIX_COLUMNS) {
				break;
			}
			logical[col] = glyph[start + j];
		}

		cursor += width + PT18_FONT_CHAR_SPACING;
	}

	LOG_DBG("print \"%s\" offset=%d", str, offset);

	return pt18_matrix_write(dev, logical, PT18_MATRIX_COLUMNS);
}
