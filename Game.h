#ifndef GAME_H

#include "Tilemap.h"
#include "Sprite.h"

namespace Game {

struct GameState
{
	Sprite* sprites;
	Tilemap* tilemap;

	bool load_map;
	const char* atlas_name;
};

void Initialise();
void Terminate();
GameState Update(byte_t input_state, double delta_time);

} // namespace Game

#define GAME_H
#endif
