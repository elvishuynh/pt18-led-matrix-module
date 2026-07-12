# PT18 LED Matrix Middleware

Zephyr RTOS middleware for the **PT18 LED V0** board. This library gives you a 17-column × 7-row logical framebuffer on top of the [TM1640](https://github.com/elvishuynh/zephyr-tm1640-driver) 16-grid physical LED controller.

### What is the PT18 LED V0?

It's a custom LED matrix daughterboard found in the Subminimal Subscale. It is driven by a Titan Micro TM1640 LED controller and is responsible for the scale's wow factor.

I developed this module firmware to hijack my scale. The stock Subminimal firmware forces a 4-5 second brand logo animation every time the device is powered on, which cannot be skipped. [This is so annoying that I'm bypassing the MCU that came with the Subscale and running my own open source firmware on an nRF54L15.](https://github.com/elvishuynh/SnailScale)

WARNING for those looking to replicate this:
Do not rely on wire colors. At least in my revision, the factory wiring does not follow normal convention. (Black is SCLK, Yellow is DIN, Blue is GND, and Red is VDD).

## Hardware Mapping

The PT18 LED V0 has a 17x7 logical layout, but the TM1640 IC only supports up to 16 grids × 8 segments. To make the 17th column fit, the hardware uses a pretty interesting wiring scheme.

### Standard Columns

| Logical Columns | Physical Grids     | Bits Used |
|-----------------|--------------------|-----------|
| 0 – 4           | 15 – 11 (reversed) | 0 – 6     |
| 6 – 16          | 10 – 0  (reversed) | 0 – 6     |

### The 17th Column (Logical Column 5)

Since there's no dedicated grid for logical column 5, its 7 row bits and the "Row -1" indicator are scattered across the **unused bit 7** of physical grids 8 through 15:

```text
Logical Col 5 Bit 7 (Row -1) → Grid 8  bit 7
Logical Col 5 Bit 0 (Row 1)  → Grid 9  bit 7
Logical Col 5 Bit 1 (Row 2)  → Grid 10 bit 7
Logical Col 5 Bit 2 (Row 3)  → Grid 11 bit 7
Logical Col 5 Bit 3 (Row 4)  → Grid 12 bit 7
Logical Col 5 Bit 4 (Row 5)  → Grid 13 bit 7
Logical Col 5 Bit 5 (Row 6)  → Grid 14 bit 7
Logical Col 5 Bit 6 (Row 7)  → Grid 15 bit 7
```

### Special Row -1

Logical columns 6 and 12 each have an extra "Row -1" indicator LED wired to bit 7. Column 12's bit 7 specifically maps to Grid 5 bit 7.

## Dependencies

You'll need the out-of-tree **zephyr-tm1640-driver** module. 

Make sure both modules are in your `west.yml` manifest, and enable them in your `prj.conf`:

```ini
CONFIG_TM1640=y
CONFIG_PT18_MATRIX=y
CONFIG_PT18_MATRIX_TEXT=y
```

## API Reference

### Core (`pt18_matrix.h`)

| Function | Description |
|----------|-------------|
| `pt18_matrix_init(dev)` | Clears the display and turns it on at full brightness. |
| `pt18_matrix_write(dev, buf, len)` | Writes a logical column buffer (up to 17 bytes) to the display. |
| `pt18_matrix_clear(dev)` | Turns off all LEDs. |
| `pt18_matrix_set_brightness(dev, level)` | Sets the brightness level (0–7). |

*Note: Each byte in the buffer represents one column. Bits 0–6 map to rows 1–7. Bit 7 is the Row -1 indicator.*

### Text (`pt18_matrix_text.h`)

| Function | Description |
|----------|-------------|
| `pt18_matrix_print(dev, str, offset)` | Renders an ASCII string with proportional spacing. |

The `offset` parameter controls horizontal positioning. You can pass a negative offset for scroll animations.

## Why Not the Zephyr Display API?

The standard Zephyr Display API expects rectangular pixel buffers with uniform geometry. Because of the PT18's scattered 17th column and those weird Row -1 bits, using the Display subsystem would add too much overhead.

This middleware uses a purpose built API to handle the hardware quirks directly instead. A Display API compatibility wrapper might or might not be added in the future.

## Thread Safety

Everything in the public API is thread safe. A `k_mutex` protects the TM1640 hardware calls, but parameter validation and buffer translation happen outside the lock so we don't hold it any longer than needed.

## Proportional Font Rendering

Text rendering uses a compile time width lookup table (`pt18_font_widths[]`) so cursor advancement is O(1) per character.

* **Monospaced numerics:** Digits `0`–`9` and the minus sign `-` are forced to a width of 5. This stops numeric readouts from jittering when values update.
* **Zero-width decimals:** The decimal point `.` is treated as width 0. Instead of taking up its own column, it gets overlaid onto bit 6 (the bottom row) of the kerning gap left by the previous character. This keeps decimal numbers looking compact. *(Edge case: If a string starts with a decimal, like `".5"`, it draws at column 0 and advances the cursor by 2.)*

## Usage

```c
#include <zephyr/device.h>
#include <pt18_matrix/pt18_matrix.h>
#include <pt18_matrix/pt18_matrix_text.h>

void main(void)
{
    const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(tm1640));

    pt18_matrix_init(dev);
    pt18_matrix_set_brightness(dev, 4);

    /* display a weight reading */
    pt18_matrix_print(dev, "12.5g", 0);
}
```