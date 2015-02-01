#include "SpriteBatch.h"

#include "utilities/ArrayMacros.h"

#define VERTEX_COMPONENTS 4
#define VERTEX_SIZE       (sizeof(GLfloat) * VERTEX_COMPONENTS)
#define NUM_VERTICES      (4 * MAX_SPRITES)
#define NUM_INDICES       (6 * MAX_SPRITES)

#define SPRITE_WIDTH  8
#define SPRITE_HEIGHT 8
#define PATTERN_COUNT_X 32
#define PATTERN_COUNT_Y 24

namespace
{
	GLfloat vertex_buffer[VERTEX_COMPONENTS * NUM_VERTICES];
}

Mesh create_sprite_batch_mesh()
{
	GLuint vertex_array;
	glGenVertexArrays(1, &vertex_array);
	glBindVertexArray(vertex_array);

	GLuint buffers[2];
	glGenBuffers(ARRAY_COUNT(buffers), buffers);

	glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
	glBufferData(GL_ARRAY_BUFFER, VERTEX_SIZE * NUM_VERTICES, nullptr, GL_DYNAMIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, VERTEX_SIZE, 0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, VERTEX_SIZE, reinterpret_cast<GLvoid*>(sizeof(GLfloat) * 2));

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	// load static indices
	{
		GLushort indices[NUM_INDICES];
		for(int i = 0; i < MAX_SPRITES; ++i)
		{
			indices[(i * 6) + 0] = (i * 4) + 0;
			indices[(i * 6) + 1] = (i * 4) + 3;
			indices[(i * 6) + 2] = (i * 4) + 1;
			indices[(i * 6) + 3] = (i * 4) + 1;
			indices[(i * 6) + 4] = (i * 4) + 3;
			indices[(i * 6) + 5] = (i * 4) + 2;
		}

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[1]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * NUM_INDICES, indices, GL_STATIC_DRAW);
	}

	Mesh mesh = {};
	mesh.vertex_array = vertex_array;
	mesh.buffers[0] = buffers[0];
	mesh.buffers[1] = buffers[1];
	mesh.num_indices = NUM_INDICES;

	return mesh;
}

void buffer_sprites(const Sprite sprites[], GLuint buffer)
{
	GLfloat* data = vertex_buffer;

	GLfloat texcoord_width = 1.0f / static_cast<GLfloat>(PATTERN_COUNT_X);
	GLfloat texcoord_height = 1.0f / static_cast<GLfloat>(PATTERN_COUNT_Y);

	for(int i = 0; i < MAX_SPRITES; ++i)
	{
		Sprite sprite = sprites[i];

		// Sprite tiles are numbered from 0 to 127
		byte_t tile_number = sprite.tile_number;

		// The two banks of tile patterns are stored side-by-side in a texture,
		// so to read from one bank, the total width should be halved.
		GLfloat u = static_cast<GLfloat>(tile_number % (PATTERN_COUNT_X / 2)) * texcoord_width;
		GLfloat v = static_cast<GLfloat>(tile_number / (PATTERN_COUNT_X / 2)) * texcoord_height;

		// Tiles in the second bank then need to offset their texture coordinates
		// by half to read from the correct side.
		byte_t attribute = sprite.attribute;

		if(attribute & SPRITE_BANK)
			u += 0.5f;

		// determine texture coordinates for horizontal/vertical tile flipping
		GLfloat texcoord_left = u;
		GLfloat texcoord_right = u + texcoord_width;

		if(attribute & SPRITE_HORIZONTAL_FLIP)
		{
			texcoord_left = u + texcoord_width;
			texcoord_right = u;
		}

		GLfloat texcoord_bottom = v;
		GLfloat texcoord_top = v + texcoord_height;

		if(attribute & SPRITE_VERTICAL_FLIP)
		{
			texcoord_bottom = v + texcoord_height;
			texcoord_top = v;
		}

		// load the positions and texture coordinates to the vertex data array
		GLfloat position_x = sprite.position_x;
		GLfloat position_y = sprite.position_y;

		int j = i * 4 * 4;

		data[j + 0] = position_x;
		data[j + 1] = position_y;
		data[j + 2] = texcoord_left;
		data[j + 3] = texcoord_bottom;

		data[j + 4] = position_x + SPRITE_WIDTH;
		data[j + 5] = position_y;
		data[j + 6] = texcoord_right;
		data[j + 7] = texcoord_bottom;

		data[j + 8] = position_x + SPRITE_WIDTH;
		data[j + 9] = position_y + SPRITE_HEIGHT;
		data[j + 10] = texcoord_right;
		data[j + 11] = texcoord_top;

		data[j + 12] = position_x;
		data[j + 13] = position_y + SPRITE_HEIGHT;
		data[j + 14] = texcoord_left;
		data[j + 15] = texcoord_top;
	}

	// actually buffer the vertex data from the array
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glBufferSubData(GL_ARRAY_BUFFER, 0, VERTEX_SIZE * NUM_VERTICES, vertex_buffer);
}