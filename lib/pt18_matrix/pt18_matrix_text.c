/* Rasterizes strings into the 17 column framebuffer and passes them through */

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include <pt18_matrix/pt18_matrix.h>
#include <pt18_matrix/pt18_matrix_text.h>
#include <pt18_matrix/pt18_font_5x7.h>

LOG_MODULE_REGISTER(pt18_matrix_text, CONFIG_LOG_DEFAULT_LEVEL);

int pt18_matrix_print(const struct device *dev, const char *str, int offset)
{
	uint8_t logical[PT18_MATRIX_COLUMNS] = {0};

	if (dev == NULL || str == NULL) {
		return -EINVAL;
	}

	for (int i = 0; str[i] != '\0'; i++) {
		uint8_t ch = (uint8_t)str[i];
		const uint8_t *glyph;

		/* Solid block for unprintable */
		if (ch < PT18_FONT_FIRST_CHAR || ch > PT18_FONT_LAST_CHAR) {
			static const uint8_t placeholder[PT18_FONT_WIDTH] = {
				0x7F, 0x7F, 0x7F, 0x7F, 0x7F
			};
			glyph = placeholder;
		} else {
			glyph = pt18_font_data[ch - PT18_FONT_FIRST_CHAR];
		}

		int col = offset + (i * PT18_FONT_STRIDE);

		/* Past the right edge done */
		if (col >= PT18_MATRIX_COLUMNS) {
			break;
		}

		/* Off the left edge skip */
		if (col + PT18_FONT_WIDTH <= 0) {
			continue;
		}

		for (int j = 0; j < PT18_FONT_WIDTH; j++) {
			int idx = col + j;

			if (idx < 0 || idx >= PT18_MATRIX_COLUMNS) {
				continue;
			}
			logical[idx] = glyph[j];
		}
	}

	LOG_DBG("print \"%s\" offset=%d", str, offset);

	return pt18_matrix_write(dev, logical, PT18_MATRIX_COLUMNS);
}
