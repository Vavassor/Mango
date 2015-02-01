#include "Tilemap.h"

#include "utilities/Logging.h"
#include "utilities/StringManipulation.h"
#include "utilities/ArrayMacros.h"
#include "utilities/Random.h"

#include <cstdio>

#define TILE_DIMENSION  8
#define PATTERN_COUNT_X 32
#define PATTERN_COUNT_Y 24

Tilemap load_tilemap(const char* filename)
{
	Tilemap map = {};

	// append filename to base path
	char path[128];
	concatenate("resources/tilemaps/", filename, path);

	// read file data
	FILE* file = fopen(path, "rb");
	if(file == nullptr)
	{
		LOG_ISSUE("couldn't open tilemap file: %s", path);
		return map;
	}

	int count_x = 32;
	int count_y = 32;
	int tile_count = count_x * count_y;
	byte_t* tiles = new byte_t[tile_count];
	byte_t* attributes = new byte_t[tile_count];

	// TODO: DO IT RIGHT
	{
		random::sow(7);
		for(int i = 0; i < tile_count; ++i)
		{
			tiles[i] = random::reap_integer(0, 255);
			attributes[i] = random::reap_integer(0, 255);
		}
	}

	fclose(file);

	map.columns = count_x;
	map.rows = count_y;
	map.tiles = tiles;
	map.attributes = attributes;

	return map;
}

void unload_tilemap(const Tilemap& map)
{
	delete[] map.tiles;
	delete[] map.attributes;
}

Mesh create_tilemap_mesh(const Tilemap& map)
{
	GLuint vertex_array;
	glGenVertexArrays(1, &vertex_array);
	glBindVertexArray(vertex_array);

	GLuint buffers[2];
	glGenBuffers(ARRAY_COUNT(buffers), buffers);

	GLsizei vertex_components = (2 + 2);
	GLsizei vertex_width = sizeof(GLfloat) * vertex_components;

	GLsizei tile_count_x = map.columns;
	GLsizei tile_count_y = map.rows;
	GLsizei num_vertices = tile_count_x * tile_count_y * 4;
	GLsizei num_indices = tile_count_x * tile_count_y * 6;

	float texcoord_width =  1.0f / static_cast<float>(PATTERN_COUNT_X);
	float texcoord_height = 1.0f / static_cast<float>(PATTERN_COUNT_Y);

	// load tile data into vertices
	{
		float* data = new float[vertex_components * num_vertices];
		for(int y = 0; y < tile_count_y; ++y)
		{
			for(int x = 0; x < tile_count_x; ++x)
			{
				// Background tiles are numbered from 128 to 383
				int tile_index = y * tile_count_x + x;
				int tile_number = 128 + map.tiles[tile_index];

				// The two banks of tile patterns are stored side-by-side in a texture,
				// so to read from one bank, the total width should be halved.
				float u = static_cast<float>(tile_number % (PATTERN_COUNT_X / 2)) * texcoord_width;
				float v = static_cast<float>(tile_number / (PATTERN_COUNT_X / 2)) * texcoord_height;
				
				// Tiles in the second bank then need to offset their texture coordinates
				// by half to read from the correct side.
				byte_t attribute = map.attributes[tile_index];

				if(attribute & TILE_BANK)
					u += 0.5f;

				// determine texture coordinates for horizontal/vertical tile flipping
				float tex_left = u;
				float tex_right = u + texcoord_width;
				if(attribute & TILE_HORIZONTAL_FLIP)
				{
					tex_left = u + texcoord_width;
					tex_right = u;
				}
				
				float tex_bottom = v;
				float tex_top = v + texcoord_height;
				if(attribute & TILE_VERTICAL_FLIP)
				{
					tex_bottom = v + texcoord_height;
					tex_top = v;
				}

				// load the positions and texture coordinates to the vertex data array
				int i = tile_index * 4 * 4;

				float x_offset = x * TILE_DIMENSION;
				float y_offset = y * TILE_DIMENSION;

				data[i + 0] = x_offset;
				data[i + 1] = y_offset;
				data[i + 2] = tex_left;
				data[i + 3] = tex_bottom;

				data[i + 4] = x_offset + TILE_DIMENSION;
				data[i + 5] = y_offset;
				data[i + 6] = tex_right;
				data[i + 7] = tex_bottom;

				data[i + 8] = x_offset + TILE_DIMENSION;
				data[i + 9] = y_offset + TILE_DIMENSION;
				data[i + 10] = tex_right;
				data[i + 11] = tex_top;

				data[i + 12] = x_offset;
				data[i + 13] = y_offset + TILE_DIMENSION;
				data[i + 14] = tex_left;
				data[i + 15] = tex_top;
			}
		}

		glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
		glBufferData(GL_ARRAY_BUFFER, vertex_width * num_vertices, data, GL_DYNAMIC_DRAW);

		delete[] data;
	}

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, vertex_width, 0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, vertex_width, reinterpret_cast<GLvoid*>(sizeof(GLfloat) * 2));

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	// load static indices
	{
		GLushort* indices = new GLushort[num_indices];
		for(int i = 0; i < tile_count_x * tile_count_y; ++i)
		{
			indices[(i * 6) + 0] = (i * 4) + 0;
			indices[(i * 6) + 1] = (i * 4) + 3;
			indices[(i * 6) + 2] = (i * 4) + 1;
			indices[(i * 6) + 3] = (i * 4) + 1;
			indices[(i * 6) + 4] = (i * 4) + 3;
			indices[(i * 6) + 5] = (i * 4) + 2;
		}

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[1]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * num_indices, indices, GL_STATIC_DRAW);

		delete[] indices;
	}
	
	glBindVertexArray(0);

	Mesh output = {};
	output.vertex_array = vertex_array;
	output.buffers[0] = buffers[0];
	output.buffers[1] = buffers[1];
	output.num_indices = num_indices;

	return output;
}