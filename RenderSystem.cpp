#include "RenderSystem.h"
#include "Shader.h"
#include "Texture.h"
#include "Mesh.h"
#include "Tilemap.h"
#include "SpriteBatch.h"
#include "Game.h"

#include "utilities/ArrayMacros.h"
#include "utilities/Logging.h"

#include "gl_core_3_3.h"

#include <cassert>

namespace RenderSystem {

namespace
{
	GLuint target_textures[1];
	GLuint framebuffers[1];

	GLuint default_shader;
	GLuint samplers[1];
	
	struct ObjectBlock
	{
		GLfloat model_view_projection[16];
	};
	GLuint object_uniform_buffer;

	GLfloat projection_matrix[16];

	GLuint tile_atlas;
	Mesh tilemap_mesh;
	Mesh sprite_batch_mesh;
	
	Mesh framebuffer_mesh;
	GLfloat framebuffer_mesh_matrix[16] =
	{
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1,
	};

	int frame_width, frame_height;
	int magnification = 1;
}

static const char* opengl_error_text(GLenum error_code)
{
	switch(error_code)
	{
		case GL_NO_ERROR:						return "NO_ERROR";
		case GL_INVALID_OPERATION:				return "INVALID_OPERATION";
		case GL_INVALID_ENUM:					return "INVALID_ENUM";
		case GL_INVALID_VALUE:					return "INVALID_VALUE";
		case GL_OUT_OF_MEMORY:					return "OUT_OF_MEMORY";
		case GL_INVALID_FRAMEBUFFER_OPERATION:	return "INVALID_FRAMEBUFFER_OPERATION";
	}
	return "UNKNOWN ERROR";
}

static const char* framebuffer_status_text(GLenum error_code)
{
	switch(error_code)
	{
		case GL_FRAMEBUFFER_COMPLETE:                      return "FRAMEBUFFER_COMPLETE";
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:         return "FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: return "FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:        return "FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:        return "FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
		case GL_FRAMEBUFFER_UNSUPPORTED:                   return "FRAMEBUFFER_UNSUPPORTED";
	}
	return "UNKNOWN ERROR";
}

static inline void copy_matrix(const float from[16], float to[16])
{
	for(int i = 0; i < 16; ++i)
		to[i] = from[i];
}

static bool check_error(const char* description)
{
	GLenum error = glGetError();
	if(error == GL_NO_ERROR) return false;

	const char* error_name = opengl_error_text(error);
	LOG_ISSUE("OpenGL: %s - %s", description, error_name);
	return true;
}

bool Initialise(int target_width, int target_height, int scale)
{
	// create target textures
	glGenTextures(ARRAY_COUNT(target_textures), target_textures);

	assert(target_textures[0] != 0);
	glBindTexture(GL_TEXTURE_2D, target_textures[0]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, target_width, target_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	if(check_error("framebuffer target texture creation failed"))
		return false;

	// create framebuffer and attach targets
	{
		glGenFramebuffers(ARRAY_COUNT(framebuffers), framebuffers);

		assert(framebuffers[0] != 0);
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[0]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target_textures[0], 0);

		GLenum draw_buffers[] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(ARRAY_COUNT(draw_buffers), draw_buffers);

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if(status != GL_FRAMEBUFFER_COMPLETE)
		{
			LOG_ISSUE("OpenGL: framebuffer incomplete - %s", framebuffer_status_text(status));
			return false;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	// load shaders
	default_shader = load_shader_program("default.vert", "default.frag");
	if(default_shader == 0) return false;

	// create samplers
	{
		glGenSamplers(ARRAY_COUNT(samplers), samplers);

		assert(samplers[0] != 0);
		
		GLuint pixel_perfect = samplers[0];
		glSamplerParameteri(pixel_perfect, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glSamplerParameteri(pixel_perfect, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glSamplerParameteri(pixel_perfect, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glSamplerParameteri(pixel_perfect, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		if(check_error("sampler creation failed"))
			return false;

		// set default samplers to texture units
		glActiveTexture(GL_TEXTURE0);
		glBindSampler(0, pixel_perfect);
	}

	// create uniform buffers
	{
		GLuint buffer;
		glGenBuffers(1, &buffer);
		glBindBuffer(GL_UNIFORM_BUFFER, buffer);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(ObjectBlock), nullptr, GL_STREAM_DRAW);
		object_uniform_buffer = buffer;
	}

	// bind uniform buffers to shaders
	{
		GLuint block_index = glGetUniformBlockIndex(default_shader, "ObjectBlock");
		glUniformBlockBinding(default_shader, block_index, 0);
		glBindBuffer(GL_UNIFORM_BUFFER, object_uniform_buffer);
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, object_uniform_buffer);
	}

	// set uniform locations for shaders
	{
		glUseProgram(default_shader);
		GLint location = glGetUniformLocation(default_shader, "texture");
		glUniform1i(location, 0);
	}

	// create framebuffer mesh
	{
		glGenVertexArrays(1, &framebuffer_mesh.vertex_array);
		glBindVertexArray(framebuffer_mesh.vertex_array);

		glGenBuffers(ARRAY_COUNT(framebuffer_mesh.buffers), framebuffer_mesh.buffers);

		GLfloat vertices[] =
		{
			-1, 1, 0, 0,
			1, 1, 1, 0,
			1, -1, 1, 1,
			-1, -1, 0, 1,
		};
		glBindBuffer(GL_ARRAY_BUFFER, framebuffer_mesh.buffers[0]);
		glBufferData(GL_ARRAY_BUFFER, sizeof vertices, vertices, GL_STATIC_DRAW);

		GLsizei stride = sizeof(GLfloat) * 4;
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, 0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<GLvoid*>(sizeof(GLfloat) * 2));

		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);

		GLushort indices[] = { 0, 3, 1, 1, 3, 2 };
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, framebuffer_mesh.buffers[1]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof indices, indices, GL_STATIC_DRAW);

		framebuffer_mesh.num_indices = ARRAY_COUNT(indices);

		glBindVertexArray(0);
	}

	// set projection matrix
	{
		GLfloat orthographic[] =
		{
			2.0f / target_width, 0, 0, 0,
			0, 2.0f / target_height, 0, 0,
			0, 0, -1, 0,
			-1, -1, 0, 1,
		};
		copy_matrix(orthographic, projection_matrix);
	}

	tile_atlas = make_texture(nullptr, 256, 192);
	if(check_error("failed to make texture for storing tile atlas"))
		return false;

	sprite_batch_mesh = create_sprite_batch_mesh();
	if(check_error("sprite batch mesh creation failed"))
		return false;

	frame_width = target_width;
	frame_height = target_height;
	magnification = scale;
	
	return true;
}

void Terminate()
{
	destroy_mesh(sprite_batch_mesh);
	destroy_mesh(tilemap_mesh);
	glDeleteTextures(1, &tile_atlas);
	destroy_mesh(framebuffer_mesh);

	glDeleteBuffers(1, &object_uniform_buffer);

	glDeleteSamplers(ARRAY_COUNT(samplers), samplers);
	glDeleteProgram(default_shader);

	glDeleteTextures(ARRAY_COUNT(target_textures), target_textures);
	glDeleteFramebuffers(ARRAY_COUNT(framebuffers), framebuffers);
}

void Load_Map(const Tilemap& map, const char* pattern_filename)
{
	tilemap_mesh = create_tilemap_mesh(map);

	// load tile atlas with pattern image
	{
		int width, height;
		void* data = load_image(pattern_filename, &width, &height);
		buffer_data_to_texture(data, width, height, tile_atlas);
		unload_image(data);
	}
}

static void Set_MVP_Matrix(GLfloat matrix[16])
{
	ObjectBlock block;
	copy_matrix(matrix, block.model_view_projection);

	glBindBuffer(GL_UNIFORM_BUFFER, object_uniform_buffer);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof block, &block);
}

static void Draw_Mesh(const Mesh& mesh)
{
	glBindVertexArray(mesh.vertex_array);
	glDrawElements(GL_TRIANGLES, mesh.num_indices, GL_UNSIGNED_SHORT, nullptr);
}

void Update(const Game::GameState& game)
{
	// check game state for anything new
	if(game.load_map)
	{
		Load_Map(*game.tilemap, game.atlas_name);
	}

	// draw to framebuffer textures
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[0]);
	glViewport(0, 0, frame_width, frame_height);
	GLfloat clear_color[4] = { 1.0f, 0.0f, 1.0f, 1.0f };
	glClearBufferfv(GL_COLOR, 0, clear_color);

	glUseProgram(default_shader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tile_atlas);
	Set_MVP_Matrix(projection_matrix);

	Draw_Mesh(tilemap_mesh);

	buffer_sprites(game.sprites, sprite_batch_mesh.buffers[0]);
	Draw_Mesh(sprite_batch_mesh);

	// draw framebuffer texture to rendering context
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, magnification * frame_width, magnification * frame_height);
	glClear(GL_COLOR_BUFFER_BIT);
	
	glUseProgram(default_shader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, target_textures[0]);
	Set_MVP_Matrix(framebuffer_mesh_matrix);
	Draw_Mesh(framebuffer_mesh);
}

} // namespace GLRenderer