#ifndef TEXTURE_H
#define TEXTURE_H

#include "gl_core_3_3.h"

GLuint make_texture(void* data, int width, int height);
void buffer_data_to_texture(void* data, GLsizei width, GLsizei height, GLuint texture);

void* load_image(const char* file_name, int* width, int* height);
void unload_image(void* data);

#endif
