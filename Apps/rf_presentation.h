#ifndef RF_PRESENTATION_H
#define RF_PRESENTATION_H
#include <stdint.h>
#include <stdbool.h>
/* Private optional client contract, identical to the shared X4 paper helper.
 * Coordinates already use the adapter logical orientation. */
#define PAPER_TEXT_LITERAL 256u
typedef struct {bool valid,down,began,released,cancelled,tap_eligible;int16_t x,y;} springboard_contact;
typedef struct {uint32_t struct_size;void (*begin)(void);void (*text)(int,int,int,const char*,unsigned,bool,bool);int (*measure)(const char*,bool);void (*circle)(int,int,int,bool);void (*contact)(springboard_contact*);bool (*clock)(uint8_t*,uint8_t*);} paper_presentation;
__attribute__((weak)) const paper_presentation *paper_presentation_get(void){return NULL;}
#endif
