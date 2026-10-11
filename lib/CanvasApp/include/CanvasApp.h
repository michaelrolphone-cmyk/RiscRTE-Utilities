#pragma once
/* App-owned raster/input client over the public Runtime capability tables.
 * Logical images never alias a queued provider surface. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "RiscTouchV1.h"
#define CANVAS_VIEWER 1u
#define CANVAS_GAME 2u
bool canvas_open(unsigned kind);
void canvas_close(void);
bool canvas_alive(void);
unsigned canvas_width(void);
unsigned canvas_height(void);
bool canvas_compact(void);
void canvas_pixel(uint8_t*,int,int,bool);
uint32_t canvas_game_buttons(void);
bool canvas_contacts(risc_touch_snapshot_v1*);
void canvas_set_controls(bool);
bool canvas_model_source(char*,size_t);
void canvas_model_select(unsigned);
unsigned canvas_model_count(void);
const char *canvas_model_name(unsigned);
void canvas_retain(const char*);
