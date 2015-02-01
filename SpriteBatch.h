#ifndef SPRITE_BATCH_H
#define SPRITE_BATCH_H

#include "gl_core_3_3.h"
#include "Mesh.h"
#include "Sprite.h"

Mesh create_sprite_batch_mesh();
void buffer_sprites(const Sprite sprites[], GLuint buffer);

#endif
