#ifndef TILEMAP_H
#define TILEMAP_H

#include "Mesh.h"
#include "gl_core_3_3.h"
#include "GameBoyTypes.h"

#define TILE_PALETTE         0x07 // least-significant three bits in byte
#define TILE_BANK            0x08 // bit 3
#define TILE_MONO_PALETTE    0x10 // bit 4
#define TILE_HORIZONTAL_FLIP 0x20 // bit 5
#define TILE_VERTICAL_FLIP   0x40 // bit 6
#define TILE_BG_PRIORITY     0x80 // bit 7

struct Tilemap
{
	int columns, rows;
	byte_t* tiles;
	byte_t* attributes;
};

Tilemap load_tilemap(const char* filename);
void unload_tilemap(const Tilemap& map);

Mesh create_tilemap_mesh(const Tilemap& map);

#endif
