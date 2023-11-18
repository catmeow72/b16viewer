#include <stdio.h>
#include <conio.h>
#include <stdint.h>
#include <cx16.h>
#include <cbm.h>
#include <stdbool.h>
#include <stdlib.h>
#include "vera.h"
#include "macptr.h"
#include "debug.h"
#include "fill.h"
uint8_t palette[512];
uint8_t vera_bit_depth;
uint8_t bitdepth;
uint16_t width, height;
uint32_t imgdatabytes;
uint8_t pixels_per_byte;
uint8_t filleridx = 255;
bool over320;
uint8_t oldhscale, oldvscale;
uint8_t significant_palette_entries;
uint8_t significant_palette_start;
uint16_t read16() {
	cbm_k_chkin(2);
	return ((cbm_k_chrin())) | ((uint16_t)cbm_k_chrin() << 8);
}
char readchar() {
	cbm_k_chkin(2);
	return cbm_k_chrin();
}
uint8_t read8() {
	cbm_k_chkin(2);
	return cbm_k_getin();
}
bool checkheader() {
	char header[4] = {0x42, 0x4D, 0x58, 0}; // X16BM
	char tested[4] = {0, 0, 0, 0};
	uint8_t i = 0;
	bool valid = true;
	char chr = 0;
	printf("Reading magic bytes...\n");
	for (; i < 3; i++) {
		chr = readchar();
		tested[i] = chr;
		printf("%2x%s", chr, i >= 4 ? "\n" : ", ");
		if (chr != header[i]) {
			valid = false;
		}
	}
	for (i = 0; i < 3; i++) {
		if (tested[i] < 0x20 || tested[i] >= 0x7F) {
			tested[i] = '.';
		}
	}
	printf("%s%c=%s\n", tested, valid ? '=' : '!', header);
	return valid;
}
void updatescale() {
	uint8_t hscale;
	uint8_t vscale;
	VERA.control &= 0b10000001;
	hscale = VERA.display.hscale;
	vscale = VERA.display.vscale;
	oldhscale = hscale;
	oldvscale = vscale;
	if (over320) return;
	if (hscale > 64) VERA.display.hscale = 64;
	if (vscale > 64) VERA.display.vscale = 64;
}
void restorescale() {
	VERA.control &= 0b10000001;
	VERA.display.hscale = oldhscale;
	VERA.display.vscale = oldvscale;
}
int uploadimage(const char *filename) {
	size_t i = 0, j = 0, x = 0, y = 0, value = 0, bitmask = 0;
	size_t vera_w = 320, vera_h = 240;
	uint16_t vera_max = 0;
	uint32_t tmp = 0, vera_max_32 = 0;
	uint16_t vera_adr_bak;
	uint8_t vera_adr_h_bak;
	bool vera_max_bank = 1;
	uint8_t config = 0b00000100;
	uint16_t bytes_read;
	uint8_t version;
	bool all_significant;
	cbm_k_setnam(filename);
	cbm_k_setlfs(2, 8, 2);
	cbm_k_open();
	if (cbm_k_readst()) {
		printf("Error opening file.\n");
		return 1;
	}
	if (!checkheader()) {
		printf("Invalid file!\n");
		return 1;
	}
	version = read8();
	bitdepth = read8();
	if (bitdepth == 0) {
		printf("Error: bitdepth was 0.\n");
		return 1;
	}
	vera_bit_depth = read8();
	vera_bit_depth &= 0b11;
	config |= vera_bit_depth;
	if (vera_bit_depth > 3) {
		printf("Error: VERA bit depth was invalid!\n");
	}
	pixels_per_byte = 8 / bitdepth;
	printf("Bit depth: %u => %u pixels per byte\n", bitdepth, pixels_per_byte);
	width = read16();
	height = read16();
	printf("Width: %u, height: %u\n", width, height);
	over320 = width > 320 || height > 240;
	filleridx = read8();
	printf("Border color: %02x\n", filleridx);
	imgdatabytes = ((uint32_t)width * (uint32_t)height) / (uint32_t)pixels_per_byte;
	printf("Bytes: %lu\n", imgdatabytes);
	significant_palette_entries = read8();
	significant_palette_start = read8();
	if (significant_palette_entries == 0) {
		printf("All palette entries significant.\n");
		all_significant = true;
		significant_palette_start = 0;
	} else {
		printf("%u palette entries significant\nstarting at %u.\n", significant_palette_entries, significant_palette_start);
	}
	printf("Skipping reserved bytes...\n");
	for (i = 0; i < 19; i++) {
		read8();
	}
	printf("Reading palette entries...\n");
	for (i = 0; i < 512; i++) {
		j = i >> 1;
		if (all_significant || j >= significant_palette_start && j <= significant_palette_entries) {
			palette[i] = read8();
		} else {
			read8();
			palette[i] = get_from_backed_up_palette(i);
		}
	}
	if (over320) {
		vera_w *= 2;
		vera_h *= 2;
	}
	vera_max_32 = ((uint32_t)vera_w * (uint32_t)vera_h)/(uint32_t)pixels_per_byte;
	vera_max = vera_max_32;
	vera_max_bank = vera_max_32 >> 16;
	VERA.layer0.config = config;
	VERA.layer0.tilebase = over320 ? 1 : 0;
	printf("Loading image bytes: $%lx\n", imgdatabytes);
	printf("Image width: %u, image height: %u\npixels per byte: %u\n", width, height, pixels_per_byte);
	VERA.control &= ~0b1;
	VERA.address = 0;
	VERA.address_hi = 0b00010000;
	i = 0;
	// Calculate the filler value.
	value = 0;
	bitmask = (1 << bitdepth) - 1;
	for (i = 0; i < pixels_per_byte; i++) {
		value |= (filleridx & bitmask) << (i * bitdepth);
	}
	// Upload the bitmap
	for (x = 0; VERA.address < vera_max || (vera_max_bank & 0b1) != (VERA.address_hi & 0b1);) {
		tmp = ((((uint32_t)VERA.address) + ((uint32_t)(VERA.address_hi & 0b1) << 16)));
		x = (tmp % (uint32_t)(vera_w/pixels_per_byte))*pixels_per_byte;
		y = tmp / (uint32_t)(vera_w/pixels_per_byte);
		#if 0
		vera_adr_bak = VERA.address;
		vera_adr_h_bak = VERA.address_hi;
		printf("Y: %u, X: %u, tmp: %lu,\nvera_w: %u\nvera_adr: %u, vera_adr_h: %s\n", y, x, tmp, vera_w, vera_adr_bak, (vera_adr_h_bak & 1) ? "1" : "0");
		VERA.address = vera_adr_bak;
		VERA.address_hi = vera_adr_h_bak;
		#endif
		if (y >= height) {
			break;
		}
		if (x < width && y < height) {
			bytes_read = cx16_k_macptr((((width) - x))/pixels_per_byte, false, &VERA.data0);
		} else {
			//VERA.data0 = value;
			tmp = (vera_w-x)/pixels_per_byte;
			if (tmp >= ((uint32_t)1 << 8)) {
				fill_vera(0xFF, value);
			} else {
				fill_vera(tmp, value);
			}
		}
	}
	for (; VERA.address < vera_max || (vera_max_bank & 0b1) != (VERA.address_hi & 0b1);) {
		tmp = vera_max_32 - (((uint32_t)VERA.address) | ((uint32_t)(VERA.address_hi & 0b1) << 16));
		if (tmp >= ((uint32_t)1 << 8)) {
			fill_vera(0xFF, value);
		} else {
			fill_vera(tmp, value);
		}
	}
	upload_palette(palette);
	updatescale();
	vera_layer_enable(0b11);
	cbm_k_clrch();
	cbm_k_close(2);
	return 0;
}
void set_text_color(uint8_t color) {
	uint16_t mw, mh;
	uint16_t base;
	size_t i;
	base = VERA.layer1.mapbase;
	VERA.control &= 0b10000001;
	VERA.address = (base << 9) + 1;
	VERA.address_hi = 0b00100000 | (base >> 7) & 0b1;
	mw = (VERA.layer1.config >> 4) & 0b11;
	mh = (VERA.layer1.config >> 6) & 0b11;
	mw = 32 << mw;
	mh = 32 << mh;
	for (i = 0; i < mw * mh; i++) {
		VERA.data0 = color;
	}
}
int main(int argc, char **argv) {
	char *buf;
	int ret;
	bool done, text_visible;
	char ch;
	uint8_t coloridx = 1;
	size_t i;
	backup_palette();
	buf = (char*)malloc(81);
	printf("Enter viewable file name: ");
	fgets(buf, 81, stdin);
	for (i = 0; i < 81; i++) {
		if (buf[i] == '\r' || buf[i] == '\n') {
			buf[i] = '\0';
			break;
		}
	}
	clrscr();
	gotoxy(0, 0);
	printf("Loading file...\n");
	ret = uploadimage(buf);
	if (ret != 0) return ret;
	set_text_256color(true);
	printf("Press arrow keys to change text colors.");
	gotoxy(0, 0);
	printf("[ESC] to exit, [SPACE] to toggle text.");
	coloridx = significant_palette_start - 1;
	set_text_color(coloridx);
	done = false;
	text_visible = true;
	while (!done) {
		while (kbhit()) {
			ch = cgetc();
			if (ch == CH_ESC) {
				done = true;
				break;
			} else if (ch == ' ') {
				text_visible = !text_visible;
				vera_layer_enable(0b1 | (text_visible ? 0b10 : 0));
			} else if (ch == CH_CURS_LEFT) {
				set_text_color(--coloridx);
			} else if (ch == CH_CURS_RIGHT) {
				set_text_color(++coloridx);
			}
		}
	}
	vera_layer_enable(0b10);
	set_text_256color(false);
	restorescale();
	clrscr();
	restore_palette();
	return 0;
}