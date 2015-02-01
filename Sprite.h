#ifndef SPRITE_H
#define SPRITE_H

#include "GameBoyTypes.h"

#define MAX_SPRITES 40

#define SPRITE_PALETTE         0x07 // **CGB Mode Only** (OBP0-7)
#define SPRITE_BANK            0x08 // **CGB Mode Only** (0=Bank 0, 1=Bank 1)
#define SPRITE_MONO_PALETTE    0x10 // Non-CGB mode only
#define SPRITE_HORIZONTAL_FLIP 0x20
#define SPRITE_VERTICAL_FLIP   0x40
#define SPRITE_BG_PRIORITY     0x80 // 0=OBJ Above BG, 1=OBJ Behind BG color 1-3

struct Sprite
{
	byte_t position_x;
	byte_t position_y;
	byte_t tile_number;
	byte_t attribute;
};

#endif
