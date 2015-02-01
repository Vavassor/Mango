#ifndef MESH_H
#define MESH_H

#include "gl_core_3_3.h"

struct Mesh
{
	GLuint vertex_array;
	GLuint buffers[2];
	GLsizei num_indices;
};

inline void destroy_mesh(const Mesh& mesh)
{
	glDeleteBuffers(sizeof(mesh.buffers) / sizeof(*mesh.buffers), mesh.buffers);
	glDeleteVertexArrays(1, &mesh.vertex_array);
}

#endif
