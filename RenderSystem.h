#ifndef RENDER_SYSTEM_H
#define RENDER_SYSTEM_H

#include "Game.h"

namespace RenderSystem
{
	bool Initialise(int target_width, int target_height, int scale);
	void Terminate();
	void Update(const Game::GameState& game);
}

#endif