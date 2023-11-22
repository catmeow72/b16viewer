#include <stdio.h>
#include <conio.h>
#include <stdint.h>
#include <cx16.h>
#include <cbm.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "vera.h"
#include "macptr.h"
#include "debug.h"
#include "memory_decompress.h"
#include "fill.h"
uint8_t palette[512];
uint8_t vera_bit_depth;
uint8_t bitdepth;
uint16_t width, height;
bool compress;
bool direct_to_vera;
uint16_t imgdatastart;
uint32_t imgdatabytes;
uint8_t pixels_per_byte;
uint8_t borderidx = 255;
bool over320;
uint8_t oldhscale, oldvscale;
uint8_t significant_palette_entries;
uint8_t significant_palette_start;
uint8_t *image_start = BANK_RAM;
/// @brief Reads a 16 bit little endian value from LFN 2
/// @return The 16 bit value
uint16_t read16() {
	cbm_k_chkin(2);
	return ((cbm_k_chrin())) | ((uint16_t)cbm_k_chrin() << 8);
}
/// @brief Reads an 8 bit value and returns it as a character
/// @return The character
char readchar() {
	cbm_k_chkin(2);
	return cbm_k_chrin();
}
/// @brief Reads an unsigned 8 bit value and returns it as is
/// @return The unsigned 8 bit value
uint8_t read8() {
	cbm_k_chkin(2);
	return cbm_k_getin();
}
/// @brief Checks if the header in LFN 2 is correct
/// @return True if it is, false otherwise
bool checkheader() {
	char header[4] = {0x42, 0x4D, 0x58, 0}; // BMX
	char tested[4] = {0, 0, 0, 0}; // The tested values, for debugging
	uint8_t i = 0;
	bool valid = true; // Set to true so that if it's wrong, the loop can set it to false, but if it's correct, the loop doesn't have to set it
	char chr = 0;
	printf("Reading magic bytes...\n");
	// Check the magic bytes one-by-one
	for (; i < 3; i++) {
		chr = readchar(); // Get a character
		tested[i] = chr; // Put it in the tested value list
		printf("%2x%s", chr, i >= 4 ? "\n" : ", ");
		if (chr != header[i]) { // Check the value
			valid = false;
		}
	}
	// Make sure the tested values don't contain any unprintable characters
	for (i = 0; i < 3; i++) {
		if (tested[i] < 0x20 || tested[i] >= 0x7F) {
			tested[i] = '.';
		}
	}
	printf("%s%c=%s\n", tested, valid ? '=' : '!', header); // Display the comparison
	return valid;
}
/// @brief Updates the scale of the VERA based on the minimum required width/height to display the image
void updatescale() {
	uint8_t hscale;
	uint8_t vscale;
	// Set DCSEL to 0
	VERA.control &= 0b10000001;
	// Get the scale to check against and later set back to
	hscale = VERA.display.hscale;
	vscale = VERA.display.vscale;
	// Back up the current scale
	oldhscale = hscale;
	oldvscale = vscale;
	// If it's over 320, the scale's probably correct.
	if (over320) return;
	// Make sure the scale doesn't end up showing garbage data.
	if (hscale > 64) VERA.display.hscale = 64;
	if (vscale > 64) VERA.display.vscale = 64;
}
/// @brief Restores the scale to what it was before updating it to fit the image
void restorescale() {
	VERA.control &= 0b10000001;
	VERA.display.hscale = oldhscale;
	VERA.display.vscale = oldvscale;
}
/// @brief Loads and uploads an image to the VERA
/// @param filename The file to load
/// @return 0 on success, exit code on failure
int uploadimage(const char *filename) {
	// Used for decompression.
	uint8_t *image_end = BANK_RAM;
	// 16 bit variables
	size_t i = 0, j = 0, x = 0, y = 0, value = 0, bitmask = 0;
	// Variables for the VERA width and height values
	size_t vera_w = 320, vera_h = 240;
	// The max address of the VERA
	uint16_t vera_max = 0;
	// 32 bit variables, used sparingly
	uint32_t tmp = 0, vera_max_32 = 0;
	// The bank the VERA will be at when the operation finishes
	bool vera_max_bank = 1;
	// Base layer config
	uint8_t config = 0b00000100;
	// The counter for the amount of bytes read
	uint16_t bytes_read;
	// The version of the BMX format
	uint8_t version;
	// True if all palette entries are significant
	bool all_significant;
	// Open the file
	cbm_k_setnam(filename);
	cbm_k_setlfs(2, 8, 2);
	cbm_k_open();
	// Check if the open was successful
	if (cbm_k_readst()) {
		printf("Error opening file.\n");
		return 1;
	}
	// Make sure it's a BMX file
	if (!checkheader()) {
		printf("Invalid file!\n");
		return 1;
	}
	// Read the header
	version = read8();
	bitdepth = read8();
	// Verify the bitdepth
	if (bitdepth == 0) {
		printf("Error: bitdepth was 0.\n");
		return 1;
	}
	vera_bit_depth = read8();
	vera_bit_depth &= 0b11;
	config |= vera_bit_depth;
	// Calculate the amount of pixels that can fit in a byte, used later
	pixels_per_byte = 8 / bitdepth;
	printf("Bit depth: %u => %u pixels per byte\n", bitdepth, pixels_per_byte); // Debugging
	width = read16(); // Get the image dimensions
	height = read16();
	printf("Width: %u, height: %u\n", width, height);
	// Make sure the later update_scale call sets the correct scale
	over320 = width > 320 || height > 240;
	// Calculate the amount of bytes used within the image
	imgdatabytes = ((uint32_t)width * (uint32_t)height) / (uint32_t)pixels_per_byte;
	printf("Bytes: %lu\n", imgdatabytes);
	// Get the list of significant palette entires
	significant_palette_entries = read8();
	significant_palette_start = read8();
	if (significant_palette_entries == 0) {
		printf("All palette entries significant.\n");
		all_significant = true;
		significant_palette_start = 0;
	} else {
		printf("%u palette entries significant\nstarting at %u.\n", significant_palette_entries, significant_palette_start);
	}
	imgdatastart = read16();
	--imgdatastart;
	printf("Image starts at 0-indexed offset %4x\n", imgdatastart);
	direct_to_vera = width == vera_w && height == vera_h;
	if ((int8_t)read8() == -1) {
		compress = true;
	};
	printf("Skipping reserved bytes...\n");
	for (i = 0; i < 16; i++) {
		read8();
	}
	// Load the border color
	borderidx = read8();
	printf("Border color: %02x\n", borderidx);
	printf("Reading palette entries...\n");
	for (i = 0; i < (uint16_t)significant_palette_entries*2; i++) {
		j = i >> 1;
		// Only load the required palette entries
		if (all_significant || j >= significant_palette_start && j < significant_palette_start+significant_palette_entries || j == borderidx) {
			palette[i] = read8();
		} else {
			read8();
			palette[i] = get_from_backed_up_palette(i);
		}
	}
	// If the VERA was set up for a 640p image, make sure to keep track of that
	if (over320) {
		vera_w *= 2;
		vera_h *= 2;
	}
	// Calculate the maximum address and bank
	vera_max_32 = ((uint32_t)vera_w * (uint32_t)vera_h)/(uint32_t)pixels_per_byte;
	// Get just the max address
	vera_max = vera_max_32;
	// ... and the max bank all by itself
	vera_max_bank = vera_max_32 >> 16;
	// Update the VERA layer config
	VERA.layer0.config = config;
	// Make sure the TILEW value is set up correctly
	VERA.layer0.tilebase = over320 ? 1 : 0;
	// Debugging
	printf("Loading %simage bytes: $%lx\n", compress ? "compressed " : "", imgdatabytes);
	printf("Image width: %u, image height: %u\npixels per byte: %u\n", width, height, pixels_per_byte);
	i = 0;
	// Calculate the border value.
	value = 0;
	bitmask = (1 << bitdepth) - 1;
	
	if (compress && !direct_to_vera) {
		printf("Error: The image cannot be loaded due to limitations in memory_decompress.");
		return 1;
	}
	for (i = 0; i < pixels_per_byte; i++) {
		value |= (borderidx & bitmask) << (i * bitdepth);
	}
	if (compress) {
		RAM_BANK = 1;
		do {
			i = cx16_k_macptr(0, true, image_end);
			image_end += i;
			if (RAM_BANK > 1) {
				printf("Error: Cannot decompress memory because compressed data is too large.\n");
				return 1;
			}
			if (i == 0) {
				break;
			}
		} while (i != 0);
	}
	// Set up the VERA's data0 address
	VERA.control &= ~0b1;
	VERA.address = 0;
	VERA.address_hi = 0b00010000;
	if (direct_to_vera) {
		if (compress) {
			RAM_BANK = 1;
			image_start = BANK_RAM;
			memory_decompress(image_start, &VERA.data0);
			DEBUG_PORT1 = 0;
		} else {
			do {
				bytes_read = cx16_k_macptr(0, false, &VERA.data0);
			} while (bytes_read > 0);
		}
	} else {
		// Upload the bitmap
		for (x = 0; VERA.address < vera_max || (vera_max_bank & 0b1) != (VERA.address_hi & 0b1);) {
			tmp = ((((uint32_t)VERA.address) + ((uint32_t)(VERA.address_hi & 0b1) << 16)));
			x = (tmp % (uint32_t)(vera_w/pixels_per_byte))*pixels_per_byte;
			y = tmp / (uint32_t)(vera_w/pixels_per_byte);
			if (y >= height) {
				break;
			}
			if (x < width && y < height) {
				// Load the image's bytes, up to the bytes per row - the x value in bytes
				bytes_read = cx16_k_macptr((((width) - x))/pixels_per_byte, false, &VERA.data0);
			} else {
				// Fill the remaining pixels in the row with the border color using a fast assembly routine
				tmp = (vera_w-x)/pixels_per_byte;
				if (tmp >= ((uint32_t)1 << 8)) {
					fill_vera(0xFF, value);
				} else {
					fill_vera(tmp, value);
				}
			}
		}
		// Fill the rest of the image with the border color using the same fast assembly routine
		for (; VERA.address < vera_max || (vera_max_bank & 0b1) != (VERA.address_hi & 0b1);) {
			tmp = vera_max_32 - (((uint32_t)VERA.address) | ((uint32_t)(VERA.address_hi & 0b1) << 16));
			if (tmp >= ((uint32_t)1 << 8)) {
				fill_vera(0xFF, value);
			} else {
				fill_vera(tmp, value);
			}
		}
	}
	// Set up the VERA to display the bitmap
	upload_palette(palette);
	updatescale();
	vera_layer_enable(0b11);
	// Close the file
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