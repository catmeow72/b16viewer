#include "vera.h"
#include <stdint.h>
#include <cx16.h>
uint8_t palette_backup[512];
bool palette_backed_up = false;
void backup_palette() {
	uint16_t i;
	VERA.control &= ~0b1;
	VERA.address_hi = 0b00010001;
	VERA.address = 0xFA00;
	for (i = 0; i < 512; i++) {
		palette_backup[i] = VERA.data0;
	}
	palette_backed_up = true;
}
void upload_palette(uint8_t *ptr) {
	uint16_t i;
	VERA.control &= ~0b1;
	VERA.address_hi = 0b00010001;
	VERA.address = 0xFA00;
	for (i = 0; i < 512; i++) {
		VERA.data0 = ptr[i];
	}
}
void restore_palette() {
	uint16_t i;
	VERA.control &= ~0b1;
	VERA.address_hi = 0b00010001;
	VERA.address = 0xFA00;
	for (i = 0; i < 512; i++) {
		VERA.data0 = palette_backup[i];
	}
}
void set_text_256color(bool enabled) {
	uint8_t config = VERA.layer1.config;
	if (enabled) {
		config |= (1 << 3);
	} else {
		config &= ~(1 << 3);
	}
	VERA.layer1.config = config;
}
uint8_t get_from_backed_up_palette(uint16_t idx) {
	if (!palette_backed_up) {
		backup_palette();
	}
	if (idx >= 512) {
		return 0;
	}
	return palette_backup[idx];
}