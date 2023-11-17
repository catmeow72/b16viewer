#ifndef VERA_H
#define VERA_H
#include <stdint.h>
#include <stdbool.h>
void upload_palette(uint8_t *ptr);
void backup_palette();
void restore_palette();
uint8_t get_from_backed_up_palette(uint16_t idx);
void set_text_256color(bool enabled);
#endif