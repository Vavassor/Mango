#include "Game.h"
#include "Input.h"

#include "utilities/Random.h"

namespace Game {

namespace
{
	Tilemap tilemap;
	Sprite sprites[MAX_SPRITES];

	bool loading_map = false;
}

void Initialise()
{
	tilemap = load_tilemap("test_map.map");

	// initialise sprites
	{
		random::sow(7);
		for(int i = 0; i < MAX_SPRITES; ++i)
		{
			Sprite& sprite = sprites[i];
			sprite.tile_number = random::reap_integer(0, 127);
			sprite.attribute = SPRITE_HORIZONTAL_FLIP | SPRITE_VERTICAL_FLIP;
			sprite.position_x = random::reap_integer(0, 255);
			sprite.position_y = random::reap_integer(0, 255);
		}
	}

	loading_map = true;
}

void Terminate()
{
	unload_tilemap(tilemap);
}

GameState Update(byte_t input_state, double delta_time)
{
	for(int i = 0; i < MAX_SPRITES; ++i)
	{
		if(input_state & INPUT_LEFT)  --sprites[i].position_x;
		if(input_state & INPUT_RIGHT) ++sprites[i].position_x;
		if(input_state & INPUT_UP)    --sprites[i].position_y;
		if(input_state & INPUT_DOWN)  ++sprites[i].position_y;
	}

	GameState state = {};
	state.sprites = sprites;
	state.tilemap = &tilemap;
	state.load_map = loading_map;

	if(loading_map)
	{
		state.atlas_name = "Tile Atlas.png";
		loading_map = false;
	}

	return state;
}

} // namespace Game
