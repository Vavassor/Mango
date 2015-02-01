#include "Texture.h"

#include "utilities/stb_image.h"
#include "utilities/Logging.h"
#include "utilities/StringManipulation.h"

#include <cstdio>

GLuint make_texture(void* data, int width, int height)
{
	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	return texture;
}

void buffer_data_to_texture(void* data, GLsizei width, GLsizei height, GLuint texture)
{
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
}

void* load_image(const char* filename, int* width, int* height)
{
	// append file name to base path
	char path[128];
	concatenate("resources/images/", filename, path);

	// read in file
	FILE* file = fopen(path, "rb");
	if(file == nullptr)
	{
		LOG_ISSUE("%s : file handle could not be opened", path);
		return nullptr;
	}

	int num_components;
	unsigned char* data = stbi_load_from_file(file, width, height, &num_components, 0);
	if(data == nullptr)
	{
		LOG_ISSUE("%s STB IMAGE ERROR: %s", path, stbi_failure_reason());
		fclose(file);
		return nullptr;
	}

	fclose(file);

	return data;
}

void unload_image(void* data)
{
	stbi_image_free(data);
}